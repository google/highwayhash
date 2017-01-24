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

#include "highwayhash/arch_specific.h"

#if HH_ARCH_X64
#if !HH_MSC_VERSION
#include <cpuid.h>
#endif
#endif

namespace highwayhash {

#if HH_ARCH_X64
void Cpuid(const uint32_t level, const uint32_t count,
           uint32_t* HH_RESTRICT abcd) {
#if HH_MSC_VERSION
  int regs[4];
  __cpuidex(regs, level, count);
  for (int i = 0; i < 4; ++i) {
    abcd[i] = regs[i];
  }
#else
  uint32_t a, b, c, d;
  __cpuid_count(level, count, a, b, c, d);
  abcd[0] = a;
  abcd[1] = b;
  abcd[2] = c;
  abcd[3] = d;
#endif
}
#endif  // HH_ARCH_X64

}  // namespace highwayhash
