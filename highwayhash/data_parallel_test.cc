#include <unistd.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <future>  //NOLINT
#include <set>

#include "base/time.h"
#include "highwayhash/data_parallel.h"
#include "testing/base/public/gunit.h"
#include "thread/threadpool.h"

#if defined(_M_X64) || defined(__x86_64) || defined(__amd64) || \
    defined(__x86_64__) || defined(__amd64__)
#if defined(_MSC_VER)
#include <intrin.h>

#define CPUID __cpuid
#else  // GCC, clang
#include <cpuid.h>

#define CPUID(regs, input) \
  __get_cpuid(input, &regs[0], &regs[1], &regs[2], &regs[3])
#endif
#else
#define CPUID(regs, input)
#endif

namespace data_parallel {
namespace {

int PopulationCount(uint64_t bits) {
  int num_set = 0;
  while (bits != 0) {
    num_set += bits & 1;
    bits >>= 1;
  }
  return num_set;
}

unsigned ProcessorID() {
  unsigned regs[4] = {0};
  // CPUID function 1 is always supported, but the APIC ID is zero on very
  // old CPUs (Pentium III).
  CPUID(regs, 1);
  return regs[1] >> 24;
}

constexpr int kBenchmarkTasks = 1000000;

// Returns elapsed time [nanoseconds] for std::async.
double BenchmarkAsync(uint64_t* total) {
  const base::Time t0 = base::Now();
  std::atomic<uint64_t> sum1{0};
  std::atomic<uint64_t> sum2{0};

  std::vector<std::future<void>> futures;
  futures.reserve(kBenchmarkTasks);
  for (int i = 0; i < kBenchmarkTasks; ++i) {
    futures.push_back(std::async(
        [&sum1, &sum2](const int i) {
          sum1.fetch_add(i);
          sum2.fetch_add(1);
        },
        i));
  }

  for (auto& future : futures) {
    future.get();
  }

  const base::Time t1 = base::Now();
  *total = sum1.load() + sum2.load();
  return base::ToDoubleNanoseconds(t1 - t0);
}

// Returns elapsed time [nanoseconds] for (atomic) ThreadPool.
double BenchmarkPoolA(uint64_t* total) {
  const base::Time t0 = base::Now();
  std::atomic<uint64_t> sum1{0};
  std::atomic<uint64_t> sum2{0};

  ThreadPool pool;
  pool.Run(0, kBenchmarkTasks, [&sum1, &sum2](const int i) {
    sum1.fetch_add(i);
    sum2.fetch_add(1);
  });

  const base::Time t1 = base::Now();
  *total = sum1.load() + sum2.load();
  return base::ToDoubleNanoseconds(t1 - t0);
}

// Returns elapsed time [nanoseconds] for ::ThreadPool.
double BenchmarkPoolG(uint64_t* total) {
  const base::Time t0 = base::Now();
  std::atomic<uint64_t> sum1{0};
  std::atomic<uint64_t> sum2{0};

  {
    ::ThreadPool pool(std::thread::hardware_concurrency());
    pool.StartWorkers();
    for (int i = 0; i < kBenchmarkTasks; ++i) {
      pool.Schedule([&sum1, &sum2, i]() {
        sum1.fetch_add(i);
        sum2.fetch_add(1);
      });
    }
  }

  const base::Time t1 = base::Now();
  *total = sum1.load() + sum2.load();
  return base::ToDoubleNanoseconds(t1 - t0);
}

std::atomic<int> func_counts{0};

void Func2() {
  usleep(200000);
  func_counts.fetch_add(4);
}

void Func3() {
  usleep(300000);
  func_counts.fetch_add(16);
}

void Func4() {
  usleep(400000);
  func_counts.fetch_add(256);
}

// Exercises the RunTasks feature (running arbitrary tasks/closures)
TEST(DataParallelTest, TestRunTasks) {
  ThreadPool pool(4);
  pool.RunTasks({Func2, Func3, Func4});
  EXPECT_EQ(276, func_counts.load());
}

// Compares ThreadPool speed to std::async and ::ThreadPool.
TEST(DataParallelTest, Benchmarks) {
  uint64_t sum1, sum2, sum3;
  const double async_ns = BenchmarkAsync(&sum1);
  const double poolA_ns = BenchmarkPoolA(&sum2);
  const double poolG_ns = BenchmarkPoolG(&sum3);

  printf("Async %11.0f ns\nPoolA %11.0f ns\nPoolG %11.0f ns\n", async_ns,
         poolA_ns, poolG_ns);
  // baseline 20x, 10x with asan or msan, 5x with tsan
  EXPECT_GT(async_ns, poolA_ns * 4);
  // baseline 200x, 180x with asan, 70x with msan, 50x with tsan.
  EXPECT_GT(poolG_ns, poolA_ns * 20);

  // Should reach same result.
  EXPECT_EQ(sum1, sum2);
  EXPECT_EQ(sum2, sum3);
}

// Ensures task parameter is in bounds, every parameter is reached,
// pool can be reused (multiple consecutive Run calls), pool can be destroyed
// (joining with its threads).
TEST(DataParallelTest, TestPool) {
  for (int num_threads = 1; num_threads <= 18; ++num_threads) {
    ThreadPool pool(num_threads);
    for (int num_tasks = 0; num_tasks < 32; ++num_tasks) {
      std::vector<int> mementos(num_tasks, 0);
      for (int begin = 0; begin < 32; ++begin) {
        std::fill(mementos.begin(), mementos.end(), 0);
        pool.Run(begin, begin + num_tasks,
                 [begin, num_tasks, &mementos](const int i) {
                   // Parameter is in the given range
                   EXPECT_GE(i, begin);
                   EXPECT_LT(i, begin + num_tasks);

                   // Store mementos to be sure we visited each i.
                   mementos.at(i - begin) = 1000 + i;
                 });
        for (int i = begin; i < begin + num_tasks; ++i) {
          EXPECT_EQ(1000 + i, mementos.at(i - begin));
        }
      }
    }
  }
}

// Ensures each of N threads processes exactly 1 of N tasks, i.e. the
// work distribution is perfectly fair for small counts.
TEST(DataParallelTest, TestSmallAssignments) {
  for (int num_threads = 1; num_threads <= 64; ++num_threads) {
    ThreadPool pool(num_threads);

    std::atomic<int> counter{0};
    // (Avoid mutex because it may perturb the worker thread scheduling)
    std::atomic<uint64_t> id_bits{0};

    pool.Run(0, num_threads, [&counter, num_threads, &id_bits](const int i) {
      const int id = counter.fetch_add(1);
      EXPECT_LT(id, num_threads);
      uint64_t bits = id_bits.load(std::memory_order_relaxed);
      while (!id_bits.compare_exchange_weak(bits, bits | (1ULL << id))) {
      }
    });

    const int num_participants = PopulationCount(id_bits.load());
    EXPECT_EQ(num_threads, num_participants);
  }
}

// Ensures multiple hardware threads are used (decided by the OS scheduler).
TEST(DataParallelTest, TestProcessorIDs) {
  for (int num_threads = 1; num_threads <= std::thread::hardware_concurrency();
       ++num_threads) {
    ThreadPool pool(num_threads);

    std::mutex mutex;
    std::set<unsigned> ids;
    double total = 0.0;
    pool.Run(0, 2 * num_threads, [&mutex, &ids, &total](const int i) {
      // Useless computations to keep the processor busy so that threads
      // can't just reuse the same processor.
      double sum = 0.0;
      for (int rep = 0; rep < 9000 * (i + 30); ++rep) {
        sum += pow(rep, 0.5);
      }

      mutex.lock();
      ids.insert(ProcessorID());
      total += sum;
      mutex.unlock();
    });

    // No core ID / APIC ID available
    if (num_threads > 1 && ids.size() == 1) {
      EXPECT_EQ(0, *ids.begin());
    } else {
      // (The Linux scheduler doesn't use all available HTs, but the
      // computations should at least keep most cores busy.)
      EXPECT_GT(ids.size() + 2, num_threads / 3);
    }

    // (Ensure the busy-work is not elided.)
    EXPECT_GT(total, 1E4);
  }
}

// Test payload for PerThread.
struct CheckUniqueIDs {
  bool IsNull() const { return false; }
  void Destroy() { id_bits = 0; }
  void Assimilate(const CheckUniqueIDs& victim) {
    // Cannot overlap because each PerThread has unique bits.
    EXPECT_EQ(0, id_bits & victim.id_bits);
    id_bits |= victim.id_bits;
  }

  uint64_t id_bits = 0;
};

// Ensures each thread has a PerThread instance, that they are successfully
// combined/reduced into a single result, and that reuse is possible after
// Destroy().
TEST(DataParallelTest, TestPerThread) {
  // We use a uint64_t bit array for convenience => no more than 64 threads.
  const int max_threads = std::min(64U, std::thread::hardware_concurrency());
  for (int num_threads = 1; num_threads <= max_threads; ++num_threads) {
    ThreadPool pool(num_threads);

    std::atomic<int> counter{0};
    pool.Run(0, num_threads, [&counter, num_threads](const int i) {
      const int id = counter.fetch_add(1);
      EXPECT_LT(id, num_threads);
      PerThread<CheckUniqueIDs>::Get().id_bits |= 1ULL << id;
    });

    // Verify each thread's bit is set.
    const uint64_t all_bits = PerThread<CheckUniqueIDs>::Reduce().id_bits;
    EXPECT_EQ((1ULL << num_threads) - 1, all_bits);
    PerThread<CheckUniqueIDs>::Destroy();
  }
}

}  // namespace
}  // namespace data_parallel
