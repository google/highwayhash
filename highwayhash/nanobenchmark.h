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

#ifndef HIGHWAYHASH_NANOBENCHMARK_H_
#define HIGHWAYHASH_NANOBENCHMARK_H_

// Benchmarks functions of a single integer argument with realistic branch
// prediction hit rates. Uses a robust estimator to summarize the measurements.
// Measurements are precise to about 0.2 cycles.
//
// Example:
// #include "highwayhash/nanobenchmark.h"
// nanobenchmark::RaiseThreadPriority();
// nanobenchmark::PinThreadToCPU();
// const std::map<size_t, float> durations =
//     nanobenchmark::MeasureWithArguments({3, 4, 7, 8}, [](const size_t size) {
//       char from[8] = {static_cast<char>(size)};
//       char to[8];
//       memcpy(to, from, size);
//       return to[0];
//     });
// printf("Cycles for input = 3: %4.1f\n", durations[3]);
// printf("Cycles for input = 7: %4.1f\n", durations[7]);
//
// Alternatively, repeating samples and measurements increases the precision:
//
// for (const auto& size_samples : nanobenchmark::RepeatedMeasureWithArguments(
//          {3, 3, 4, 4, 7, 7, 8, 8}, [](const size_t size) {
//            char from[8] = {static_cast<char>(size)};
//            char to[8];
//            memcpy(to, from, size);
//            return to[0];
//          })) {
//   nanobenchmark::PrintMedianAndVariability(size_samples);
// }
//
// Output:
//   3: median= 20.8 cycles; median abs. deviation= 0.1 cycles
//   4: median=  8.8 cycles; median abs. deviation= 0.2 cycles
//   7: median=  8.8 cycles; median abs. deviation= 0.2 cycles
//   8: median= 27.5 cycles; median abs. deviation= 0.1 cycles
// (7 is presumably faster because it can use two unaligned 32-bit load/stores.)
//
// Background: Microbenchmarks such as http://github.com/google/benchmark
// can measure elapsed times on the order of a microsecond. Shorter functions
// are typically measured by repeating them thousands of times and dividing
// the total elapsed time by this count. Unfortunately, repetition (especially
// with the same input parameter!) influences the runtime. In time-critical
// code, it is reasonable to expect warm instruction/data caches and TLBs,
// but a perfect record of which branches will be taken is unrealistic.
// Unless the application also repeatedly invokes the measured function with
// the same parameter, the benchmark is measuring something very different -
// a best-case result, almost as if the parameter were made a compile-time
// constant. This may lead to erroneous conclusions about branch-heavy
// algorithms outperforming branch-free alternatives.
//
// Our approach differs in three ways. Adding fences to the timer functions
// reduces variability due to instruction reordering, improving the timer
// resolution to about 10 nanoseconds. However, shorter functions must still
// be invoked repeatedly. For more realistic branch prediction performance,
// we vary the input parameter according to a user-specified distribution.
// Thus, instead of VaryInputs(Measure(Repeat(func))), we change the
// loop nesting to Measure(Repeat(VaryInputs(func))). We also estimate the
// central tendency of the measurement samples with the "half sample mode",
// which is more robust to outliers and skewed data than the mean or median.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

#include "highwayhash/arch_specific.h"
#include "highwayhash/compiler_specific.h"
#include "highwayhash/os_specific.h"
#include "highwayhash/tsc_timer.h"

// Enables sanity checks that verify correct operation at the cost of
// longer benchmark runs.
#ifndef NANOBENCHMARK_ENABLE_CHECKS
#define NANOBENCHMARK_ENABLE_CHECKS 0
#endif

#define NANOBENCHMARK_CHECK_ALWAYS(condition)                    \
  while (!(condition)) {                                         \
    printf("Nanobenchmark check failed at line %d\n", __LINE__); \
    abort();                                                     \
  }

#if NANOBENCHMARK_ENABLE_CHECKS
#define NANOBENCHMARK_CHECK(condition) NANOBENCHMARK_CHECK_ALWAYS(condition)
#else
#define NANOBENCHMARK_CHECK(condition)
#endif

