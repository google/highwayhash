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

#ifndef HIGHWAYHASH_INSTRUCTION_SETS_H_
#define HIGHWAYHASH_INSTRUCTION_SETS_H_

// Calls the best specialization of a template supported by the current CPU.
//
// Usage: for each dispatch site, declare a Functor template with a 'Target'
// argument, add a source file defining its operator() and instantiating
// Functor<HH_TARGET>, add a cc_library_for_targets rule for that source file,
// and call InstructionSets::Run<Functor>(/*args*/).
//
// WARNING: any source or header file that is compiled with special flags
// (i.e. *_target.cc and its transitive dependencies, including this header)
// must not define inline functions also used from other code.
//
// Background: AVX2 intrinsics require a compiler flag that also allows the
// compiler to generate AVX2 code. Compiling inline functions with differing
// flags violates the one definition rule because the generated code may differ.
// This can lead to crashes if the linker chooses the AVX2 version and uses it
// outside the codepaths guarded by the CPU capability checks in Run().
//
// Workaround: please ensure such source/header files do NOT include any
// headers containing inline functions, nor define/instantiate any inline
// functions themselves that might be used in other code. Note that Run* below
// are only instantiated from normal code, so they are safe.

#include <stdint.h>

#include "highwayhash/arch_specific.h"
#include "highwayhash/compiler_specific.h"

namespace highwayhash {

// Forward declarations because the definitions require target-specific copts.
#if HH_ARCH_X64
struct TargetAVX2;
struct TargetSSE41;
#endif
struct TargetPortable;

// Detects instruction sets and dispatches to the best available specialization
// of a user-defined functor.
class InstructionSets {
 public:
  // Chooses the best available Target* for the current CPU and returns
  // Func<Target>::operator()(a1..a5). Dispatch overhead is low, about 4 cycles,
  // but this should be called infrequently (by hoisting it out of loops).
  // We cannot use variadic arguments because std::forward is defined by
  // <utility>, which also defines other inline functions.
  template <template <class Target> class Func, typename T1, typename T2,
            typename T3, typename T4, typename T5>
  static HH_INLINE void Run(const T1& a1, const T2 a2, const T3 a3, const T4 a4,
                            const T5 a5) {
    const uint64_t flags = Supported();

#if HH_ARCH_X64
    if (HH_LIKELY((flags & kGroupAVX2) == kGroupAVX2)) {
      return Func<TargetAVX2>()(a1, a2, a3, a4, a5);
    } else if (HH_LIKELY((flags & kGroupSSE41) == kGroupSSE41)) {
      return Func<TargetSSE41>()(a1, a2, a3, a4, a5);
    } else
#endif  // HH_ARCH_X64
    {
      return Func<TargetPortable>()(a1, a2, a3, a4, a5);
    }
  }

  // Calls Func<Target>::operator()(a1..a5) for all targets supported by the
  // current CPU. We cannot use variadic arguments because std::forward is
  // defined by <utility>, which also defines other inline functions.
  template <template <class Target> class Func, typename T1, typename T2,
            typename T3, typename T4, typename T5>
  static HH_INLINE void RunAll(const T1& a1, const T2 a2, const T3 a3,
                               const T4 a4, const T5 a5) {
    const uint64_t flags = Supported();

#if HH_ARCH_X64
    if (HH_LIKELY((flags & kGroupAVX2) == kGroupAVX2)) {
      Func<TargetAVX2>()(a1, a2, a3, a4, a5);
    }
    if (HH_LIKELY((flags & kGroupSSE41) == kGroupSSE41)) {
      Func<TargetSSE41>()(a1, a2, a3, a4, a5);
    }
#endif  // HH_ARCH_X64

    Func<TargetPortable>()(a1, a2, a3, a4, a5);
  }

 private:
  // Bits indicating which instruction set extensions are supported.
  // This enables compact/fast implementations of Has*() below.
  enum {
    // Always set so we can distinguish between "not yet initialized" and
    // "no extensions available".
    kInitialized = 1,

#if HH_ARCH_X64
    kSSE = 2,
    kSSE2 = 4,
    kSSE3 = 8,
    kSSSE3 = 0x10,
    kSSE41 = 0x20,
    kSSE42 = 0x40,
    kPOPCNT = 0x80,
    kAVX = 0x100,
    kAVX2 = 0x200,
    kFMA = 0x400,
    kLZCNT = 0x800,
    kBMI = 0x1000,
    kBMI2 = 0x2000,

    kGroupAVX2 = kAVX | kAVX2 | kFMA | kLZCNT | kBMI | kBMI2,
    kGroupSSE41 = kSSE | kSSE2 | kSSE3 | kSSSE3 | kSSE41 | kPOPCNT
#endif  // HH_ARCH_X64
  };

  // Returns bitfield of all instruction sets supported on this CPU.
  // Thread-safe, only detects CPU support once.
  static uint64_t Supported();
};

}  // namespace highwayhash

#endif  // HIGHWAYHASH_INSTRUCTION_SETS_H_
