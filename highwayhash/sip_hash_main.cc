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
// Algo \              3       4       7       8       9      10  [bytes]
// SSE41HighwayTreeHash & 40.40 & 33.34 & 18.09 & 16.84 & 14.42 & 12.93 &  0.39
//      HighwayTreeHash & 40.75 & 31.59 & 17.48 & 15.80 & 13.76 & 12.41 &  0.38
//          SipTreeHash & 55.43 & 42.83 & 23.99 & 21.46 & 18.82 & 16.92 &  0.61
//              SipHash & 36.57 & 25.82 & 14.69 & 15.20 & 14.49 & 13.16 &  1.32

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "highwayhash/highway_tree_hash.h"
#include "highwayhash/nanobenchmark.h"
#include "highwayhash/os_specific.h"
#include "highwayhash/scalar_highway_tree_hash.h"
#include "highwayhash/scalar_sip_tree_hash.h"
#include "highwayhash/sip_hash.h"
#include "highwayhash/sip_tree_hash.h"
#include "highwayhash/sse41_highway_tree_hash.h"
#include "highwayhash/types.h"
#include "highwayhash/vec2.h"

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
      printf("Failed for length %d %llx %llx\n", size, hash, hash2);
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
      printf("%21s", item.first.c_str());
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

    for (int i = 0; i < static_cast<int>(num_sizes); ++i) {
      printf("%d ", i);
      for (auto& it : iterators) {
        printf("%5.2f ", *it);
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
       nanobenchmark::RepeatedMeasureWithArguments(in_sizes, func)) {
    const size_t size = size_samples.first;
    auto& samples = size_samples.second;
    const float median = nanobenchmark::Median(&samples);
    const float mad = nanobenchmark::MedianAbsoluteDeviation(samples, median);
    printf("%s %4zu: median=%6.1f cycles; median L1 norm =%4.1f cycles\n",
           caption, size, median, mad);
    measurements->Add(caption, size, median);
  }
}

void RunTests(int argc, char* argv[]) {
  // os_specific::PinThreadToRandomCPU();

  const std::vector<size_t> in_sizes = {3, 3, 4, 4, 7, 7, 8, 8, 9, 10, 1023};
  Measurements measurements;

  uint64 key2[2] = {0, 1};
  AddMeasurements(in_sizes, "SipHash", &measurements,
                  [&key2](const size_t size) {
                    char in[1024] = {static_cast<char>(size)};
                    return SipHash(key2, in, size);
                  });

#ifdef __AVX2__
  {
    uint64 key4[4] = {0, 1, 2, 3};
    AddMeasurements(in_sizes, "SipTreeHash", &measurements,
                    [&key4](const size_t size) {
                      char in[1024] = {static_cast<char>(size)};
                      return SipTreeHash(key4, in, size);
                    });

    AddMeasurements(in_sizes, "HighwayTreeHash", &measurements,
                    [&key4](const size_t size) {
                      char in[1024] = {static_cast<char>(size)};
                      return HighwayTreeHash(key4, in, size);
                    });
  }
#endif

#ifdef __SSE4_1__
  {
    uint64 key4[4] = {0, 1, 2, 3};
    AddMeasurements(in_sizes, "SSE41HighwayTreeHash", &measurements,
                    [&key4](const size_t size) {
                      char in[1024] = {static_cast<char>(size)};
                      return SSE41HighwayTreeHash(key4, in, size);
                    });
  }
#endif

  measurements.PrintTable(in_sizes);

#ifdef __SSE4_1__
  VerifyEqual("SSE41HighwayTreeHash", ScalarHighwayTreeHash,
              SSE41HighwayTreeHash);
#endif
#ifdef __AVX2__
  VerifyEqual("SipTreeHash", ScalarSipTreeHash, SipTreeHash);
  VerifyEqual("HighwayTreeHash", ScalarHighwayTreeHash, HighwayTreeHash);
#endif
}

}  // namespace
}  // namespace highwayhash

int main(int argc, char* argv[]) {
  highwayhash::RunTests(argc, argv);
  return 0;
}
