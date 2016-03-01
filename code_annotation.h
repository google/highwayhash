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

#ifndef HIGHWAYHASH_CODE_ANNOTATION_H_
#define HIGHWAYHASH_CODE_ANNOTATION_H_

// Compiler/OS detection

// #if is easier and safer than #ifdef. compiler version is zero if
// not detected, or 100 * major + minor.

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

//-----------------------------------------------------------------------------

// Marks a function parameter as unused and avoids
// the corresponding compiler warning.
// Wrap around the parameter name, e.g. void f(int UNUSED(x))
#define UNUSED(param)

// Marks a function local variable or parameter as unused and avoids
// the corresponding compiler warning.
// Use instead of UNUSED when the parameter is conditionally unused.
#define UNUSED2(param) ((void)(param))

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

// Defines a variable such that its address is a multiple of "multiple".
// Example: ALIGNED(int, 8) aligned = 0;
#if MSC_VERSION
#define ALIGNED(type, multiple) __declspec(align(multiple)) type
#elif GCC_VERSION
#define ALIGNED(type, multiple) type __attribute__((aligned(multiple)))
#else
#define ALIGNED(type, multiple) type
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
#if MSC_VERSION
#define UNIQUE(prefix) CONCAT(prefix, __COUNTER__)
#else
#define UNIQUE(prefix) CONCAT(prefix, __LINE__)
#endif

#endif  // #ifndef HIGHWAYHASH_CODE_ANNOTATION_H_
