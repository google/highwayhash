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
bool VerifyResult(Target, const uint64_t size, const HHResult64& expected,
                  const HHResult64& actual) {
  if (expected != actual) {
    printf("%8s: mismatch at %zu: %016lX %016lX\n", Target::Name(), size,
           expected, actual);
    return false;
  }
  return true;
}

// Overload for HHResult128 or HHResult256 (arrays).
template <class Target, size_t kNumLanes>
bool VerifyResult(Target, const size_t size,
                  const uint64_t (&expected)[kNumLanes],
                  const uint64_t (&actual)[kNumLanes]) {
  for (size_t i = 0; i < kNumLanes; ++i) {
    if (expected[i] != actual[i]) {
      printf("%8s: mismatch at %zu[%zu]: %016lX %016lX\n", Target::Name(), size,
             i, expected[i], actual[i]);
      return false;
    }
  }
  return true;
}

}  // namespace

template <class Target>
void HighwayHashTest<Target>::operator()(const HHKey& key,
                                         const char* HH_RESTRICT bytes,
                                         const size_t size,
                                         const HHResult64* expected,
                                         bool* ok) const {
  HHState<Target> state(key);
  HHResult64 actual;
  HighwayHashT(&state, bytes, size, &actual);
  *ok &= VerifyResult(Target(), size, *expected, actual);
}

template <class Target>
void HighwayHashTest<Target>::operator()(const HHKey& key,
                                         const char* HH_RESTRICT bytes,
                                         const size_t size,
                                         const HHResult128* expected,
                                         bool* ok) const {
  HHState<Target> state(key);
  HHResult128 actual;
  HighwayHashT(&state, bytes, size, &actual);
  *ok &= VerifyResult(Target(), size, *expected, actual);
}

template <class Target>
void HighwayHashTest<Target>::operator()(const HHKey& key,
                                         const char* HH_RESTRICT bytes,
                                         const size_t size,
                                         const HHResult256* expected,
                                         bool* ok) const {
  HHState<Target> state(key);
  HHResult256 actual;
  HighwayHashT(&state, bytes, size, &actual);
  *ok &= VerifyResult(Target(), size, *expected, actual);
}

// Instantiate for the current target.
template class HighwayHash<HH_TARGET>;
template class HighwayHashTest<HH_TARGET>;

}  // namespace highwayhash
