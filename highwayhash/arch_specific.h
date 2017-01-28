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

#ifndef HIGHWAYHASH_ARCH_SPECIFIC_H_
#define HIGHWAYHASH_ARCH_SPECIFIC_H_

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

#include <stdint.h>
#include <cstdlib>  // _byteswap_*
#include "highwayhash/compiler_specific.h"

namespace highwayhash {

#if defined(__x86_64__) || defined(_M_X64)
#define HH_ARCH_X64 1
#else
#define HH_ARCH_X64 0
#endif

// TODO(janwas): add other platforms as needed.
#if HH_ARCH_X64
#define HH_LITTLE_ENDIAN 1
#define HH_BIG_ENDIAN 0
#else
#define HH_LITTLE_ENDIAN 0
#define HH_BIG_ENDIAN 1
#endif

#if (HH_ARCH_X64 && HH_MSC_VERSION) || defined(__SSE4_1__)
#define HH_ENABLE_SSE41 1
#else
#define HH_ENABLE_SSE41 0
#endif

#if (HH_ARCH_X64 && HH_MSC_VERSION) || defined(__AVX2__)
#define HH_ENABLE_AVX2 1
#else
#define HH_ENABLE_AVX2 0
#endif

#ifdef _MSC_VER
#define HH_BSWAP32(x) _byteswap_ulong(x)
#define HH_BSWAP64(x) _byteswap_uint64(x)
#else
#define HH_BSWAP32(x) __builtin_bswap32(x)
#define HH_BSWAP64(x) __builtin_bswap64(x)
#endif

#if HH_ARCH_X64
void Cpuid(const uint32_t level, const uint32_t count,
           uint32_t* HH_RESTRICT abcd);
#endif

}  // namespace highwayhash

#endif  // HIGHWAYHASH_ARCH_SPECIFIC_H_
