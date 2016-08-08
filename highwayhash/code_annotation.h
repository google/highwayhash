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

// #if is shorter and safer than #ifdef. COMPILER_* is zero if not detected,
// otherwise 100 * major + minor version.

#ifdef _MSC_VER
#define COMPILER_MSVC _MSC_VER
#else
#define COMPILER_MSVC 0
#endif

#ifdef __GNUC__
#define COMPILER_GCC (__GNUC__ * 100 + __GNUC_MINOR__)
#else
#define COMPILER_GCC 0
#endif

#ifdef __clang__
#define COMPILER_CLANG (__clang_major__ * 100 + __clang_minor__)
#else
#define COMPILER_CLANG 0
#endif

// Operating system

#if defined(_WIN32) || defined(_WIN64)
#define OS_WIN 1
#else
#define OS_WIN 0
#endif

#ifdef __linux__
#define OS_LINUX 1
#else
#define OS_LINUX 0
#endif

#ifdef __MACH__
#define OS_MAC 1
#else
#define OS_MAC 0
#endif

// Architecture

#if defined(__x86_64__) || defined(_M_X64)
#define ARCH_X64 1
#else
#define ARCH_X64 0
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

#if COMPILER_MSVC
#define RESTRICT __restrict
#elif COMPILER_GCC
#define RESTRICT __restrict__
#else
#define RESTRICT
#endif

#if COMPILER_MSVC
#define INLINE __forceinline
#else
#define INLINE inline
#endif

#if COMPILER_MSVC
#define NORETURN __declspec(noreturn)
#elif COMPILER_GCC
#define NORETURN __attribute__((noreturn))
#endif

#if COMPILER_MSVC
#include <intrin.h>
#pragma intrinsic(_ReadWriteBarrier)
#define COMPILER_FENCE _ReadWriteBarrier()
#elif COMPILER_GCC
#define COMPILER_FENCE asm volatile("" : : : "memory")
#else
#define COMPILER_FENCE
#endif

// Informs the compiler that the preceding function returns a pointer with the
// specified alignment. This may improve code generation.
#if COMPILER_MSVC
#define CACHE_ALIGNED_RETURN /* not supported */
#else
#define CACHE_ALIGNED_RETURN __attribute__((assume_aligned(64)))
#endif

#if COMPILER_MSVC
#define DEBUG_BREAK __debugbreak()
#elif COMPILER_CLANG
#define DEBUG_BREAK __builtin_debugger()
#elif COMPILER_GCC
#define DEBUG_BREAK __builtin_trap()
#endif

#if COMPILER_MSVC
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
