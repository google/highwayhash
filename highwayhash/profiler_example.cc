#include <cassert>
#include <cmath>
#include <cstdlib>

#include "highwayhash/os_specific.h"
#include "highwayhash/profiler.h"

namespace {

void Spin(const double min_time) {
  const double t0 = os_specific::Now();
  int iterations = 0;
  for (;;) {
    ++iterations;
    const double elapsed = os_specific::Now() - t0;
    if (elapsed > min_time) {
      assert(iterations > 2);
      break;
    }
  }
}

void Spin10() {
  PROFILER_FUNC;
  Spin(10E-6);
}

void Spin20() {
  PROFILER_FUNC;
  Spin(20E-6);
}

void Spin3060() {
  {
    PROFILER_ZONE("spin30");
    Spin(30E-6);
  }
  {
    PROFILER_ZONE("spin60");
    Spin(60E-6);
  }
}

void Level3() {
  PROFILER_FUNC;
  for (int rep = 0; rep < 10; ++rep) {
    double total = 0.0;
    for (int i = 0; i < 100 - rep; ++i) {
      total += pow(0.9, i);
    }
    if (std::abs(total - 9.999) > 1E-2) {
      abort();
    }
  }
}

void Level2() {
  PROFILER_FUNC;
  Level3();
}

void Level1() {
  PROFILER_FUNC;
  Level2();
}

}  // namespace

int main(int argc, char* argv[]) {
  os_specific::PinThreadToRandomCPU();
  {
    PROFILER_FUNC;
    Spin10();
    Spin20();
    Spin3060();
    Level1();
  }
  PROFILER_PRINT_RESULTS();
  return 0;
}
