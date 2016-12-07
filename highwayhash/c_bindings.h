#ifndef HIGHWAYHASH_HIGHWAYHASH_C_BINDINGS_H_
#define HIGHWAYHASH_HIGHWAYHASH_C_BINDINGS_H_

// C-callable function prototypes, documented in the other header files.

#include "third_party/highwayhash/highwayhash/types.h"

uint64 SipHashC(const uint64* key, const char* bytes, const uint64 size);
uint64 SipHash13C(const uint64* key, const char* bytes, const uint64 size);

uint64 ScalarSipTreeHashC(const uint64* key, const char* bytes,
                          const uint64 size);
uint64 ScalarSipTreeHash13C(const uint64* key, const char* bytes,
                          const uint64 size);

uint64 SipTreeHashC(const uint64* key, const char* bytes, const uint64 size);
uint64 SipTreeHash13C(const uint64* key, const char* bytes, const uint64 size);

uint64 ScalarHighwayTreeHashC(const uint64* key, const char* bytes,
                              const uint64 size);

uint64 SSE41HighwayTreeHashC(const uint64* key, const char* bytes,
                             const uint64 size);

uint64 HighwayTreeHashC(const uint64* key, const char* bytes,
                        const uint64 size);

#endif  // HIGHWAYHASH_HIGHWAYHASH_C_BINDINGS_H_
