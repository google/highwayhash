#ifndef HIGHWAYHASH_HIGHWAYHASH_ARCH_SPECIFIC_H_
#define HIGHWAYHASH_HIGHWAYHASH_ARCH_SPECIFIC_H_

#include <cstdlib>  // _byteswap_*

#if defined(__x86_64__) || defined(_M_X64)
#define ARCH_X64 1
#else
#define ARCH_X64 0
#endif

#ifdef _MSC_VER
#define HIGHWAYHASH_BSWAP32(x) _byteswap_ulong(x)
#define HIGHWAYHASH_BSWAP64(x) _byteswap_uint64(x)
#else
#define HIGHWAYHASH_BSWAP32(x) __builtin_bswap32(x)
#define HIGHWAYHASH_BSWAP64(x) __builtin_bswap64(x)
#endif

#endif  // HIGHWAYHASH_HIGHWAYHASH_ARCH_SPECIFIC_H_
