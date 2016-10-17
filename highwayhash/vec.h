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

// Primary template for 128-bit SSE4.1 vectors; only specializations are used.
template <typename T>
class V128 {};

template <>
class V128<uint8> {
 public:
  using T = uint8;
  static constexpr size_t N = 16;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V128() {}

  // Broadcasts i to all lanes (usually by loading from memory).
  INLINE explicit V128(T i) : v_(_mm_set1_epi8(i)) {}

  // Converts to/from intrinsics.
  INLINE explicit V128(const __m128i& v) : v_(v) {}
  INLINE operator __m128i() const { return v_; }
  INLINE V128& operator=(const __m128i& v) {
    v_ = v;
    return *this;
  }
  INLINE V128& operator=(const V128& other) {
    v_ = other.v_;
    return *this;
  }

  // There are no greater-than comparison instructions for unsigned T.
  INLINE V128 operator==(const V128& other) const {
    return V128(_mm_cmpeq_epi8(v_, other.v_));
  }

  INLINE V128& operator+=(const V128& other) {
    v_ = _mm_add_epi8(v_, other);
    return *this;
  }
  INLINE V128& operator-=(const V128& other) {
    v_ = _mm_sub_epi8(v_, other);
    return *this;
  }

  INLINE V128& operator&=(const V128& other) {
    v_ = _mm_and_si128(v_, other);
    return *this;
  }
  INLINE V128& operator|=(const V128& other) {
    v_ = _mm_or_si128(v_, other);
    return *this;
  }
  INLINE V128& operator^=(const V128& other) {
    v_ = _mm_xor_si128(v_, other);
    return *this;
  }

 private:
  __m128i v_;
};

template <>
class V128<uint16> {
 public:
  using T = uint16;
  static constexpr size_t N = 8;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V128() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V128(T p_7, T p_6, T p_5, T p_4, T p_3, T p_2, T p_1, T p_0)
      : v_(_mm_set_epi16(p_7, p_6, p_5, p_4, p_3, p_2, p_1, p_0)) {}

  // Broadcasts i to all lanes (usually by loading from memory).
  INLINE explicit V128(T i) : v_(_mm_set1_epi16(i)) {}

  // Converts to/from intrinsics.
  INLINE explicit V128(const __m128i& v) : v_(v) {}
  INLINE operator __m128i() const { return v_; }
  INLINE V128& operator=(const __m128i& v) {
    v_ = v;
    return *this;
  }
  INLINE V128& operator=(const V128& other) {
    v_ = other.v_;
    return *this;
  }

  // There are no greater-than comparison instructions for unsigned T.
  INLINE V128 operator==(const V128& other) const {
    return V128(_mm_cmpeq_epi16(v_, other.v_));
  }

  INLINE V128& operator+=(const V128& other) {
    v_ = _mm_add_epi16(v_, other);
    return *this;
  }
  INLINE V128& operator-=(const V128& other) {
    v_ = _mm_sub_epi16(v_, other);
    return *this;
  }

  INLINE V128& operator&=(const V128& other) {
    v_ = _mm_and_si128(v_, other);
    return *this;
  }
  INLINE V128& operator|=(const V128& other) {
    v_ = _mm_or_si128(v_, other);
    return *this;
  }
  INLINE V128& operator^=(const V128& other) {
    v_ = _mm_xor_si128(v_, other);
    return *this;
  }

  INLINE V128& operator<<=(const int count) {
    v_ = _mm_slli_epi16(v_, count);
    return *this;
  }
  INLINE V128& operator<<=(const __m128i& count) {
    v_ = _mm_sll_epi16(v_, count);
    return *this;
  }

  INLINE V128& operator>>=(const int count) {
    v_ = _mm_srli_epi16(v_, count);
    return *this;
  }
  INLINE V128& operator>>=(const __m128i& count) {
    v_ = _mm_srl_epi16(v_, count);
    return *this;
  }

 private:
  __m128i v_;
};

template <>
class V128<uint32> {
 public:
  using T = uint32;
  static constexpr size_t N = 4;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V128() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V128(T p_3, T p_2, T p_1, T p_0)
      : v_(_mm_set_epi32(p_3, p_2, p_1, p_0)) {}

  // Broadcasts i to all lanes (usually by loading from memory).
  INLINE explicit V128(T i) : v_(_mm_set1_epi32(i)) {}

  // Converts to/from intrinsics.
  INLINE explicit V128(const __m128i& v) : v_(v) {}
  INLINE operator __m128i() const { return v_; }
  INLINE V128& operator=(const __m128i& v) {
    v_ = v;
    return *this;
  }
  INLINE V128& operator=(const V128& other) {
    v_ = other.v_;
    return *this;
  }

