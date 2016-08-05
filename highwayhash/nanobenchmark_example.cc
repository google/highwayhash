#include "highwayhash/nanobenchmark.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

namespace nanobenchmark {
namespace {

void TestMemcpy() {
  RaiseThreadPriority();
  PinThreadToCPU();

  for (auto& size_samples : RepeatedMeasureWithArguments(
           {3, 3, 4, 4, 7, 7, 8, 8}, [](const size_t size) {
             char from[8] = {static_cast<char>(size)};
             char to[8];
             memcpy(to, from, size);
             return to[0];
           })) {
    // Print duration and variability for this size.
    const size_t size = size_samples.first;
    auto& samples = size_samples.second;
    const float median = Median(&samples);
    const float variability = MedianAbsoluteDeviation(samples, median);
    printf("%2lu: median=%5.1f cycles; median abs. deviation=%4.1f cycles\n",
           size, median, variability);
  }
}

}  // namespace
}  // namespace nanobenchmark

int main(int argc, char* argv[]) {
  nanobenchmark::TestMemcpy();
  return 0;
}
