// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef HIGHWAYHASH_HIGHWAYHASH_CODE_ANNOTATION_H_
#define HIGHWAYHASH_HIGHWAYHASH_CODE_ANNOTATION_H_

// Compiler

// #if is shorter and safer than #ifdef. *_VERSION are zero if not detected,
// otherwise 100 * major + minor version. Note that other packages check for
// #ifdef COMPILER_MSVC, so we cannot use that same name.

#ifdef _MSC_VER
#define MSC_VERSION _MSC_VER
#else
#define MSC_VERSION 0
#endif

#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define GCC_VERSION 0
#endif

#ifdef __clang__
#define CLANG_VERSION (__clang_major__ * 100 + __clang_minor__)
#else
#define CLANG_VERSION 0
#endif

// Architecture

#if defined(__x86_64__) || defined(_M_X64)
#define ARCH_X64 1
#else
#define ARCH_X64 0
#endif

//-----------------------------------------------------------------------------

#define NONCOPYABLE(className)          \
  className(const className&) = delete; \
  const className& operator=(const className&) = delete;

#if MSC_VERSION
#define RESTRICT __restrict
#elif GCC_VERSION
#define RESTRICT __restrict__
#else
#define RESTRICT
#endif

#if MSC_VERSION
#define INLINE __forceinline
#else
#define INLINE inline
#endif

#if MSC_VERSION
#define NORETURN __declspec(noreturn)
#elif GCC_VERSION
#define NORETURN __attribute__((noreturn))
#endif

#if MSC_VERSION
#include <intrin.h>
#pragma intrinsic(_ReadWriteBarrier)
#define COMPILER_FENCE _ReadWriteBarrier()
#elif GCC_VERSION
#define COMPILER_FENCE asm volatile("" : : : "memory")
#else
#define COMPILER_FENCE
#endif

// Informs the compiler that the preceding function returns a pointer with the
// specified alignment. This may improve code generation.
#if MSC_VERSION
#define CACHE_ALIGNED_RETURN /* not supported */
#else
#define CACHE_ALIGNED_RETURN __attribute__((assume_aligned(64)))
#endif

#if MSC_VERSION
#define DEBUG_BREAK __debugbreak()
#elif CLANG_VERSION
#define DEBUG_BREAK __builtin_debugger()
#elif GCC_VERSION
#define DEBUG_BREAK __builtin_trap()
#endif

#if MSC_VERSION
#include <sal.h>
#define FORMAT_STRING(s) _Printf_format_string_ s
#else
#define FORMAT_STRING(s) s
#endif

// Function taking a reference to an array and returning a pointer to
// an array of characters. Only declared and never defined; we just
// need it to determine n, the size of the array that was passed.
template <typename T, int n>
char (*ArraySizeDeducer(T (&)[n]))[n];

// Number of elements in an array. Safer than sizeof(name) / sizeof(name[0])
// because it doesn't compile when a pointer is passed.
#define ARRAY_SIZE(name) (sizeof(*ArraySizeDeducer(name)))

// decltype(T)::x cannot be used directly when T is a reference type, so
// we need to remove the reference first.
#define TYPE(T) std::remove_reference<decltype(T)>::type

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

// Generates a unique lvalue name.
#define UNIQUE(prefix) CONCAT(prefix, __LINE__)

#endif  // #ifndef HIGHWAYHASH_HIGHWAYHASH_CODE_ANNOTATION_H_
