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

#include "scalar_highway_tree_hash.h"

#include <cstring>  // memcpy
#include "vec2.h"

namespace {

const int kNumLanes = 4;
using Lanes = uint64_t[kNumLanes];
const int kPacketSize = sizeof(Lanes);

class ScalarHighwayTreeHashState {
 public:
  INLINE ScalarHighwayTreeHashState(const Lanes& keys) {
    const Lanes init0 = {0xdbe6d5d5fe4cce2full, 0xa4093822299f31d0ull,
                         0x13198a2e03707344ull, 0x243f6a8885a308d3ull};
    const Lanes init1 = {0x3bd39e10cb0ef593ull, 0xc0acf169b5f18a8cull,
                         0xbe5466cf34e90c6cull, 0x452821e638d01377ull};
    for (int lane = 0; lane < kNumLanes; ++lane) {
      const uint64_t key = keys[lane];
      v0[lane] = init0[lane] ^ key;
      v1[lane] = init1[lane] ^ key;
    }
  }

  INLINE void Update(const uint64_t* packets) {
    Lanes mul0, mul1;
    for (int lane = 0; lane < kNumLanes; ++lane) {
      v1[lane] += packets[lane];
      const uint64_t mask32 = 0xFFFFFFFFULL;
      v0[lane] |= 0x70000001ULL;
      const uint64_t mul32 = v0[lane] & mask32;
      mul0[lane] = mul32 * (v1[lane] & mask32);
      mul1[lane] = mul32 * (v1[lane] >> 32);
    }

    Lanes merged;
    ZipperMerge(reinterpret_cast<const uint8_t*>(&mul0),
                reinterpret_cast<uint8_t*>(&merged));
    for (int lane = 0; lane < kNumLanes; ++lane) {
      v0[lane] += merged[lane];
      v1[lane] += mul1[lane];
    }
  }

  INLINE uint64_t Finalize() {
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    return v0[0] + v1[0];
  }

 private:
  static INLINE void ZipperMerge(const uint8_t* mul0, uint8_t* v0) {
    for (int half = 0; half < kPacketSize; half += kPacketSize / 2) {
      v0[half + 0] = mul0[half + 3];
      v0[half + 1] = mul0[half + 12];
      v0[half + 2] = mul0[half + 2];
      v0[half + 3] = mul0[half + 5];
      v0[half + 4] = mul0[half + 14];
      v0[half + 5] = mul0[half + 1];
      v0[half + 6] = mul0[half + 15];
      v0[half + 7] = mul0[half + 0];
      v0[half + 8] = mul0[half + 11];
      v0[half + 9] = mul0[half + 4];
      v0[half + 10] = mul0[half + 10];
      v0[half + 11] = mul0[half + 13];
      v0[half + 12] = mul0[half + 9];
      v0[half + 13] = mul0[half + 6];
      v0[half + 14] = mul0[half + 8];
      v0[half + 15] = mul0[half + 7];
    }
  }

  static INLINE uint64_t Rot32(const uint64_t x) {
    return (x >> 32) | (x << 32);
  }

  INLINE void PermuteAndUpdate() {
    const Lanes permuted = {Rot32(v0[2]), Rot32(v0[3]), Rot32(v0[0]),
                            Rot32(v0[1])};
    Update(permuted);
  }

  uint64_t v0[kNumLanes];
  uint64_t v1[kNumLanes];
};

}  // namespace

uint64_t ScalarHighwayTreeHash(const Lanes& key, const uint8_t* bytes,
                               const uint64_t size) {
  // "j-lanes" tree hashing interleaves 8-byte input packets.
  ScalarHighwayTreeHashState state(key);

  // Hash entire 32-byte packets.
  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64_t* packets = reinterpret_cast<const uint64_t*>(bytes);
  for (size_t i = 0; i < truncated_size / sizeof(uint64_t); i += kNumLanes) {
    state.Update(packets + i);
  }

  // Update with final 32-byte packet.
  const size_t remainder_mod4 = remainder & 3;
  uint32_t packet4 = static_cast<uint32_t>(size) << 24;
  const uint8_t* final_bytes = bytes + size - remainder_mod4;
  for (size_t i = 0; i < remainder_mod4; ++i) {
    packet4 += static_cast<uint32_t>(final_bytes[i]) << (i * 8);
  }

  uint8_t final_packet[kPacketSize] = {0};
  memcpy(final_packet, bytes + truncated_size, remainder - remainder_mod4);
  memcpy(final_packet + kPacketSize - 4, &packet4, sizeof(packet4));
  packets = reinterpret_cast<const uint64_t*>(final_packet);
  state.Update(packets);

  return state.Finalize();
}
