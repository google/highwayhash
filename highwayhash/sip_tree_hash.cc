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

#include "highwayhash/sip_tree_hash.h"

#ifdef __AVX2__
#include <cstring>  // memcpy

#include "highwayhash/sip_hash.h"
#include "highwayhash/vec2.h"

namespace highwayhash {
namespace {

// Paper: https://www.131002.net/siphash/siphash.pdf
// SSE41 implementation: https://goo.gl/80GBSD
// Tree hash extension: http://dx.doi.org/10.4236/jis.2014.53010

// The hash state is updated by injecting 4x8-byte packets;
// XORing together all state vectors yields 32 bytes that are
// reduced to 64 bits via 8-byte SipHash.

const int kPacketSize = 32;
const int kNumLanes = kPacketSize / sizeof(uint64);

// 32 bytes key. Parameters are hardwired to c=2, d=4 [rounds].
class SipTreeHashState {
 public:
  explicit HIGHWAYHASH_INLINE SipTreeHashState(const uint64 (&keys)[kNumLanes]) {
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

  HIGHWAYHASH_INLINE void Update(const V4x64U& packet) {
    v3 ^= packet;

    Compress<2>();

    v0 ^= packet;
  }

  HIGHWAYHASH_INLINE V4x64U Finalize() {
    // Mix in bits to avoid leaking the key if all packets were zero.
    v2 ^= V4x64U(0xFF);

    Compress<4>();

    return (v0 ^ v1) ^ (v2 ^ v3);
  }

 private:
  static HIGHWAYHASH_INLINE V4x64U RotateLeft16(const V4x64U& v) {
    const V4x64U control(0x0D0C0B0A09080F0EULL, 0x0504030201000706ULL,
                         0x0D0C0B0A09080F0EULL, 0x0504030201000706ULL);
    return V4x64U(_mm256_shuffle_epi8(v, control));
  }

  // Rotates each 64-bit element of "v" left by N bits.
  template <uint64 bits>
  static HIGHWAYHASH_INLINE V4x64U RotateLeft(const V4x64U& v) {
    const V4x64U left = v << bits;
    const V4x64U right = v >> (64 - bits);
    return left | right;
  }

  static HIGHWAYHASH_INLINE V4x64U Rotate32(const V4x64U& v) {
    return V4x64U(_mm256_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
  }

  template <size_t rounds>
  HIGHWAYHASH_INLINE void Compress() {
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
static HIGHWAYHASH_INLINE
V4x64U LoadFinalPacket32(const char* bytes, const uint64 size,
                         const uint64 remainder) {
  // Copying into an aligned buffer incurs a store-to-load-forwarding stall.
  // Instead, we use masked loads to read any remaining whole uint32
  // without incurring page faults for the others.
  const size_t remaining_32 = remainder >> 2;  // 0..7

  // mask[32*i+31] := uint32 #i valid/accessible ? 1 : 0.
  // To avoid large lookup tables, we pack uint32 lanes into bytes,
  // compute the packed mask by shifting, and then sign-extend 0xFF to
  // 0xFFFFFFFF (although only the MSB needs to be set).
  // remaining_32 = 0 => mask = 00000000; remaining_32 = 7 => mask = 01111111.
  const uint64 packed_mask = 0x00FFFFFFFFFFFFFFULL >> ((7 - remaining_32) * 8);
  const V4x64U mask(_mm256_cvtepi8_epi32(_mm_cvtsi64_si128(packed_mask)));
  // Load 0..7 remaining (potentially unaligned) uint32.
  const V4x64U packet28(
      _mm256_maskload_epi32(reinterpret_cast<const int*>(bytes), mask));

  // Load any remaining bytes individually and combine into a uint32.
  const int remainder_mod4 = remainder & 3;
  // Length padding ensures that zero-valued buffers of different lengths
  // result in different hashes.
  uint32 packet4 = static_cast<uint32>(remainder << 24);
  const char* final_bytes = bytes + (remaining_32 * 4);
  for (int i = 0; i < remainder_mod4; ++i) {
    const uint32 byte = static_cast<unsigned char>(final_bytes[i]);
    packet4 += byte << (i * 8);
  }

  // The upper 4 bytes of packet28 are zero; replace with packet4 to
  // obtain the (length-padded) 32-byte packet.
  const __m256i v4 = _mm256_broadcastd_epi32(_mm_cvtsi32_si128(packet4));
  const V4x64U packet(_mm256_blend_epi32(packet28, v4, 0x80));
  return packet;
}

}  // namespace

uint64 SipTreeHash(const uint64 (&key)[kNumLanes], const char* bytes,
                   const uint64 size) {
  SipTreeHashState state(key);

  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64* packets = reinterpret_cast<const uint64*>(bytes);
  for (size_t i = 0; i < truncated_size / sizeof(uint64); i += kNumLanes) {
    const V4x64U packet = LoadU(packets + i);
    state.Update(packet);
  }

  const V4x64U final_packet =
      LoadFinalPacket32(bytes + truncated_size, size, remainder);

  state.Update(final_packet);

  // Faster than passing __m256i and extracting.
  alignas(64) uint64 hashes[kNumLanes];
  Store(state.Finalize(), hashes);

  SipHashState::Key reduce_key;
  memcpy(&reduce_key, &key, sizeof(reduce_key));
  return ReduceSipTreeHash(reduce_key, hashes);
}

}  // namespace highwayhash

using highwayhash::uint64;
using highwayhash::SipTreeHash;
using Key = uint64[4];

extern "C" {

uint64 SipTreeHashC(const uint64* key, const char* bytes, const uint64 size) {
  return SipTreeHash(*reinterpret_cast<const Key*>(key), bytes, size);
}

}  // extern "C"

#endif  // #ifdef __AVX2__
