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

// Measures hash function throughput for various input sizes.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "highwayhash/compiler_specific.h"
#include "highwayhash/nanobenchmark.h"
#include "highwayhash/os_specific.h"

// Which functions to enable (includes check for compiler support)
#define BENCHMARK_SIP 1
#define BENCHMARK_SIP_TREE 1 && defined(__AVX2__)
#define BENCHMARK_HIGHWAY_AVX2 1 && defined(__AVX2__)
#define BENCHMARK_HIGHWAY_SSE41 1 && defined(__SSE4_1__)
#define BENCHMARK_HIGHWAY_PORTABLE 1
#define BENCHMARK_FARM 0

#if BENCHMARK_HIGHWAY_AVX2 || BENCHMARK_HIGHWAY_SSE41 || \
    BENCHMARK_HIGHWAY_PORTABLE
#include "highwayhash/highwayhash.h"
#endif
#if BENCHMARK_SIP
#include "highwayhash/sip_hash.h"
#endif
#if BENCHMARK_SIP_TREE
#include "highwayhash/scalar_sip_tree_hash.h"
#include "highwayhash/sip_tree_hash.h"
#endif
#if BENCHMARK_FARM
#include "third_party/farmhash/src/farmhash.h"
#endif

namespace highwayhash {
namespace {

constexpr size_t kMaxInputSize = 1024;
static_assert(kMaxInputSize >= sizeof(size_t), "Too small");

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

void AddMeasurementsSip(const std::vector<size_t>& in_sizes,
                        Measurements* measurements) {
#if BENCHMARK_SIP
  const HH_U64 key2[2] HH_ALIGNAS(16) = {0, 1};
  AddMeasurements(in_sizes, "SipHash", measurements,
                  [&key2](const size_t size) {
                    char in[kMaxInputSize];
                    memcpy(in, &size, sizeof(size));
                    return SipHash(key2, in, size);
                  });

  AddMeasurements(in_sizes, "SipHash13", measurements,
                  [&key2](const size_t size) {
                    char in[kMaxInputSize];
                    memcpy(in, &size, sizeof(size));
                    return SipHash13(key2, in, size);
                  });
#endif
}

void AddMeasurementsSipTree(const std::vector<size_t>& in_sizes,
                            Measurements* measurements) {
#if BENCHMARK_SIP_TREE
  const HH_U64 key4[4] HH_ALIGNAS(32) = {0, 1, 2, 3};
  AddMeasurements(in_sizes, "SipTreeHash", measurements,
                  [&key4](const size_t size) {
                    char in[kMaxInputSize];
                    memcpy(in, &size, sizeof(size));
                    return SipTreeHash(key4, in, size);
                  });

  AddMeasurements(in_sizes, "SipTreeHash13", measurements,
                  [&key4](const size_t size) {
                    char in[kMaxInputSize];
                    memcpy(in, &size, sizeof(size));
                    return SipTreeHash13(key4, in, size);
                  });
#endif
}

void AddMeasurementsHighway(const std::vector<size_t>& in_sizes,
                            Measurements* measurements) {
#if BENCHMARK_HIGHWAY_AVX2 || BENCHMARK_HIGHWAY_SSE41 || \
    BENCHMARK_HIGHWAY_PORTABLE
  const HHKey key HH_ALIGNAS(32) = {0, 1, 2, 3};
#endif
#if BENCHMARK_HIGHWAY_AVX2
  AddMeasurements(in_sizes, "HighwayHashAVX2", measurements,
                  [&key](const size_t size) {
                    char in[kMaxInputSize];
                    memcpy(in, &size, sizeof(size));
                    HHResult64 result;
                    HHState<TargetAVX2> state(key);
                    HighwayHashT(&state, in, size, &result);
                    return result;
                  });
#endif
#if BENCHMARK_HIGHWAY_SSE41
  AddMeasurements(in_sizes, "HighwayHashSSE41", measurements,
                  [&key](const size_t size) {
                    char in[kMaxInputSize];
                    memcpy(in, &size, sizeof(size));
                    HHResult64 result;
                    HHState<TargetSSE41> state(key);
                    HighwayHashT(&state, in, size, &result);
                    return result;
                  });
#endif
#if BENCHMARK_HIGHWAY_PORTABLE
  AddMeasurements(in_sizes, "HighwayHashPortable", measurements,
                  [&key](const size_t size) {
                    char in[kMaxInputSize];
                    memcpy(in, &size, sizeof(size));
                    HHResult64 result;
                    HHState<TargetPortable> state(key);
                    HighwayHashT(&state, in, size, &result);
                    return result;
                  });
#endif
}

void AddMeasurementsFarm(const std::vector<size_t>& in_sizes,
                         Measurements* measurements) {
#if BENCHMARK_FARM
  AddMeasurements(in_sizes, "Farm", measurements, [](const size_t size) {
    char in[kMaxInputSize];
    memcpy(in, &size, sizeof(size));
    return farmhash::Fingerprint64(reinterpret_cast<const char*>(in), size);
  });
#endif
}

void AddMeasurements(const std::vector<size_t>& in_sizes,
                     Measurements* measurements) {
  AddMeasurementsSip(in_sizes, measurements);
  AddMeasurementsSipTree(in_sizes, measurements);
  AddMeasurementsHighway(in_sizes, measurements);
  AddMeasurementsFarm(in_sizes, measurements);
}

void PrintTable() {
  const std::vector<size_t> in_sizes = {8, 31, 32, 63, 64, kMaxInputSize};
  Measurements measurements;
  AddMeasurements(in_sizes, &measurements);
  measurements.PrintTable(in_sizes);
}

void PrintPlots() {
  std::vector<size_t> in_sizes;
  for (int num_vectors = 0; num_vectors < 12; ++num_vectors) {
    for (int remainder : {0, 9, 18, 27}) {
      in_sizes.push_back(num_vectors * 32 + remainder);
      assert(in_sizes.back() <= kMaxInputSize);
    }
  }

  Measurements measurements;
  AddMeasurements(in_sizes, &measurements);
  measurements.PrintPlots();
}

}  // namespace
}  // namespace highwayhash

int main(int argc, char* argv[]) {
  os_specific::PinThreadToRandomCPU();
  // No argument or t => table
  if (argc < 2 || argv[1][0] == 't') {
    highwayhash::PrintTable();
  } else if (argv[1][0] == 'p') {
    highwayhash::PrintPlots();
  }
  return 0;
}
