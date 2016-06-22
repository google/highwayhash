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

#ifndef HIGHWAYHASH_HIGHWAYHASH_SCALAR_HIGHWAY_TREE_HASH_H_
#define HIGHWAYHASH_HIGHWAYHASH_SCALAR_HIGHWAY_TREE_HASH_H_

// Portable, scalar (non-vector/SIMD) version.

#include <cstring>  // memcpy
#include "highwayhash/state_helpers.h"

namespace highwayhash {

class ScalarHighwayTreeHashState {
 public:
  static const int kNumLanes = 4;
  using Lanes = uint64[kNumLanes];
  using Key = Lanes;
  static const int kPacketSize = sizeof(Lanes);

  INLINE ScalarHighwayTreeHashState(const Key& keys) {
    static const Lanes init0 = {0xdbe6d5d5fe4cce2full, 0xa4093822299f31d0ull,
                                0x13198a2e03707344ull, 0x243f6a8885a308d3ull};
    static const Lanes init1 = {0x3bd39e10cb0ef593ull, 0xc0acf169b5f18a8cull,
                                0xbe5466cf34e90c6cull, 0x452821e638d01377ull};
    Lanes permuted_keys;
    Permute(keys, &permuted_keys);
    Copy(init0, &mul0);
    Copy(init1, &mul1);
    Xor(init0, keys, &v0);
    Xor(init1, permuted_keys, &v1);
  }

  INLINE void Update(const char* bytes) {
    const Lanes& packets = *reinterpret_cast<const Lanes*>(bytes);

    Add(packets, &v1);
    Add(mul0, &v1);

    // (Loop is faster than unrolling)
    for (int lane = 0; lane < kNumLanes; ++lane) {
      const uint32 v0_32 = v0[lane];
      const uint32 v1_32 = v1[lane];
      mul0[lane] ^= v0_32 * (v1[lane] >> 32);
      v0[lane] += mul1[lane];
      mul1[lane] ^= v1_32 * (v0[lane] >> 32);
    }

    ZipperMergeAndAdd(v1[0], v1[1], &v0[0], &v0[1]);
    ZipperMergeAndAdd(v1[2], v1[3], &v0[2], &v0[3]);

    ZipperMergeAndAdd(v0[0], v0[1], &v1[0], &v1[1]);
    ZipperMergeAndAdd(v0[2], v0[3], &v1[2], &v1[3]);
  }

  INLINE uint64 Finalize() {
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    return v0[0] + v1[0] + mul0[0] + mul1[0];
  }

  // private:
  static INLINE void Copy(const Lanes& source, Lanes* dest) {
    for (int lane = 0; lane < kNumLanes; ++lane) {
      (*dest)[lane] = source[lane];
    }
  }

  static INLINE void Add(const Lanes& source, Lanes* dest) {
    for (int lane = 0; lane < kNumLanes; ++lane) {
      (*dest)[lane] += source[lane];
    }
  }

  static INLINE void Xor(const Lanes& op1, const Lanes& op2, Lanes* dest) {
    for (int lane = 0; lane < kNumLanes; ++lane) {
      (*dest)[lane] = op1[lane] ^ op2[lane];
    }
  }

// Clears all bits except one byte at the given offset.
#define MASK(v, bytes) ((v) & (0xFFull << ((bytes)*8)))

  // 16-byte permutation; shifting is about 10% faster than byte loads.
  // Adds zipper-merge result to add*.
  static INLINE void ZipperMergeAndAdd(const uint64 v0, const uint64 v1,
                                       uint64* RESTRICT add0,
                                       uint64* RESTRICT add1) {
    *add0 += ((MASK(v0, 3) + MASK(v1, 4)) >> 24) +
             ((MASK(v0, 5) + MASK(v1, 6)) >> 16) + MASK(v0, 2) +
             (MASK(v0, 1) << 32) + (MASK(v1, 7) >> 8) + (v0 << 56);

    *add1 += ((MASK(v1, 3) + MASK(v0, 4)) >> 24) + MASK(v1, 2) +
             (MASK(v1, 5) >> 16) + (MASK(v1, 1) << 24) + (MASK(v0, 6) >> 8) +
             (MASK(v1, 0) << 48) + MASK(v0, 7);
  }

#undef MASK

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
    Update(reinterpret_cast<const char*>(permuted));
  }

  uint64 v0[kNumLanes];
  uint64 v1[kNumLanes];
  uint64 mul0[kNumLanes];
  uint64 mul1[kNumLanes];
};

// J-lanes tree hash based upon multiplication and "zipper merges".
//
// Robust versus timing attacks because memory accesses are sequential
// and the algorithm is branch-free. Portable, no particular CPU requirements.
//
// "key" is a secret 256-bit key unknown to attackers.
// "bytes" is the data to hash (possibly unaligned).
// "size" is the number of bytes to hash; exactly that many bytes are read.
//
// Returns a 64-bit hash of the given data bytes, the same value that
// HighwayTreeHash would return (drop-in compatible).
//
// Throughput: 1.1 GB/s for 1 KB inputs (about 10% of the AVX-2 version).
uint64 ScalarHighwayTreeHash(const uint64 (&key)[4], const char* bytes,
                             const uint64 size) {
  return ComputeHash<ScalarHighwayTreeHashState>(key, bytes, size);
}

}  // namespace highwayhash

#endif  // HIGHWAYHASH_HIGHWAYHASH_SCALAR_HIGHWAY_TREE_HASH_H_
