// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Benchmark and verification for hashing algorithms. Output (cycles per byte):
// Algo \          bytes: 8       31      32      63      64     1024
//      HighwayHash     14.48    3.72    3.64    1.87    1.91    0.31
// SSE41HighwayHash     13.95    3.63    3.62    1.89    1.88    0.36
//          SipHash     20.67    6.60    6.57    3.96    4.01    1.76
//        SipHash13     13.83    4.04    4.20    2.47    2.42    0.75
//      SipTreeHash     23.33    6.22    6.00    3.17    3.19    0.62
//    SipTreeHash13     19.63    5.32    4.81    2.61    2.54    0.37

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "third_party/highwayhash/highwayhash/nanobenchmark.h"
#include "third_party/highwayhash/highwayhash/os_specific.h"
#include "third_party/highwayhash/highwayhash/scalar_highway_tree_hash.h"
#include "third_party/highwayhash/highwayhash/types.h"
#include "third_party/highwayhash/highwayhash/vec2.h"

#define BENCHMARK_SIP 1
#define BENCHMARK_SIP_TREE 1 && defined(__AVX2__)
#define BENCHMARK_HIGHWAY 1 && defined(__AVX2__)
#define BENCHMARK_SSE41_HIGHWAY 1 && defined(__SSE4_1__)

#if BENCHMARK_HIGHWAY
#include "third_party/highwayhash/highwayhash/highway_tree_hash.h"
#endif
#if BENCHMARK_SIP
#include "third_party/highwayhash/highwayhash/sip_hash.h"
#endif
#if BENCHMARK_SIP_TREE
#include "third_party/highwayhash/highwayhash/scalar_sip_tree_hash.h"
#include "third_party/highwayhash/highwayhash/sip_tree_hash.h"
#endif
#if BENCHMARK_SSE41_HIGHWAY
#include "third_party/highwayhash/highwayhash/sse41_highway_tree_hash.h"
#endif

namespace highwayhash {
namespace {

template <class Function1, class Function2>
static void VerifyEqual(const char* caption, const Function1& hash_function1,
                        const Function2& hash_function2) {
  const int kMaxSize = 128;
  char in[kMaxSize] = {0};

  const uint64 key[4] = {0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
                         0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL};

  for (int size = 0; size < kMaxSize; ++size) {
    in[size] = static_cast<char>(size);
    const uint64 hash = hash_function1(key, in, size);
    const uint64 hash2 = hash_function2(key, in, size);
    if (hash != hash2) {
      printf("Failed %s at length %d %llx %llx\n", caption, size, hash, hash2);
      exit(1);
    }
  }
  printf("Verified %s.\n", caption);
}

// Stores time measurements from benchmarks, with support for printing them
// as LaTeX figures or tables.
class Measurements {
 public:
  void Add(const char* caption, const size_t bytes, const double cycles) {
    const float cpb = static_cast<float>(cycles / bytes);
    results_.emplace_back(caption, static_cast<int>(bytes), cpb);
  }

  // Prints results as a LaTeX table (only for in_sizes matching the
  // desired values).
  void PrintTable(const std::vector<size_t>& in_sizes) {
    std::vector<size_t> unique = in_sizes;
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());

    printf("\\begin{tabular}{");
    for (size_t i = 0; i < unique.size() + 1; ++i) {
      printf("%s", i == 0 ? "r" : "|r");
    }
    printf("}\n\\toprule\nAlgorithm");
    for (const size_t in_size : unique) {
      printf(" & %zu", in_size);
    }
    printf("\\\\\n\\midrule\n");

    const SpeedsForCaption cpb_for_caption = SortByCaptionFilterBySize(unique);
    for (const auto& item : cpb_for_caption) {
      printf("%17s", item.first.c_str());
      for (const float cpb : item.second) {
        printf(" & %5.2f", cpb);
      }
      printf("\\\\\n");
    }
  }

  // Prints results suitable for pgfplots.
  void PrintPlots() {
    const SpeedsForCaption cpb_for_caption = SortByCaption();
    assert(!cpb_for_caption.empty());
    const size_t num_sizes = cpb_for_caption.begin()->second.size();

    printf("Size ");
    // Flatten per-caption vectors into one iterator.
    std::vector<std::vector<float>::const_iterator> iterators;
    for (const auto& item : cpb_for_caption) {
      printf("%21s ", item.first.c_str());
      assert(item.second.size() == num_sizes);
      iterators.push_back(item.second.begin());
    }
    printf("\n");

    const std::vector<int>& sizes = UniqueSizes();
    assert(num_sizes == sizes.size());
    for (int i = 0; i < static_cast<int>(num_sizes); ++i) {
      printf("%d ", sizes[i]);
      for (auto& it : iterators) {
        printf("%5.2f ", 1.0f / *it);  // bytes per cycle
        ++it;
      }
      printf("\n");
    }
  }

 private:
  struct Result {
    Result(const char* caption, const int in_size, const float cpb)
        : caption(caption), in_size(in_size), cpb(cpb) {}

    // Algorithm name (string literal).
    const char* caption;
    // Size of the input data [bytes].
    int in_size;
    // Measured throughput [cycles per byte].
    float cpb;
  };

  // Returns set of all input sizes for the first column of a size/speed plot.
  std::vector<int> UniqueSizes() {
    std::vector<int> sizes;
    sizes.reserve(results_.size());
    for (const Result& result : results_) {
      sizes.push_back(result.in_size);
    }
    std::sort(sizes.begin(), sizes.end());
    sizes.erase(std::unique(sizes.begin(), sizes.end()), sizes.end());
    return sizes;
  }

