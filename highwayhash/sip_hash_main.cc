// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#elif __MACH__
#include <mach/mach.h>
#include <mach/mach_time.h>
#else
#include <time.h>
#endif
#include "highwayhash/highway_tree_hash.h"
#include "highwayhash/scalar_highway_tree_hash.h"
#include "highwayhash/scalar_sip_tree_hash.h"
#include "highwayhash/sip_hash.h"
#include "highwayhash/sip_tree_hash.h"
#include "highwayhash/sse41_highway_tree_hash.h"
#include "highwayhash/vec2.h"

namespace highwayhash {
namespace {

// Returns a timestamp in unspecified 'tick' units.
uint64 TimerTicks() {
#ifdef _WIN32
  LARGE_INTEGER counter;
  (void)QueryPerformanceCounter(&counter);
  return counter.QuadPart;
#elif __MACH__
  return mach_absolute_time();
#else
  timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  return static_cast<uint64>(t.tv_sec) * 1000000000 + t.tv_nsec;
#endif
}

// Returns duration [seconds] of the specified number of ticks.
double ToSeconds(uint64 elapsed) {
  const double elapsed_d = static_cast<double>(elapsed);

#ifdef _WIN32
  LARGE_INTEGER frequency;
  (void)QueryPerformanceFrequency(&frequency);
  return elapsed_d / frequency.QuadPart;
#elif __MACH__
  // On OSX/iOS platform the elapsed time is cpu time unit
  // We have to query the time base information to convert it back
  // See https://developer.apple.com/library/mac/qa/qa1398/_index.html
  static mach_timebase_info_data_t    sTimebaseInfo;
  if (sTimebaseInfo.denom == 0) {
    (void) mach_timebase_info(&sTimebaseInfo);
  }
  return elapsed_d * sTimebaseInfo.numer / sTimebaseInfo.denom / 1000000000;
#else
  return elapsed_d / 1000000000;  // ns
#endif
}

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
  void Add(const char* caption, const int in_size, const int reps,
           const uint64 ticks) {
    const int bytes = in_size * reps;
    const double seconds = ToSeconds(ticks);
    const float GBps = static_cast<float>(bytes / seconds * 1E-9);
    results_.emplace_back(caption, in_size, GBps);
  }

  // Prints results as a LaTeX table (only for in_sizes matching the
  // desired values).
  void PrintTable(const std::vector<int>& in_sizes) {
    printf("\\begin{tabular}{");
    for (size_t i = 0; i < in_sizes.size() + 1; ++i) {
      printf("%s", i == 0 ? "r" : "|r");
    }
    printf("}\n\\toprule\nAlgorithm");
    for (const int in_size : in_sizes) {
      printf(" & %d", in_size);
    }
    printf("\\\\\n\\midrule\n");

    const SpeedsForCaption gbps_for_caption =
        SortByCaptionFilterBySize(in_sizes);
    for (const auto& item : gbps_for_caption) {
      printf("%21s", item.first.c_str());
      for (const float gbps : item.second) {
        printf(" & %5.2f", gbps);
      }
      printf("\\\\\n");
    }
  }

