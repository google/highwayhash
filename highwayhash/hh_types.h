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

#ifndef HIGHWAYHASH_HH_TYPES_H_
#define HIGHWAYHASH_HH_TYPES_H_

// WARNING: included from c_bindings => must be C-compatible.
// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

#include <stddef.h>  // size_t
#include <stdint.h>

#ifdef __cplusplus
namespace highwayhash {
#endif

// 256-bit secret key that should remain unknown to attackers.
// We recommend initializing it to a random value.
typedef uint64_t HHKey[4];

// How much input is hashed by one call to HHStateT::Update.
typedef char HHPacket[32];

// Hash 'return' types.
typedef uint64_t HHResult64;  // returned directly
typedef uint64_t HHResult128[2];
typedef uint64_t HHResult256[4];

// Called if a test fails, indicating which target and size.
typedef void (*HHNotify)(const char*, size_t);

#ifdef __cplusplus
}  // namespace highwayhash
#endif

#endif  // HIGHWAYHASH_HH_TYPES_H_
