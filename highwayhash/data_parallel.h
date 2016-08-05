#ifndef HIGHWAYHASH_HIGHWAYHASH_DATA_PARALLEL_H_
#define HIGHWAYHASH_HIGHWAYHASH_DATA_PARALLEL_H_

// Portable C++11 alternative to OpenMP for data-parallel computations:
// provides low-overhead ThreadPool, plus PerThread with support for reduction.

#include <algorithm>  // find_if
#include <atomic>
#include <condition_variable>  //NOLINT
#include <mutex>  //NOLINT
#include <thread>  //NOLINT
#include <vector>

namespace data_parallel {

// Highly scalable thread pool, especially suitable for data-parallel
// computations in the fork-join model, where clients need to know when all
// tasks have completed.
//
// Thread pools usually store small numbers of heterogeneous tasks in a queue.
// When tasks are identical or differ only by an integer input parameter, it is
// much faster to store just one function of an integer parameter and call it
// for each value.
//
// This thread pool can efficiently load-balance millions of tasks using an
// atomic counter, thus avoiding per-task syscalls. With 48 hyperthreads and
// 1M tasks that add to an atomic counter, overall runtime is 10-20x higher
// when using std::async, and up to 200x for a queue-based ThreadPool.
//
// Usage:
// ThreadPool pool;
// pool.Run(0, 1000000, [](const int i) { Func1(i); });
// // When Run returns, all of its tasks have finished.
//
// pool.RunTasks({Func2, Func3, Func4});
// // The destructor waits until all worker threads have exited cleanly.
class ThreadPool {
 public:
  // Starts the given number of worker threads and blocks until they are ready.
  // "num_threads" defaults to one per hyperthread.
  explicit ThreadPool(
      const int num_threads = std::thread::hardware_concurrency())
      : num_threads_(num_threads) {
    if (num_threads_ <= 0) {
      abort();
    }
    threads_.reserve(num_threads_);
    for (int i = 0; i < num_threads_; ++i) {
      threads_.emplace_back(ThreadFunc, this);
    }

    WorkersReadyBarrier();
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator&(const ThreadPool&) = delete;

  // Waits for all threads to exit.
  ~ThreadPool() {
    StartWorkers(kWorkerExit);

    for (std::thread& thread : threads_) {
      thread.join();
    }
  }

  // Runs func(i) on worker thread(s) for every i in [begin, end).
  // Not thread-safe - no two calls to Run and RunTasks may overlap.
  // Subsequent calls will reuse the same threads.
  //
  // Precondition: 0 <= begin <= end.
  template <typename Func>
  void Run(const int begin, const int end, const Func& func) {
    if (begin < 0 || begin > end) {
      abort();
    }
    if (begin == end) {
      return;
    }
    const WorkerCommand worker_command = (WorkerCommand(end) << 32) + begin;
    // Ensure the inputs do not result in a reserved command.
    if (worker_command == kWorkerWait || worker_command == kWorkerExit) {
      abort();
    }

    // If Func is large (many captures), this will allocate memory, but it is
    // still slower to use a std::ref wrapper.
    task_ = func;
    num_reserved_.store(0);

    StartWorkers(worker_command);
    WorkersReadyBarrier();
  }

  // Runs each task (closure, typically a lambda function) on worker thread(s).
  // Not thread-safe - no two calls to Run and RunTasks may overlap.
  // Subsequent calls will reuse the same threads.
  //
  // This is a more conventional interface for heterogeneous tasks that may be
  // independent/unrelated.
  void RunTasks(const std::vector<std::function<void(void)>>& tasks) {
    Run(0, static_cast<int>(tasks.size()),
        [&tasks](const int i) { tasks[i](); });
  }

 private:
  // After construction and between calls to Run, workers are "ready", i.e.
  // waiting on worker_start_cv_. They are "started" by sending a "command"
  // and notifying all worker_start_cv_ waiters. (That is why all workers
  // must be ready/waiting - otherwise, the notification will not reach all of
  // them and the main thread waits in vain for them to report readiness.)
  using WorkerCommand = uint64_t;

  // Special values; all others encode the begin/end parameters.
  static constexpr WorkerCommand kWorkerWait = 0;
  static constexpr WorkerCommand kWorkerExit = ~0ULL;

  void WorkersReadyBarrier() {
    std::unique_lock<std::mutex> lock(mutex_);
    workers_ready_cv_.wait(lock,
                           [this]() { return workers_ready_ == num_threads_; });
    workers_ready_ = 0;
  }

  // Precondition: all workers are ready.
  void StartWorkers(const WorkerCommand worker_command) {
    std::unique_lock<std::mutex> lock(mutex_);
    worker_start_command_ = worker_command;
    // Workers will need this lock, so release it before they wake up.
    lock.unlock();
    worker_start_cv_.notify_all();
  }

  // Attempts to reserve and perform some work from the global range of tasks,
  // which is encoded within "command". Returns after all tasks are reserved.
  static void RunRange(ThreadPool* self, const WorkerCommand command) {
    const int begin = command & 0xFFFFFFFF;
    const int end = command >> 32;
    const int num_tasks = end - begin;

    // OpenMP introduced several "schedule" strategies:
    // "single" (static assignment of exactly one chunk per thread): slower.
    // "dynamic" (allocates k tasks at a time): competitive for well-chosen k.
    // "guided" (allocates k tasks, decreases k): computing k = remaining/n
    //   is faster than halving k each iteration. We prefer this strategy
    //   because it avoids user-specified parameters.

    // Until (any) thread reserves the final tasks (about 20-40 iterations
    // for 1M tasks, 0-20 due to contention)
    int num_reserved = self->num_reserved_.load();
    while (num_reserved < num_tasks) {
      const int num_remaining = num_tasks - num_reserved;
      const int my_size = std::max(num_remaining / self->num_threads_, 1);
      if (!self->num_reserved_.compare_exchange_weak(num_reserved,
                                                     num_reserved + my_size)) {
        _mm_pause();  // slight delay to let other threads make progress.
        continue;     // num_reserved is already reloaded.
      }
      for (int i = 0; i < my_size; ++i) {
        self->task_(begin + num_reserved + i);
      }
      num_reserved = self->num_reserved_.load();
    }
  }

  static void ThreadFunc(ThreadPool* self) {
    // Until kWorkerExit command received:
    for (;;) {
      std::unique_lock<std::mutex> lock(self->mutex_);
      // Notify main thread that this thread is ready.
      if (++self->workers_ready_ == self->num_threads_) {
        self->workers_ready_cv_.notify_one();
      }
    RESUME_WAIT:
      // Wait for a command.
      self->worker_start_cv_.wait(lock);
      const WorkerCommand command = self->worker_start_command_;
      switch (command) {
        case kWorkerWait:    // spurious wakeup:
          goto RESUME_WAIT;  // lock still held, avoid incrementing ready.
        case kWorkerExit:
          return;  // exits thread
      }

      lock.unlock();
      RunRange(self, command);
    }
  }

  const int num_threads_;

  // Unmodified after ctor, but cannot be const because we call thread::join().
  std::vector<std::thread> threads_;

  std::mutex mutex_;  // guards both cv and their variables.
  std::condition_variable workers_ready_cv_;
  int workers_ready_ = 0;
  std::condition_variable worker_start_cv_;
  WorkerCommand worker_start_command_;

  // Written by main thread, read by workers (after mutex lock/unlock).
  std::function<void(int)> task_;

  // Updated by workers; alignment/padding avoids false sharing.
  alignas(64) std::atomic<int> num_reserved_{0};
  int padding[15];
};

// Thread-local storage with support for reduction (combining into one result).
// The "T" type must be unique to the call site because the list of threads'
// copies is a static member. (With knowledge of the underlying threads, we
// could eliminate this list and T allocations, but that is difficult to
// arrange and we prefer this to be usable independently of ThreadPool.)
//
// Usage:
// for (int i = 0; i < N; ++i) {
//   // in each thread:
//   T& my_copy = PerThread<T>::Get();
//   my_copy.Modify();
//
//   // single-threaded:
//   T& combined = PerThread<T>::Reduce();
//   Use(combined);
//   PerThread<T>::Destroy();
// }
//
// T is duck-typed and implements the following interface:
//
// // Returns true if T is default-initialized or Destroy was called without
// // any subsequent re-initialization.
// bool IsNull() const;
//
// // Releases any resources. Postcondition: IsNull() == true.
// void Destroy();
//
// // Merges in data from "victim". Precondition: !IsNull() && !victim.IsNull().
// void Assimilate(const T& victim);
template <class T>
class PerThread {
 public:
  // Returns reference to this thread's T instance (dynamically allocated,
  // so its address is unique). Callers are responsible for any initialization
  // beyond the default ctor.
  static T& Get() {
    static thread_local T* t;
    if (t == nullptr) {
      t = new T;
      static std::mutex mutex;
      std::lock_guard<std::mutex> lock(mutex);
      Threads().push_back(t);
    }
    return *t;
  }

