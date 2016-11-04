#ifndef HIGHWAYHASH_HIGHWAYHASH_TYPES_H_
#define HIGHWAYHASH_HIGHWAYHASH_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
namespace highwayhash {
#endif

// uint64_t is unsigned long on Linux; we need 'unsigned long long'
// for interoperability with TensorFlow.
typedef unsigned long long uint64;  // NOLINT

typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

#ifdef __cplusplus
}  // namespace highwayhash
#endif

#endif  // HIGHWAYHASH_HIGHWAYHASH_TYPES_H_