  // There are no greater-than comparison instructions for unsigned T.
  INLINE V128 operator==(const V128& other) const {
    return V128(_mm_cmpeq_epi32(v_, other.v_));
  }

  INLINE V128& operator+=(const V128& other) {
    v_ = _mm_add_epi32(v_, other);
    return *this;
  }
  INLINE V128& operator-=(const V128& other) {
    v_ = _mm_sub_epi32(v_, other);
    return *this;
  }

  INLINE V128& operator&=(const V128& other) {
    v_ = _mm_and_si128(v_, other);
    return *this;
  }
  INLINE V128& operator|=(const V128& other) {
    v_ = _mm_or_si128(v_, other);
    return *this;
  }
  INLINE V128& operator^=(const V128& other) {
    v_ = _mm_xor_si128(v_, other);
    return *this;
  }

  INLINE V128& operator<<=(const int count) {
    v_ = _mm_slli_epi32(v_, count);
    return *this;
  }
  INLINE V128& operator<<=(const __m128i& count) {
    v_ = _mm_sll_epi32(v_, count);
    return *this;
  }

  INLINE V128& operator>>=(const int count) {
    v_ = _mm_srli_epi32(v_, count);
    return *this;
  }
  INLINE V128& operator>>=(const __m128i& count) {
    v_ = _mm_srl_epi32(v_, count);
    return *this;
  }

 private:
  __m128i v_;
};

template <>
class V128<uint64> {
 public:
  using T = uint64;
  static constexpr size_t N = 2;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V128() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V128(T p_1, T p_0) : v_(_mm_set_epi64x(p_1, p_0)) {}

  // Broadcasts i to all lanes (usually by loading from memory).
  INLINE explicit V128(T i) : v_(_mm_set_epi64x(i, i)) {}

  // Converts to/from intrinsics.
  INLINE explicit V128(const __m128i& v) : v_(v) {}
  INLINE operator __m128i() const { return v_; }
  INLINE V128& operator=(const __m128i& v) {
    v_ = v;
    return *this;
  }
  INLINE V128& operator=(const V128& other) {
    v_ = other.v_;
    return *this;
  }

  // There are no greater-than comparison instructions for unsigned T.
  INLINE V128 operator==(const V128& other) const {
    return V128(_mm_cmpeq_epi64(v_, other.v_));
  }

  INLINE V128& operator+=(const V128& other) {
    v_ = _mm_add_epi64(v_, other);
    return *this;
  }
  INLINE V128& operator-=(const V128& other) {
    v_ = _mm_sub_epi64(v_, other);
    return *this;
  }

  INLINE V128& operator&=(const V128& other) {
    v_ = _mm_and_si128(v_, other);
    return *this;
  }
  INLINE V128& operator|=(const V128& other) {
    v_ = _mm_or_si128(v_, other);
    return *this;
  }
  INLINE V128& operator^=(const V128& other) {
    v_ = _mm_xor_si128(v_, other);
    return *this;
  }

  INLINE V128& operator<<=(const int count) {
    v_ = _mm_slli_epi64(v_, count);
    return *this;
  }
  INLINE V128& operator<<=(const __m128i& count) {
    v_ = _mm_sll_epi64(v_, count);
    return *this;
  }

  INLINE V128& operator>>=(const int count) {
    v_ = _mm_srli_epi64(v_, count);
    return *this;
  }
  INLINE V128& operator>>=(const __m128i& count) {
    v_ = _mm_srl_epi64(v_, count);
    return *this;
  }

 private:
  __m128i v_;
};

template <>
class V128<float> {
 public:
  using T = float;
  static constexpr size_t N = 4;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V128() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V128(T p_3, T p_2, T p_1, T p_0)
      : v_(_mm_set_ps(p_3, p_2, p_1, p_0)) {}

  // Broadcasts to all lanes.
  INLINE explicit V128(T f) : v_(_mm_set1_ps(f)) {}

  // Converts to/from intrinsics.
  INLINE explicit V128(const __m128& v) : v_(v) {}
  INLINE operator __m128() const { return v_; }
  INLINE V128& operator=(const __m128& v) {
    v_ = v;
    return *this;
  }
  INLINE V128& operator=(const V128& other) {
    v_ = other.v_;
    return *this;
  }

  // There are too many comparison operators; use _mm_cmp_ps instead.

