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

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

#include <stdint.h>
#include "highwayhash/instruction_sets.h"

namespace highwayhash {

// 256-bit secret key that should remain unknown to attackers.
// We recommend initializing it to a random value.
using HHKey = uint64_t[4];

// Hash 'return' types
using HHResult64 = uint64_t;  // returned directly
using HHResult128 = uint64_t[2];
using HHResult256 = uint64_t[4];

// Primary template, specialized for TargetAVX2 etc.
template <class Target>
class HHState {};

}  // namespace highwayhash

#endif  // HIGHWAYHASH_HH_TYPES_H_
