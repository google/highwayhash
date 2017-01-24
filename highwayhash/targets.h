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

#ifndef HIGHWAYHASH_TARGETS_H_
#define HIGHWAYHASH_TARGETS_H_

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

// Defines 'traits' for the current target (mainly its vector type). The
// HH_TARGET BUILD macro expands to one of the Target* structs defined here.
//
// This header must only be included from target-specific source files. Example:
// #include "highwayhash/targets.h"
// template <class Target> void Functor<Target>::operator()(/*args*/) const {}
// template class Functor<HH_TARGET>;  // instantiate
//
// The corresponding header need only contain:
// template <class Target> struct Functor { void operator()(/*args*/) const; };

// To avoid inadvertent usage of other targets' functionality, we only provide
// Target* if the corresponding HH_TARGET_* macro is defined. This is necessary
// because the preprocessor can't test whether #HH_TARGET == "TargetAVX2".
#ifdef HH_TARGET_AVX2
#include "highwayhash/vector256.h"
#endif
#ifdef HH_TARGET_SSE41
#include "highwayhash/vector128.h"
#endif
#ifdef HH_TARGET_PORTABLE
#include "highwayhash/scalar.h"
#endif

namespace highwayhash {

// Target traits: each defines a "vector" type plus some static member
// functions not provided by the vector class. The Target* type can also be
// used as a tag for function overloading or template specialization.

#ifdef HH_TARGET_AVX2
// Intel Haswell and AMD Zen CPUs also support FMA/BMI2/AVX2
// [https://sourceware.org/ml/binutils/2015-03/msg00078.html]
struct TargetAVX2 {
  template <typename T>
  using V = V256<T>;

  static const char* Name() { return "AVX2"; }
};
#endif

#ifdef HH_TARGET_SSE41
// SSE4.1 is available since 2008 (Intel) and 2011 (AMD).
struct TargetSSE41 {
  template <typename T>
  using V = V128<T>;

  static const char* Name() { return "SSE41"; }
};
#endif

#ifdef HH_TARGET_PORTABLE
struct TargetPortable {
  template <typename T>
  using V = Scalar<T>;

  static const char* Name() { return "Portable"; }
};
#endif

}  // namespace highwayhash

#endif  // HIGHWAYHASH_TARGETS_H_
