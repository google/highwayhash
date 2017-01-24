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

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

// Adapters for the InstructionSets::Run (or RunAll) dispatcher, which invokes
// the best (or all) implementations available on the current CPU.

#include "highwayhash/compiler_specific.h"
#include "highwayhash/hh_types.h"

namespace highwayhash {

// Usage: InstructionSets::Run<HighwayHash>(key, bytes, size, hash, 0).
// This incurs some small dispatch overhead. If the entire program is compiled
// for the target CPU, you can instead call HighwayHashT directly to avoid any
// overhead. This template is instantiated in the source file, which is
// compiled once for every target with the required flags (e.g. -mavx2).
template <class Target>
struct HighwayHash {
  // Stores a 64/128/256 bit hash of "bytes" using the HighwayHash
  // implementation for the "Target" CPU. The hash result is identical
  // regardless of which implementation is used.
  //
  // "key" is a (randomly generated or hard-coded) HHKey.
  // "bytes" is the data to hash (possibly unaligned).
  // "size" is the number of bytes to hash; we do not read any additional bytes.
  // "hash" is a HHResult* (either 64, 128 or 256 bits).
  // The final parameter ensures the argument count matches HighwayHashTest.
  //
  // HighwayHash is a strong pseudorandom function with security claims
  // [https://arxiv.org/abs/1612.06257]. It is intended as a safer
  // general-purpose hash, 4x faster than SipHash and 10x faster than BLAKE2.
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, HHResult64* HH_RESTRICT hash, int) const;
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, HHResult128* HH_RESTRICT hash, int) const;
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, HHResult256* HH_RESTRICT hash, int) const;
};

// Intended for use with a test; packaging this in target-specific code allows
// invocation via InstructionSets::RunAll.
template <class Target>
struct HighwayHashTest {
  // Verifies the hash result matches "expected"; if not, sets "ok" to false and
  // prints a message to stdout.
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, const HHResult64* expected,
                  bool* ok) const;
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, const HHResult128* expected,
                  bool* ok) const;
  void operator()(const HHKey& key, const char* HH_RESTRICT bytes,
                  const size_t size, const HHResult256* expected,
                  bool* ok) const;
};

}  // namespace highwayhash

#endif  // HIGHWAYHASH_HIGHWAYHASH_TARGET_H_
