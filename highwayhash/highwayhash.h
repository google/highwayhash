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
// template<class Target> CodeUsingHash() { HighwayHashT<Target>(...); }, or if
// Target matches the minimum CPU requirement (specified via compiler flag).
#include <stddef.h>

#include "highwayhash/arch_specific.h"  // HH_ENABLE_*
#include "highwayhash/compiler_specific.h"
#include "highwayhash/hh_types.h"
#include "highwayhash/iaca.h"

// HH_TARGET_PREFERRED enables us to provide new specializations without
// having to update each call site. Example usage:
//
// HHState<HH_TARGET_PREFERRED> state(key);
// HighwayHashT(&state, in, size, &result);
//
// This is useful for binaries that target a lowest-common denominator CPU
// without conditionally using newer instructions if available. Such binaries
// are compiled with the same flags for each translation unit, which avoids
// difficulties with inline functions mentioned in instruction_sets.h.
//
// If you want to select the best available specialization at runtime,
// use InstructionSets<HighwayHash>() instead.

#include "highwayhash/hh_portable.h"
#define HH_TARGET_PREFERRED TargetPortable

#if HH_ENABLE_SSE41
#include "highwayhash/hh_sse41.h"
#undef HH_TARGET_PREFERRED
#define HH_TARGET_PREFERRED TargetSSE41
#endif

#if HH_ENABLE_AVX2
#include "highwayhash/hh_avx2.h"
#undef HH_TARGET_PREFERRED
#define HH_TARGET_PREFERRED TargetAVX2
#endif

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
  const size_t remainder = size & (sizeof(HHPacket) - 1);
  const size_t truncated = size & ~(sizeof(HHPacket) - 1);
  for (size_t offset = 0; offset < truncated; offset += sizeof(HHPacket)) {
    state->Update(*reinterpret_cast<const HHPacket*>(bytes + offset));
  }

  if (remainder != 0) {
    state->UpdateRemainder(bytes + truncated, remainder);
  }

  state->Finalize(hash);
  EndIACA();
}

// Wrapper class for incrementally hashing a series of data ranges. The final
// result is the same as HighwayHashT of the concatenation of all the ranges.
// This is useful for computing the hash of cords, iovecs, and similar
// data structures.

template <class Target>
class HighwayHashCatT {
 public:
  HighwayHashCatT(const HHKey& key) : state_(key) {}

  // Adds "bytes" to the internal buffer, feeding it to HHState::Update as
  // required. Call this as often as desired. There are no alignment
  // requirements. No effect if "num_bytes" == 0.
  void Append(const char* HH_RESTRICT bytes, size_t num_bytes) {
    char* buffer_bytes = reinterpret_cast<char*>(buffer_);
    // Have prior bytes to flush.
    if (buffer_usage_ != 0) {
      const size_t capacity = sizeof(HHPacket) - buffer_usage_;
      if (num_bytes < capacity) {
        // New bytes fit within buffer, but still not enough to Update.
        memcpy(buffer_bytes + buffer_usage_, bytes, num_bytes);
        buffer_usage_ += num_bytes;
        return;
      }
      memcpy(buffer_bytes + buffer_usage_, bytes, capacity);
      state_.Update(*reinterpret_cast<const HHPacket*>(buffer_));
      buffer_usage_ = 0;
      bytes += capacity;
      num_bytes -= capacity;
    }

    // Buffer currently empty => Update directly from the source.
    while (num_bytes >= sizeof(HHPacket)) {
      state_.Update(*reinterpret_cast<const HHPacket*>(bytes));
      bytes += sizeof(HHPacket);
      num_bytes -= sizeof(HHPacket);
    }

    // Store any remainders in buffer, no-op if multiple of a packet.
    memcpy(buffer_bytes, bytes, num_bytes);
    buffer_usage_ = num_bytes;
  }

  // Stores the resulting 64, 128 or 256-bit hash of all data passed to Append.
  // Must be called exactly once.
  template <typename Result>  // HHResult*
  void Finalize(Result* HH_RESTRICT hash) {
    if (buffer_usage_ != 0) {
      const char* buffer_bytes = reinterpret_cast<const char*>(buffer_);
      state_.UpdateRemainder(buffer_bytes, buffer_usage_);
    }
    state_.Finalize(hash);
  }

 private:
  HHPacket buffer_ HH_ALIGNAS(64);
  HHState<Target> state_;
  // How many bytes in buffer_ (starting with offset 0) are valid.
  size_t buffer_usage_ = 0;
};

}  // namespace highwayhash

#endif  // HIGHWAYHASH_HIGHWAYHASH_H_
