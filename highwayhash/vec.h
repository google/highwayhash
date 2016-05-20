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

#ifndef HIGHWAYHASH_HIGHWAYHASH_VEC_H_
#define HIGHWAYHASH_HIGHWAYHASH_VEC_H_
#ifdef __SSE4_1__

// Defines SIMD vector classes ("V2x64U") with overloaded arithmetic operators:
// const V2x64U masked_sum = (a + b) & m;
// This is shorter and more readable than compiler intrinsics:
// const __m128i masked_sum = _mm_and_si128(_mm_add_epi64(a, b), m);
// There is typically no runtime cost for these abstractions.
//
// The naming convention is VNxBBT where N is the number of lanes, BB the
// number of bits per lane and T is the lane type: unsigned integer (U),
// signed integer (I), or floating-point (F).
//
// Requires reasonable C++11 support (VC2015) and an SSE4.1-capable CPU.

#include <immintrin.h>
#include "highwayhash/code_annotation.h"
#include "highwayhash/types.h"

namespace highwayhash {

// 128-bit AVX-2 vector with 2 uint64 lanes.
class V2x64U {
 public:
  using T = uint64;
  static constexpr size_t kNumLanes = sizeof(__m128i) / sizeof(T);

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V2x64U() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V2x64U(T p_1, T p_0) : v_(_mm_set_epi64x(p_1, p_0)) {}
  //

  // Broadcasts i to all lanes (usually by loading from memory).
  INLINE explicit V2x64U(T i) : v_(_mm_set_epi64x(i, i)) {}
  //

  // Converts to/from intrinsics.
  INLINE explicit V2x64U(const __m128i& v) : v_(v) {}
  INLINE operator __m128i() const { return v_; }
  INLINE V2x64U& operator=(const __m128i& v) {
    v_ = v;
    return *this;
  }

  // _mm_setzero_epi64 generates suboptimal code. Instead set
  // z = x - x (given an existing "x"), or x == x to set all bits.
  INLINE V2x64U& operator=(const V2x64U& other) {
    v_ = other.v_;
    return *this;
  }

  INLINE V2x64U& operator+=(const V2x64U& other) {
    v_ = _mm_add_epi64(v_, other);
    return *this;
  }
  INLINE V2x64U& operator-=(const V2x64U& other) {
    v_ = _mm_sub_epi64(v_, other);
    return *this;
  }

  INLINE V2x64U& operator&=(const V2x64U& other) {
    v_ = _mm_and_si128(v_, other);
    return *this;
  }
  INLINE V2x64U& operator|=(const V2x64U& other) {
    v_ = _mm_or_si128(v_, other);
    return *this;
  }
  INLINE V2x64U& operator^=(const V2x64U& other) {
    v_ = _mm_xor_si128(v_, other);
    return *this;
  }

  INLINE V2x64U& operator<<=(const int count) {
    v_ = _mm_slli_epi64(v_, count);
    return *this;
  }
  INLINE V2x64U& operator<<=(const __m128i& count) {
    v_ = _mm_sll_epi64(v_, count);
    return *this;
  }

  INLINE V2x64U& operator>>=(const int count) {
    v_ = _mm_srli_epi64(v_, count);
    return *this;
  }
  INLINE V2x64U& operator>>=(const __m128i& count) {
    v_ = _mm_srl_epi64(v_, count);
    return *this;
  }

 private:
  __m128i v_;
};

// Nonmember functions implemented in terms of member functions

static INLINE V2x64U operator+(const V2x64U& left, const V2x64U& right) {
  V2x64U t(left);
  return t += right;
}

static INLINE V2x64U operator-(const V2x64U& left, const V2x64U& right) {
  V2x64U t(left);
  return t -= right;
}

static INLINE V2x64U operator<<(const V2x64U& v, const int count) {
  V2x64U t(v);
  return t <<= count;
}

static INLINE V2x64U operator>>(const V2x64U& v, const int count) {
  V2x64U t(v);
  return t >>= count;
}

static INLINE V2x64U operator<<(const V2x64U& v, const __m128i& count) {
  V2x64U t(v);
  return t <<= count;
}

static INLINE V2x64U operator>>(const V2x64U& v, const __m128i& count) {
  V2x64U t(v);
  return t >>= count;
}

static INLINE V2x64U operator&(const V2x64U& left, const V2x64U& right) {
  V2x64U t(left);
  return t &= right;
}

static INLINE V2x64U operator|(const V2x64U& left, const V2x64U& right) {
  V2x64U t(left);
  return t |= right;
}

static INLINE V2x64U operator^(const V2x64U& left, const V2x64U& right) {
  V2x64U t(left);
  return t ^= right;
}

// Load/Store.

// "from" must be vector-aligned.
static INLINE V2x64U Load(const uint64* RESTRICT const from) {
  return V2x64U(_mm_load_si128(reinterpret_cast<const __m128i*>(from)));
}

static INLINE V2x64U LoadU(const uint64* RESTRICT const from) {
  return V2x64U(_mm_loadu_si128(reinterpret_cast<const __m128i*>(from)));
}

// "to" must be vector-aligned.
static INLINE void Store(const V2x64U& v, uint64* RESTRICT const to) {
  _mm_store_si128(reinterpret_cast<__m128i*>(to), v);
}

static INLINE void StoreU(const V2x64U& v, uint64* RESTRICT const to) {
  _mm_storeu_si128(reinterpret_cast<__m128i*>(to), v);
}

// Writes directly to (aligned) memory, bypassing the cache. This is useful for
// data that will not be read again in the near future.
static INLINE void Stream(const V2x64U& v, uint64* RESTRICT const to) {
  _mm_stream_si128(reinterpret_cast<__m128i*>(to), v);
}

// Miscellaneous functions.

static INLINE V2x64U AndNot(const V2x64U& neg_mask, const V2x64U& values) {
  return V2x64U(_mm_andnot_si128(neg_mask, values));
}

static INLINE V2x64U UnpackLow(const V2x64U& low, const V2x64U& high) {
  return V2x64U(_mm_unpacklo_epi64(low, high));
}
static INLINE V2x64U UnpackHigh(const V2x64U& low, const V2x64U& high) {
  return V2x64U(_mm_unpackhi_epi64(low, high));
}

// There are no greater-than comparison instructions for unsigned T.
static INLINE V2x64U operator==(const V2x64U& left, const V2x64U& right) {
  return V2x64U(_mm_cmpeq_epi64(left, right));
}

static INLINE V2x64U RotateLeft(const V2x64U& v, const int count) {
  return (v << count) | (v >> (64-count));
}

}  // namespace highwayhash

#endif  // #ifdef __SSE4_1__
#endif  // #ifndef HIGHWAYHASH_HIGHWAYHASH_VEC_H_
