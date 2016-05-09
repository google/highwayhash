// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "scalar_sip_hash.h"
#include "code_annotation.h"

#include <cstring>  // memcpy

namespace {

// Paper: https://www.131002.net/siphash/siphash.pdf

// The hash state is updated by injecting 8-byte packets.
static const size_t kPacketSize = sizeof(uint64_t);

class ScalarSipHashState {
 public:
  explicit INLINE ScalarSipHashState(const uint64_t key[2]) {
    v0 = 0x736f6d6570736575ull ^ key[0];
    v1 = 0x646f72616e646f6dull ^ key[1];
    v2 = 0x6c7967656e657261ull ^ key[0];
    v3 = 0x7465646279746573ull ^ key[1];
  }

  INLINE void Update(const uint64_t packet) {
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

uint64_t ScalarSipHash(const uint64_t key[2], const uint8_t* bytes,
                       const uint64_t size) {
  ScalarSipHashState state(key);

  // Hash entire packets.
  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64_t* packets = reinterpret_cast<const uint64_t*>(bytes);
  for (size_t i = 0; i < truncated_size / kPacketSize; ++i) {
    state.Update(packets[i]);
  }

  // Update with final packet (avoid overrunning the input buffer).
  uint8_t final_packet[kPacketSize] = {0};
  memcpy(final_packet, bytes + truncated_size, remainder);
  final_packet[kPacketSize - 1] = static_cast<uint8_t>(size & 0xFF);
  packets = reinterpret_cast<const uint64_t*>(final_packet);
  state.Update(packets[0]);

  return state.Finalize();
}
