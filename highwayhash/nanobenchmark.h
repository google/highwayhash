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
//   #include "highwayhash/nanobenchmark.h"
//   using namespace highwayhash;
//
//   uint64_t RegionToMeasure(const void*, size_t size) {
//     char from[8] = {static_cast<char>(size)};
//     char to[8];
//     memcpy(to, from, size);
//     return to[0];
//   }
//
//   PinThreadToRandomCPU();
//
//   static const size_t distribution[] = {3, 3, 4, 4, 7, 7, 8, 8};
//   DurationsForInputs input_map = MakeDurationsForInputs(distribution, 10);
//   MeasureDurations(&RegionToMeasure, &input_map);
//   for (size_t i = 0; i < input_map.num_items; ++i) {
//     input_map.items[i].PrintMedianAndVariability();
//   }
//
// Output:
//   3: median= 25.2 ticks; median abs. deviation= 0.1 ticks
//   4: median= 13.5 ticks; median abs. deviation= 0.1 ticks
//   7: median= 13.5 ticks; median abs. deviation= 0.1 ticks
//   8: median= 27.5 ticks; median abs. deviation= 0.2 ticks
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

// WARNING: this is a "restricted" header because it is included from
// translation units compiled with different flags. This header and its
// dependencies must not define any function unless it is static inline and/or
// within namespace HH_TARGET_NAME. See arch_specific.h for details.

#include <stddef.h>
#include <stdint.h>
#include "highwayhash/arch_specific.h"
#include "highwayhash/compiler_specific.h"

namespace highwayhash {

// Argument to the function being measured (e.g. number of bytes to copy).
using FuncInput = size_t;

// "Proof of work" returned by the function to ensure it is not elided.
using FuncOutput = uint64_t;

// Function to measure (cannot use std::function in a restricted header).
// Users either pass a function pointer or captureless lambda with this
// signature, or use MeasureClosureDuration to convert a closure (e.g. lambda
// with captures) to this kind of function pointer.
using Func = FuncOutput (*)(const void*, FuncInput);

// Flat map of input -> durations[]. NOTE: durations are 'ticks' (tsc_timer.h);
// convert to seconds via division by InvariantTicksPerSecond.
class DurationsForInputs {
 public:
  struct Item {
    // The optional "mul" scaling factor is applied to median and variability
    // (useful for reporting cycles per byte etc.)
    void PrintMedianAndVariability(const double mul = 1.0);

    FuncInput input;       // read-only (set by AddItem).
    size_t num_durations;  // written so far: [0, max_durations).
    float* durations;      // max_durations entries; points into all_durations.
  };

  // "inputs" is an array of "num_inputs" (not necessarily unique) arguments to
  // "func". The values are chosen to maximize coverage of "func". The pointer
  // must remain valid until after MeasureDurations. This represents a
  // distribution, so a value's frequency should reflect its probability in the
  // real application. Order does not matter; for example, a uniform
  // distribution over [0, 4) could be represented as {3,0,2,1}. Repeating each
  // value at least once ensures the leave-one-out distribution is closer to the
  // original distribution, leading to more realistic results.
  //
  // "max_durations" is the number of duration samples to measure for each
  // unique input value. Larger values decrease variability.
  //
  // Runtime is proportional to "num_inputs" * #unique * "max_durations".
  DurationsForInputs(const FuncInput* inputs, const size_t num_inputs,
                     const size_t max_durations);
  ~DurationsForInputs();

  // Adds an item with the given "input" and "sample". Must only be called once
  // per unique "input" value.
  void AddItem(const FuncInput input, const float sample);

  // Adds "sample" to an already existing Item with the given "input".
  void AddSample(const FuncInput input, const float sample);

  // Allow direct inspection of items[0..num_items-1] because accessor or
  // ForeachItem functions are unsafe in a restricted header.
  Item* items;       // owned by this class, do not allocate/free.
  size_t num_items;  // safe to reset to zero.

 private:
  friend void MeasureDurations(Func, DurationsForInputs*, const uint8_t*);

  const FuncInput* const inputs_;
  const size_t num_inputs_;
  const size_t max_durations_;
  float* const all_durations_;
};

// Helper function to detect num_inputs from arrays.
template <size_t N>
static HH_INLINE DurationsForInputs MakeDurationsForInputs(
    const FuncInput (&inputs)[N], const size_t max_durations) {
  return DurationsForInputs(&inputs[0], N, max_durations);
}

// Returns precise measurements of the number of ticks (see tsc_timer.h)
// elapsed when calling "func" with each unique input value in "input_map",
// taking special care to maintain realistic branch prediction hit rates.
//
// "func" returns a 'proof of work' to ensure its computations are not elided.
// "arg*" are for use by MeasureClosureDurations.
void MeasureDurations(const Func func, DurationsForInputs* input_map,
                      const uint8_t* arg = nullptr);

namespace HH_TARGET_NAME {
// Calls operator() of the given closure (lambda function).
template <class Closure>
static FuncOutput CallClosure(const Closure* f, const FuncInput input) {
  return (*f)(input);
}
}  // namespace HH_TARGET_NAME

// Returns a function pointer that will be called with the address of "closure".
template <class Closure>
static HH_INLINE Func MakeThunk(const Closure& closure) {
  return reinterpret_cast<Func>(&HH_TARGET_NAME::CallClosure<Closure>);
}

// Same as MeasureDurations, except "closure" is typically a lambda function of
// FuncInput -> FuncOutput with a capture list.
template <class Closure>
void HH_INLINE MeasureClosureDurations(const Closure& closure,
                                       DurationsForInputs* input_map) {
  MeasureDurations(MakeThunk(closure), input_map,
                   reinterpret_cast<const uint8_t*>(&closure));
}

}  // namespace highwayhash

#endif  // HIGHWAYHASH_NANOBENCHMARK_H_
