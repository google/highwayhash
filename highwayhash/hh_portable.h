// Copyright 2015-2017 Google Inc. All Rights Reserved.
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

#ifndef HIGHWAYHASH_HH_PORTABLE_H_
#define HIGHWAYHASH_HH_PORTABLE_H_

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

#include <stddef.h>
#include <cstdio>
#include <cstring>  // memcpy

#include "highwayhash/compiler_specific.h"
#include "highwayhash/hh_types.h"

namespace highwayhash {

template <>
class HHState<TargetPortable> {
 public:
  static const int kNumLanes = 4;
  using Lanes = uint64_t[kNumLanes];

  explicit HH_INLINE HHState(const HHKey& keys) {
    static const Lanes init0 = {0xdbe6d5d5fe4cce2full, 0xa4093822299f31d0ull,
                                0x13198a2e03707344ull, 0x243f6a8885a308d3ull};
    static const Lanes init1 = {0x3bd39e10cb0ef593ull, 0xc0acf169b5f18a8cull,
                                0xbe5466cf34e90c6cull, 0x452821e638d01377ull};
    Lanes rotated_keys;
    Rotate64By32(keys, &rotated_keys);
    Copy(init0, &mul0);
    Copy(init1, &mul1);
    Xor(init0, keys, &v0);
    Xor(init1, rotated_keys, &v1);
  }

  HH_INLINE void Update(const char* bytes) {
    const Lanes& packets = *reinterpret_cast<const Lanes*>(bytes);

    Add(packets, &v1);
    Add(mul0, &v1);

    // (Loop is faster than unrolling)
    for (int lane = 0; lane < kNumLanes; ++lane) {
      const uint32_t v1_32 = static_cast<uint32_t>(v1[lane]);
      mul0[lane] ^= v1_32 * (v0[lane] >> 32);
      v0[lane] += mul1[lane];
      const uint32_t v0_32 = static_cast<uint32_t>(v0[lane]);
      mul1[lane] ^= v0_32 * (v1[lane] >> 32);
    }

    ZipperMergeAndAdd(v1[1], v1[0], &v0[1], &v0[0]);
    ZipperMergeAndAdd(v1[3], v1[2], &v0[3], &v0[2]);

    ZipperMergeAndAdd(v0[1], v0[0], &v1[1], &v1[0]);
    ZipperMergeAndAdd(v0[3], v0[2], &v1[3], &v1[2]);
  }

  HH_INLINE void UpdateRemainder(const char* bytes, const uint64_t size_mod32) {
    // 'Length padding' differentiates zero-valued inputs that have the same
    // size/32. mod32 is sufficient because each Update behaves as if a
    // counter were injected, because the state is large and mixed thoroughly.
    const uint64_t mod32_pair = (size_mod32 << 32) + size_mod32;
    for (int lane = 0; lane < kNumLanes; ++lane) {
      v0[lane] += mod32_pair;
    }
    Rotate32By(reinterpret_cast<uint32_t*>(&v1), size_mod32);

    const uint64_t size_mod4 = size_mod32 & 3;

    char packet[kNumLanes * 8] HH_ALIGNAS(32) = {0};
    memcpy(packet, bytes, size_mod32 & ~3);

    if (size_mod32 & 16) {  // 16..31 bytes left
      // Read the last 0..3 bytes and previous 1..4 into the upper bits.
      uint32_t last4;
      memcpy(&last4, bytes + size_mod32 - 4, 4);

      // The upper four bytes of packet are zero, so insert there.
      memcpy(packet + 28, &last4, 4);
    } else {  // size_mod32 < 16
      // Read the last 0..3 bytes into the least significant bytes (faster than
      // two conditional branches with 16/8 bit loads).
      uint64_t last4 = 0;
      if (size_mod4 != 0) {
        // {idx0, idx1, idx2} is a subset of [0, size_mod4), so it is
        // safe to read final_bytes at those offsets.
        const char* final_bytes = bytes + (size_mod32 & ~3);
        const uint64_t idx0 = 0;
        const uint64_t idx1 = size_mod4 >> 1;
        const uint64_t idx2 = size_mod4 - 1;
        // Store into least significant bytes (avoids one shift).
        last4 = static_cast<uint64_t>(final_bytes[idx0]);
        last4 += static_cast<uint64_t>(final_bytes[idx1]) << 8;
        last4 += static_cast<uint64_t>(final_bytes[idx2]) << 16;
      }

      // Rather than insert at packet + 12, it is faster to initialize
      // the otherwise empty packet + 16 with up to 64 bits of padding.
      memcpy(packet + 16, &last4, 8);
    }
    Update(packet);
  }

  HH_INLINE void Finalize(HHResult64* HH_RESTRICT result) {
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    *result = v0[0] + v1[0] + mul0[0] + mul1[0];
  }

  HH_INLINE void Finalize(HHResult128* HH_RESTRICT result) {
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    (*result)[0] = v0[0] + mul0[0] + v1[2] + mul1[2];
    (*result)[1] = v0[1] + mul0[1] + v1[3] + mul1[3];
  }

  HH_INLINE void Finalize(HHResult256* HH_RESTRICT result) {
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    ModularReduction(v1[1] + mul1[1], v1[0] + mul1[0], v0[1] + mul0[1],
                     v0[0] + mul0[0], &(*result)[1], &(*result)[0]);
    ModularReduction(v1[3] + mul1[3], v1[2] + mul1[2], v0[3] + mul0[3],
                     v0[2] + mul0[2], &(*result)[3], &(*result)[2]);
  }

