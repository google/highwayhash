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

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

#include "highwayhash/highwayhash_target.h"

#include "highwayhash/highwayhash.h"
#include "highwayhash/targets.h"

namespace highwayhash {

extern "C" {
#define HH_CONCAT(first, second) first##second
// Need to expand in a second macro because HH_TARGET is a predefined macro.
#define HH_EXPAND_CONCAT(first, second) HH_CONCAT(first, second)
uint64_t HH_EXPAND_CONCAT(HighwayHash64_, HH_TARGET)(const uint64_t* key,
                                                     const char* bytes,
                                                     const uint64_t size) {
  HHState<HH_TARGET> state(*reinterpret_cast<const HHKey*>(key));
  HHResult64 result;
  HighwayHashT(&state, bytes, size, &result);
  return result;
}
}  // extern "C"

template <class Target>
void HighwayHash<Target>::operator()(const HHKey& key,
                                     const char* HH_RESTRICT bytes,
                                     const size_t size,
                                     HHResult64* HH_RESTRICT hash, int) const {
  HHState<Target> state(key);
  HighwayHashT(&state, bytes, size, hash);
}

template <class Target>
void HighwayHash<Target>::operator()(const HHKey& key,
                                     const char* HH_RESTRICT bytes,
                                     const size_t size,
                                     HHResult128* HH_RESTRICT hash, int) const {
  HHState<Target> state(key);
  HighwayHashT(&state, bytes, size, hash);
}

template <class Target>
void HighwayHash<Target>::operator()(const HHKey& key,
                                     const char* HH_RESTRICT bytes,
                                     const size_t size,
                                     HHResult256* HH_RESTRICT hash, int) const {
  HHState<Target> state(key);
  HighwayHashT(&state, bytes, size, hash);
}

namespace {

template <class Target>
void NotifyWhetherEqual(Target, const uint64_t size, const HHResult64& expected,
                        const HHResult64& actual,
                        void (*notify)(const char*, bool)) {
  if (expected != actual) {
    printf("%8s: mismatch at %zu: %016lX %016lX\n", Target::Name(), size,
           expected, actual);
    (*notify)(Target::Name(), false);
  } else {
    (*notify)(Target::Name(), true);
  }
}

// Overload for HHResult128 or HHResult256 (arrays).
template <class Target, size_t kNumLanes>
void NotifyWhetherEqual(Target, const size_t size,
                        const uint64_t (&expected)[kNumLanes],
                        const uint64_t (&actual)[kNumLanes],
                        void (*notify)(const char*, bool)) {
  for (size_t i = 0; i < kNumLanes; ++i) {
    if (expected[i] != actual[i]) {
      printf("%8s: mismatch at %zu[%zu]: %016lX %016lX\n", Target::Name(), size,
             i, expected[i], actual[i]);
      (*notify)(Target::Name(), false);
      return;
    }
  }
  (*notify)(Target::Name(), true);
}

// Shared logic for all HighwayHashTest::operator() overloads.
template <class Target, typename Result>
void TestHighwayHash(HHState<Target>* state, const char* HH_RESTRICT bytes,
                     const size_t size, const Result* expected,
                     void (*notify)(const char*, bool)) {
  Result actual;
  HighwayHashT(state, bytes, size, &actual);
  NotifyWhetherEqual(Target(), size, *expected, actual, notify);
}

// Shared logic for all HighwayHashCatTest::operator() overloads.
template <class Target, typename Result>
void TestHighwayHashCat(const HHKey& key, HHState<Target>* state,
                        const char* HH_RESTRICT bytes, const size_t size,
                        const Result* expected,
                        void (*notify)(const char*, bool)) {
  // Slightly faster to compute the expected prefix hashes only once.
  // Use new instead of vector to avoid headers with inline functions.
  Result* results = new Result[size];
  for (size_t i = 0; i < size; ++i) {
    HHState<Target> state_flat(key);
    HighwayHashT(&state_flat, bytes, i, &results[i]);
  }

  // Splitting into three fragments/Append should cover all codepaths.
  const size_t max_fragment_size = size / 3;
  for (size_t size1 = 0; size1 < max_fragment_size; ++size1) {
    for (size_t size2 = 0; size2 < max_fragment_size; ++size2) {
      for (size_t size3 = 0; size3 < max_fragment_size; ++size3) {
        HighwayHashCatT<Target> cat(key);
        const char* pos = bytes;
        cat.Append(pos, size1);
        pos += size1;
        cat.Append(pos, size2);
        pos += size2;
        cat.Append(pos, size3);
        pos += size3;

        Result result_cat;
        cat.Finalize(&result_cat);

        const size_t total_size = pos - bytes;
        NotifyWhetherEqual(Target(), total_size, results[total_size],
                           result_cat, notify);
      }
    }
  }

  delete[] results;
}

}  // namespace

template <class Target>
void HighwayHashTest<Target>::operator()(
    const HHKey& key, const char* HH_RESTRICT bytes, const size_t size,
    const HHResult64* expected, void (*notify)(const char*, bool)) const {
  HHState<Target> state(key);
  TestHighwayHash(&state, bytes, size, expected, notify);
}

template <class Target>
void HighwayHashTest<Target>::operator()(
    const HHKey& key, const char* HH_RESTRICT bytes, const size_t size,
    const HHResult128* expected, void (*notify)(const char*, bool)) const {
  HHState<Target> state(key);
  TestHighwayHash(&state, bytes, size, expected, notify);
}

template <class Target>
void HighwayHashTest<Target>::operator()(
    const HHKey& key, const char* HH_RESTRICT bytes, const size_t size,
    const HHResult256* expected, void (*notify)(const char*, bool)) const {
  HHState<Target> state(key);
  TestHighwayHash(&state, bytes, size, expected, notify);
}

template <class Target>
void HighwayHashCatTest<Target>::operator()(
    const HHKey& key, const char* HH_RESTRICT bytes, const uint64_t size,
    const HHResult64* expected, void (*notify)(const char*, bool)) const {
  HHState<Target> state(key);
  TestHighwayHashCat(key, &state, bytes, size, expected, notify);
}

template <class Target>
void HighwayHashCatTest<Target>::operator()(
    const HHKey& key, const char* HH_RESTRICT bytes, const uint64_t size,
    const HHResult128* expected, void (*notify)(const char*, bool)) const {
  HHState<Target> state(key);
  TestHighwayHashCat(key, &state, bytes, size, expected, notify);
}

template <class Target>
void HighwayHashCatTest<Target>::operator()(
    const HHKey& key, const char* HH_RESTRICT bytes, const uint64_t size,
    const HHResult256* expected, void (*notify)(const char*, bool)) const {
  HHState<Target> state(key);
  TestHighwayHashCat(key, &state, bytes, size, expected, notify);
}

// Instantiate for the current target.
template struct HighwayHash<HH_TARGET>;
template struct HighwayHashTest<HH_TARGET>;
template struct HighwayHashCatTest<HH_TARGET>;

}  // namespace highwayhash