  INLINE V128& operator+=(const V128& other) {
    v_ = _mm_add_ps(v_, other);
    return *this;
  }
  INLINE V128& operator-=(const V128& other) {
    v_ = _mm_sub_ps(v_, other);
    return *this;
  }

  INLINE V128& operator&=(const V128& other) {
    v_ = _mm_and_ps(v_, other);
    return *this;
  }
  INLINE V128& operator|=(const V128& other) {
    v_ = _mm_or_ps(v_, other);
    return *this;
  }
  INLINE V128& operator^=(const V128& other) {
    v_ = _mm_xor_ps(v_, other);
    return *this;
  }

 private:
  __m128 v_;
};

template <>
class V128<double> {
 public:
  using T = double;
  static constexpr size_t N = 2;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V128() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V128(T p_1, T p_0)
      : v_(_mm_set_pd(p_1, p_0)) {}

  // Broadcasts to all lanes.
  INLINE explicit V128(T f) : v_(_mm_set1_pd(f)) {}

  // Converts to/from intrinsics.
  INLINE explicit V128(const __m128d& v) : v_(v) {}
  INLINE operator __m128d() const { return v_; }
  INLINE V128& operator=(const __m128d& v) {
    v_ = v;
    return *this;
  }
  INLINE V128& operator=(const V128& other) {
    v_ = other.v_;
    return *this;
  }

  // There are too many comparison operators; use _mm_cmp_pd instead.

  INLINE V128& operator+=(const V128& other) {
    v_ = _mm_add_pd(v_, other);
    return *this;
  }
  INLINE V128& operator-=(const V128& other) {
    v_ = _mm_sub_pd(v_, other);
    return *this;
  }

  INLINE V128& operator&=(const V128& other) {
    v_ = _mm_and_pd(v_, other);
    return *this;
  }
  INLINE V128& operator|=(const V128& other) {
    v_ = _mm_or_pd(v_, other);
    return *this;
  }
  INLINE V128& operator^=(const V128& other) {
    v_ = _mm_xor_pd(v_, other);
    return *this;
  }

 private:
  __m128d v_;
};

// Nonmember functions for any V128 via member functions.

template <typename T>
INLINE V128<T> operator+(const V128<T>& left, const V128<T>& right) {
  V128<T> t(left);
  return t += right;
}

template <typename T>
INLINE V128<T> operator-(const V128<T>& left, const V128<T>& right) {
  V128<T> t(left);
  return t -= right;
}

template <typename T>
INLINE V128<T> operator&(const V128<T>& left, const V128<T>& right) {
  V128<T> t(left);
  return t &= right;
}

template <typename T>
INLINE V128<T> operator|(const V128<T>& left, const V128<T>& right) {
  V128<T> t(left);
  return t |= right;
}

template <typename T>
INLINE V128<T> operator^(const V128<T>& left, const V128<T>& right) {
  V128<T> t(left);
  return t ^= right;
}

template <typename T>
INLINE V128<T> operator<<(const V128<T>& v, const int count) {
  V128<T> t(v);
  return t <<= count;
}

template <typename T>
INLINE V128<T> operator>>(const V128<T>& v, const int count) {
  V128<T> t(v);
  return t >>= count;
}

template <typename T>
INLINE V128<T> operator<<(const V128<T>& v, const __m128i& count) {
  V128<T> t(v);
  return t <<= count;
}

template <typename T>
INLINE V128<T> operator>>(const V128<T>& v, const __m128i& count) {
  V128<T> t(v);
  return t >>= count;
}

// Load/Store for any V128.

// The function name requires a "128" suffix to differentiate from AVX2 loads.
// "from" must be vector-aligned.
template <typename T>
INLINE V128<T> Load128(crpc<T> from) {
  crpc<__m128i> p = reinterpret_cast<crpc<__m128i>>(from);
  return V128<T>(_mm_load_si128(p));
}
INLINE V128<float> Load128(crpc<float> from) {
  return V128<float>(_mm_load_ps(from));
}
INLINE V128<double> Load128(crpc<double> from) {
  return V128<double>(_mm_load_pd(from));
}

template <typename T>
INLINE V128<T> LoadUnaligned128(crpc<T> from) {
  crpc<__m128i> p = reinterpret_cast<crpc<__m128i>>(from);
  return V128<T>(_mm_loadu_si128(p));
}
INLINE V128<float> LoadUnaligned128(crpc<float> from) {
  return V128<float>(_mm_loadu_ps(from));
}
INLINE V128<double> LoadUnaligned128(crpc<double> from) {
  return V128<double>(_mm_loadu_pd(from));
}

