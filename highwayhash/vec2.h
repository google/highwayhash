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

// Primary template for 256-bit AVX2 vectors; only specializations are used.
template <typename T>
class V256 {};

template <>
class V256<uint8> {
 public:
  using T = uint8;
  static constexpr size_t N = 32;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V256() {}

  // Broadcasts i to all lanes.
  INLINE explicit V256(T i)
      : v_(_mm256_broadcastb_epi8(_mm_cvtsi32_si128(i))) {}

  // Converts to/from intrinsics.
  INLINE explicit V256(const __m256i& v) : v_(v) {}
  INLINE operator __m256i() const { return v_; }
  INLINE V256& operator=(const __m256i& v) {
    v_ = v;
    return *this;
  }
  INLINE V256& operator=(const V256& other) {
    v_ = other.v_;
    return *this;
  }

  // There are no greater-than comparison instructions for unsigned T.
  INLINE V256 operator==(const V256& other) {
    return V256(_mm256_cmpeq_epi8(v_, other.v_));
  }

  INLINE V256& operator+=(const V256& other) {
    v_ = _mm256_add_epi8(v_, other);
    return *this;
  }
  INLINE V256& operator-=(const V256& other) {
    v_ = _mm256_sub_epi8(v_, other);
    return *this;
  }

  INLINE V256& operator&=(const V256& other) {
    v_ = _mm256_and_si256(v_, other);
    return *this;
  }
  INLINE V256& operator|=(const V256& other) {
    v_ = _mm256_or_si256(v_, other);
    return *this;
  }
  INLINE V256& operator^=(const V256& other) {
    v_ = _mm256_xor_si256(v_, other);
    return *this;
  }

 private:
  __m256i v_;
};

template <>
class V256<uint16> {
 public:
  using T = uint16;
  static constexpr size_t N = 16;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V256() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V256(T p_F, T p_E, T p_D, T p_C, T p_B, T p_A, T p_9, T p_8, T p_7,
              T p_6, T p_5, T p_4, T p_3, T p_2, T p_1, T p_0)
      : v_(_mm256_set_epi16(p_F, p_E, p_D, p_C, p_B, p_A, p_9, p_8, p_7, p_6,
                            p_5, p_4, p_3, p_2, p_1, p_0)) {}

  // Broadcasts i to all lanes.
  INLINE explicit V256(T i)
      : v_(_mm256_broadcastw_epi16(_mm_cvtsi32_si128(i))) {}

  // Converts to/from intrinsics.
  INLINE explicit V256(const __m256i& v) : v_(v) {}
  INLINE operator __m256i() const { return v_; }
  INLINE V256& operator=(const __m256i& v) {
    v_ = v;
    return *this;
  }
  INLINE V256& operator=(const V256& other) {
    v_ = other.v_;
    return *this;
  }

  // There are no greater-than comparison instructions for unsigned T.
  INLINE V256 operator==(const V256& other) {
    return V256(_mm256_cmpeq_epi16(v_, other.v_));
  }

  INLINE V256& operator+=(const V256& other) {
    v_ = _mm256_add_epi16(v_, other);
    return *this;
  }
  INLINE V256& operator-=(const V256& other) {
    v_ = _mm256_sub_epi16(v_, other);
    return *this;
  }

  INLINE V256& operator&=(const V256& other) {
    v_ = _mm256_and_si256(v_, other);
    return *this;
  }
  INLINE V256& operator|=(const V256& other) {
    v_ = _mm256_or_si256(v_, other);
    return *this;
  }
  INLINE V256& operator^=(const V256& other) {
    v_ = _mm256_xor_si256(v_, other);
    return *this;
  }

  INLINE V256& operator<<=(const int count) {
    v_ = _mm256_slli_epi16(v_, count);
    return *this;
  }
  INLINE V256& operator<<=(const __m128i& count) {
    v_ = _mm256_sll_epi16(v_, count);
    return *this;
  }

  INLINE V256& operator>>=(const int count) {
    v_ = _mm256_srli_epi16(v_, count);
    return *this;
  }
  INLINE V256& operator>>=(const __m128i& count) {
    v_ = _mm256_srl_epi16(v_, count);
    return *this;
  }

 private:
  __m256i v_;
};

template <>
class V256<uint32> {
 public:
  using T = uint32;
  static constexpr size_t N = 8;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V256() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V256(T p_7, T p_6, T p_5, T p_4, T p_3, T p_2, T p_1, T p_0)
      : v_(_mm256_set_epi32(p_7, p_6, p_5, p_4, p_3, p_2, p_1, p_0)) {}

  // Broadcasts i to all lanes.
  INLINE explicit V256(T i)
      : v_(_mm256_broadcastd_epi32(_mm_cvtsi32_si128(i))) {}