namespace nanobenchmark {

#if HH_MSC_VERSION

// MSVC does not support inline assembly anymore (and never supported GCC's
// RTL constraints used below), so we instead pass the address to another
// translation unit and assume that link-time code generation is disabled.
void UseCharPointer(volatile const char*);

template <class T>
inline void PreventElision(T&& output) {
  UseCharPointer(reinterpret_cast<volatile const char*>(&output));
}

#else

// Convenience function similar to std::enable_if_t (C++14).
// "Condition" provides a bool value, like std::integral_constant.
// Evaluates to a void return type if Condition is true, otherwise removes from
// consideration the function it annotates.
template <typename Condition>
using EnableIf = typename std::enable_if<Condition::value>::type;

// Simplifies the predicates below; we only special-case floats on x86.
#if HH_ARCH_X64
#define NANOBENCHMARK_IS_FLOAT(T) std::is_floating_point<T>::value
#else
#define NANOBENCHMARK_IS_FLOAT(T) false
#endif

// True for T that can efficiently satisfy +r constraints.
template <typename T>
struct IsRegister {
  // Exclude member pointers, which may be larger. On x86, also exclude
  // floating-point numbers because they reside in separate registers.
  static constexpr bool value = std::is_scalar<T>::value &&
                                !NANOBENCHMARK_IS_FLOAT(T) &&
                                !std::is_member_pointer<T>::value;
};

// True for all other T that do not need special handling.
template <typename T>
struct IsMemory {
  static constexpr bool value =
      !NANOBENCHMARK_IS_FLOAT(T) && !IsRegister<T>::value;
};

// Prevents the compiler from eliding the computations that led to "output".
// Works by indicating to the compiler that "output" is being read and modified.
// To avoid unnecessary writes to memory, this should use a +r constraint. That
// fails to compile with T = string/iterator etc, which we also need to protect
// from elision. Always using +m generates unnecessary stores to memory for
// integer/float arguments. +r,+m?? or even +m! should penalize m and only
// choose it if necessary, but that has the same effect as +m on clang.
// We therefore choose between functions specifying +r and +m based on T.
// SFINAE must be applied to the return type instead of the argument so that T
// can be deduced.
template <class T>
inline EnableIf<IsRegister<T>> PreventElision(T&& output) {
  asm volatile("" : "+r"(output) : : "memory");
}

// x86: Avoids copying floating-point numbers to memory/eax.
#if defined(__x86_64__) || defined(_M_X64)
template <class T>
inline EnableIf<std::is_floating_point<T>> PreventElision(T&& output) {
  // +x = SSE register (used on all x64 for float/double arithmetic).
  asm volatile("" : "+x"(output) : : "memory");
}
#endif

template <class T>
inline EnableIf<IsMemory<T>> PreventElision(T&& output) {
  // Clang generates redundant stores when using +X (anything) or +g
  // (register, memory or immediate).
  asm volatile("" : "+m"(output) : : "memory");
}

#endif

// Input parameter for the function being measured.
using Input = size_t;

// Cycles elapsed = difference between two cycle counts. Must be unsigned to
// ensure wraparound on overflow.
using Duration = uint32_t;

// Returns cycles elapsed when passing each of "inputs" (after in-place
// shuffling) to "func", which must return something it has computed
// so the compiler does not optimize it away.
template <typename Func>
Duration CyclesElapsed(const Duration resolution, const Func& func,
                       std::vector<Input>* inputs) {
  // This benchmark attempts to measure the performance of "func" when
  // called with realistic inputs, which we assume are randomly drawn
  // from the given "inputs" distribution, so we shuffle those values.
  std::random_shuffle(inputs->begin(), inputs->end());

  const Duration t0 = tsc_timer::Start<Duration>();
  for (const Input input : *inputs) {
    PreventElision(func(input));
  }
  const Duration t1 = tsc_timer::Stop<Duration>();
  const Duration elapsed = t1 - t0;
  NANOBENCHMARK_CHECK(elapsed > resolution);
  return elapsed - resolution;
}

// Stores input values for a series of calls to the function to measure.
// We assume inputs are drawn from a known discrete probability distribution,
// modeled as a vector<Input> v. The probability of a value X in v is
// count(v.begin(), v.end(), X) / v.size().
//
// Parameterizing the entire class on Func avoids std::function overhead or
// requiring users to call InitReplicas. Code size is not a major concern.
template <typename Func>
class Inputs {
  Inputs(const Inputs&) = delete;
  Inputs& operator=(const Inputs&) = delete;

 public:
  Inputs(const Duration resolution, const std::vector<Input>& distribution,
         const Func& func)
      : unique_(InitUnique(distribution)),
        replicas_(InitReplicas(distribution, resolution, func)),
        num_replicas_(replicas_.size() / distribution.size()) {
    printf("NumReplicas %zu\n", num_replicas_);
  }

  // Returns vector of the unique values from the input distribution.
  const std::vector<Input>& Unique() const { return unique_; }

  // Returns how many instances of "distribution" are in "replicas_", i.e.
  // the number of occurrences of an input value that occurred only once
  // in the distribution. This is the divisor for computing the duration
  // of a single call.
  size_t NumReplicas() const { return num_replicas_; }

  // Returns the (replicated) input distribution. Modified by caller
  // (shuffled in-place) => not thread-safe.
  std::vector<Input>& Replicas() { return replicas_; }

