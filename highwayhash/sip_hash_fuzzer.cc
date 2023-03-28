#include "highwayhash/sip_hash.h"

using highwayhash::HH_U64;
using highwayhash::SipHash;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < sizeof(HH_U64) * 2) {
    return 0;
  }

  // Generate the key.
  const HH_U64 *hhU64s = reinterpret_cast<const HH_U64*>(data);
  HH_ALIGNAS(16) const HH_U64 key[2] = {hhU64s[0], hhU64s[1]};
  data += sizeof(HH_U64) * 2;
  size -= sizeof(HH_U64) * 2;

  // Compute the hash.
  SipHash(key, reinterpret_cast<const char*>(data), size);
  return 0;
}