  using SpeedsForCaption = std::map<std::string, std::vector<float>>;

  SpeedsForCaption SortByCaption() const {
    SpeedsForCaption cpb_for_caption;
    for (const Result& result : results_) {
      cpb_for_caption[result.caption].push_back(result.cpb);
    }
    return cpb_for_caption;
  }

  // Only includes measurement results matching one of the given sizes.
  SpeedsForCaption SortByCaptionFilterBySize(
      const std::vector<size_t>& in_sizes) const {
    SpeedsForCaption cpb_for_caption;
    for (const Result& result : results_) {
      for (const size_t in_size : in_sizes) {
        if (result.in_size == static_cast<int>(in_size)) {
          cpb_for_caption[result.caption].push_back(result.cpb);
        }
      }
    }
    return cpb_for_caption;
  }

  std::vector<Result> results_;
};

template <class Func>
void AddMeasurements(const std::vector<size_t>& in_sizes, const char* caption,
                     Measurements* measurements, const Func& func) {
  for (auto& size_samples :
       nanobenchmark::RepeatedMeasureWithArguments(in_sizes, func, 40)) {
    const size_t size = size_samples.first;
    auto& samples = size_samples.second;
    const float median = nanobenchmark::Median(&samples);
    const float mad = nanobenchmark::MedianAbsoluteDeviation(samples, median);
    printf("%s %4zu: median=%6.1f cycles; median L1 norm =%4.1f cycles\n",
           caption, size, median, mad);
    measurements->Add(caption, size, median);
  }
}

#define PRINT_PLOT 0

void AddMeasurementsSip(const std::vector<size_t>& in_sizes,
                        Measurements* measurements) {
#if BENCHMARK_SIP
  uint64 key2[2] = {0, 1};
  AddMeasurements(in_sizes, "SipHash", measurements,
                  [&key2](const size_t size) {
                    char in[1024] = {static_cast<char>(size)};
                    return SipHash(key2, in, size);
                  });

  AddMeasurements(in_sizes, "SipHash13", measurements,
                  [&key2](const size_t size) {
                    char in[1024] = {static_cast<char>(size)};
                    return SipHash13(key2, in, size);
                  });
#endif
}

void AddMeasurementsSipTree(const std::vector<size_t>& in_sizes,
                            Measurements* measurements) {
#if BENCHMARK_SIP_TREE
  uint64 key4[4] = {0, 1, 2, 3};
  AddMeasurements(in_sizes, "SipTreeHash", measurements,
                  [&key4](const size_t size) {
                    char in[1024] = {static_cast<char>(size)};
                    return SipTreeHash(key4, in, size);
                  });

  AddMeasurements(in_sizes, "SipTreeHash13", measurements,
                  [&key4](const size_t size) {
                    char in[1024] = {static_cast<char>(size)};
                    return SipTreeHash13(key4, in, size);
                  });
#endif
}

void AddMeasurementsHighway(const std::vector<size_t>& in_sizes,
                            Measurements* measurements) {
#if BENCHMARK_HIGHWAY
  alignas(32) uint64 key4[4] = {0, 1, 2, 3};
  AddMeasurements(in_sizes, "HighwayHash", measurements,
                  [&key4](const size_t size) {
                    char in[1024] = {static_cast<char>(size)};
                    return HighwayTreeHash(key4, in, size);
                  });
#endif
}

void AddMeasurementsSSE41Highway(const std::vector<size_t>& in_sizes,
                                 Measurements* measurements) {
#if BENCHMARK_SSE41_HIGHWAY
  alignas(32) uint64 key4[4] = {0, 1, 2, 3};
  AddMeasurements(in_sizes, "SSE41HighwayHash", measurements,
                  [&key4](const size_t size) {
                    char in[1024] = {static_cast<char>(size)};
                    return SSE41HighwayTreeHash(key4, in, size);
                  });
#endif
}

void RunTests(int argc, char* argv[]) {
#if BENCHMARK_SSE41_HIGHWAY
  VerifyEqual("SSE41HighwayTreeHash", ScalarHighwayTreeHash,
              SSE41HighwayTreeHash);
#endif
#if BENCHMARK_SIP_TREE
  VerifyEqual("SipTreeHash", ScalarSipTreeHash, SipTreeHash);
  VerifyEqual("SipTreeHash13", ScalarSipTreeHash13, SipTreeHash13);
#endif
#if BENCHMARK_HIGHWAY
  VerifyEqual("HighwayTreeHash", ScalarHighwayTreeHash, HighwayTreeHash);
#endif

#if PRINT_PLOT
  std::vector<size_t> in_sizes;
  for (int i = 0; i < 256; i += 4) {
    in_sizes.push_back(i);
  }
#else
  const std::vector<size_t> in_sizes = {8, 31, 32, 63, 64, 1024};
#endif

  Measurements measurements;
  os_specific::PinThreadToRandomCPU();

  AddMeasurementsSip(in_sizes, &measurements);
  AddMeasurementsSipTree(in_sizes, &measurements);
  AddMeasurementsHighway(in_sizes, &measurements);
  AddMeasurementsSSE41Highway(in_sizes, &measurements);

#if PRINT_PLOT
  measurements.PrintPlots();
#else
  measurements.PrintTable(in_sizes);
#endif
}

}  // namespace
}  // namespace highwayhash

int main(int argc, char* argv[]) {
  highwayhash::RunTests(argc, argv);
  return 0;
}
