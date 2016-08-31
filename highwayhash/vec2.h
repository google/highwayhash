// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef HIGHWAYHASH_HIGHWAYHASH_VEC2_H_
#define HIGHWAYHASH_HIGHWAYHASH_VEC2_H_

#ifdef __AVX2__

// Defines SIMD vector classes ("V4x64U") with overloaded arithmetic operators:
// const V4x64U masked_sum = (a + b) & m;
// This is shorter and more readable than compiler intrinsics:
// const __m256i masked_sum = _mm256_and_si256(_mm256_add_epi64(a, b), m);
// There is typically no runtime cost for these abstractions.
//
// The naming convention is VNxBBT where N is the number of lanes, BB the
// number of bits per lane and T is the lane type: unsigned integer (U),
// signed integer (I), or floating-point (F).
//
// Requires reasonable C++11 support (VC2015) and an AVX2-capable CPU.

#include <immintrin.h>
#include "highwayhash/code_annotation.h"
#include "highwayhash/types.h"

namespace highwayhash {

// 256-bit AVX-2 vector with 4 uint64 lanes.
class V4x64U {
 public:
  using T = uint64;
  static constexpr size_t kNumLanes = sizeof(__m256i) / sizeof(T);

  // Leaves v_ uninitialized - typically used for output parameters.
  HIGHWAYHASH_INLINE V4x64U() {}

  // Lane 0 (p_0) is the lowest.
  HIGHWAYHASH_INLINE V4x64U(T p_3, T p_2, T p_1, T p_0)
      : v_(_mm256_set_epi64x(p_3, p_2, p_1, p_0)) {}

  // Broadcasts i to all lanes.
  HIGHWAYHASH_INLINE explicit V4x64U(T i)
      : v_(_mm256_broadcastq_epi64(_mm_cvtsi64_si128(i))) {}

  // Converts to/from intrinsics.
  HIGHWAYHASH_INLINE explicit V4x64U(const __m256i& v) : v_(v) {}
  HIGHWAYHASH_INLINE operator __m256i() const { return v_; }
  HIGHWAYHASH_INLINE V4x64U& operator=(const __m256i& v) {
    v_ = v;
    return *this;
  }

  // _mm256_setzero_epi64 generates suboptimal code. Instead set
  // z = x - x (given an existing "x"), or x == x to set all bits.
  HIGHWAYHASH_INLINE V4x64U& operator=(const V4x64U& other) {
    v_ = other.v_;
    return *this;
  }

  HIGHWAYHASH_INLINE V4x64U& operator+=(const V4x64U& other) {
    v_ = _mm256_add_epi64(v_, other);
    return *this;
  }
  HIGHWAYHASH_INLINE V4x64U& operator-=(const V4x64U& other) {
    v_ = _mm256_sub_epi64(v_, other);
    return *this;
  }

  HIGHWAYHASH_INLINE V4x64U& operator&=(const V4x64U& other) {
    v_ = _mm256_and_si256(v_, other);
    return *this;
  }
  HIGHWAYHASH_INLINE V4x64U& operator|=(const V4x64U& other) {
    v_ = _mm256_or_si256(v_, other);
    return *this;
  }
  HIGHWAYHASH_INLINE V4x64U& operator^=(const V4x64U& other) {
    v_ = _mm256_xor_si256(v_, other);
    return *this;
  }

  HIGHWAYHASH_INLINE V4x64U& operator<<=(const int count) {
    v_ = _mm256_slli_epi64(v_, count);
    return *this;
  }
  HIGHWAYHASH_INLINE V4x64U& operator<<=(const __m128i& count) {
    v_ = _mm256_sll_epi64(v_, count);
    return *this;
  }

  HIGHWAYHASH_INLINE V4x64U& operator>>=(const int count) {
    v_ = _mm256_srli_epi64(v_, count);
    return *this;
  }
  HIGHWAYHASH_INLINE V4x64U& operator>>=(const __m128i& count) {
    v_ = _mm256_srl_epi64(v_, count);
    return *this;
  }

 private:
  __m256i v_;
};

// Nonmember functions implemented in terms of member functions

static HIGHWAYHASH_INLINE
V4x64U operator+(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t += right;
}

static HIGHWAYHASH_INLINE
V4x64U operator-(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t -= right;
}

static HIGHWAYHASH_INLINE
V4x64U operator<<(const V4x64U& v, const int count) {
  V4x64U t(v);
  return t <<= count;
}

static HIGHWAYHASH_INLINE
V4x64U operator>>(const V4x64U& v, const int count) {
  V4x64U t(v);
  return t >>= count;
}

static HIGHWAYHASH_INLINE
V4x64U operator<<(const V4x64U& v, const __m128i& count) {
  V4x64U t(v);
  return t <<= count;
}

static HIGHWAYHASH_INLINE
V4x64U operator>>(const V4x64U& v, const __m128i& count) {
  V4x64U t(v);
  return t >>= count;
}

static HIGHWAYHASH_INLINE
V4x64U operator&(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t &= right;
}

static HIGHWAYHASH_INLINE
V4x64U operator|(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t |= right;
}

static HIGHWAYHASH_INLINE
V4x64U operator^(const V4x64U& left, const V4x64U& right) {
  V4x64U t(left);
  return t ^= right;
}

// Load/Store.

// "from" must be vector-aligned.
static HIGHWAYHASH_INLINE
V4x64U Load(const uint64* HIGHWAYHASH_RESTRICT const from) {
  return V4x64U(_mm256_load_si256(reinterpret_cast<const __m256i*>(from)));
}

static HIGHWAYHASH_INLINE
V4x64U LoadU(const uint64* HIGHWAYHASH_RESTRICT const from) {
  return V4x64U(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(from)));
}

// "to" must be vector-aligned.
static HIGHWAYHASH_INLINE
void Store(const V4x64U& v, uint64* HIGHWAYHASH_RESTRICT const to) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(to), v);
}

static HIGHWAYHASH_INLINE
void StoreU(const V4x64U& v, uint64* HIGHWAYHASH_RESTRICT const to) {
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(to), v);
}

// Writes directly to (aligned) memory, bypassing the cache. This is useful for
// data that will not be read again in the near future.
static HIGHWAYHASH_INLINE
void Stream(const V4x64U& v, uint64* HIGHWAYHASH_RESTRICT const to) {
  _mm256_stream_si256(reinterpret_cast<__m256i*>(to), v);
}

// Miscellaneous functions.

static HIGHWAYHASH_INLINE
V4x64U AndNot(const V4x64U& neg_mask, const V4x64U& values) {
  return V4x64U(_mm256_andnot_si256(neg_mask, values));
}

static HIGHWAYHASH_INLINE
V4x64U UnpackLow(const V4x64U& low, const V4x64U& high) {
  return V4x64U(_mm256_unpacklo_epi64(low, high));
}
static HIGHWAYHASH_INLINE
V4x64U UnpackHigh(const V4x64U& low, const V4x64U& high) {
  return V4x64U(_mm256_unpackhi_epi64(low, high));
}

// There are no greater-than comparison instructions for unsigned T.
static HIGHWAYHASH_INLINE
V4x64U operator==(const V4x64U& left, const V4x64U& right) {
  return V4x64U(_mm256_cmpeq_epi64(left, right));
}

}  // namespace highwayhash

#endif  // #ifdef __AVX2__
#endif  // #ifndef HIGHWAYHASH_HIGHWAYHASH_VEC2_H_
