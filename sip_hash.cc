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

#include <cstdio>
#include "sip_hash.h"
#include "sse41_sip_hash.h"

#include <cstring>  // memcpy
#include "vec.h"

namespace {

// Paper: https://www.131002.net/siphash/siphash.pdf
// SSE41 implementation: https://goo.gl/80GBSD

// The hash state is updated by injecting 8-byte packets;
// XORing together all state variables yields the final hash.

// 32 bytes. Parameters are hardwired to c=2, d=4 [rounds].
class SipHashState {
 public:
  explicit INLINE SipHashState(const uint64_t keys[2]) {
    const V2x64U key = LoadU(keys);
    const V2x64U init0 =
        key ^ V2x64U(0x646f72616e646f6dull, 0x736f6d6570736575ull);
    const V2x64U init1 =
        key ^ V2x64U(0x7465646279746573ull, 0x6c7967656e657261ull);
    v20 = UnpackLow(init0, init1);
    v31 = UnpackHigh(init0, init1);
  }

  INLINE void Update(const V2x64U& packet) {
    v31 ^= V2x64U(_mm_slli_si128(packet, 8));  // Only v3.

    Compress<2>();

    v20 ^= packet;  // Only v0.
  }

  INLINE uint64_t Finalize() {
    // Mix in bits to avoid leaking the key if all packets were zero.
    v20 ^= V2x64U(0xFF, 0);

    Compress<4>();

    const V2x64U v32_10 = v20 ^ v31;
    const V2x64U v32_32(_mm_unpackhi_epi64(v32_10, v32_10));
    return _mm_cvtsi128_si64(v32_10 ^ v32_32);
  }

 private:
  // Independently rotates v3 and v1 by bits3 and bits1.
  template <uint64_t bits3, uint64_t bits1>
  static INLINE V2x64U RotateLeft(const V2x64U& v31) {
    const V2x64U shiftL(bits3, bits1);
    const V2x64U shiftR(64 - bits3, 64 - bits1);
    // The new AVX2 variable shift instruction is chiefly responsible for a
    // 1.5x speedup vs. SSE41, which needs to rotate v3 and v1 independently.
    const V2x64U left(_mm_sllv_epi64(v31, shiftL));
    const V2x64U right(_mm_srlv_epi64(v31, shiftR));
    return left | right;
  }

  // Swaps and rotates v2 and v0 by 32 bits.
  static INLINE V2x64U RotateLeft32(const V2x64U& v20) {
    return V2x64U(_mm_shuffle_epi32(v20, _MM_SHUFFLE(0, 1, 3, 2)));
  }

  // ARX network: add, rotate, exclusive-or.
  template <uint64_t bits3, uint64_t bits1>
  INLINE void HalfRound() {
    v20 += v31;
    v31 = RotateLeft<bits3, bits1>(v31);
    v31 ^= v20;
  }

  template <size_t rounds>
  INLINE void Compress() {
    // Loop is faster than unrolling!
    for (size_t i = 0; i < rounds; ++i) {
      HalfRound<16, 13>();
      v20 = RotateLeft32(v20);  // Faster than moving into HalfRound.
      HalfRound<21, 17>();
      v20 = RotateLeft32(v20);
    }
  }

  // The four 64-bit state variables are arranged in pairs to benefit from
  // 2-way SIMD. This utilizes only half of the AVX-2 lanes, but processing
  // multiple inputs is difficult because their lengths generally differ,
  // and SipHash is not designed for independent processing of packets.
  V2x64U v20;
  V2x64U v31;
};

// Returns packet, with upper half zero.
static INLINE V2x64U LoadPacket64(const uint8_t* bytes) {
  // Slightly faster than _mm_loadl_epi64, but only in the main loop.
  uint64_t packet;
  memcpy(&packet, bytes, sizeof(packet));
  return V2x64U(_mm_cvtsi64_si128(packet));
}

// Returns final packet, loading the remaining 0..7 bytes, storing
// "size & 0xFF" in the upper byte, and zeroing any intervening bytes.
// Loading past the end of the input risks page fault exceptions which even
// LDDQU cannot prevent. Masked loads are not helpful for such small packets.
static INLINE V2x64U LoadFinalPacket64(const uint8_t* bytes,
                                       const uint64_t size,
                                       const size_t offset) {
  ALIGNED(uint8_t, 16) buffer[8] = {0};
  memcpy(buffer, bytes, size - offset);
  buffer[7] = size;
  return LoadPacket64(buffer);
}

}  // namespace

uint64_t AVX2SipHash(const uint64_t key[2], const uint8_t* bytes,
                 const uint64_t size) {
  SipHashState state(key);

  size_t offset = 0;
  for (; offset < (size & ~7); offset += 8) {
    const V2x64U packet = LoadPacket64(bytes + offset);
    state.Update(packet);
  }

  const V2x64U packet = LoadFinalPacket64(bytes + offset, size, offset);
  state.Update(packet);

  return state.Finalize();
}

uint64_t AVX2ReduceSipTreeHash(const uint64_t key[2], const uint64_t hashes[4]) {
  SipHashState state(key);

  for (int i = 0; i < 4; ++i) {
    const V2x64U packet(_mm_cvtsi64_si128(hashes[i]));
    state.Update(packet);
  }

  return state.Finalize();
}

static uint64_t (*siphashFP)(const uint64_t [], const uint8_t*, const uint64_t);
static uint64_t (*reducesiptreehashFP)(const uint64_t [], const uint64_t []);

uint64_t SipHash(const uint64_t key[2], const uint8_t* bytes,
                 const uint64_t size) {
// __builtin_cpu_supports is available in GCC 4.8 and higher
#if __GNUC__ > 4 || \
  (__GNUC__ == 4 && (__GNUC_MINOR__ > 8 || __GNUC_MINOR__ == 8))
  if (!siphashFP) {
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2")) {
      siphashFP = &AVX2SipHash;
    } else {
      siphashFP = &SSE41SipHash;
    }
#ifdef DEBUG
    printf("checking SipHash implementation... %s\n",
      __builtin_cpu_supports("avx2")? "avx2": "sse4.1");
#endif
  }
  return siphashFP(key, bytes, size);
#else
  AVX2SipHash(key, bytes, size);
#endif
}

uint64_t ReduceSipTreeHash(const uint64_t key[2], const uint64_t hashes[4]) {
// __builtin_cpu_supports is available in GCC 4.8 and higher
#if __GNUC__ > 4 || \
  (__GNUC__ == 4 && (__GNUC_MINOR__ > 8 || __GNUC_MINOR__ == 8))
  if (!reducesiptreehashFP) {
    __builtin_cpu_init();
    if (__builtin_cpu_supports("avx2")) {
      reducesiptreehashFP = &AVX2ReduceSipTreeHash;
    } else {
      reducesiptreehashFP = &SSE41ReduceSipTreeHash;
    }
#ifdef DEBUG
    printf("checking ReduceSipTreeHash implementation... %s\n",
      __builtin_cpu_supports("avx2")? "avx2": "sse4.1");
#endif
  }
  return reducesiptreehashFP(key, hashes);
#else
  AVX2ReduceSipTreeHash(key, hashes);
#endif
}