 private:
  static HH_INLINE void Copy(const Lanes& source, Lanes* HH_RESTRICT dest) {
    for (int lane = 0; lane < kNumLanes; ++lane) {
      (*dest)[lane] = source[lane];
    }
  }

  static HH_INLINE void Add(const Lanes& source, Lanes* HH_RESTRICT dest) {
    for (int lane = 0; lane < kNumLanes; ++lane) {
      (*dest)[lane] += source[lane];
    }
  }

  static HH_INLINE void Xor(const Lanes& op1, const Lanes& op2,
                            Lanes* HH_RESTRICT dest) {
    for (int lane = 0; lane < kNumLanes; ++lane) {
      (*dest)[lane] = op1[lane] ^ op2[lane];
    }
  }

// Clears all bits except one byte at the given offset.
#define MASK(v, bytes) ((v) & (0xFFull << ((bytes)*8)))

  // 16-byte permutation; shifting is about 10% faster than byte loads.
  // Adds zipper-merge result to add*.
  static HH_INLINE void ZipperMergeAndAdd(const uint64_t v1, const uint64_t v0,
                                          uint64_t* HH_RESTRICT add1,
                                          uint64_t* HH_RESTRICT add0) {
    *add0 += ((MASK(v0, 3) + MASK(v1, 4)) >> 24) +
             ((MASK(v0, 5) + MASK(v1, 6)) >> 16) + MASK(v0, 2) +
             (MASK(v0, 1) << 32) + (MASK(v1, 7) >> 8) + (v0 << 56);

    *add1 += ((MASK(v1, 3) + MASK(v0, 4)) >> 24) + MASK(v1, 2) +
             (MASK(v1, 5) >> 16) + (MASK(v1, 1) << 24) + (MASK(v0, 6) >> 8) +
             (MASK(v1, 0) << 48) + MASK(v0, 7);
  }

#undef MASK

  static HH_INLINE uint64_t Rotate64By32(const uint64_t x) {
    return (x >> 32) | (x << 32);
  }

  static HH_INLINE void Rotate64By32(const Lanes& v,
                                     Lanes* HH_RESTRICT rotated) {
    for (int i = 0; i < kNumLanes; ++i) {
      (*rotated)[i] = Rotate64By32(v[i]);
    }
  }

  static HH_INLINE void Rotate32By(uint32_t* halves, const uint64_t count) {
    for (int i = 0; i < 2 * kNumLanes; ++i) {
      const uint32_t x = halves[i];
      halves[i] = (x << count) | (x >> (32 - count));
    }
  }

  static HH_INLINE void Permute(const Lanes& v, Lanes* HH_RESTRICT permuted) {
    (*permuted)[0] = Rotate64By32(v[2]);
    (*permuted)[1] = Rotate64By32(v[3]);
    (*permuted)[2] = Rotate64By32(v[0]);
    (*permuted)[3] = Rotate64By32(v[1]);
  }

  HH_INLINE void PermuteAndUpdate() {
    Lanes permuted;
    Permute(v0, &permuted);
    Update(reinterpret_cast<const char*>(permuted));
  }

  // Computes a << kBits for 128-bit a = (a1, a0).
  // Bit shifts are only possible on independent 64-bit lanes. We therefore
  // insert the upper bits of a0 that were lost into a1. This is slightly
  // shorter than Lemire's (a << 1) | (((a >> 8) << 1) << 8) approach.
  template <int kBits>
  static HH_INLINE void Shift128Left(uint64_t* HH_RESTRICT a1,
                                     uint64_t* HH_RESTRICT a0) {
    const uint64_t shifted1 = (*a1) << kBits;
    const uint64_t top_bits = (*a0) >> (64 - kBits);
    *a0 <<= kBits;
    *a1 = shifted1 | top_bits;
  }

  // Modular reduction by the irreducible polynomial (x^128 + x^2 + x).
  // Input: a 256-bit number a3210.
  static HH_INLINE void ModularReduction(const uint64_t a3_unmasked,
                                         const uint64_t a2, const uint64_t a1,
                                         const uint64_t a0,
                                         uint64_t* HH_RESTRICT m1,
                                         uint64_t* HH_RESTRICT m0) {
    // The upper two bits must be clear, otherwise a3 << 2 would lose bits,
    // in which case we're no longer computing a reduction.
    const uint64_t a3 = a3_unmasked & 0x3FFFFFFFFFFFFFFFull;
    // See Lemire, https://arxiv.org/pdf/1503.03465v8.pdf.
    uint64_t a3_shl1 = a3;
    uint64_t a2_shl1 = a2;
    uint64_t a3_shl2 = a3;
    uint64_t a2_shl2 = a2;
    Shift128Left<1>(&a3_shl1, &a2_shl1);
    Shift128Left<2>(&a3_shl2, &a2_shl2);
    *m1 = a1 ^ a3_shl1 ^ a3_shl2;
    *m0 = a0 ^ a2_shl1 ^ a2_shl2;
  }

  static void Print(const Lanes& lanes) {
    printf("P: %016lX %016lX %016lX %016lX\n", lanes[3], lanes[2], lanes[1],
           lanes[0]);
  }

  Lanes v0;
  Lanes v1;
  Lanes mul0;
  Lanes mul1;
};

}  // namespace highwayhash

#endif  // HIGHWAYHASH_HH_PORTABLE_H_
