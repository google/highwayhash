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
#include <cstring>  // memcpy
#include "highwayhash/vec.h"

namespace highwayhash {
namespace {

// J-lanes tree hashing: see http://dx.doi.org/10.4236/jis.2014.53010

// Two (64-bit) hash states are updated in parallel by injecting
// two 64-bit packets per Update(). Finalize() combines the states into
// one final 64-bit digest.
const int kNumLanes = 2;
const int kPacketSize = kNumLanes * sizeof(uint64);

class SSE41HighwayTreeHashState {
 public:
  explicit INLINE SSE41HighwayTreeHashState(const uint64 (&keys)[4]) {
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
    const V2x64U init0(0xa4093822299f31d0ull, 0xdbe6d5d5fe4cce2full);
    const V2x64U init1(0xc0acf169b5f18a8cull, 0x3bd39e10cb0ef593ull);
    const V2x64U key = LoadU(keys);
    v0 = key ^ init0;
    v1 = Permute(key) ^ init1;
    mul0 = init0;
    mul1 = init1;
  }

  INLINE void Update(const V2x64U& packet) {
    v1 += packet;
    v1 += mul0;
    mul0 ^= V2x64U(_mm_mul_epu32(v0, v1 >> 32));
    v0 += mul1;
    mul1 ^= V2x64U(_mm_mul_epu32(v1, v0 >> 32));
    v0 += ZipperMerge(v1);
    v1 += ZipperMerge(v0);
  }

  INLINE uint64 Finalize() {
    // Mix together all lanes.
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    const V2x64U sum = v0 + v1 + mul0 + mul1;
    // Much faster than Store(v0 + v1) to uint64[].
    return _mm_cvtsi128_si64(sum);
  }

 private:
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

  static INLINE V2x64U Permute(const V2x64U& v) {
    // We swap all 32-bit halves and the lanes.
    return V2x64U(_mm_shuffle_epi32(v, _MM_SHUFFLE(0, 1, 2, 3)));
  }

  INLINE void PermuteAndUpdate() {
    // It is slightly better to permute v0 than v1; it will be added to v1.
    Update(Permute(v0));
  }

  V2x64U v0;
  V2x64U v1;
  V2x64U mul0;
  V2x64U mul1;
};

// Returns 16-byte packet by loading the remaining 0..15 bytes, storing
// "remainder" in the upper byte, and zeroing any intervening bytes.
// "remainder" is the number of accessible/remaining bytes (size % 16).
// Loading past the end of the input risks page fault exceptions which even
// LDDQU cannot prevent.
static INLINE V2x64U LoadFinalPacket16(const char* bytes, const uint64 size,
                                       const uint64 remainder) {
  // Copying into an aligned buffer incurs a store-to-load-forwarding stall,
  // but SSE4.1 lacks masked loads.
  ALIGNED(char, 16) buffer[16] = {0};
  memcpy(buffer, bytes, remainder);
  buffer[15] = static_cast<char>(size);
  return Load(reinterpret_cast<const uint64*>(buffer));
}

}  // namespace

uint64 SSE41HighwayTreeHash(const uint64 (&key)[4], const char* bytes,
                            const uint64 size) {
  SSE41HighwayTreeHashState state(key);

  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64* packets = reinterpret_cast<const uint64*>(bytes);
  for (size_t i = 0; i < truncated_size / sizeof(uint64); i += kNumLanes) {
    const V2x64U packet = LoadU(packets + i);
    state.Update(packet);
  }

  const V2x64U final_packet =
      LoadFinalPacket16(bytes + truncated_size, size, remainder);
  state.Update(final_packet);

  return state.Finalize();
}

}  // namespace highwayhash

#endif  // #ifdef __SSE4_1__
