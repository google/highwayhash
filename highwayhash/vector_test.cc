#include <algorithm>
#include <limits>
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

#include "highwayhash/vec.h"
#include "highwayhash/vec2.h"

namespace highwayhash {
namespace {

template <class V>
void AllEqual(const V& v, const typename V::T expected) {
  using T = typename V::T;
  alignas(32) T lanes[V::N];
  Store(v, lanes);
  for (const T lane : lanes) {
    EXPECT_EQ(lane, expected);
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

// SSE does not allow shifting uint8, so instantiate for all other types.
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
void TestLoadStore128() {
  const size_t n = V::N;
  using T = typename V::T;
  alignas(16) T lanes[2 * n];
  std::fill(lanes, lanes + n, 4);
  std::fill(lanes + n, lanes + 2 * n, 5);
  // Aligned load
  const V v4 = Load128(lanes);
  AllEqual(v4, T(4));

  // Aligned store
  alignas(16) T lanes4[n];
  Store(v4, lanes4);
  for (const T value4 : lanes4) {
    EXPECT_EQ(4, value4);
  }

  // Unaligned load
  const V vu = LoadUnaligned128(lanes + 1);
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

template <class V>
void TestLoadStore256() {
  const size_t n = V::N;
  using T = typename V::T;
  alignas(32) T lanes[2 * n];
  std::fill(lanes, lanes + n, 4);
  std::fill(lanes + n, lanes + 2 * n, 5);
  // Aligned load
  const V v4 = Load256(lanes);
  AllEqual(v4, T(4));

  // Aligned store
  alignas(32) T lanes4[n];
  Store(v4, lanes4);
  for (const T value4 : lanes4) {
    EXPECT_EQ(4, value4);
  }

  // Unaligned load
  const V vu = LoadUnaligned256(lanes + 1);
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

TEST(VectorTest, TestMembersAndBinaryOperatorsExceptShifts) {
#ifdef __SSE4_1__
  TestMembersAndBinaryOperatorsExceptShifts<V16x8U, __m128i>();
  TestMembersAndBinaryOperatorsExceptShifts<V8x16U, __m128i>();
  TestMembersAndBinaryOperatorsExceptShifts<V4x32U, __m128i>();
  TestMembersAndBinaryOperatorsExceptShifts<V2x64U, __m128i>();
#endif
#ifdef __AVX2__
  TestMembersAndBinaryOperatorsExceptShifts<V32x8U, __m256i>();
  TestMembersAndBinaryOperatorsExceptShifts<V16x16U, __m256i>();
  TestMembersAndBinaryOperatorsExceptShifts<V8x32U, __m256i>();
  TestMembersAndBinaryOperatorsExceptShifts<V4x64U, __m256i>();
#endif
}

TEST(VectorTest, TestShifts) {
#ifdef __SSE4_1__
  TestShifts<V8x16U>();
  TestShifts<V4x32U>();
  TestShifts<V2x64U>();
#endif
#ifdef __AVX2__
  TestShifts<V16x16U>();
  TestShifts<V8x32U>();
  TestShifts<V4x64U>();
#endif
}

#ifdef __SSE4_1__
TEST(VectorTest, TestLoadStore128) {
  TestLoadStore128<V16x8U>();
  TestLoadStore128<V8x16U>();
  TestLoadStore128<V4x32U>();
  TestLoadStore128<V2x64U>();
}
#endif

#ifdef __AVX2__
TEST(VectorTest, TestLoadStore256) {
  TestLoadStore256<V32x8U>();
  TestLoadStore256<V16x16U>();
  TestLoadStore256<V8x32U>();
  TestLoadStore256<V4x64U>();
}
#endif

}  // namespace
}  // namespace highwayhash