  // Converts to/from intrinsics.
  INLINE explicit V256(const __m256i& v) : v_(v) {}
  INLINE operator __m256i() const { return v_; }
  INLINE V256& operator=(const __m256i& v) {
    v_ = v;
    return *this;
  }
  INLINE V256& operator=(const V256& other) {
    v_ = other.v_;
    return *this;
  }

  // There are no greater-than comparison instructions for unsigned T.
  INLINE V256 operator==(const V256& other) {
    return V256(_mm256_cmpeq_epi32(v_, other.v_));
  }

  INLINE V256& operator+=(const V256& other) {
    v_ = _mm256_add_epi32(v_, other);
    return *this;
  }
  INLINE V256& operator-=(const V256& other) {
    v_ = _mm256_sub_epi32(v_, other);
    return *this;
  }

  INLINE V256& operator&=(const V256& other) {
    v_ = _mm256_and_si256(v_, other);
    return *this;
  }
  INLINE V256& operator|=(const V256& other) {
    v_ = _mm256_or_si256(v_, other);
    return *this;
  }
  INLINE V256& operator^=(const V256& other) {
    v_ = _mm256_xor_si256(v_, other);
    return *this;
  }

  INLINE V256& operator<<=(const int count) {
    v_ = _mm256_slli_epi32(v_, count);
    return *this;
  }
  INLINE V256& operator<<=(const __m128i& count) {
    v_ = _mm256_sll_epi32(v_, count);
    return *this;
  }

  INLINE V256& operator>>=(const int count) {
    v_ = _mm256_srli_epi32(v_, count);
    return *this;
  }
  INLINE V256& operator>>=(const __m128i& count) {
    v_ = _mm256_srl_epi32(v_, count);
    return *this;
  }

 private:
  __m256i v_;
};

template <>
class V256<uint64> {
 public:
  using T = uint64;
  static constexpr size_t N = 4;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V256() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V256(T p_3, T p_2, T p_1, T p_0)
      : v_(_mm256_set_epi64x(p_3, p_2, p_1, p_0)) {}

  // Broadcasts i to all lanes.
  INLINE explicit V256(T i)
      : v_(_mm256_broadcastq_epi64(_mm_cvtsi64_si128(i))) {}

  // Converts to/from intrinsics.
  INLINE explicit V256(const __m256i& v) : v_(v) {}
  INLINE operator __m256i() const { return v_; }
  INLINE V256& operator=(const __m256i& v) {
    v_ = v;
    return *this;
  }
  INLINE V256& operator=(const V256& other) {
    v_ = other.v_;
    return *this;
  }

  // There are no greater-than comparison instructions for unsigned T.
  INLINE V256 operator==(const V256& other) {
    return V256(_mm256_cmpeq_epi64(v_, other.v_));
  }

  INLINE V256& operator+=(const V256& other) {
    v_ = _mm256_add_epi64(v_, other);
    return *this;
  }
  INLINE V256& operator-=(const V256& other) {
    v_ = _mm256_sub_epi64(v_, other);
    return *this;
  }

  INLINE V256& operator&=(const V256& other) {
    v_ = _mm256_and_si256(v_, other);
    return *this;
  }
  INLINE V256& operator|=(const V256& other) {
    v_ = _mm256_or_si256(v_, other);
    return *this;
  }
  INLINE V256& operator^=(const V256& other) {
    v_ = _mm256_xor_si256(v_, other);
    return *this;
  }

  INLINE V256& operator<<=(const int count) {
    v_ = _mm256_slli_epi64(v_, count);
    return *this;
  }
  INLINE V256& operator<<=(const __m128i& count) {
    v_ = _mm256_sll_epi64(v_, count);
    return *this;
  }

  INLINE V256& operator>>=(const int count) {
    v_ = _mm256_srli_epi64(v_, count);
    return *this;
  }
  INLINE V256& operator>>=(const __m128i& count) {
    v_ = _mm256_srl_epi64(v_, count);
    return *this;
  }

 private:
  __m256i v_;
};

template <>
class V256<float> {
 public:
  using T = float;
  static constexpr size_t N = 8;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V256() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V256(T p_7, T p_6, T p_5, T p_4, T p_3, T p_2, T p_1, T p_0)
      : v_(_mm256_set_ps(p_7, p_6, p_5, p_4, p_3, p_2, p_1, p_0)) {}

  // Broadcasts to all lanes.
  INLINE explicit V256(T f) : v_(_mm256_set1_ps(f)) {}

  // Converts to/from intrinsics.
  INLINE explicit V256(const __m256& v) : v_(v) {}
  INLINE operator __m256() const { return v_; }
  INLINE V256& operator=(const __m256& v) {
    v_ = v;
    return *this;
  }
  INLINE V256& operator=(const V256& other) {
    v_ = other.v_;
    return *this;
  }

