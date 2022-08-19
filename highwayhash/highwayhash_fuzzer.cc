#include "highwayhash/highwayhash_target.h"
#include "highwayhash/instruction_sets.h"

using highwayhash::HHKey;
using highwayhash::HHResult64;
using highwayhash::HighwayHash;
using highwayhash::InstructionSets;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < sizeof(uint64_t) * 4) {
    return 0;
  }

  // Generate the key.
  const uint64_t *u64s = reinterpret_cast<const uint64_t*>(data);
  HH_ALIGNAS(32) const HHKey key = {u64s[0], u64s[1], u64s[2], u64s[3]};
  data += sizeof(uint64_t) * 4;
  size -= sizeof(uint64_t) * 4;

  // Compute the hash.
  HHResult64 result;
  InstructionSets::Run<HighwayHash>(key, reinterpret_cast<const char *>(data),
                                    size, &result);
  return 0;
}
