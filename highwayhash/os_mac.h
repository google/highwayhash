//
// Created by Alexander Gryanko on 16/09/2017.
//

#ifndef HIGHWAYHASH_OS_MAC_H_
#define HIGHWAYHASH_OS_MAC_H_

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/sysctl.h>

#include <mach/mach_types.h>
#include <mach/thread_act.h>
#include <pthread.h>


typedef unsigned long int __cpu_mask;

#define __SYSCTL_CORE_COUNT "machdep.cpu.thread_count"
#define __NR_CPUS 512 // from the linux kernel limit
#define __NR_CPUBITS (8 * sizeof(__cpu_mask))

typedef struct
{
  __cpu_mask __bits[__NR_CPUS / __NR_CPUBITS];
} cpu_set_t;

static inline void __CPU_ZERO(size_t setsize, cpu_set_t* set) {
  memset(set, 0, setsize);
}
#define CPU_ZERO(cpusetp) __CPU_ZERO(sizeof(cpu_set_t), cpusetp)

static inline int CPU_ISSET(int cpu, const cpu_set_t* set) {
  if (cpu < __NR_CPUS) {
    return (set->__bits[cpu / __NR_CPUBITS] & 1L << (cpu % __NR_CPUBITS)) != 0;
  }
  return 0;
}

static inline void CPU_SET(int cpu, cpu_set_t* set) {
  if (cpu < __NR_CPUS) {
    set->__bits[cpu / __NR_CPUBITS] |= 1L << (cpu % __NR_CPUBITS);
  }
}

int mac_getaffinity(cpu_set_t* set);
int mac_setaffinity(cpu_set_t* set);

#endif // HIGHWAYHASH_OS_MAC_H_


