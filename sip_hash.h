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

#ifndef HIGHWAYHASH_SIP_HASH_H_
#define HIGHWAYHASH_SIP_HASH_H_

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Fast, cryptographically strong pseudo-random function. Useful for:
// . hash tables holding attacker-controlled data. This function is
//   immune to hash flooding DOS attacks because multi-collisions are
//   infeasible to compute, provided the key remains secret.
// . deterministic/idempotent 'random' number generation, e.g. for
//   choosing a subset of items based on their contents.
//
// Robust versus timing attacks because memory accesses are sequential
// and the algorithm is branch-free. Compute time is proportional to the
// number of 8-byte packets and 1.5x faster than an sse41 implementation.
// Requires an AVX-2 capable CPU.
//
// "key" is a secret 128-bit key unknown to attackers.
// "bytes" is the data to hash; ceil(size / 8) * 8 bytes are read.
// Returns a 64-bit hash of the given data bytes.
uint64_t SipHash(const uint64_t key[2], const uint8_t* bytes,
                 const uint64_t size);

uint64_t ReduceSipTreeHash(const uint64_t key[2], const uint64_t hashes[4]);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // #ifndef HIGHWAYHASH_SIP_HASH_H_
