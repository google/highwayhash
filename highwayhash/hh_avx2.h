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

#ifndef HIGHWAYHASH_HH_AVX2_H_
#define HIGHWAYHASH_HH_AVX2_H_
#ifdef __AVX2__

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

#include <cstddef>
#include <cstdio>
#include <cstring>
#include "highwayhash/hh_types.h"
#include "highwayhash/vector128.h"
#include "highwayhash/vector256.h"

namespace highwayhash {

template <>
class HHState<TargetAVX2> {
 public:
  explicit HH_INLINE HHState(const HHKey& key_lanes) {
    // "Nothing up my sleeve" numbers, concatenated hex digits of Pi from
    // http://www.numberworld.org/digits/Pi/, retrieved Feb 22, 2016.
    //
    // We use this python code to generate the fourth number to have
    // more even mixture of bits:
    /*
def x(a,b,c):
  retval = 0
  for i in range(64):
    count = ((a >> i) & 1) + ((b >> i) & 1) + ((c >> i) & 1)
    if (count <= 1):
      retval |= 1 << i
  return retval
    */
    const V4x64U init0(0x243f6a8885a308d3ull, 0x13198a2e03707344ull,
                       0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    const V4x64U init1(0x452821e638d01377ull, 0xbe5466cf34e90c6cull,
                       0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    const V4x64U key = LoadUnaligned<V4x64U>(key_lanes);
    v0 = key ^ init0;
    v1 = Rotate64By32(key) ^ init1;
    mul0 = init0;
    mul1 = init1;
  }

  HH_INLINE void Update(const char* bytes) {
    const V4x64U packet =
        LoadUnaligned<V4x64U>(reinterpret_cast<const uint64_t*>(bytes));
    Update(packet);
  }

  HH_INLINE void UpdateRemainder(const char* bytes, const uint64_t size_mod32) {
    // 'Length padding' differentiates zero-valued inputs that have the same
    // size/32. mod32 is sufficient because each Update behaves as if a
    // counter were injected, because the state is large and mixed thoroughly.
    const V8x32U vsize_mod32(
        _mm256_broadcastd_epi32(_mm_cvtsi64_si128(size_mod32)));
    // Equivalent to storing size_mod32 in packet.
    v0 += V4x64U(vsize_mod32);
    // Boosts the avalanche effect of mod32.
    v1 = Rotate32By(v1, vsize_mod32);

    const uint64_t size_mod4 = size_mod32 & 3;

    // (Branching is faster than a single _mm256_maskload_epi32 and is
    // similar to what is required for SSE41.)
    if (HH_UNLIKELY(size_mod32 & 16)) {  // 16..31 bytes left
      const V4x32U packetL =
          LoadUnaligned<V4x32U>(reinterpret_cast<const uint32_t*>(bytes));

      // 0..15 bytes left; a masked load prevents reading past the end.
      // We can read int n=0..3 if size_mod32 >= 16 + (n + 1) * 4; subtract
      // one because we only have > comparisons.
      const V4x32U min_minus_one(31, 27, 23, 19);
      const V4x32U whole_ints(
          _mm_cmpgt_epi32(_mm256_castsi256_si128(vsize_mod32), min_minus_one));
      V4x32U packetH(_mm_maskload_epi32(
          reinterpret_cast<const int*>(bytes + 16), whole_ints));

      // Read the last 0..3 bytes into the most significant bytes.
      uint32_t last4;
      memcpy(&last4, bytes + size_mod32 - 4, 4);

      // The upper four bytes of packetH are zero, so insert there.
      packetH = V4x32U(_mm_insert_epi32(packetH, last4, 3));
      Update(V256From128(packetH, packetL));
    } else {  // size_mod32 < 16
      const V4x32U min_minus_one(15, 11, 7, 3);
      const V4x32U whole_ints(
          _mm_cmpgt_epi32(_mm256_castsi256_si128(vsize_mod32), min_minus_one));
      const V4x32U packetL(
          _mm_maskload_epi32(reinterpret_cast<const int*>(bytes), whole_ints));

      // Read the last 0..3 bytes into the most significant bytes (faster than
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

      // Rather than insert into packetL[3], it is faster to initialize
      // the otherwise empty packetH.
      const V4x32U packetH(_mm_cvtsi64_si128(last4));
      Update(V256From128(packetH, packetL));
    }
  }

  HH_INLINE void Finalize(HHResult64* HH_RESTRICT result) {
    // Mix together all lanes. It is slightly better to permute v0 than v1;
    // it will be added to v1.
    Update(Permute(v0));
    Update(Permute(v0));
    Update(Permute(v0));
    Update(Permute(v0));

    const V2x64U sum0(_mm256_castsi256_si128(v0 + mul0));
    const V2x64U sum1(_mm256_castsi256_si128(v1 + mul1));
    const V2x64U hash = sum0 + sum1;
    // Each lane is sufficiently mixed, so just truncate to 64 bits.
    _mm_storel_epi64(reinterpret_cast<__m128i*>(result), hash);
  }

  HH_INLINE void Finalize(HHResult128* HH_RESTRICT result) {
    Update(Permute(v0));
    Update(Permute(v0));
    Update(Permute(v0));
    Update(Permute(v0));

    const V2x64U sum0(_mm256_castsi256_si128(v0 + mul0));
    const V2x64U sum1(_mm256_extracti128_si256(v1 + mul1, 1));
    const V2x64U hash = sum0 + sum1;
    _mm_storeu_si128(reinterpret_cast<__m128i*>(result), hash);
  }

  HH_INLINE void Finalize(HHResult256* HH_RESTRICT result) {
    Update(Permute(v0));
    Update(Permute(v0));
    Update(Permute(v0));
    Update(Permute(v0));

    const V4x64U sum0 = v0 + mul0;
    const V4x64U sum1 = v1 + mul1;
    const V4x64U hash = ModularReduction(sum1, sum0);
    StoreUnaligned(hash, &(*result)[0]);
  }

 private:
  static HH_INLINE V4x64U V256From128(const V4x32U& hi, const V4x32U& lo) {
    return V4x64U(_mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1));
  }

  static HH_INLINE V4x64U Rotate64By32(const V4x64U& v) {
    return V4x64U(_mm256_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
  }

  // Rotates 32-bit lanes by "count" bits.
  static HH_INLINE V4x64U Rotate32By(const V4x64U& v, const V8x32U& count) {
    // Use variable shifts because sll_epi32 has 4 cycle latency (presumably
    // to broadcast the shift count).
    const V4x64U shifted_left(_mm256_sllv_epi32(v, count));
    const V4x64U shifted_right(_mm256_srlv_epi32(v, V8x32U(32) - count));
    return shifted_left | shifted_right;
  }

  static HH_INLINE V4x64U Permute(const V4x64U& v) {
    // For complete mixing, we need to swap the upper and lower 128-bit halves;
    // we also swap all 32-bit halves. This is faster than extracti128 plus
    // inserti128 followed by Rotate64By32.
    const V4x64U indices(0x0000000200000003ull, 0x0000000000000001ull,
                         0x0000000600000007ull, 0x0000000400000005ull);
    return V4x64U(_mm256_permutevar8x32_epi32(v, indices));
  }

  static HH_INLINE V4x64U MulLow32(const V4x64U& a, const V4x64U& b) {
    return V4x64U(_mm256_mul_epu32(a, b));
  }

  static HH_INLINE V4x64U ZipperMerge(const V4x64U& v) {
    // Multiplication mixes/scrambles bytes 0-7 of the 64-bit result to
    // varying degrees. In descending order of goodness, bytes
    // 3 4 2 5 1 6 0 7 have quality 228 224 164 160 100 96 36 32.
    // As expected, the upper and lower bytes are much worse.
    // For each 64-bit lane, our objectives are:
    // 1) maximizing and equalizing total goodness across the four lanes.
    // 2) mixing with bytes from the neighboring lane (AVX-2 makes it difficult
    //    to cross the 128-bit wall, but PermuteAndUpdate takes care of that);
    // 3) placing the worst bytes in the upper 32 bits because those will not
    //    be used in the next 32x32 multiplication.
    const uint64_t hi = 0x070806090D0A040Bull;
    const uint64_t lo = 0x000F010E05020C03ull;
    return V4x64U(_mm256_shuffle_epi8(v, V4x64U(hi, lo, hi, lo)));
  }

  // Updates four hash lanes in parallel by injecting four 64-bit packets.
  HH_INLINE void Update(const V4x64U& packet) {
    v1 += packet;
    v1 += mul0;
    mul0 ^= MulLow32(v1, v0 >> 32);
    HH_COMPILER_FENCE;
    v0 += mul1;
    mul1 ^= MulLow32(v0, v1 >> 32);
    HH_COMPILER_FENCE;
    v0 += ZipperMerge(v1);
    v1 += ZipperMerge(v0);
  }

  // XORs a << 1 and a << 2 into *out after clearing the upper two bits of a.
  // Also does the same for the upper 128 bit lane "b". Bit shifts are only
  // possible on independent 64-bit lanes. We therefore insert the upper bits
  // of a[0] that were lost into a[1]. Thanks to D. Lemire for helpful comments!
  static HH_INLINE void XorByShift128Left12(const V4x64U& ba,
                                            V4x64U* HH_RESTRICT out) {
    const V4x64U zero = ba ^ ba;
    const V4x64U top_bits2 = ba >> (64 - 2);
    const V4x64U ones = ba == ba;              // FF .. FF
    const V4x64U shifted1_unmasked = ba + ba;  // (avoids needing port0)
    HH_COMPILER_FENCE;

    // Only the lower halves of top_bits1's 128 bit lanes will be used, so we
    // can compute it before clearing the upper two bits of ba.
    const V4x64U top_bits1 = ba >> (64 - 1);
    const V4x64U upper_8bytes(_mm256_slli_si256(ones, 8));  // F 0 F 0
    const V4x64U shifted2 = shifted1_unmasked + shifted1_unmasked;
    HH_COMPILER_FENCE;

    const V4x64U upper_bit_of_128 = upper_8bytes << 63;  // 80..00 80..00
    const V4x64U new_low_bits2(_mm256_unpacklo_epi64(zero, top_bits2));
    *out ^= shifted2;
    HH_COMPILER_FENCE;

    // The result must be as if the upper two bits of the input had been clear,
    // otherwise we're no longer computing a reduction.
    const V4x64U shifted1 = AndNot(upper_bit_of_128, shifted1_unmasked);
    *out ^= new_low_bits2;
    HH_COMPILER_FENCE;

    const V4x64U new_low_bits1(_mm256_unpacklo_epi64(zero, top_bits1));
    *out ^= shifted1;

    *out ^= new_low_bits1;
  }

  // Modular reduction by the irreducible polynomial (x^128 + x^2 + x).
  // Input: two 256-bit numbers a3210 and b3210, interleaved in 2 vectors.
  // The upper and lower 128-bit halves are processed independently.
  static HH_INLINE V4x64U ModularReduction(const V4x64U& b32a32,
                                           const V4x64U& b10a10) {
    // See Lemire, https://arxiv.org/pdf/1503.03465v8.pdf.
    V4x64U out = b10a10;
    XorByShift128Left12(b32a32, &out);
    return out;
  }

  static void Print(const V4x64U& v) {
    uint64_t lanes[V4x64U::N] HH_ALIGNAS(32);
    Store(v, lanes);
    printf("A: %016lX %016lX %016lX %016lX\n", lanes[3], lanes[2], lanes[1],
           lanes[0]);
  }

  V4x64U v0;
  V4x64U v1;
  V4x64U mul0;
  V4x64U mul1;
};

}  // namespace highwayhash

#endif  // #ifdef __AVX2__
#endif  // #ifndef HIGHWAYHASH_HH_AVX2_H_