  // Returns a copy of Replicas() with NumReplicas() occurrences of "input"
  // removed. Used for the leave-one-out measurement.
  std::vector<Input> Without(const Input input_to_remove) const {
    // "input_to_remove" should be in the original distribution.
    NANOBENCHMARK_CHECK(std::find(unique_.begin(), unique_.end(),
                                  input_to_remove) != unique_.end());

    std::vector<Input> copy = replicas_;
    auto pos = std::partition(copy.begin(), copy.end(),
                              [input_to_remove](const Input input) {
                                return input_to_remove != input;
                              });
    // Must occur at least num_replicas_ times.
    NANOBENCHMARK_CHECK(copy.end() - pos >= num_replicas_);
    // (Avoids unused-variable warning.)
    PreventElision(pos);
    copy.resize(copy.size() - num_replicas_);
    return copy;
  }

 private:
  // Returns a copy with any duplicate values removed. Initializing unique_
  // through this function allows it to be const.
  static std::vector<Input> InitUnique(const std::vector<Input>& distribution) {
    std::vector<Input> unique = distribution;
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
    // Our leave-one-out measurement technique only makes sense when
    // there are multiple input values.
    NANOBENCHMARK_CHECK(unique.size() >= 2);
    return unique;
  }

  // Returns how many replicas of "distribution" are required before
  // CyclesElapsed is large enough compared to the timer resolution.
  static std::vector<Input> InitReplicas(const std::vector<Input>& distribution,
                                         const Duration resolution,
                                         const Func& func) {
    // We compute the difference in duration for inputs = Replicas() vs.
    // Without(). Dividing this by num_replicas must yield a value where the
    // quantization error (from the timer resolution) is sufficiently small.
    const uint64_t min_elapsed = distribution.size() * resolution * 400;

    std::vector<Input> replicas;
    for (;;) {
      AppendReplica(distribution, &replicas);

#if NANOBENCHMARK_ENABLE_CHECKS
      const uint64_t t0 = tsc_timer::Start64();
#endif
      const Duration elapsed = CyclesElapsed(resolution, func, &replicas);
#if NANOBENCHMARK_ENABLE_CHECKS
      const uint64_t t1 = tsc_timer::Stop64();
#endif
      // Ensure the 32-bit timer didn't and won't overflow.
      NANOBENCHMARK_CHECK((t1 - t0) < (1ULL << 30));

      if (elapsed >= min_elapsed) {
        return replicas;
      }
    }
  }

  // Appends all values in "distribution" to "replicas".
  static void AppendReplica(const std::vector<Input>& distribution,
                            std::vector<Input>* replicas) {
    replicas->reserve(replicas->size() + distribution.size());
    for (const Input input : distribution) {
      replicas->push_back(input);
    }
  }

  const std::vector<Input> unique_;

  // Modified by caller (shuffled in-place) => non-const.
  std::vector<Input> replicas_;

  // Initialized from replicas_.
  const size_t num_replicas_;
};

// Holds samples of measured durations, and (robustly) reduces them to a
// single result for each unique input value.
class DurationSamples {
 public:
  DurationSamples(const std::vector<Input>& unique_inputs,
                  const size_t num_samples)
      : num_samples_(num_samples) {
    // Preallocate storage.
    for (const Input input : unique_inputs) {
      samples_for_input_[input].reserve(num_samples);
    }
  }

  void Add(const Input input, const Duration sample) {
    // "input" should be one of the values passed to the ctor.
    NANOBENCHMARK_CHECK(samples_for_input_.find(input) !=
                        samples_for_input_.end());

    samples_for_input_[input].push_back(sample);
  }

  // Invokes "lambda" for each (input, duration) pair. The per-call duration
  // is the central tendency (the mode) of the samples.
  template <class Lambda>
  void Reduce(const Lambda& lambda) {
    for (auto& input_and_samples : samples_for_input_) {
      const Input input = input_and_samples.first;
      std::vector<Duration>& samples = input_and_samples.second;

      NANOBENCHMARK_CHECK(samples.size() <= num_samples_);
      std::sort(samples.begin(), samples.end());
      const Duration duration = tsc_timer::Mode(samples.data(), samples.size());
      lambda(input, duration);
    }
  }

