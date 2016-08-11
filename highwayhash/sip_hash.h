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

#ifndef HIGHWAYHASH_HIGHWAYHASH_SIP_HASH_H_
#define HIGHWAYHASH_HIGHWAYHASH_SIP_HASH_H_

// Portable but fast SipHash implementation.

#include <cstddef>
#include <cstring>  // memcpy

#include "highwayhash/code_annotation.h"
#include "highwayhash/state_helpers.h"
#include "highwayhash/types.h"

namespace highwayhash {

// Paper: https://www.131002.net/siphash/siphash.pdf
class SipHashState {
 public:
  using Key = uint64[2];
  static const size_t kPacketSize = sizeof(uint64);

  explicit INLINE SipHashState(const Key& key) {
    v0 = 0x736f6d6570736575ull ^ key[0];
    v1 = 0x646f72616e646f6dull ^ key[1];
    v2 = 0x6c7967656e657261ull ^ key[0];
    v3 = 0x7465646279746573ull ^ key[1];
  }

  INLINE void Update(const char* bytes) {
    uint64 packet;
    memcpy(&packet, bytes, sizeof(packet));

    v3 ^= packet;

    Compress<2>();

    v0 ^= packet;
  }

  INLINE uint64 Finalize() {
    // Mix in bits to avoid leaking the key if all packets were zero.
    v2 ^= 0xFF;

    Compress<4>();

    return (v0 ^ v1) ^ (v2 ^ v3);
  }

 private:
  // Rotate a 64-bit value "v" left by N bits.
  template <uint64 bits>
  static INLINE uint64 RotateLeft(const uint64 v) {
    const uint64 left = v << bits;
    const uint64 right = v >> (64 - bits);
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

  uint64 v0;
  uint64 v1;
  uint64 v2;
  uint64 v3;
};

// Override the HighwayTreeHash padding scheme with that of SipHash so that
// the hash output matches the known-good values in sip_hash_test.
template <>
INLINE void PaddedUpdate<SipHashState>(const uint64 size,
                                       const char* remaining_bytes,
                                       const uint64 remaining_size,
                                       SipHashState* state) {
  // Copy to avoid overrunning the input buffer.
  char final_packet[SipHashState::kPacketSize] = {0};
  memcpy(final_packet, remaining_bytes, remaining_size);
  final_packet[SipHashState::kPacketSize - 1] = static_cast<char>(size & 0xFF);
  state->Update(final_packet);
}

// Fast, cryptographically strong pseudo-random function. Useful for:
// . hash tables holding attacker-controlled data. This function is
//   immune to hash flooding DOS attacks because multi-collisions are
//   infeasible to compute, provided the key remains secret.
// . deterministic/idempotent 'random' number generation, e.g. for
//   choosing a subset of items based on their contents.
//
// Robust versus timing attacks because memory accesses are sequential
// and the algorithm is branch-free. Compute time is proportional to the
// number of 8-byte packets and about twice as fast as an sse41 implementation.
//
// "key" is a secret 128-bit key unknown to attackers.
// "bytes" is the data to hash; ceil(size / 8) * 8 bytes are read.
// Returns a 64-bit hash of the given data bytes.
static INLINE uint64 SipHash(const SipHashState::Key& key, const char* bytes,
                             const uint64 size) {
  return ComputeHash<SipHashState>(key, bytes, size);
}

template <int kNumLanes>
static INLINE uint64 ReduceSipTreeHash(const SipHashState::Key& key,
                                       const uint64 (&hashes)[kNumLanes]) {
  SipHashState state(key);

  for (int i = 0; i < kNumLanes; ++i) {
    state.Update(reinterpret_cast<const char*>(&hashes[i]));
  }

  return state.Finalize();
}

}  // namespace highwayhash

#endif  // #ifndef HIGHWAYHASH_HIGHWAYHASH_SIP_HASH_H_
