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

#ifndef HIGHWAYHASH_HIGHWAYHASH_TARGET_H_
#define HIGHWAYHASH_HIGHWAYHASH_TARGET_H_

// Tests called by InstructionSets::RunAll, so we can verify all
// implementations supported by the current CPU.

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

#include <stddef.h>

#include "highwayhash/arch_specific.h"
#include "highwayhash/compiler_specific.h"
#include "highwayhash/hh_types.h"
#include "highwayhash/nanobenchmark.h"

namespace highwayhash {

// Verifies the hash result matches "expected" and calls "notify" if not.
template <TargetBits Target>
struct HighwayHashTest {
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, const HHResult64* expected,
                  const HHNotify notify) const;
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, const HHResult128* expected,
                  const HHNotify notify) const;
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, const HHResult256* expected,
                  const HHNotify notify) const;
};

// For every possible partition of "bytes" into zero to three fragments,
// verifies HighwayHashCat returns the same result as HighwayHashT of the
// concatenated fragments, and calls "notify" if not. The value of "expected"
// is ignored; it is only used for overloading.
template <TargetBits Target>
struct HighwayHashCatTest {
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const uint64_t size, const HHResult64* expected,
                  const HHNotify notify) const;
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const uint64_t size, const HHResult128* expected,
                  const HHNotify notify) const;
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const uint64_t size, const HHResult256* expected,
                  const HHNotify notify) const;
};

// Called by benchmark with prefix, target_name, input_map, context.
// This function must set input_map->num_items to 0.
using NotifyBenchmark = void (*)(const char*, const char*, DurationsForInputs*,
                                 void*);

constexpr size_t kMaxBenchmarkInputSize = 1024;

// Calls "notify" with benchmark results for the input sizes specified by
// "input_map" (<= kMaxBenchmarkInputSize) plus a "context" parameter.
template <TargetBits Target>
struct HighwayHashBenchmark {
  void operator()(DurationsForInputs* input_map, NotifyBenchmark notify,
                  void* context) const;
};

template <TargetBits Target>
struct HighwayHashCatBenchmark {
  void operator()(DurationsForInputs* input_map, NotifyBenchmark notify,
                  void* context) const;
};

}  // namespace highwayhash

#endif  // HIGHWAYHASH_HIGHWAYHASH_TARGET_H_
