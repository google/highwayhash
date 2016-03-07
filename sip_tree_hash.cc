// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "sip_tree_hash.h"

#include <cstring>  // memcpy
#include "sip_hash.h"
#include "vec2.h"

namespace {

// Paper: https://www.131002.net/siphash/siphash.pdf
// SSE41 implementation: https://goo.gl/80GBSD
// Tree hash extension: http://dx.doi.org/10.4236/jis.2014.53010

// The hash state is updated by injecting 4x8-byte packets;
// XORing together all state vectors yields 32 bytes that are
// reduced to 64 bits via 8-byte SipHash.

const int kPacketSize = 32;
const int kNumLanes = kPacketSize / sizeof(uint64_t);

// 32 bytes key. Parameters are hardwired to c=2, d=4 [rounds].
class SipTreeHashState {
 public:
  explicit INLINE SipTreeHashState(const uint64_t (&keys)[kNumLanes]) {
    const V4x64U init(0x7465646279746573ull, 0x6c7967656e657261ull,
                      0x646f72616e646f6dull, 0x736f6d6570736575ull);
    const V4x64U lanes(kNumLanes | 3, kNumLanes | 2, kNumLanes | 1,
                       kNumLanes | 0);
    const V4x64U key = LoadU(keys) ^ lanes;
    v0 = V4x64U(_mm256_permute4x64_epi64(init, 0x00)) ^ key;
    v1 = V4x64U(_mm256_permute4x64_epi64(init, 0x55)) ^ key;
    v2 = V4x64U(_mm256_permute4x64_epi64(init, 0xAA)) ^ key;
    v3 = V4x64U(_mm256_permute4x64_epi64(init, 0xFF)) ^ key;
  }

  INLINE void Update(const V4x64U& packet) {
    v3 ^= packet;

    Compress<2>();

    v0 ^= packet;
  }

  INLINE V4x64U Finalize() {
    // Mix in bits to avoid leaking the key if all packets were zero.
    v2 ^= V4x64U(0xFF);

    Compress<4>();

    return (v0 ^ v1) ^ (v2 ^ v3);
  }

 private:
  static INLINE V4x64U RotateLeft16(const V4x64U& v) {
    const V4x64U control(0x0D0C0B0A09080F0EULL, 0x0504030201000706ULL,
                         0x0D0C0B0A09080F0EULL, 0x0504030201000706ULL);
    return V4x64U(_mm256_shuffle_epi8(v, control));
  }

  // Rotates each 64-bit element of "v" left by N bits.
  template <uint64_t bits>
  static INLINE V4x64U RotateLeft(const V4x64U& v) {
    const V4x64U left = v << bits;
    const V4x64U right = v >> (64 - bits);
    return left | right;
  }

  static INLINE V4x64U Rotate32(const V4x64U& v) {
    return V4x64U(_mm256_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
  }

  template <size_t rounds>
  INLINE void Compress() {
    // Loop is faster than unrolling!
    for (size_t i = 0; i < rounds; ++i) {
      // ARX network: add, rotate, exclusive-or.
      v0 += v1;
      v2 += v3;
      v1 = RotateLeft<13>(v1);
      v3 = RotateLeft16(v3);
      v1 ^= v0;
      v3 ^= v2;

      v0 = Rotate32(v0);

      v2 += v1;
      v0 += v3;
      v1 = RotateLeft<17>(v1);
      v3 = RotateLeft<21>(v3);
      v1 ^= v2;
      v3 ^= v0;

      v2 = Rotate32(v2);
    }
  }

  V4x64U v0;
  V4x64U v1;
  V4x64U v2;
  V4x64U v3;
};

// Returns 32-byte packet by loading the remaining 0..31 bytes, storing
// "remainder" in the upper byte, and zeroing any intervening bytes.
// "remainder" is the number of accessible/remaining bytes (size % 32).
// Loading past the end of the input risks page fault exceptions which even
// LDDQU cannot prevent.
static INLINE V4x64U LoadFinalPacket32(const uint8_t* bytes,
                                       const uint64_t size,
                                       const uint64_t remainder) {
  // Copying into an aligned buffer incurs a store-to-load-forwarding stall.
  // Instead, we use masked loads to read any remaining whole uint32_t
  // without incurring page faults for the others.
  const size_t remaining_32 = remainder >> 2;  // 0..7

  // mask[32*i+31] := uint32_t #i valid/accessible ? 1 : 0.
  // To avoid large lookup tables, we pack uint32_t lanes to uint8_t,
  // compute the packed mask by shifting, and then sign-extend 0xFF to
  // 0xFFFFFFFF (although only the MSB needs to be set).
  // remaining_32 = 0 => mask = 00000000; remaining_32 = 7 => mask = 01111111.
  const uint64_t packed_mask =
      0x00FFFFFFFFFFFFFFULL >> ((7 - remaining_32) * 8);
  const V4x64U mask(_mm256_cvtepi8_epi32(_mm_cvtsi64_si128(packed_mask)));
  // Load 0..7 remaining (potentially unaligned) uint32_t.
  const V4x64U packet28(
      _mm256_maskload_epi32(reinterpret_cast<const int*>(bytes), mask));

  // Load any remaining bytes individually and combine into a uint32_t.
  const int remainder_mod4 = remainder & 3;
  // Length padding ensures that zero-valued buffers of different lengths
  // result in different hashes.
  uint32_t packet4 = remainder << 24;
  const uint8_t* final_bytes = bytes + (remaining_32 * 4);
  for (int i = 0; i < remainder_mod4; ++i) {
    packet4 += static_cast<uint32_t>(final_bytes[i]) << (i * 8);
  }

  // The upper 4 bytes of packet28 are zero; replace with packet4 to
  // obtain the (length-padded) 32-byte packet.
  const __m256i v4 = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(packet4));
  const V4x64U packet(_mm256_blend_epi32(packet28, v4, 0x80));
  return packet;
}

}  // namespace

uint64_t SipTreeHash(const uint64_t (&key)[kNumLanes], const uint8_t* bytes,
                     const uint64_t size) {
  SipTreeHashState state(key);

  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64_t* packets = reinterpret_cast<const uint64_t*>(bytes);
  for (size_t i = 0; i < truncated_size / sizeof(uint64_t); i += kNumLanes) {
    const V4x64U packet = LoadU(packets + i);
    state.Update(packet);
  }

  const V4x64U final_packet =
      LoadFinalPacket32(bytes + truncated_size, size, remainder);

  state.Update(final_packet);

  // Faster than passing __m256i and extracting.
  ALIGNED(uint64_t, 64) hashes[kNumLanes];
  Store(state.Finalize(), hashes);

  return ReduceSipTreeHash(key, hashes);
}
