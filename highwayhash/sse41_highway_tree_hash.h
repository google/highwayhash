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

#include <cstdio>
#include <cstring>  // memcpy
#include "third_party/highwayhash/highwayhash/state_helpers.h"
#include "third_party/highwayhash/highwayhash/vec.h"

namespace highwayhash {

// J-lanes tree hashing: see http://dx.doi.org/10.4236/jis.2014.53010
// Uses pairs of SSE4.1 instructions to emulate the AVX-2 algorithm.
class SSE41HighwayTreeHashState {
 public:
  // Four 64-bit hash states are updated in parallel by injecting 2 x 2
  // 64-bit packets per Update(). Finalize() combines the four states into
  // one final 64-bit digest.
  using Key = uint64[4];
  static const int kPacketSize = sizeof(Key);

  explicit HH_INLINE SSE41HighwayTreeHashState(const Key& key) {
    // "Nothing up my sleeve numbers"; see HighwayTreeHashState.
    const V2x64U init0L(0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    const V2x64U init0H(0x243f6a8885a308d3ull, 0x13198a2e03707344ull);
    const V2x64U init1L(0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    const V2x64U init1H(0x452821e638d01377ull, 0xbe5466cf34e90c6cull);
    const V2x64U keyL = LoadUnaligned128(key + 0);
    const V2x64U keyH = LoadUnaligned128(key + 2);
    v0L = keyL ^ init0L;
    v0H = keyH ^ init0H;
    // swapped 128-bit halves to match Permute().
    v1L = Rotate32(keyH) ^ init1L;
    v1H = Rotate32(keyL) ^ init1H;
    mul0L = init0L;
    mul0H = init0H;
    mul1L = init1L;
    mul1H = init1H;
  }

  HH_INLINE void Update(const V2x64U& packetL, const V2x64U& packetH) {
    v1L += packetL;
    v1H += packetH;
    v1L += mul0L;
    v1H += mul0H;
    mul0L ^= V2x64U(_mm_mul_epu32(v1L, Rotate32(v0L)));
    mul0H ^= V2x64U(_mm_mul_epu32(v1H, v0H >> 32));
    v0L += mul1L;
    v0H += mul1H;
    mul1L ^= V2x64U(_mm_mul_epu32(v0L, Rotate32(v1L)));
    mul1H ^= V2x64U(_mm_mul_epu32(v0H, v1H >> 32));
    v0L += ZipperMerge(v1L);
    v0H += ZipperMerge(v1H);
    v1L += ZipperMerge(v0L);
    v1H += ZipperMerge(v0H);
  }

  HH_INLINE void Update(const char* bytes) {
    const uint64* words = reinterpret_cast<const uint64*>(bytes);
    const V2x64U packetL = LoadUnaligned128(words + 0);
    const V2x64U packetH = LoadUnaligned128(words + 2);
    Update(packetL, packetH);
  }

  HH_INLINE uint64 Finalize() {
    // Mix together all lanes.
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    const V2x64U sum = v0L + v1L + mul0L + mul1L;
    // Much faster than Store(v0 + v1) to uint64[].
    return _mm_cvtsi128_si64(sum);
  }

 private:
  static void Print(const V2x64U& L, const V2x64U& H) {
    alignas(16) uint64 lanesL[2] = {0};
    alignas(16) uint64 lanesH[2] = {0};
    Store(L, lanesL);
    Store(H, lanesH);
    printf("%016llX %016llX %016llX %016llX\n", lanesH[1], lanesH[0], lanesL[1],
           lanesL[0]);
  }

  static HH_INLINE V2x64U ZipperMerge(const V2x64U& v) {
    // Multiplication mixes/scrambles bytes 0-7 of the 64-bit result to
    // varying degrees. In descending order of goodness, bytes
    // 3 4 2 5 1 6 0 7 have quality 228 224 164 160 100 96 36 32.
    // As expected, the upper and lower bytes are much worse.
    // For each 64-bit lane, our objectives are:
    // 1) maximizing and equalizing total goodness across each lane's bytes;
    // 2) mixing with bytes from the neighboring lane;
    // 3) placing the worst bytes in the upper 32 bits because those will not
    //    be used in the next 32x32 multiplication.
    const uint64 hi = 0x070806090D0A040Bull;
    const uint64 lo = 0x000F010E05020C03ull;
    return V2x64U(_mm_shuffle_epi8(v, V2x64U(hi, lo)));
  }

  // Swap 32-bit halves of each lane (caller swaps 128-bit halves)
  static HH_INLINE V2x64U Rotate32(const V2x64U& v) {
    return V2x64U(_mm_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
  }

  HH_INLINE void PermuteAndUpdate() {
    // It is slightly better to permute v0 than v1; it will be added to v1.
    // AVX-2 Permute also swaps 128-bit halves, so swap input operands.
    Update(Rotate32(v0H), Rotate32(v0L));
  }

  V2x64U v0L;
  V2x64U v0H;
  V2x64U v1L;
  V2x64U v1H;
  V2x64U mul0L;
  V2x64U mul0H;
  V2x64U mul1L;
  V2x64U mul1H;
};

// Replacement for maskload_epi32. Returns zero-initialized vector with the
// lower "size" = 0, 4, 8 or 12 bytes loaded from "bytes".
HH_INLINE V2x64U LoadMultipleOfFour(const char* bytes, const uint64 size) {
  const uint32_t* words = reinterpret_cast<const uint32_t*>(bytes);
  // Mask of 1-bits where the final 4 bytes should be inserted (replacement for
  // variable shift/insert using broadcast+blend).
  V2x64U mask4(_mm_cvtsi32_si128(~0U));  // 'insert' into lane 0
  V2x64U ret(0);
  if (size & 8) {
    ret = V2x64U(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(words)));
    // mask4 = 0 ~0 0 0 ('insert' into lane 2)
    mask4 = V2x64U(_mm_bslli_si128(mask4, 8));
    words += 2;
  }
  // Final 4 (possibly after the 8 above); 'insert' into lane 0 or 2 of ret.
  if (size & 4) {
    const __m128i word2 = _mm_cvtsi32_si128(words[0]);
    // = 0 word2 0 word2; mask4 will select which lane to keep.
    const V2x64U broadcast(_mm_shuffle_epi32(word2, 0x00));
    // (slightly faster than blendv_epi8)
    ret |= V2x64U(broadcast & mask4);
  }
  return ret;
}

template <>
HH_INLINE void PaddedUpdate<SSE41HighwayTreeHashState>(
    const uint64 size, const char* remaining_bytes, const uint64 remaining_size,
    SSE41HighwayTreeHashState* state) {
  // Length padding ensures that zero-valued buffers of different lengths
  // result in different hashes.
  uint32 packet4 = static_cast<uint32>(size) << 24;

  // Load any remaining bytes individually into packet4. This is nearly
  // branch-free and much more efficient than a loop.
  const uint32 remainder_mod4 = remaining_size & 3;
  if (remainder_mod4 != 0) {
    const char* final_bytes = remaining_bytes + remaining_size - remainder_mod4;
    // The offsets [0, remainder_mod4) are a subset of {idx0=0, idx1 and idx2}.
    // Loading from those offsets will not overrun final_bytes, but it does
    // repeat bytes rather than zero-pad.
    packet4 += static_cast<uint32>(final_bytes[0]);
    const uint32_t idx1 = remainder_mod4 >> 1;
    const uint32_t idx2 = remainder_mod4 - 1;
    packet4 += static_cast<uint32>(final_bytes[idx1]) << 8;
    packet4 += static_cast<uint32>(final_bytes[idx2]) << 16;
  }

  // Avoid memcpy because it saves/restores all XMM registers used by "state"!
  // SSE4 lacks maskload, so we need branches to avoid overrunning the input.
  // Update is called until remaining_size < 32, so we only need to test
  // bits[0,4] of remaining_size, and bits[0,1] are checked via remainder_mod4.
  if (HH_UNLIKELY(remaining_size & 16)) {
    const V2x64U packetL =
        LoadUnaligned128(reinterpret_cast<const uint64*>(remaining_bytes));
    V2x64U packetH = LoadMultipleOfFour(remaining_bytes + 16, remaining_size);
    packetH = V2x64U(_mm_insert_epi32(packetH, packet4, 3));
    state->Update(packetL, packetH);
  } else {
    const V2x64U packetH(_mm_bslli_si128(_mm_cvtsi32_si128(packet4), 12));
    const V2x64U packetL = LoadMultipleOfFour(remaining_bytes, remaining_size);
    state->Update(packetL, packetH);
  }
}

// J-lanes tree hash based upon multiplication and "zipper merges".
//
// Robust versus timing attacks because memory accesses are sequential
// and the algorithm is branch-free. Requires an SSE4.1 capable CPU.
//
// "key" is a secret 256-bit key unknown to attackers.
// "bytes" is the data to hash (possibly unaligned).
// "size" is the number of bytes to hash; exactly that many bytes are read.
//
// Returns a 64-bit hash of the given data bytes, the same value that
// HighwayTreeHash would return (drop-in compatible).
//
// About 0.36 cycles per byte for 1 KB inputs (86% of the AVX-2 version).
static HH_INLINE uint64 SSE41HighwayTreeHash(const uint64 (&key)[4],
                                             const char* bytes,
                                             const uint64 size) {
  SSE41HighwayTreeHashState state(key);
  UpdateState(bytes, size, &state);
  return state.Finalize();
}

}  // namespace highwayhash

#endif  // #ifdef __SSE4_1__

#endif  // #ifndef HIGHWAYHASH_HIGHWAYHASH_SSE41_HIGHWAY_TREE_HASH_H_
