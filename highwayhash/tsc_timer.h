#ifndef HIGHWAYHASH_HIGHWAYHASH_TSC_TIMER_H_
#define HIGHWAYHASH_HIGHWAYHASH_TSC_TIMER_H_

// High-resolution (~10 ns) timestamps, using fences to prevent reordering and
// ensure exactly the desired regions are measured.

#include <cstdint>

#include "highwayhash/code_annotation.h"

namespace tsc_timer {

// [Start/Stop][32/64] return absolute timestamps and must be placed
// immediately before and after the region to measure. We provide separate
// Start/Stop functions because they use different fences.
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

// Returns a 32-bit timestamp with about 4 cycles less overhead than
// Start64. Only suitable for measuring very short regions because the
// timestamp overflows about once a second.
static inline uint32_t Start32() {
  uint32_t t;
#if CLANG_VERSION || GCC_VERSION
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

static inline uint32_t Stop32() {
  uint32_t t;
#if CLANG_VERSION || GCC_VERSION
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

static inline uint64_t Start64() {
  uint64_t t;
#if CLANG_VERSION || GCC_VERSION
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

static inline uint64_t Stop64() {
  uint64_t t;
#if CLANG_VERSION || GCC_VERSION
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

}  // namespace tsc_timer

#endif  // HIGHWAYHASH_HIGHWAYHASH_TSC_TIMER_H_
