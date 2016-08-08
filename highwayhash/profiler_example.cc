#include "highwayhash/profiler.h"

#if OS_WIN
#define NOMINMAX
#include <windows.h>
#elif OS_MAC
#include <mach/mach.h>
#include <mach/mach_time.h>
#else
#include <time.h>
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

double Now() {
#if OS_WIN
  LARGE_INTEGER counter;
  (void)QueryPerformanceCounter(&counter);
  LARGE_INTEGER freq;
  (void)QueryPerformanceFrequency(&freq);
  return double(counter.QuadPart) / freq.QuadPart;
#elif OS_MAC
  const auto t = mach_absolute_time();
  // On OSX/iOS platform the elapsed time is cpu time unit
  // We have to query the time base information to convert it back
  // See https://developer.apple.com/library/mac/qa/qa1398/_index.html
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    (void)mach_timebase_info(&timebase);
  }
  return double(t) * timebase.numer / timebase.denom * 1E-9;
#else
  timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return t.tv_sec + t.tv_nsec * 1E-9;
#endif
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
