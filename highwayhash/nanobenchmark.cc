#include "highwayhash/nanobenchmark.h"

#if COMPILER_MSVC

#pragma optimize("", off)

namespace nanobenchmark {

// Self-assignment with #pragma optimize("off") might be expected to prevent
// elision, but it does not with MSVC 2015.
void UseCharPointer(volatile const char*) {}
}

#pragma optimize("", on)

#endif
