#ifndef HIGHWAYHASH_HIGHWAYHASH_OS_SPECIFIC_H_
#define HIGHWAYHASH_HIGHWAYHASH_OS_SPECIFIC_H_

#include <vector>

namespace os_specific {

// Returns current wall-clock time [seconds].
double Now();

// Sets this thread's priority to the maximum. This should not be called on
// single-core systems. Requires elevated permissions. No effect on Linux
// because it increases runtime and variability (issue #19).
void RaiseThreadPriority();

// Returns CPU numbers in [0, N), where N is the number of bits in the
// thread's initial affinity (unaffected by any SetThreadAffinity).
std::vector<int> AvailableCPUs();

// Opaque.
struct ThreadAffinity;

// Caller must free() the return value.
ThreadAffinity* GetThreadAffinity();

// Restores a previous affinity returned by GetThreadAffinity.
void SetThreadAffinity(ThreadAffinity* affinity);

// Ensures the thread is running on the specified cpu, and no others.
// Useful for reducing nanobenchmark variability (fewer context switches).
// Uses SetThreadAffinity.
void PinThreadToCPU(const int cpu);

// Random choice of CPU avoids overloading any one core.
// Uses SetThreadAffinity.
void PinThreadToRandomCPU();

}  // namespace os_specific

#endif  // HIGHWAYHASH_HIGHWAYHASH_OS_SPECIFIC_H_