 private:
  const size_t num_samples_;
  std::map<Input, std::vector<Duration>> samples_for_input_;
};

// Gathers "num_samples" durations via repeated leave-one-out measurements.
template <typename Func>
DurationSamples GatherDurationSamples(const Duration resolution,
                                      Inputs<Func>& inputs, const Func& func,
                                      const size_t num_samples) {
  DurationSamples samples(inputs.Unique(), num_samples);
  for (size_t i = 0; i < num_samples; ++i) {
    // Total duration for all shuffled input values. This may change over time,
    // so recompute it for each sample.
    const Duration total = CyclesElapsed(resolution, func, &inputs.Replicas());

    for (const Input input : inputs.Unique()) {
      // To isolate the durations of the calls with this input value,
      // we measure the duration without those values and subtract that
      // from the total, and later divide by NumReplicas.
      std::vector<Input> without = inputs.Without(input);
      for (int rep = 0; rep < 3; ++rep) {
        const Duration elapsed = CyclesElapsed(resolution, func, &without);
        if (elapsed < total) {
          samples.Add(input, total - elapsed);
          break;
        }
      }
    }
  }
  return samples;
}

// Public API follows:

// Returns measurements of the cycles elapsed when calling "func" with each
// unique input value from the given "distribution", taking special care to
// maintain realistic branch prediction hit rates.
//
// "distribution" should contain the input values required to trigger both
// sides of all conditional branches in "func". A value's probability of
// being used in the real application should be proportional to its frequency
// in the vector. Order does not matter; for example, a uniform distribution
// over [0, 4) could be represented as {3,0,2,1}. The benchmark duration is
// proportional to |distribution| * |unique values|. Repeating each value at
// least once ensures the leave-one-out distribution is closer to the original
// distribution, leading to more realistic results.
//
// "func" acts like std::function<T(Input)>, where T is a 'proof of work'
// return value used to ensure the computations are not elided.
template <typename Func>
std::map<Input, float> MeasureWithArguments(
    const std::vector<Input>& distribution, const Func& func) {
  const Duration resolution = tsc_timer::Resolution<Duration>();

  // Adds enough 'replicas' of the distribution to measure "func" given
  // the timer resolution.
  Inputs<Func> inputs(resolution, distribution, func);
  const double per_call = 1.0 / static_cast<int>(inputs.NumReplicas());

  auto samples = GatherDurationSamples(resolution, inputs, func, 1024);

  // Return map of duration [cycles] for every unique input value.
  std::map<Input, float> durations;
  samples.Reduce(
      [&durations, per_call](const Input input, const Duration duration) {
        durations[input] = static_cast<float>(duration * per_call);
      });
  NANOBENCHMARK_CHECK(durations.size() == inputs.Unique().size());
  return durations;
}

// Optional functions for robust pooling of multiple measurements:

// Returns vectors of duration samples for each input, for subsequent analysis
// by Median/MedianAbsoluteDeviation. The parameters are documented in
// MeasureWithArguments.
template <typename Func>
std::map<Input, std::vector<float>> RepeatedMeasureWithArguments(
    const std::vector<Input>& distribution, const Func& func,
    const int repetitions = 25) {
  const Duration resolution = tsc_timer::Resolution<Duration>();

  // Adds enough 'replicas' of the distribution to measure "func" given
  // the timer resolution.
  Inputs<Func> inputs(resolution, distribution, func);
  const double per_call = 1.0 / static_cast<int>(inputs.NumReplicas());

  // Preallocate sample storage.
  std::map<Input, std::vector<float>> samples_for_input;
  for (const Input input : distribution) {
    samples_for_input[input].reserve(repetitions);
  }

  for (int i = 0; i < repetitions; ++i) {
    auto samples = GatherDurationSamples(resolution, inputs, func, 512);
    // Scatter each input's duration into the sample arrays.
    samples.Reduce([&samples_for_input, per_call](const Input input,
                                                  const Duration duration) {
      const float sample = static_cast<float>(duration * per_call);
      samples_for_input[input].push_back(sample);
    });
  }
  return samples_for_input;
}

// Returns the median value. Side effect: sorts "samples".
template <typename T>
T Median(std::vector<T>* samples) {
  NANOBENCHMARK_CHECK(!samples->empty());
  std::sort(samples->begin(), samples->end());
  const size_t half = samples->size() / 2;
  // Odd count: return middle
  if (samples->size() % 2) {
    return (*samples)[half];
  }
  // Even count: return average of middle two.
  return ((*samples)[half] + (*samples)[half - 1]) / 2;
}

// Returns a robust measure of variability.
template <typename T>
T MedianAbsoluteDeviation(const std::vector<T>& samples, const T median) {
  NANOBENCHMARK_CHECK(!samples.empty());
  std::vector<T> abs_deviations;
  abs_deviations.reserve(samples.size());
  for (const T sample : samples) {
    abs_deviations.push_back(std::abs(sample - median));
  }
  return Median(&abs_deviations);
}

// Print median duration and variability for this size's samples.
inline void PrintMedianAndVariability(
    const std::pair<Input, std::vector<float>>& input_samples) {
  const Input input = input_samples.first;
  auto samples = input_samples.second;  // Copy (modified by Median)
  const float median = Median(&samples);
  const float variability = MedianAbsoluteDeviation(samples, median);
  printf("%5zu: median=%5.1f cycles; median abs. deviation=%4.1f cycles\n",
         input, median, variability);
}

}  // namespace nanobenchmark

#endif  // HIGHWAYHASH_NANOBENCHMARK_H_