  // There are too many comparison operators; use _mm256_cmp_ps instead.

  INLINE V256& operator+=(const V256& other) {
    v_ = _mm256_add_ps(v_, other);
    return *this;
  }
  INLINE V256& operator-=(const V256& other) {
    v_ = _mm256_sub_ps(v_, other);
    return *this;
  }

  INLINE V256& operator&=(const V256& other) {
    v_ = _mm256_and_ps(v_, other);
    return *this;
  }
  INLINE V256& operator|=(const V256& other) {
    v_ = _mm256_or_ps(v_, other);
    return *this;
  }
  INLINE V256& operator^=(const V256& other) {
    v_ = _mm256_xor_ps(v_, other);
    return *this;
  }

 private:
  __m256 v_;
};

template <>
class V256<double> {
 public:
  using T = double;
  static constexpr size_t N = 4;

  // Leaves v_ uninitialized - typically used for output parameters.
  INLINE V256() {}

  // Lane 0 (p_0) is the lowest.
  INLINE V256(T p_3, T p_2, T p_1, T p_0)
      : v_(_mm256_set_pd(p_3, p_2, p_1, p_0)) {}

  // Broadcasts to all lanes.
  INLINE explicit V256(T f) : v_(_mm256_set1_pd(f)) {}

  // Converts to/from intrinsics.
  INLINE explicit V256(const __m256d& v) : v_(v) {}
  INLINE operator __m256d() const { return v_; }
  INLINE V256& operator=(const __m256d& v) {
    v_ = v;
    return *this;
  }
  INLINE V256& operator=(const V256& other) {
    v_ = other.v_;
    return *this;
  }

  // There are too many comparison operators; use _mm256_cmp_pd instead.

  INLINE V256& operator+=(const V256& other) {
    v_ = _mm256_add_pd(v_, other);
    return *this;
  }
  INLINE V256& operator-=(const V256& other) {
    v_ = _mm256_sub_pd(v_, other);
    return *this;
  }

  INLINE V256& operator&=(const V256& other) {
    v_ = _mm256_and_pd(v_, other);
    return *this;
  }
  INLINE V256& operator|=(const V256& other) {
    v_ = _mm256_or_pd(v_, other);
    return *this;
  }
  INLINE V256& operator^=(const V256& other) {
    v_ = _mm256_xor_pd(v_, other);
    return *this;
  }

 private:
  __m256d v_;
};

// Nonmember functions for any V256 via member functions.

template <typename T>
INLINE V256<T> operator+(const V256<T>& left, const V256<T>& right) {
  V256<T> t(left);
  return t += right;
}

template <typename T>
INLINE V256<T> operator-(const V256<T>& left, const V256<T>& right) {
  V256<T> t(left);
  return t -= right;
}

template <typename T>
INLINE V256<T> operator&(const V256<T>& left, const V256<T>& right) {
  V256<T> t(left);
  return t &= right;
}

template <typename T>
INLINE V256<T> operator|(const V256<T> left, const V256<T>& right) {
  V256<T> t(left);
  return t |= right;
}

template <typename T>
INLINE V256<T> operator^(const V256<T>& left, const V256<T>& right) {
  V256<T> t(left);
  return t ^= right;
}

template <typename T>
INLINE V256<T> operator<<(const V256<T>& v, const int count) {
  V256<T> t(v);
  return t <<= count;
}

template <typename T>
INLINE V256<T> operator>>(const V256<T>& v, const int count) {
  V256<T> t(v);
  return t >>= count;
}

template <typename T>
INLINE V256<T> operator<<(const V256<T>& v, const __m128i& count) {
  V256<T> t(v);
  return t <<= count;
}

template <typename T>
INLINE V256<T> operator>>(const V256<T>& v, const __m128i& count) {
  V256<T> t(v);
  return t >>= count;
}

// Load/Store for any V256.

// The function name requires a "256" suffix to differentiate from SSE4 loads.
// "from" must be vector-aligned.
template <typename T>
INLINE V256<T> Load256(crpc<T> from) {
  crpc<__m256i> p = reinterpret_cast<crpc<__m256i>>(from);
  return V256<T>(_mm256_load_si256(p));
}
INLINE V256<float> Load256(crpc<float> from) {
  return V256<float>(_mm256_load_ps(from));
}
INLINE V256<double> Load256(crpc<double> from) {
  return V256<double>(_mm256_load_pd(from));
}

template <typename T>
INLINE V256<T> LoadUnaligned256(crpc<T> from) {
  crpc<__m256i> p = reinterpret_cast<crpc<__m256i>>(from);
  return V256<T>(_mm256_loadu_si256(p));
}
INLINE V256<float> LoadUnaligned256(crpc<float> from) {
  return V256<float>(_mm256_loadu_ps(from));
}
INLINE V256<double> LoadUnaligned256(crpc<double> from) {
  return V256<double>(_mm256_loadu_pd(from));
}

