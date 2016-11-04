#include "third_party/highwayhash/highwayhash/nanobenchmark.h"

#if HH_MSC_VERSION

#pragma optimize("", off)

namespace nanobenchmark {

// Self-assignment with #pragma optimize("off") might be expected to prevent
// elision, but it does not with MSVC 2015.
void UseCharPointer(volatile const char*) {}
}

#pragma optimize("", on)

#endif
