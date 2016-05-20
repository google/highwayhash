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

#ifndef HIGHWAYHASH_HIGHWAYHASH_SSE41_HIGHWAY_TREE_HASH_H_
#define HIGHWAYHASH_HIGHWAYHASH_SSE41_HIGHWAY_TREE_HASH_H_
#ifdef __SSE4_1__

#include "highwayhash/types.h"

#ifdef __cplusplus
namespace highwayhash {
extern "C" {
#endif

// J-lanes tree hash based upon multiplication and "zipper merges".
//
// Robust versus timing attacks because memory accesses are sequential
// and the algorithm is branch-free. Requires an SSE4.1 capable CPU.
//
// "key" is a secret 256-bit key unknown to attackers.
// "bytes" is the data to hash (possibly unaligned).
// "size" is the number of bytes to hash; exactly that many bytes are read.
//
// Returns a 64-bit hash of the given data bytes.
uint64 SSE41HighwayTreeHash(const uint64 (&key)[4], const char* bytes,
                            const uint64 size);

#ifdef __cplusplus
}  // extern "C"
}  // namespace highwayhash
#endif

#endif  // #ifdef __SSE4_1__
#endif  // #ifndef HIGHWAYHASH_HIGHWAYHASH_SSE41_HIGHWAY_TREE_HASH_H_
