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

#include "highwayhash/instruction_sets.h"

#include <atomic>

#if HH_ARCH_X64
#include <xmmintrin.h>  // _mm_pause
#endif

namespace highwayhash {
namespace {

bool IsBitSet(const uint32_t reg, const int index) {
  return (reg & (1U << index)) != 0;
}

#if HH_ARCH_X64
// Returns the lower 32 bits of extended control register 0.
// Requires CPU support for "OSXSAVE" (see below).
uint32_t ReadXCR0() {
#if HH_MSC_VERSION
  return static_cast<uint32_t>(_xgetbv(0));
#else
  uint32_t xcr0, xcr0_high;
  const uint32_t index = 0;
  asm volatile(".byte 0x0F, 0x01, 0xD0"
               : "=a"(xcr0), "=d"(xcr0_high)
               : "c"(index));
  return xcr0;
#endif
}
#endif  // HH_ARCH_X64

// The first thread to increment this will initialize instruction_set_bits_.
std::atomic<int> init_counter{0};

}  // namespace

// 0 iff not yet initialized by Supported().
// Not function-local => no compiler-generated locking.
std::atomic<uint64_t> instruction_set_bits_{0};

uint64_t InstructionSets::Supported() {
  uint64_t flags = instruction_set_bits_.load(std::memory_order_acquire);
  // Already initialized, return that.
  if (HH_LIKELY(flags != 0)) {
    return flags;
  }

  // Another thread is initializing; wait until it finishes.
  if (HH_UNLIKELY(init_counter.fetch_add(1) != 0)) {
    for (;;) {
      flags = instruction_set_bits_.load(std::memory_order_acquire);
      if (flags != 0) {
        return flags;
      }
#if HH_ARCH_X64
      _mm_pause();
#endif
    }
  }

  flags = kInitialized;

#if HH_ARCH_X64
  const uint32_t max_level = []() {
    uint32_t abcd[4];
    Cpuid(0, 0, abcd);
    return abcd[0];
  }();

  // Standard feature flags
  const bool has_osxsave = [&flags]() {
    uint32_t abcd[4];
    Cpuid(1, 0, abcd);
    flags += IsBitSet(abcd[3], 25) ? kSSE : 0;
    flags += IsBitSet(abcd[3], 26) ? kSSE2 : 0;
    flags += IsBitSet(abcd[2], 0) ? kSSE3 : 0;
    flags += IsBitSet(abcd[2], 9) ? kSSSE3 : 0;
    flags += IsBitSet(abcd[2], 19) ? kSSE41 : 0;
    flags += IsBitSet(abcd[2], 20) ? kSSE42 : 0;
    flags += IsBitSet(abcd[2], 23) ? kPOPCNT : 0;
    flags += IsBitSet(abcd[2], 12) ? kFMA : 0;
    flags += IsBitSet(abcd[2], 28) ? kAVX : 0;
    return IsBitSet(abcd[2], 27);  // has_osxsave
  }();

  // Extended feature flags
  {
    uint32_t abcd[4];
    Cpuid(0x80000001U, 0, abcd);
    flags += IsBitSet(abcd[2], 5) ? kLZCNT : 0;
  }

  // Extended features
  if (max_level >= 7) {
    uint32_t abcd[4];
    Cpuid(7, 0, abcd);
    flags += IsBitSet(abcd[1], 3) ? kBMI : 0;
    flags += IsBitSet(abcd[1], 5) ? kAVX2 : 0;
    flags += IsBitSet(abcd[1], 8) ? kBMI2 : 0;
  }

  // Verify OS support for XSAVE, without which XMM/YMM registers are not
  // preserved across context switches and are not safe to use.
  if (has_osxsave) {
    const uint32_t xcr0 = ReadXCR0();
    // XMM
    if ((xcr0 & 2) == 0) {
      flags &= ~(kSSE | kSSE2 | kSSE3 | kSSSE3 | kSSE41 | kSSE42 | kAVX |
                 kAVX2 | kFMA);
    }
    // YMM
    if ((xcr0 & 4) == 0) {
      flags &= ~(kAVX | kAVX2);
    }
  }
#endif  // HH_ARCH_X64

  instruction_set_bits_.store(flags, std::memory_order_release);
  return flags;
}

}  // namespace highwayhash
