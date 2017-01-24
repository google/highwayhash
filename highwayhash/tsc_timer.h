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

#ifndef HIGHWAYHASH_TSC_TIMER_H_
#define HIGHWAYHASH_TSC_TIMER_H_

// High-resolution (~10 ns) timestamps, using fences to prevent reordering and
// ensure exactly the desired regions are measured.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>
#include <vector>

#include "highwayhash/compiler_specific.h"
#include "highwayhash/os_specific.h"

#if HH_MSC_VERSION
#include <emmintrin.h>  // _mm_lfence
#include <intrin.h>
#endif

namespace tsc_timer {

#define TSC_TIMER_CHECK(condition)                           \
  while (!(condition)) {                                     \
    printf("tsc_timer check failed at line %d\n", __LINE__); \
    abort();                                                 \
  }

// Start/Stop return absolute timestamps and must be placed immediately before
// and after the region to measure. We provide separate Start/Stop functions
// because they use different fences.
//
// Background: RDTSC is not 'serializing'; earlier instructions may complete
// after it, and/or later instructions may complete before it. 'Fences' ensure
// regions' elapsed times are independent of such reordering. The only
// documented unprivileged serializing instruction is CPUID, which acts as a
// full fence (no reordering across it in either direction). Unfortunately
// the latency of CPUID varies wildly (perhaps made worse by not initializing
// its EAX input). Because it cannot reliably be deducted from the region's
// elapsed time, it must not be included in the region to measure (i.e.
// between the two RDTSC).
//
// The newer RDTSCP is sometimes described as serializing, but it actually
// only serves as a half-fence with release semantics. Although all
// instructions in the region will complete before the final timestamp is
// captured, subsequent instructions may leak into the region and increase the
// elapsed time. Inserting another fence after the final RDTSCP would prevent
// such reordering without affecting the measured region.
//
// Fortunately, such a fence exists. The LFENCE instruction is only documented
// to delay later loads until earlier loads are visible. However, Intel's
// reference manual says it acts as a full fence (waiting until all earlier
// instructions have completed, and delaying later instructions until it
// completes). AMD assigns the same behavior to MFENCE.
//
// We need a fence before the initial RDTSC to prevent earlier instructions
// from leaking into the region, and arguably another after RDTSC to avoid
// region instructions from completing before the timestamp is recorded.
// When surrounded by fences, the additional RDTSCP half-fence provides no
// benefit, so the initial timestamp can be recorded via RDTSC, which has
// lower overhead than RDTSCP because it does not read TSC_AUX. In summary,
// we define Start = LFENCE/RDTSC/LFENCE; Stop = RDTSCP/LFENCE.
//
// Using Start+Start leads to higher variance and overhead than Stop+Stop.
// However, Stop+Stop includes an LFENCE in the region measurements, which
// adds a delay dependent on earlier loads. The combination of Start+Stop
// is faster than Start+Start and more consistent than Stop+Stop because
// the first LFENCE already delayed subsequent loads before the measured
// region. This combination seems not to have been considered in prior work:
// http://akaros.cs.berkeley.edu/lxr/akaros/kern/arch/x86/rdtsc_test.c
//
// Note: performance counters can measure 'exact' instructions-retired or
// (unhalted) cycle counts. The RDPMC instruction is not serializing and also
// requires fences. Unfortunately, it is not accessible on all OSes and we
// prefer to avoid kernel-mode drivers. Performance counters are also affected
// by several under/over-count errata, so we use the TSC instead.

template <typename T>
inline T Start() {
  TSC_TIMER_CHECK(false);  // Must use one of the specializations.
}

template <typename T>
inline T Stop() {
  TSC_TIMER_CHECK(false);  // Must use one of the specializations.
}

// Returns a 32-bit timestamp with about 4 cycles less overhead than
// Start<uint64_t>. Only suitable for measuring very short regions because the
// timestamp overflows about once a second.
template <>
inline uint32_t Start<uint32_t>() {
  uint32_t t;
#if HH_MSC_VERSION
  _mm_lfence();
  HH_COMPILER_FENCE;
  t = static_cast<uint32_t>(__rdtsc());
  _mm_lfence();
  HH_COMPILER_FENCE;
#elif HH_CLANG_VERSION || HH_GCC_VERSION
  asm volatile(
      "lfence\n\t"
      "rdtsc\n\t"
      "lfence"
      : "=a"(t)
      :
      // "memory" avoids reordering. rdx = TSC >> 32.
      : "rdx", "memory");
#endif
  return t;
}

template <>
inline uint32_t Stop<uint32_t>() {
  uint32_t t;
#if HH_MSC_VERSION
  HH_COMPILER_FENCE;
  unsigned aux;
  t = static_cast<uint32_t>(__rdtscp(&aux));
  _mm_lfence();
  HH_COMPILER_FENCE;
#elif HH_CLANG_VERSION || HH_GCC_VERSION
  // Use inline asm because __rdtscp generates code to store TSC_AUX (ecx).
  asm volatile(
      "rdtscp\n\t"
      "lfence"
      : "=a"(t)
      :
      // "memory" avoids reordering. rcx = TSC_AUX. rdx = TSC >> 32.
      : "rcx", "rdx", "memory");
#endif
  return t;
}

template <>
inline uint64_t Start<uint64_t>() {
  uint64_t t;
#if HH_MSC_VERSION
  _mm_lfence();
  HH_COMPILER_FENCE;
  t = __rdtsc();
  _mm_lfence();
  HH_COMPILER_FENCE;
#elif HH_CLANG_VERSION || HH_GCC_VERSION
  asm volatile(
      "lfence\n\t"
      "rdtsc\n\t"
      "shl $32, %%rdx\n\t"
      "or %%rdx, %0\n\t"
      "lfence"
      : "=a"(t)
      :
      // "memory" avoids reordering. rdx = TSC >> 32.
      // "cc" = flags modified by SHL.
      : "rdx", "memory", "cc");
#endif
  return t;
}

template <>
inline uint64_t Stop<uint64_t>() {
  uint64_t t;
#if HH_MSC_VERSION
  HH_COMPILER_FENCE;
  unsigned aux;
  t = __rdtscp(&aux);
  _mm_lfence();
  HH_COMPILER_FENCE;
#elif HH_CLANG_VERSION || HH_GCC_VERSION
  // Use inline asm because __rdtscp generates code to store TSC_AUX (ecx).
  asm volatile(
      "rdtscp\n\t"
      "shl $32, %%rdx\n\t"
      "or %%rdx, %0\n\t"
      "lfence"
      : "=a"(t)
      :
      // "memory" avoids reordering. rcx = TSC_AUX. rdx = TSC >> 32.
      // "cc" = flags modified by SHL.
      : "rcx", "rdx", "memory", "cc");
#endif
  return t;
}

// Even with high-priority pinned threads and frequency throttling disabled,
// elapsed times are noisy due to interrupts or SMM operations. It might help
// to detect such events via transactions and omit affected measurements.
// Unfortunately, TSX is currently unavailable due to a bug. We achieve
// repeatable results with a robust measure of the central tendency. The mode
// is less affected by outliers in highly-skewed distributions than the
// median. We use the Half Sample Mode estimator proposed by Bickel in
// "On a fast, robust estimator of the mode". It requires N log N time.

// @return i in [idx_begin, idx_begin + half_count) that minimizes
// sorted[i + half_count] - sorted[i].
template <typename T>
size_t MinRange(const T* const HH_RESTRICT sorted, const size_t idx_begin,
                const size_t half_count) {
  T min_range = std::numeric_limits<T>::max();
  size_t min_idx = 0;

  for (size_t idx = idx_begin; idx < idx_begin + half_count; ++idx) {
    TSC_TIMER_CHECK(sorted[idx] <= sorted[idx + half_count]);
    const T range = sorted[idx + half_count] - sorted[idx];
    if (range < min_range) {
      min_range = range;
      min_idx = idx;
    }
  }

  return min_idx;
}

// Returns an estimate of the mode by calling MinRange on successively
// halved intervals. "sorted" must be in ascending order.
template <typename T>
T Mode(const T* const HH_RESTRICT sorted, const size_t num_values) {
  size_t idx_begin = 0;
  size_t half_count = num_values / 2;
  while (half_count > 1) {
    idx_begin = MinRange(sorted, idx_begin, half_count);
    half_count >>= 1;
  }

  const T x = sorted[idx_begin + 0];
  if (half_count == 0) {
    return x;
  }
  TSC_TIMER_CHECK(half_count == 1);
  const T average = (x + sorted[idx_begin + 1] + 1) / 2;
  return average;
}

// Sorts integral values in ascending order. About 3x faster than std::sort for
// input distributions with very few unique values.
template <class T>
void CountingSort(T* begin, T* end) {
  // Unique values and their frequency (similar to flat_map).
  using Unique = std::pair<T, int>;
  std::vector<Unique> unique;
  for (const T* p = begin; p != end; ++p) {
    const T value = *p;
    const auto pos =
        std::find_if(unique.begin(), unique.end(),
                     [value](const Unique& u) { return u.first == value; });
    if (pos == unique.end()) {
      unique.push_back(std::make_pair(*p, 1));
    } else {
      ++pos->second;
    }
  }

  // Sort in ascending order of value (pair.first).
  std::sort(unique.begin(), unique.end());

  // Write that many copies of each unique value to the array.
  T* HH_RESTRICT p = begin;
  for (const auto& value_count : unique) {
    std::fill(p, p + value_count.second, value_count.first);
    p += value_count.second;
  }
  TSC_TIMER_CHECK(p == end);
}

// Returns an estimate of timer overhead on the current CPU.
template <typename T>
T EstimateResolution() {
  // Even 128K samples are not enough to achieve repeatable results when
  // throttling is enabled; the caller must perform additional aggregation.
  const size_t kNumSamples = 512;
  T samples[kNumSamples];
  for (size_t i = 0; i < kNumSamples; ++i) {
    const volatile T t0 = Start<T>();
    const volatile T t1 = Stop<T>();
    TSC_TIMER_CHECK(t0 <= t1);
    samples[i] = t1 - t0;
  }
  CountingSort(samples, samples + kNumSamples);
  const T resolution = Mode(samples, kNumSamples);
  TSC_TIMER_CHECK(resolution != 0);
  return resolution;
}

// Returns cycles elapsed when running an empty region, i.e. the timer
// resolution/overhead, which will be deducted from other measurements and
// also used by InitReplicas.
// T is the timestamp type, uint32_t or uint64_t.
template <typename T>
T Resolution() {
  // Initialization is expensive and should only happen once. This function is
  // called from function templates; we need to keep and initialize a static
  // variable here because each function template has its own static variables.
  static const T resolution = []() {
    // It is important to return consistent results between runs. Individual
    // CPUs may be slowed down by interrupts or throttled down. We instead
    // measure on all CPUs, and repeat several times so the mode is distinct.
    std::vector<T> resolutions;
    const auto cpus = os_specific::AvailableCPUs();
    const size_t repetitions_per_cpu = 512 / cpus.size();

    auto affinity = os_specific::GetThreadAffinity();
    for (const int cpu : cpus) {
      os_specific::PinThreadToCPU(cpu);
      for (size_t i = 0; i < repetitions_per_cpu; ++i) {
        resolutions.push_back(EstimateResolution<T>());
      }
    }
    os_specific::SetThreadAffinity(affinity);
    free(affinity);

    T* const begin = resolutions.data();
    CountingSort(begin, begin + resolutions.size());
    const T resolution = Mode(begin, resolutions.size());
    printf("Resolution<%zu> %lu\n", sizeof(T) * 8, long(resolution));
    return resolution;
  }();
  return resolution;
}

}  // namespace tsc_timer

#endif  // HIGHWAYHASH_TSC_TIMER_H_