// "to" must be vector-aligned.
template <typename T>
INLINE void Store(const V256<T>& v, crp<T> to) {
  _mm256_store_si256(reinterpret_cast<crp<__m256i>>(to), v);
}
INLINE void Store(const V256<float>& v, crp<float> to) {
  _mm256_store_ps(to, v);
}
INLINE void Store(const V256<double>& v, crp<double> to) {
  _mm256_store_pd(to, v);
}

template <typename T>
INLINE void StoreUnaligned(const V256<T>& v, crp<T> to) {
  _mm256_storeu_si256(reinterpret_cast<crp<__m256i>>(to), v);
}
INLINE void StoreUnaligned(const V256<float>& v, crp<float> to) {
  _mm256_storeu_ps(to, v);
}
INLINE void StoreUnaligned(const V256<double>& v, crp<double> to) {
  _mm256_storeu_pd(to, v);
}

// Writes directly to (aligned) memory, bypassing the cache. This is useful for
// data that will not be read again in the near future.
template <typename T>
INLINE void Stream(const V256<T>& v, crp<T> to) {
  _mm256_stream_si256(reinterpret_cast<crp<__m256i>>(to), v);
}
INLINE void Stream(const V256<float>& v, crp<float> to) {
  _mm256_stream_ps(to, v);
}
INLINE void Stream(const V256<double>& v, crp<double> to) {
  _mm256_stream_pd(to, v);
}

// Miscellaneous functions.

template <typename T>
INLINE V256<T> RotateLeft(const V256<T>& v, const int count) {
  constexpr size_t num_bits = sizeof(T) * 8;
  return (v << count) | (v >> (num_bits - count));
}

template <typename T>
INLINE V256<T> AndNot(const V256<T>& neg_mask, const V256<T>& values) {
  return V256<T>(_mm256_andnot_si256(neg_mask, values));
}
template <>
INLINE V256<float> AndNot(const V256<float>& neg_mask,
                          const V256<float>& values) {
  return V256<float>(_mm256_andnot_ps(neg_mask, values));
}
template <>
INLINE V256<double> AndNot(const V256<double>& neg_mask,
                           const V256<double>& values) {
  return V256<double>(_mm256_andnot_pd(neg_mask, values));
}

using V32x8U = V256<uint8>;
using V16x16U = V256<uint16>;
using V8x32U = V256<uint32>;
using V4x64U = V256<uint64>;
using V8x32F = V256<float>;
using V4x64F = V256<double>;

static INLINE V32x8U UnpackLow(const V32x8U& low, const V32x8U& high) {
  return V32x8U(_mm256_unpacklo_epi8(low, high));
}
static INLINE V32x8U UnpackHigh(const V32x8U& low, const V32x8U& high) {
  return V32x8U(_mm256_unpackhi_epi8(low, high));
}

static INLINE V16x16U UnpackLow(const V16x16U& low, const V16x16U& high) {
  return V16x16U(_mm256_unpacklo_epi16(low, high));
}
static INLINE V16x16U UnpackHigh(const V16x16U& low, const V16x16U& high) {
  return V16x16U(_mm256_unpackhi_epi16(low, high));
}

static INLINE V8x32U UnpackLow(const V8x32U& low, const V8x32U& high) {
  return V8x32U(_mm256_unpacklo_epi32(low, high));
}
static INLINE V8x32U UnpackHigh(const V8x32U& low, const V8x32U& high) {
  return V8x32U(_mm256_unpackhi_epi32(low, high));
}

static INLINE V4x64U UnpackLow(const V4x64U& low, const V4x64U& high) {
  return V4x64U(_mm256_unpacklo_epi64(low, high));
}
static INLINE V4x64U UnpackHigh(const V4x64U& low, const V4x64U& high) {
  return V4x64U(_mm256_unpackhi_epi64(low, high));
}

static INLINE V8x32F UnpackLow(const V8x32F& low, const V8x32F& high) {
  return V8x32F(_mm256_unpacklo_ps(low, high));
}
static INLINE V8x32F UnpackHigh(const V8x32F& low, const V8x32F& high) {
  return V8x32F(_mm256_unpackhi_ps(low, high));
}

static INLINE V4x64F UnpackLow(const V4x64F& low, const V4x64F& high) {
  return V4x64F(_mm256_unpacklo_pd(low, high));
}
static INLINE V4x64F UnpackHigh(const V4x64F& low, const V4x64F& high) {
  return V4x64F(_mm256_unpackhi_pd(low, high));
}

}  // namespace highwayhash

#endif  // #ifdef __AVX2__
#endif  // #ifndef HIGHWAYHASH_HIGHWAYHASH_VEC2_H_
