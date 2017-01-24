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

#ifndef HIGHWAYHASH_HIGHWAYHASH_H_
#define HIGHWAYHASH_HIGHWAYHASH_H_

// WARNING: compiled with different flags => must not define/instantiate any
// inline functions, nor include any headers that do - see instruction_sets.h.

// Function template for direct invocation via CPU-specific templates (e.g.
// template<class Target> CodeUsingHash() { HighwayHashT<Target>(...); }, or
// when the entire binary is built with the required compiler flags. If run on
// a CPU that might lack AVX2 or even SSE41, you should instead call
// InstructionSets<HighwayHash>() - see highwayhash_target.h.

#include <stddef.h>

#include "highwayhash/compiler_specific.h"
#include "highwayhash/iaca.h"
// These headers are empty if the requisite __AVX2__ etc. are undefined.
#include "highwayhash/hh_avx2.h"
#include "highwayhash/hh_portable.h"
#include "highwayhash/hh_sse41.h"
#include "highwayhash/hh_types.h"

namespace highwayhash {

// Computes HighwayHash of "bytes" using the implementation for "Target" CPU.
//
// "state" is a HHState<> initialized with a key.
// "bytes" is the data to hash (possibly unaligned).
// "size" is the number of bytes to hash; we do not read any additional bytes.
// "hash" is a HHResult* (either 64, 128 or 256 bits).
//
// HighwayHash is a strong pseudorandom function with security claims
// [https://arxiv.org/abs/1612.06257]. It is intended as a safer general-purpose
// hash, about 4x faster than SipHash and 10x faster than BLAKE2.
//
// This template allows callers (e.g. tests) to invoke a specific
// implementation. It must be compiled with the flags required by the desired
// implementation. If the entire program cannot be built with these flags, use
// the wrapper in highwayhash_target.h instead.
//
// Callers wanting to hash multiple pieces of data should duplicate this
// function, calling HHState::Update for each input and only Finalizing once.
template <class Target, typename Result>
HH_INLINE void HighwayHashT(HHState<Target>* HH_RESTRICT state,
                            const char* HH_RESTRICT bytes, const size_t size,
                            Result* HH_RESTRICT hash) {
  BeginIACA();
  const int kPacketSize = 32;
  const size_t remainder = size & (kPacketSize - 1);
  const size_t truncated = size & ~(kPacketSize - 1);
  for (size_t i = 0; i < truncated; i += kPacketSize) {
    state->Update(bytes + i);
  }

  if (remainder != 0) {
    state->UpdateRemainder(bytes + truncated, remainder);
  }

  state->Finalize(hash);
  EndIACA();
}

}  // namespace highwayhash

#endif  // HIGHWAYHASH_HIGHWAYHASH_H_
