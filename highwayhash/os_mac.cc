//
// Created by Alexander Gryanko on 16/09/2017.
//

#include "os_mac.h"

int mac_getaffinity(cpu_set_t* set) {
  uint16_t core_count = 0;
  size_t core_count_size = sizeof(core_count); // size is a pointer
  const int err = sysctlbyname(__SYSCTL_CORE_COUNT, &core_count,
                                 &core_count_size, 0, 0);
  if (err != 0)
    return err;

  CPU_ZERO(set);
  for (uint16_t i = 0; i < core_count; ++i) {
    CPU_SET(i, set);
  }

  return 0;
}

int mac_setaffinity(cpu_set_t* set) {
  thread_port_t thread = pthread_mach_thread_np(pthread_self());

  uint16_t current_core;
  for (current_core = 0; current_core < __NR_CPUS; ++current_core) {
    if (CPU_ISSET(current_core, set)) break;
  }
  thread_affinity_policy_data_t policy = { current_core };
  return thread_policy_set(thread, THREAD_AFFINITY_POLICY,
                                                (thread_policy_t)&policy, 1);
}