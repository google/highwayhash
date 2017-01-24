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

#ifndef HIGHWAYHASH_HIGHWAYHASH_C_BINDINGS_H_
#define HIGHWAYHASH_HIGHWAYHASH_C_BINDINGS_H_

// C-callable function prototypes, documented in the other header files.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t SipHashC(const uint64_t* key, const char* bytes, const uint64_t size);
uint64_t SipHash13C(const uint64_t* key, const char* bytes,
                    const uint64_t size);

// Defined by highwayhash_target.cc, which requires a _Target* suffix.
uint64_t HighwayHash64_TargetPortable(const uint64_t* key, const char* bytes,
                                      const uint64_t size);
uint64_t HighwayHash64_TargetSSE41(const uint64_t* key, const char* bytes,
                                   const uint64_t size);
uint64_t HighwayHash64_TargetAVX2(const uint64_t* key, const char* bytes,
                                  const uint64_t size);

// Detects current CPU (once) and invokes the best _Target* variant.
// "key" points to an array of four 64-bit values.
uint64_t HighwayHash64_Dispatcher(const uint64_t* key, const char* bytes,
                                  const uint64_t size);

#ifdef __cplusplus
}
#endif

#endif  // HIGHWAYHASH_HIGHWAYHASH_C_BINDINGS_H_
