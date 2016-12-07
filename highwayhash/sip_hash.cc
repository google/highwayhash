// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "third_party/highwayhash/highwayhash/sip_hash.h"

namespace highwayhash {
uint64 SipHash(const SipHashState::Key& key, const char* bytes,
                                const uint64 size) {
  return ComputeHash<SipHashStateImp<2,4>>(key, bytes, size);
}
uint64 SipHash13(const SipHash13State::Key& key, const char* bytes,
                                const uint64 size) {
  return ComputeHash<SipHashStateImp<1,3>>(key, bytes, size);
}
}
using highwayhash::uint64;
using highwayhash::SipHash;
using highwayhash::SipHash13;
using Key = highwayhash::SipHashState::Key;
using Key13 = highwayhash::SipHash13State::Key;

extern "C" {

uint64 SipHashC(const uint64* key, const char* bytes, const uint64 size) {
  return SipHash(*reinterpret_cast<const Key*>(key), bytes, size);
}

uint64 SipHash13C(const uint64* key, const char* bytes, const uint64 size) {
  return SipHash13(*reinterpret_cast<const Key13*>(key), bytes, size);
}

}  // extern "C"