  // Prints results suitable for pgfplots.
  void PrintPlots() {
    const SpeedsForCaption gbps_for_caption = SortByCaption();
    assert(!gbps_for_caption.empty());
    const size_t num_sizes = gbps_for_caption.begin()->second.size();

    printf("Size ");
    // Flatten per-caption vectors into one iterator.
    std::vector<std::vector<float>::const_iterator> iterators;
    for (const auto& item : gbps_for_caption) {
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
    Result(const char* caption, const int in_size, const float GBps)
        : caption(caption), in_size(in_size), GBps(GBps) {}

    // Algorithm name, printed as table caption or legend entry.
    const std::string caption;
    // Size of the input data [bytes].
    const int in_size;
    // Measured throughput [gigabytes per second].
    const float GBps;
  };

  using SpeedsForCaption = std::map<std::string, std::vector<float>>;

  SpeedsForCaption SortByCaption() const {
    SpeedsForCaption gbps_for_caption;
    for (const Result& result : results_) {
      gbps_for_caption[result.caption].push_back(result.GBps);
    }
    return gbps_for_caption;
  }

  // Only includes measurement results matching one of the given sizes.
  SpeedsForCaption SortByCaptionFilterBySize(
      const std::vector<int>& in_sizes) const {
    SpeedsForCaption gbps_for_caption;
    for (const Result& result : results_) {
      for (const int in_size : in_sizes) {
        if (result.in_size == in_size) {
          gbps_for_caption[result.caption].push_back(result.GBps);
        }
      }
    }
    return gbps_for_caption;
  }

  std::vector<Result> results_;
};

// Allocates (at aligned addresses) and initializes an input buffer.
char* CreateInput(const int in_size) {
  char* in = reinterpret_cast<char*>(_mm_malloc(in_size, 64));
  for (int i = 0; i < in_size; ++i) {
    in[i] = static_cast<char>(i);
  }
  return in;
}

void DeleteInput(char* in) { _mm_free(in); }

// Returns the shortest elapsed time across several repetitions of the given
// function (typically a lambda expression).
template <class Function>
uint64 MinTicks(const Function& func) {
  uint64 min_ticks = 99999999999;
  for (int rep = 0; rep < 25; ++rep) {
    const uint64 t0 = TimerTicks();
    COMPILER_FENCE;
    func();
    const uint64 t1 = TimerTicks();
    COMPILER_FENCE;
    min_ticks = std::min(min_ticks, t1 - t0);
  }
  return min_ticks;
}

template <class Function>
static void BenchmarkFunc(const std::vector<int>& in_sizes, const char* caption,
                          const Function& hash_function,
                          Measurements* measurements) {
  uint64 sum = 1;
  for (const int in_size : in_sizes) {
    char* const in = CreateInput(in_size);

    // hash_function accepts a decayed pointer, so we do not need to copy
    // into State::Key as below.
    uint64 key[4] = {0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
                     0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL};

    const int kLoops = 50000;
    const uint64 min_ticks =
        MinTicks([in, in_size, &hash_function, &sum, &key]() {
          for (int loop = 0; loop < kLoops; ++loop) {
            const uint64 hash = hash_function(key, in, in_size);
            key[1] = hash;
            sum += hash;
          }
        });
    measurements->Add(caption, in_size, kLoops, min_ticks);
    DeleteInput(in);
  }
  printf("%lld\n", sum);
}

template <class State>
static void BenchmarkState(const std::vector<int>& in_sizes,
                           const char* caption, Measurements* measurements) {
  // Cannot initialize State::Key directly because some require only two keys.
  const uint64 all_keys[4] = {0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
                              0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL};

  uint64 sum = 1;
  for (const int in_size : in_sizes) {
    char* const in = CreateInput(in_size);

    typename State::Key key;
    memcpy(&key, all_keys, sizeof(key));

    const int kLoops = 50000;
    const uint64 min_ticks = MinTicks([in, in_size, &sum, &key]() {
      for (int loop = 0; loop < kLoops; ++loop) {
        const uint64 hash = highwayhash::ComputeHash<State>(key, in, in_size);
        key[1] = hash;
        sum += hash;
      }
    });
    measurements->Add(caption, in_size, kLoops, min_ticks);
    DeleteInput(in);
  }
  printf("%lld\n", sum);
}

template <class State>
static void BenchmarkUpdate(const std::vector<int>& in_sizes,
                            const char* caption, Measurements* measurements) {
  const uint64 all_keys[4] = {0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL,
                              0x1716151413121110ULL, 0x1F1E1D1C1B1A1918ULL};

  uint64 sum = 1;
  for (const int in_size : in_sizes) {
    char* const in = CreateInput(in_size);

    typename State::Key key;
    memcpy(&key, all_keys, sizeof(key));

    const int kLoops = 500;
    const int kItems = 100;
    const uint64 min_ticks = MinTicks([in, in_size, &sum, &key]() {
      for (int loop = 0; loop < kLoops; ++loop) {
        State state(key);
        for (int item = 0; item < kItems; ++item) {
          highwayhash::UpdateState(in, in_size, &state);
        }
        const uint64 hash = state.Finalize();
        key[1] = hash;
        sum += hash;
      }
    });
    measurements->Add(caption, in_size, kLoops * kItems, min_ticks);
    DeleteInput(in);
  }
  printf("%lld\n", sum);
}

void RunTests() {
#ifdef __SSE4_1__
  VerifyEqual("SSE41HighwayTreeHash", ScalarHighwayTreeHash,
              SSE41HighwayTreeHash);
#endif
#ifdef __AVX2__
  VerifyEqual("SipTreeHash", ScalarSipTreeHash, SipTreeHash);
  VerifyEqual("HighwayTreeHash", ScalarHighwayTreeHash, HighwayTreeHash);
#endif

  std::vector<int> in_sizes;
  for (int i = 1; i < 256; ++i) {
    in_sizes.push_back(i);
  }
  in_sizes.push_back(1023);
  Measurements results;
  BenchmarkState<SipHashState>(in_sizes, "SipHash", &results);
  BenchmarkUpdate<SipHashState>(in_sizes, "Update: SipHash", &results);
  printf("\n");
  BenchmarkFunc(in_sizes, "ScalarSipTreeHash", ScalarSipTreeHash, &results);
#ifdef __AVX2__
  BenchmarkFunc(in_sizes, "SipTreeHash", SipTreeHash, &results);
#endif
  printf("\n");
  BenchmarkFunc(in_sizes, "ScalarHighwayTreeHash", ScalarHighwayTreeHash,
                &results);
#ifdef __SSE4_1__
  BenchmarkFunc(in_sizes, "SSE41HighwayTreeHash", SSE41HighwayTreeHash,
                &results);
#endif
#ifdef __AVX2__
  BenchmarkState<HighwayTreeHashState>(in_sizes, "HighwayTreeHash", &results);
  BenchmarkUpdate<HighwayTreeHashState>(in_sizes, "Update: HighwayTree",
                                        &results);
#endif
  const std::vector<int> table_sizes = {7, 8, 20, 63, 64, 1023};
  results.PrintTable(table_sizes);
  results.PrintPlots();
  printf("\n");
}

}  // namespace
}  // namespace highwayhash

int main(int argc, char* argv[]) {
  highwayhash::RunTests();
  return 0;
}
