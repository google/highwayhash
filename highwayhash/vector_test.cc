// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <algorithm>
#include <limits>

#ifdef HH_GOOGLETEST
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#endif

#include "highwayhash/vector128.h"
#include "highwayhash/vector256.h"

namespace highwayhash {
namespace {

#ifndef HH_GOOGLETEST
template <typename T1, typename T2>
void EXPECT_EQ(const T1 expected, const T2 actual) {
  if (actual != expected) {
    printf("Mismatch\n");
    abort();
  }
}
#endif

template <class V>
void AllEqual(const V& v, const typename V::T expected) {
  using T = typename V::T;
  T lanes[V::N] HH_ALIGNAS(32);
  Store(v, lanes);
  for (size_t i = 0; i < V::N; ++i) {
    EXPECT_EQ(expected, lanes[i]);
  }
}

// "Native" is the __m256i etc. underlying "V".
template <class V, typename Native>
void TestMembersAndBinaryOperatorsExceptShifts() {
  using T = typename V::T;

  // uninitialized
  V v;

  // broadcast
  const V v2(2);
  AllEqual(v2, T(2));

  // assign from V
  const V v3(3);
  V v3b;
  v3b = v3;
  AllEqual(v3b, T(3));

  // equal
  const V veq(v3 == v3b);
  AllEqual(veq, std::numeric_limits<T>::max());

  // Copying to native and constructing from native yields same result.
  Native nv2 = v2;
  V v2b(nv2);
  AllEqual(v2b, T(2));

  // .. same for assignment from native.
  V v2c;
  v2c = nv2;
  AllEqual(v2c, T(2));

  const V add = v2 + v3;
  AllEqual(add, T(5));

  const V sub = v3 - v2;
  AllEqual(sub, T(1));

  const V vand = v3 & v2;
  AllEqual(vand, T(2));

  const V vor = add | v2;
  AllEqual(vor, T(7));

  const V vxor = v3 ^ v2;
  AllEqual(vxor, T(1));
}

// SSE does not allow shifting uint8_t, so instantiate for all other types.
template <class V>
void TestShifts() {
  using T = typename V::T;

  const V v1(1);
  // Shifting out of right side => zero
  AllEqual(v1 >> 1, T(0));

  // Simple left shift
  AllEqual(v1 << 1, T(2));

  // Sign bit
  constexpr int kSign = (sizeof(T) * 8) - 1;
  constexpr T max = std::numeric_limits<T>::max();
  constexpr T sign = ~(max >> 1);
  AllEqual(v1 << kSign, sign);

  // Shifting out of left side => zero
  AllEqual(v1 << (kSign + 1), T(0));
}

template <class V>
void TestLoadStore() {
  const size_t n = V::N;
  using T = typename V::T;
  T lanes[2 * n] HH_ALIGNAS(32);
  std::fill(lanes, lanes + n, 4);
  std::fill(lanes + n, lanes + 2 * n, 5);
  // Aligned load
  const V v4 = Load<V>(lanes);
  AllEqual(v4, T(4));

  // Aligned store
  T lanes4[n] HH_ALIGNAS(32);
  Store(v4, lanes4);
  for (const T value4 : lanes4) {
    EXPECT_EQ(4, value4);
  }

  // Unaligned load
  const V vu = LoadUnaligned<V>(lanes + 1);
  Store(vu, lanes4);
  EXPECT_EQ(5, lanes4[n - 1]);
  for (size_t i = 1; i < n - 1; ++i) {
    EXPECT_EQ(4, lanes4[i]);
  }

  // Unaligned store
  StoreUnaligned(v4, lanes + n / 2);
  size_t i;
  for (i = 0; i < 3 * n / 2; ++i) {
    EXPECT_EQ(4, lanes[i]);
  }
  // Subsequent values remain unchanged.
  for (; i < 2 * n; ++i) {
    EXPECT_EQ(5, lanes[i]);
  }
}

void TestVector() {
#ifdef __SSE4_1__
  TestMembersAndBinaryOperatorsExceptShifts<V16x8U, __m128i>();
  TestMembersAndBinaryOperatorsExceptShifts<V8x16U, __m128i>();
  TestMembersAndBinaryOperatorsExceptShifts<V4x32U, __m128i>();
  TestMembersAndBinaryOperatorsExceptShifts<V2x64U, __m128i>();

  TestShifts<V8x16U>();
  TestShifts<V4x32U>();
  TestShifts<V2x64U>();

  TestLoadStore<V16x8U>();
  TestLoadStore<V8x16U>();
  TestLoadStore<V4x32U>();
  TestLoadStore<V2x64U>();
#endif

#ifdef __AVX2__
  TestMembersAndBinaryOperatorsExceptShifts<V32x8U, __m256i>();
  TestMembersAndBinaryOperatorsExceptShifts<V16x16U, __m256i>();
  TestMembersAndBinaryOperatorsExceptShifts<V8x32U, __m256i>();
  TestMembersAndBinaryOperatorsExceptShifts<V4x64U, __m256i>();

  TestShifts<V16x16U>();
  TestShifts<V8x32U>();
  TestShifts<V4x64U>();

  TestLoadStore<V32x8U>();
  TestLoadStore<V16x16U>();
  TestLoadStore<V8x32U>();
  TestLoadStore<V4x64U>();
#endif
}

#ifdef HH_GOOGLETEST
TEST(VectorTest, Run) { TestVector(); }
#endif

}  // namespace
}  // namespace highwayhash

#ifndef HH_GOOGLETEST
int main(int argc, char* argv[]) {
  highwayhash::TestVector();
  printf("TestVector succeeded.\n");
  return 0;
}
#endif
