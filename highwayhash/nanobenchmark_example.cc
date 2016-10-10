#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>

#include "highwayhash/nanobenchmark.h"
#include "highwayhash/os_specific.h"

namespace nanobenchmark {
namespace {

void TestMemcpy() {
  os_specific::PinThreadToRandomCPU();

  for (const auto& size_samples : RepeatedMeasureWithArguments(
           {3, 3, 4, 4, 7, 7, 8, 8}, [](const size_t size) {
             char from[8] = {static_cast<char>(size)};
             char to[8];
             memcpy(to, from, size);
             return to[0];
           })) {
    PrintMedianAndVariability(size_samples);
  }
}

}  // namespace
}  // namespace nanobenchmark

int main(int argc, char* argv[]) {
  nanobenchmark::TestMemcpy();
  return 0;
}
