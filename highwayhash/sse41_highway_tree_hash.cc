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

#include "highwayhash/sse41_highway_tree_hash.h"

#ifdef __SSE4_1__
#include <cstdio>
#include <cstring>  // memcpy
#include "highwayhash/vec.h"

namespace highwayhash {
namespace {

// J-lanes tree hashing: see http://dx.doi.org/10.4236/jis.2014.53010
// Uses pairs of SSE4.1 instructions to emulate the AVX-2 algorithm.

class SSE41HighwayTreeHashState {
 public:
  using Key = uint64[4];

  explicit INLINE SSE41HighwayTreeHashState(const Key& key) {
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
    const V2x64U init0L(0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    const V2x64U init0H(0x243f6a8885a308d3ull, 0x13198a2e03707344ull);
    const V2x64U init1L(0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    const V2x64U init1H(0x452821e638d01377ull, 0xbe5466cf34e90c6cull);
    const V2x64U keyL = LoadU(key + 0);
    const V2x64U keyH = LoadU(key + 2);
    v0L = keyL ^ init0L;
    v0H = keyH ^ init0H;
    v1L = Rot32(keyH) ^ init1L;
    v1H = Rot32(keyL) ^ init1H;
    mul0L = init0L;
    mul0H = init0H;
    mul1L = init1L;
    mul1H = init1H;
  }

  INLINE void Update(const V2x64U& packetL, const V2x64U& packetH) {
    v1L += packetL;
    v1H += packetH;
    v1L += mul0L;
    v1H += mul0H;
    mul0L ^= V2x64U(_mm_mul_epu32(v0L, v1L >> 32));
    mul0H ^= V2x64U(_mm_mul_epu32(v0H, v1H >> 32));
    v0L += mul1L;
    v0H += mul1H;
    mul1L ^= V2x64U(_mm_mul_epu32(v1L, v0L >> 32));
    mul1H ^= V2x64U(_mm_mul_epu32(v1H, v0H >> 32));
    v0L += ZipperMerge(v1L);
    v0H += ZipperMerge(v1H);
    v1L += ZipperMerge(v0L);
    v1H += ZipperMerge(v0H);
  }

  INLINE void Update(const char* bytes) {
    const uint64* words = reinterpret_cast<const uint64*>(bytes);
    const V2x64U packetL = LoadU(words + 0);
    const V2x64U packetH = LoadU(words + 2);
    Update(packetL, packetH);
  }

  INLINE uint64 Finalize() {
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
  void Print(const V2x64U& L, const V2x64U& H) {
    ALIGNED(uint64, 16) lanesL[2] = {0};
    ALIGNED(uint64, 16) lanesH[2] = {0};
    Store(L, lanesL);
    Store(H, lanesH);
    printf("%016llX %016llX %016llX %016llX\n", lanesH[1], lanesH[0], lanesL[1],
           lanesL[0]);
  }

  static INLINE V2x64U ZipperMerge(const V2x64U& v) {
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
  static INLINE V2x64U Rot32(const V2x64U& v) {
    return V2x64U(_mm_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
  }

  INLINE void PermuteAndUpdate() {
    // It is slightly better to permute v0 than v1; it will be added to v1.
    // AVX-2 Permute also swaps 128-bit halves, so swap input operands.
    Update(Rot32(v0H), Rot32(v0L));
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

// Fills a zero-initialized buffer with the final 0..31 bytes, storing
// size % 256 in the upper byte. Copying into an aligned buffer incurs a
// store-to-load-forwarding stall, but SSE4.1 lacks masked loads.
// "remainder" is the number of accessible/remaining bytes (size % 32).
static INLINE void LoadFinalPacket(const char* bytes, const uint64 size,
                                   const uint64 remainder, char* buffer) {
  const size_t remainder_mod4 = remainder & 3;
  uint32 packet4 = static_cast<uint32>(size) << 24;
  const char* final_bytes = bytes + remainder - remainder_mod4;
  for (size_t i = 0; i < remainder_mod4; ++i) {
    const uint32 byte = static_cast<unsigned char>(final_bytes[i]);
    packet4 += byte << (i * 8);
  }

  memcpy(buffer, bytes, remainder - remainder_mod4);
  memcpy(buffer + 28, &packet4, sizeof(packet4));
}

}  // namespace

uint64 SSE41HighwayTreeHash(const uint64 (&key)[4], const char* bytes,
                            const uint64 size) {
  SSE41HighwayTreeHashState state(key);

  const size_t remainder = size & 31;
  const size_t truncated_size = size - remainder;
  for (size_t i = 0; i < truncated_size; i += 32) {
    state.Update(bytes + i);
  }

  ALIGNED(char, 16) buffer[32] = {0};
  LoadFinalPacket(bytes + truncated_size, size, remainder, buffer);
  state.Update(buffer);

  return state.Finalize();
}

}  // namespace highwayhash

#endif  // #ifdef __SSE4_1__