// "to" must be vector-aligned.
template <typename T>
INLINE void Store(const V128<T>& v, crp<T> to) {
  _mm_store_si128(reinterpret_cast<crp<__m128i>>(to), v);
}
INLINE void Store(const V128<float>& v, crp<float> to) {
  _mm_store_ps(to, v);
}
INLINE void Store(const V128<double>& v, crp<double> to) {
  _mm_store_pd(to, v);
}

template <typename T>
INLINE void StoreUnaligned(const V128<T>& v, crp<T> to) {
  _mm_storeu_si128(reinterpret_cast<crp<__m128i>>(to), v);
}
INLINE void StoreUnaligned(const V128<float>& v, crp<float> to) {
  _mm_storeu_ps(to, v);
}
INLINE void StoreUnaligned(const V128<double>& v, crp<double> to) {
  _mm_storeu_pd(to, v);
}

// Writes directly to (aligned) memory, bypassing the cache. This is useful for
// data that will not be read again in the near future.
template <typename T>
INLINE void Stream(const V128<T>& v, crp<T> to) {
  _mm_stream_si128(reinterpret_cast<crp<__m128i>>(to), v);
}
INLINE void Stream(const V128<float>& v, crp<float> to) {
  _mm_stream_ps(to, v);
}
INLINE void Stream(const V128<double>& v, crp<double> to) {
  _mm_stream_pd(to, v);
}

// Miscellaneous functions.

template<typename T>
INLINE V128<T> RotateLeft(const V128<T>& v, const int count) {
  constexpr size_t num_bits = sizeof(T) * 8;
  return (v << count) | (v >> (num_bits - count));
}

template <typename T>
INLINE V128<T> AndNot(const V128<T>& neg_mask, const V128<T>& values) {
  return V128<T>(_mm_andnot_si128(neg_mask, values));
}
template <>
INLINE V128<float> AndNot(const V128<float>& neg_mask,
                          const V128<float>& values) {
  return V128<float>(_mm_andnot_ps(neg_mask, values));
}
template <>
INLINE V128<double> AndNot(const V128<double>& neg_mask,
                           const V128<double>& values) {
  return V128<double>(_mm_andnot_pd(neg_mask, values));
}

using V16x8U = V128<uint8>;
using V8x16U = V128<uint16>;
using V4x32U = V128<uint32>;
using V2x64U = V128<uint64>;
using V4x32F = V128<float>;
using V2x64F = V128<double>;

static INLINE V16x8U UnpackLow(const V16x8U& low, const V16x8U& high) {
  return V16x8U(_mm_unpacklo_epi8(low, high));
}
static INLINE V16x8U UnpackHigh(const V16x8U& low, const V16x8U& high) {
  return V16x8U(_mm_unpackhi_epi8(low, high));
}

static INLINE V8x16U UnpackLow(const V8x16U& low, const V8x16U& high) {
  return V8x16U(_mm_unpacklo_epi16(low, high));
}
static INLINE V8x16U UnpackHigh(const V8x16U& low, const V8x16U& high) {
  return V8x16U(_mm_unpackhi_epi16(low, high));
}

static INLINE V4x32U UnpackLow(const V4x32U& low, const V4x32U& high) {
  return V4x32U(_mm_unpacklo_epi32(low, high));
}
static INLINE V4x32U UnpackHigh(const V4x32U& low, const V4x32U& high) {
  return V4x32U(_mm_unpackhi_epi32(low, high));
}

static INLINE V2x64U UnpackLow(const V2x64U& low, const V2x64U& high) {
  return V2x64U(_mm_unpacklo_epi64(low, high));
}
static INLINE V2x64U UnpackHigh(const V2x64U& low, const V2x64U& high) {
  return V2x64U(_mm_unpackhi_epi64(low, high));
}

static INLINE V4x32F UnpackLow(const V4x32F& low, const V4x32F& high) {
  return V4x32F(_mm_unpacklo_ps(low, high));
}
static INLINE V4x32F UnpackHigh(const V4x32F& low, const V4x32F& high) {
  return V4x32F(_mm_unpackhi_ps(low, high));
}

static INLINE V2x64F UnpackLow(const V2x64F& low, const V2x64F& high) {
  return V2x64F(_mm_unpacklo_pd(low, high));
}
static INLINE V2x64F UnpackHigh(const V2x64F& low, const V2x64F& high) {
  return V2x64F(_mm_unpackhi_pd(low, high));
}

}  // namespace highwayhash

#endif  // #ifdef __SSE4_1__
#endif  // #ifndef HIGHWAYHASH_HIGHWAYHASH_VEC_H_
