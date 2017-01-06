#ifndef THIRD_PARTY_HIGHWAYHASH_HIGHWAYHASH_IACA_H_
#define THIRD_PARTY_HIGHWAYHASH_HIGHWAYHASH_IACA_H_

#include "third_party/highwayhash/highwayhash/code_annotation.h"

// IACA (Intel's Code Analyzer, go/intel-iaca) analyzes instruction latencies,
// but only for code between special markers. These functions embed such markers
// in an executable, but only for reading via IACA - they deliberately trigger
// a crash if executed to ensure they are removed in normal builds.

// Default off; callers must `#define HH_ENABLE_IACA 1` before including this.
#ifndef HH_ENABLE_IACA
#define HH_ENABLE_IACA 0
#endif

namespace highwayhash {

// Call before the region of interest. Fences hopefully prevent reordering.
HH_INLINE void BeginIACA() {
#if HH_ENABLE_IACA && (HH_GCC_VERSION || HH_CLANG_VERSION)
  HH_COMPILER_FENCE;
  asm volatile(
      ".byte 0x0F, 0x0B\n\t"  // UD2
      "movl $111, %ebx\n\t"
      ".byte 0x64, 0x67, 0x90\n\t");
  HH_COMPILER_FENCE;
#endif
}

// Call after the region of interest. Fences hopefully prevent reordering.
HH_INLINE void EndIACA() {
#if HH_ENABLE_IACA && (HH_GCC_VERSION || HH_CLANG_VERSION)
  HH_COMPILER_FENCE;
  asm volatile(
      "movl $222, %ebx\n\t"
      ".byte 0x64, 0x67, 0x90\n\t"
      ".byte 0x0F, 0x0B\n\t");  // UD2
  HH_COMPILER_FENCE;
#endif
}

}  // namespace highwayhash

#endif  // THIRD_PARTY_HIGHWAYHASH_HIGHWAYHASH_IACA_H_
