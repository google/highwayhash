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

#include "highwayhash/scalar_highway_tree_hash.h"

#include <cstring>  // memcpy
#include "highwayhash/code_annotation.h"
#include "highwayhash/vec2.h"

namespace highwayhash {
namespace {

const int kNumLanes = 4;
using Lanes = uint64[kNumLanes];
const int kPacketSize = sizeof(Lanes);

class ScalarHighwayTreeHashState {
 public:
  INLINE ScalarHighwayTreeHashState(const Lanes& keys) {
    const Lanes init0 = {0xdbe6d5d5fe4cce2full, 0xa4093822299f31d0ull,
                         0x13198a2e03707344ull, 0x243f6a8885a308d3ull};
    const Lanes init1 = {0x3bd39e10cb0ef593ull, 0xc0acf169b5f18a8cull,
                         0xbe5466cf34e90c6cull, 0x452821e638d01377ull};
    Lanes permuted_keys;
    Permute(keys, &permuted_keys);
    for (int lane = 0; lane < kNumLanes; ++lane) {
      v0[lane] = init0[lane] ^ keys[lane];
      v1[lane] = init1[lane] ^ permuted_keys[lane];
      mul0[lane] = init0[lane];
      mul1[lane] = init1[lane];
    }
  }

  INLINE void Update(const uint64* packets) {
    const uint64 mask32 = 0xFFFFFFFFULL;

    for (int lane = 0; lane < kNumLanes; ++lane) {
      v1[lane] += packets[lane];
      v1[lane] += mul0[lane];
      const uint64 v0_32 = v0[lane] & mask32;
      const uint64 v1_32 = v1[lane] & mask32;
      mul0[lane] ^= v0_32 * (v1[lane] >> 32);
      v0[lane] += mul1[lane];
      mul1[lane] ^= v1_32 * (v0[lane] >> 32);
    }

    Lanes merged1;
    ZipperMerge(reinterpret_cast<const char*>(&v1),
                reinterpret_cast<char*>(&merged1));
    for (int lane = 0; lane < kNumLanes; ++lane) {
      v0[lane] += merged1[lane];
    }

    Lanes merged0;
    ZipperMerge(reinterpret_cast<const char*>(&v0),
                reinterpret_cast<char*>(&merged0));
    for (int lane = 0; lane < kNumLanes; ++lane) {
      v1[lane] += merged0[lane];
    }
  }

  INLINE uint64 Finalize() {
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    return v0[0] + v1[0] + mul0[0] + mul1[0];
  }

 private:
  static INLINE void ZipperMerge(const char* mul0, char* v0) {
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

  static INLINE uint64 Rot32(const uint64 x) { return (x >> 32) | (x << 32); }

  static INLINE void Permute(const Lanes& v, Lanes* permuted) {
    (*permuted)[0] = Rot32(v[2]);
    (*permuted)[1] = Rot32(v[3]);
    (*permuted)[2] = Rot32(v[0]);
    (*permuted)[3] = Rot32(v[1]);
  }

  INLINE void PermuteAndUpdate() {
    Lanes permuted;
    Permute(v0, &permuted);
    Update(permuted);
  }

  uint64 v0[kNumLanes];
  uint64 v1[kNumLanes];
  uint64 mul0[kNumLanes];
  uint64 mul1[kNumLanes];
};

}  // namespace

uint64 ScalarHighwayTreeHash(const Lanes& key, const char* bytes,
                             const uint64 size) {
  // "j-lanes" tree hashing interleaves 8-byte input packets.
  ScalarHighwayTreeHashState state(key);

  // Hash entire 32-byte packets.
  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64* packets = reinterpret_cast<const uint64*>(bytes);
  for (size_t i = 0; i < truncated_size / sizeof(uint64); i += kNumLanes) {
    state.Update(packets + i);
  }

  // Update with final 32-byte packet.
  const size_t remainder_mod4 = remainder & 3;
  uint32 packet4 = static_cast<uint32>(size) << 24;
  const char* final_bytes = bytes + size - remainder_mod4;
  for (size_t i = 0; i < remainder_mod4; ++i) {
    const uint32 byte = static_cast<unsigned char>(final_bytes[i]);
    packet4 += byte << (i * 8);
  }

  char final_packet[kPacketSize] = {0};
  memcpy(final_packet, bytes + truncated_size, remainder - remainder_mod4);
  memcpy(final_packet + kPacketSize - 4, &packet4, sizeof(packet4));
  packets = reinterpret_cast<const uint64*>(final_packet);
  state.Update(packets);

  return state.Finalize();
}

}  // namespace highwayhash
