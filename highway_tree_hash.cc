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

#include "highway_tree_hash.h"

#include <cstring>  // memcpy
#include "vec2.h"

namespace {

// J-lanes tree hashing: see http://dx.doi.org/10.4236/jis.2014.53010

// Four (2 x 64-bit) hash states are updated in parallel by injecting
// four 64-bit packets per Update(). Finalize() combines the four states into
// one final 64-bit digest.
const int kNumLanes = 4;
const int kPacketSize = kNumLanes * sizeof(uint64_t);

class HighwayTreeHashState {
 public:
  explicit INLINE HighwayTreeHashState(const uint64_t (&keys)[kNumLanes]) {
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
    const V4x64U key = LoadU(keys);
    v0 = key ^ init0;
    v1 = key ^ init1;
  }

  INLINE void Update(const V4x64U& packet) {
    v1 += packet;
    // Improves the scrambling (otherwise the hash would have difficulty
    // escaping states with near-zero v0). Adding more bits risks bias.
    v0 |= V4x64U(0x0000000070000001ULL);
    V4x64U mul0(_mm256_mul_epu32(v0, v1));
    V4x64U mul1(_mm256_mul_epu32(v0, v1 >> 32));

    v0 += ZipperMerge(mul0);
    v1 += mul1;
  }

  INLINE uint64_t Finalize() {
    // Mix together all lanes.
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();
    PermuteAndUpdate();

    // Much faster than Store(v0 + v1) to uint64_t[].
    return _mm_cvtsi128_si64(_mm256_extracti128_si256(v0 + v1, 0));
  }

 private:
  static INLINE V4x64U ZipperMerge(const V4x64U& v) {
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

  INLINE void PermuteAndUpdate() {
    // For complete mixing, we need to swap the upper and lower 128-bit halves;
    // we also swap all 32-bit halves.
    const V4x64U indices(0x0000000200000003ull, 0x0000000000000001ull,
                         0x0000000600000007ull, 0x0000000400000005ull);
    // Slightly better to permute v0 than v1; it will be added to v1.
    const V4x64U permuted(_mm256_permutevar8x32_epi32(v0, indices));
    Update(permuted);
  }

  V4x64U v0;
  V4x64U v1;
};

// Returns 32-byte packet by loading the remaining 0..31 bytes, storing
// "remainder" in the upper byte, and zeroing any intervening bytes.
// "remainder" is the number of accessible/remaining bytes (size % 32).
// Loading past the end of the input risks page fault exceptions which even
// LDDQU cannot prevent.
static INLINE V4x64U LoadFinalPacket32(const uint8_t* bytes,
                                       const uint64_t size,
                                       const uint64_t remainder) {
  // Copying into an aligned buffer incurs a store-to-load-forwarding stall.
  // Instead, we use masked loads to read any remaining whole uint32_t
  // without incurring page faults for the others.
  const size_t remaining_32 = remainder >> 2;  // 0..7

  // mask[32*i+31] := uint32_t #i valid/accessible ? 1 : 0.
  // To avoid large lookup tables, we pack uint32_t lanes to uint8_t,
  // compute the packed mask by shifting, and then sign-extend 0xFF to
  // 0xFFFFFFFF (although only the MSB needs to be set).
  // remaining_32 = 0 => mask = 00000000; remaining_32 = 7 => mask = 01111111.
  const uint64_t packed_mask =
      0x00FFFFFFFFFFFFFFULL >> ((7 - remaining_32) * 8);
  const V4x64U mask(_mm256_cvtepi8_epi32(_mm_cvtsi64_si128(packed_mask)));
  // Load 0..7 remaining (potentially unaligned) uint32_t.
  const V4x64U packet28(
      _mm256_maskload_epi32(reinterpret_cast<const int*>(bytes), mask));

  // Load any remaining bytes individually and combine into a uint32_t.
  const int remainder_mod4 = remainder & 3;
  // Length padding ensures that zero-valued buffers of different lengths
  // result in different hashes.
  uint32_t packet4 = static_cast<uint32_t>(size) << 24;
  const uint8_t* final_bytes = bytes + (remaining_32 * 4);
  for (int i = 0; i < remainder_mod4; ++i) {
    packet4 += static_cast<uint32_t>(final_bytes[i]) << (i * 8);
  }

  // The upper 4 bytes of packet28 are zero; replace with packet4 to
  // obtain the (length-padded) 32-byte packet.
  const V4x64U v4(_mm256_broadcastd_epi32(_mm_cvtsi32_si128(packet4)));
  const V4x64U packet(_mm256_blend_epi32(packet28, v4, 0x80));
  return packet;
}

}  // namespace

uint64_t HighwayTreeHash(const uint64_t (&key)[kNumLanes], const uint8_t* bytes,
                         const uint64_t size) {
  HighwayTreeHashState state(key);

  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated_size = size - remainder;
  const uint64_t* packets = reinterpret_cast<const uint64_t*>(bytes);
  for (size_t i = 0; i < truncated_size / sizeof(uint64_t); i += kNumLanes) {
    const V4x64U packet = LoadU(packets + i);
    state.Update(packet);
  }

  const V4x64U final_packet =
      LoadFinalPacket32(bytes + truncated_size, size, remainder);
  state.Update(final_packet);

  return state.Finalize();
}