  // Returns the first non-null T after assimilating all other threads' T
  // into it. Precondition: at least one non-null T exists (caller must have
  // called Get() and initialized the result).
  static T& Reduce() {
    std::vector<T*>& threads = Threads();

    // Find first non-null T
    const auto it = std::find_if(threads.begin(), threads.end(),
                                 [](const T* t) { return !t->IsNull(); });
    if (it == threads.end()) {
      abort();
    }
    T* const first = *it;

    for (const T* t : threads) {
      if (t != first && !t->IsNull()) {
        first->Assimilate(*t);
      }
    }
    return *first;
  }

  // Calls each thread's T::Destroy to release resources and/or prepare for
  // reuse by the same threads/ThreadPool. Note that all T remain allocated
  // (we need thread-independent pointers for iterating over each thread's T,
  // and deleting them would leave dangling pointers in each thread, which is
  // unacceptable because the same thread may call Get() again later.)
  static void Destroy() {
    for (T* t : Threads()) {
      t->Destroy();
    }
  }

 private:
  // Function wrapper avoids separate static member variable definition.
  static std::vector<T*>& Threads() {
    static std::vector<T*> threads;
    return threads;
  }
};

}  // namespace data_parallel

#endif  // HIGHWAYHASH_HIGHWAYHASH_DATA_PARALLEL_H_
