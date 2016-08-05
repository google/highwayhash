#include "profiler.h"

#include <sys/time.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

double Now() {
  timeval tv;
  const int ret = gettimeofday(&tv, nullptr);
  if (ret != 0) {
    abort();
  }
  return tv.tv_sec + tv.tv_usec * 1E-6;
}

void Spin(const double min_time) {
  const double t0 = Now();
  int iterations = 0;
  for (;;) {
    ++iterations;
    const double elapsed = Now() - t0;
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

int main(int argc, char* argv[]) {
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
