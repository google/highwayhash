#ifndef HIGHWAYHASH_HIGHWAYHASH_TYPES_H_
#define HIGHWAYHASH_HIGHWAYHASH_TYPES_H_

#ifdef __cplusplus
namespace highwayhash {
#endif

// cstdint's uint64_t is unsigned long on Linux; we need 'unsigned long long'
// for interoperability with other software.
typedef unsigned long long uint64;  // NOLINT

typedef unsigned int uint32;

#ifdef __cplusplus
}  // namespace highwayhash
#endif

#endif  // HIGHWAYHASH_HIGHWAYHASH_TYPES_H_
