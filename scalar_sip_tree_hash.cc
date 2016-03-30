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

#include "scalar_sip_tree_hash.h"
#include "sip_hash.h"

#include <cstring>  // memcpy
#include "code_annotation.h"

namespace {

// Paper: https://www.131002.net/siphash/siphash.pdf
// SSE41 implementation: https://goo.gl/80GBSD
// Tree hash extension: http://dx.doi.org/10.4236/jis.2014.53010

// The hash state is updated by injecting 4x8-byte packets;
// XORing together all state vectors yields 32 bytes that are
// reduced to 64 bits via 8-byte SipHash.

const int kNumLanes = 4;
using Lanes = uint64_t[kNumLanes];
const int kPacketSize = sizeof(Lanes);

class ScalarSipTreeHashState {
 public:
  INLINE ScalarSipTreeHashState(const Lanes& keys, const int lane) {
    const uint64_t key = keys[lane] ^ (kNumLanes | lane);
    v0 = 0x736f6d6570736575ull ^ key;
    v1 = 0x646f72616e646f6dull ^ key;
    v2 = 0x6c7967656e657261ull ^ key;
    v3 = 0x7465646279746573ull ^ key;
  }

  INLINE void Update(const uint64_t& packet) {
    v3 ^= packet;

    Compress<2>();

    v0 ^= packet;
  }

  INLINE uint64_t Finalize() {
    // Mix in bits to avoid leaking the key if all packets were zero.
    v2 ^= 0xFF;

    Compress<4>();

    return (v0 ^ v1) ^ (v2 ^ v3);
  }

 private:
  // Rotate a 64-bit value "v" left by N bits.
  template <uint64_t bits>
  static INLINE uint64_t RotateLeft(const uint64_t v) {
    const uint64_t left = v << bits;
    const uint64_t right = v >> (64 - bits);
    return left | right;
  }

  template <size_t rounds>
  INLINE void Compress() {
    for (size_t i = 0; i < rounds; ++i) {
      // ARX network: add, rotate, exclusive-or.
      v0 += v1;
      v2 += v3;
      v1 = RotateLeft<13>(v1);
      v3 = RotateLeft<16>(v3);
      v1 ^= v0;
      v3 ^= v2;

      v0 = RotateLeft<32>(v0);

      v2 += v1;
      v0 += v3;
      v1 = RotateLeft<17>(v1);
      v3 = RotateLeft<21>(v3);
      v1 ^= v2;
      v3 ^= v0;

      v2 = RotateLeft<32>(v2);
    }
  }

  uint64_t v0;
  uint64_t v1;
  uint64_t v2;
  uint64_t v3;
};

}  // namespace

uint64_t ScalarSipTreeHash(const Lanes& key,
                           const uint8_t* bytes, const uint64_t size) {
  // "j-lanes" tree hashing interleaves 8-byte input packets.
  ScalarSipTreeHashState state[kNumLanes] = {
      ScalarSipTreeHashState(key, 0), ScalarSipTreeHashState(key, 1),
      ScalarSipTreeHashState(key, 2), ScalarSipTreeHashState(key, 3)};

  // Hash entire 32-byte packets.
  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64_t* packets = reinterpret_cast<const uint64_t*>(bytes);
  for (size_t i = 0; i < truncated_size / kPacketSize; ++i) {
    for (int lane = 0; lane < kNumLanes; ++lane) {
      const uint64_t packet = *packets++;
      state[lane].Update(packet);
    }
  }

  // Update with final 32-byte packet.
  const size_t remainder_mod4 = remainder & 3;
  uint32_t packet4 = remainder << 24;
  const uint8_t* final_bytes = bytes + size - remainder_mod4;
  for (size_t i = 0; i < remainder_mod4; ++i) {
    packet4 += static_cast<uint32_t>(final_bytes[i]) << (i * 8);
  }

  uint8_t final_packet[kPacketSize] = {0};
  memcpy(final_packet, bytes + truncated_size, remainder - remainder_mod4);
  memcpy(final_packet + kPacketSize - 4, &packet4, sizeof(packet4));
  packets = reinterpret_cast<const uint64_t*>(final_packet);
  for (int lane = 0; lane < kNumLanes; ++lane) {
    state[lane].Update(packets[lane]);
  }

  // Store the resulting hashes.
  Lanes hashes;
  for (int lane = 0; lane < kNumLanes; ++lane) {
    hashes[lane] = state[lane].Finalize();
  }

  return ReduceSipTreeHash(key, hashes);
}
