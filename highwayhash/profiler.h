#ifndef THIRD_PARTY_BUTTERAUGLI_PROFILER_H_
#define THIRD_PARTY_BUTTERAUGLI_PROFILER_H_

// High precision, low overhead time measurements. Returns exact call counts and
// total elapsed time for user-defined 'zones' (code regions, i.e. C++ scopes).
//
// Usage: add this header to BUILD srcs; instrument regions of interest:
// { PROFILER_ZONE("name"); /*code*/ } or
// void FuncToMeasure() { PROFILER_FUNC; /*code*/ }.
// After all threads have exited any zones, invoke PROFILER_PRINT_RESULTS() to
// print call counts and average durations [CPU cycles] to stdout, sorted in
// descending order of total duration.

// Configuration settings:

// If zero, this file has no effect and no measurements will be recorded.
#ifndef PROFILER_ENABLED
#define PROFILER_ENABLED 1
#endif

// How many mebibytes to allocate (if PROFILER_ENABLED) per thread that
// enters at least one zone. Once this buffer is full, the thread will analyze
// and discard packets, thus temporarily adding some observer overhead.
// Each zone occupies 16 bytes.
#ifndef PROFILER_THREAD_STORAGE
#define PROFILER_THREAD_STORAGE 200ULL
#endif

#if PROFILER_ENABLED

#include <emmintrin.h>

#include <algorithm>  // min/max
#include <atomic>
#include <cassert>
#include <cstddef>  // ptrdiff_t
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>  // memcpy
#include <new>

// Non-portable aspects:
// - SSE2 128-bit load/store (write-combining, UpdateOrAdd)
// - RDTSCP timestamps (serializing, high-resolution)
// - assumes string literals are stored within an 8 MiB range
// - compiler-specific annotations (restrict, alignment, fences)
#if MSC_VERSION
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include "highwayhash/code_annotation.h"
#include "highwayhash/tsc_timer.h"

#define PROFILER_CHECK(condition)                           \
  while (!(condition)) {                                    \
    printf("Profiler check failed at line %d\n", __LINE__); \
    abort();                                                \
  }

namespace profiler {

// Upper bounds for various fixed-size data structures (guarded via assert):

// How many threads can actually enter a zone (those that don't do not count).
// Memory use is about kMaxThreads * PROFILER_THREAD_STORAGE MiB.
// WARNING: a fiber library can spawn hundreds of threads.
static constexpr size_t kMaxThreads = 128;

// Maximum nesting of zones.
static constexpr size_t kMaxDepth = 64;

// Total number of zones.
static constexpr size_t kMaxZones = 256;

// Functions that depend on the cache line size.
class CacheAligned {
 public:
  static constexpr size_t kPointerSize = sizeof(void*);
  static constexpr size_t kCacheLineSize = 64;

  static void* Allocate(const size_t bytes) CACHE_ALIGNED_RETURN {
    char* const allocated = static_cast<char*>(malloc(bytes + kCacheLineSize));
    if (allocated == nullptr) {
      return nullptr;
    }
    const uintptr_t misalignment =
        reinterpret_cast<uintptr_t>(allocated) & (kCacheLineSize - 1);
    // malloc is at least kPointerSize aligned, so we can store the "allocated"
    // pointer immediately before the aligned memory.
    assert(misalignment % kPointerSize == 0);
    char* const aligned = allocated + kCacheLineSize - misalignment;
    memcpy(aligned - kPointerSize, &allocated, kPointerSize);
    return aligned;
  }

  static void Free(void* aligned_pointer) {
    if (aligned_pointer == nullptr) {
      return;
    }
    char* const aligned = static_cast<char*>(aligned_pointer);
    assert(reinterpret_cast<uintptr_t>(aligned) % kCacheLineSize == 0);
    char* allocated;
    memcpy(&allocated, aligned - kPointerSize, kPointerSize);
    assert(allocated <= aligned - kPointerSize);
    assert(allocated >= aligned - kCacheLineSize);
    free(allocated);
  }

  // Overwrites "to" without loading it into the cache (read-for-ownership).
  template <typename T>
  static void StreamCacheLine(const T* from, T* to) {
    static_assert(sizeof(__m128i) % sizeof(T) == 0, "Cannot divide");
    const size_t kLanes = sizeof(__m128i) / sizeof(T);
    const __m128i v0 = LoadVector(from + 0 * kLanes);
    COMPILER_FENCE;
    const __m128i v1 = LoadVector(from + 1 * kLanes);
    COMPILER_FENCE;
    const __m128i v2 = LoadVector(from + 2 * kLanes);
    COMPILER_FENCE;
    const __m128i v3 = LoadVector(from + 3 * kLanes);
    COMPILER_FENCE;
    // Fences prevent the compiler from reordering loads/stores, which may
    // interfere with write-combining.
    StreamVector(v0, to + 0 * kLanes);
    COMPILER_FENCE;
    StreamVector(v1, to + 1 * kLanes);
    COMPILER_FENCE;
    StreamVector(v2, to + 2 * kLanes);
    COMPILER_FENCE;
    StreamVector(v3, to + 3 * kLanes);
    COMPILER_FENCE;
  }

 private:
  // Loads 128-bit vector from memory.
  static __m128i LoadVector(const void* from) {
    return _mm_load_si128(reinterpret_cast<const __m128i*>(from));
  }

  // Adds a 128-bit vector to the CPU's write-combine buffer.
  static void StreamVector(const __m128i& v, void* to) {
    _mm_stream_si128(reinterpret_cast<__m128i*>(to), v);
  }
};

// Represents zone entry/exit events. Stores a full-resolution timestamp plus
// an offset (representing zone name or identifying exit packets). POD.
class Packet {
 public:
  // If offsets do not fit, UpdateOrAdd will overrun our heap allocation
  // (governed by kMaxZones). We have seen multi-megabyte offsets.
  static constexpr size_t kOffsetBits = 25;
  static constexpr ptrdiff_t kOffsetBias = 1LL << (kOffsetBits - 1);

  // We need full-resolution timestamps; at an effective rate of 4 GHz,
  // this permits 1 minute zone durations (for longer durations, split into
  // multiple zones). Wraparound is handled by masking.
  static constexpr size_t kTimestampBits = 64 - kOffsetBits;
  static constexpr uint64_t kTimestampMask = (1ULL << kTimestampBits) - 1;

  static Packet Make(const size_t biased_offset, const uint64_t timestamp) {
    assert(biased_offset < (1ULL << kOffsetBits));

    Packet packet;
    packet.bits_ =
        (biased_offset << kTimestampBits) + (timestamp & kTimestampMask);
    return packet;
  }

  uint64_t Timestamp() const { return bits_ & kTimestampMask; }

  size_t BiasedOffset() const { return (bits_ >> kTimestampBits); }

 private:
  uint64_t bits_;
};
static_assert(sizeof(Packet) == 8, "Wrong Packet size");

// Returns the address of a string literal. Assuming zone names are also
// literals and stored nearby, we can represent them as offsets, which are
// faster to compute than hashes or even a static index.
static inline const char* StringOrigin() {
  // Chosen such that no zone name is a prefix nor suffix of this string
  // to ensure they aren't merged (offset 0 identifies zone-exit packets).
  static const char* string_origin = "__#__";
  return string_origin - Packet::kOffsetBias;
}

// Representation of an active zone, stored in a stack. Used to deduct
// child duration from the parent's self time. POD.
struct Node {
  Packet packet;
  uint64_t child_total;
};

// Holds statistics for all zones with the same name. POD.
struct Accumulator {
  static constexpr size_t kNumCallBits = 64 - Packet::kOffsetBits;

  uint64_t BiasedOffset() const { return num_calls >> kNumCallBits; }
  uint64_t NumCalls() const { return num_calls & ((1ULL << kNumCallBits) - 1); }

  // UpdateOrAdd relies upon this layout.
  uint64_t num_calls = 0;  // upper bits = biased_offset.
  uint64_t total_duration = 0;
};
static_assert(sizeof(Accumulator) == sizeof(__m128i), "Wrong Accumulator size");

// Per-thread call graph (stack) and Accumulator for each zone.
class Results {
 public:
  Results() {
    // Zero-initialize first accumulator to avoid a check for num_zones_ == 0.
    memset(zones_, 0, sizeof(Accumulator));
  }

  // Draw all required information from the packets, which can be discarded
  // afterwards. Called whenever this thread's storage is full.
  void AnalyzePackets(const Packet* begin, const Packet* end) {
    const uint64_t t0 = tsc_timer::Start<uint64_t>();
    static const uint64_t overhead = tsc_timer::Resolution<uint64_t>();
    const __m128i one_64 = _mm_set1_epi64x(1);

    for (const Packet* p = begin; p < end; ++p) {
      // Entering a zone
      if (p->BiasedOffset() != Packet::kOffsetBias) {
        assert(depth_ < kMaxDepth);
        nodes_[depth_].packet = *p;
        nodes_[depth_].child_total = 0;
        ++depth_;
        continue;
      }

      assert(depth_ != 0);
      const Node& node = nodes_[depth_ - 1];
      // Masking correctly handles unsigned wraparound.
      const uint64_t duration =
          (p->Timestamp() - node.packet.Timestamp()) & Packet::kTimestampMask;
      const uint64_t self_duration = duration - overhead - node.child_total;

      UpdateOrAdd(node.packet.BiasedOffset(), self_duration, one_64);
      --depth_;

      // Deduct this nested node's time from its parent's self_duration.
      if (depth_ != 0) {
        nodes_[depth_ - 1].child_total += duration + overhead;
      }
    }
    const uint64_t t1 = tsc_timer::Stop<uint64_t>();
    analyze_elapsed_ += t1 - t0;
  }

  // Incorporates results from another thread. Call after all threads have
  // exited any zones.
  void Assimilate(const Results& other) {
    const uint64_t t0 = tsc_timer::Start<uint64_t>();
    assert(depth_ == 0);
    assert(other.depth_ == 0);

    const __m128i one_64 = _mm_set1_epi64x(1);
    for (size_t i = 0; i < other.num_zones_; ++i) {
      const Accumulator& zone = other.zones_[i];
      UpdateOrAdd(zone.BiasedOffset(), zone.total_duration, one_64);
    }
    const uint64_t t1 = tsc_timer::Stop<uint64_t>();
    analyze_elapsed_ += t1 - t0 + other.analyze_elapsed_;
  }

  // Single-threaded.
  void Print() {
    const uint64_t t0 = tsc_timer::Start<uint64_t>();
    MergeDuplicates();

    // Sort by decreasing total (self) cost.
    std::sort(zones_, zones_ + num_zones_,
              [](const Accumulator& r1, const Accumulator& r2) {
                return r1.total_duration > r2.total_duration;
              });

    const char* string_origin = StringOrigin();
    for (size_t i = 0; i < num_zones_; ++i) {
      const Accumulator& r = zones_[i];
      const uint64_t num_calls = r.NumCalls();
      printf("%40s: %10zu x %15zu\n", string_origin + r.BiasedOffset(),
             num_calls, r.total_duration / num_calls);
    }

    const uint64_t t1 = tsc_timer::Stop<uint64_t>();
    analyze_elapsed_ += t1 - t0;
    printf("Total clocks during analysis: %zu\n", analyze_elapsed_);
  }

 private:
  static bool SameOffset(const __m128i& zone, const size_t biased_offset) {
    const uint64_t num_calls = _mm_cvtsi128_si64(zone);
    return (num_calls >> Accumulator::kNumCallBits) == biased_offset;
  }

  // Updates an existing Accumulator (uniquely identified by biased_offset) or
  // adds one if this is the first time this thread analyzed that zone.
  // Uses a self-organizing list data structure, which avoids dynamic memory
  // allocations and is far faster than unordered_map. Loads, updates and
  // stores the entire Accumulator with vector instructions.
  void UpdateOrAdd(const size_t biased_offset, const uint64_t duration,
                   const __m128i& one_64) {
    assert(biased_offset < (1ULL << Packet::kOffsetBits));

    const __m128i duration_64 = _mm_cvtsi64_si128(duration);
    const __m128i add_duration_call = _mm_unpacklo_epi64(one_64, duration_64);

    __m128i* const RESTRICT zones = reinterpret_cast<__m128i*>(zones_);

    // Special case for first zone: (maybe) update, without swapping.
    __m128i prev = _mm_load_si128(zones);
    if (SameOffset(prev, biased_offset)) {
      prev = _mm_add_epi64(prev, add_duration_call);
      assert(SameOffset(prev, biased_offset));
      _mm_store_si128(zones, prev);
      return;
    }

    // Look for a zone with the same offset.
    for (size_t i = 1; i < num_zones_; ++i) {
      __m128i zone = _mm_load_si128(zones + i);
      if (SameOffset(zone, biased_offset)) {
        zone = _mm_add_epi64(zone, add_duration_call);
        assert(SameOffset(zone, biased_offset));
        // Swap with predecessor (more conservative than move to front,
        // but at least as successful).
        _mm_store_si128(zones + i - 1, zone);
        _mm_store_si128(zones + i, prev);
        return;
      }
      prev = zone;
    }

    // Not found; create a new Accumulator.
    const __m128i biased_offset_64 = _mm_slli_epi64(
        _mm_cvtsi64_si128(biased_offset), Accumulator::kNumCallBits);
    const __m128i zone = _mm_add_epi64(biased_offset_64, add_duration_call);
    assert(SameOffset(zone, biased_offset));

    assert(num_zones_ < kMaxZones);
    _mm_store_si128(zones + num_zones_, zone);
    ++num_zones_;
  }

  // Each instantiation of a function template seems to get its own copy of
  // __func__ and GCC doesn't merge them. An N^2 search for duplicates is
  // acceptable because we only expect a few dozen zones.
  void MergeDuplicates() {
    const char* string_origin = StringOrigin();
    for (size_t i = 0; i < num_zones_; ++i) {
      const size_t biased_offset = zones_[i].BiasedOffset();
      const char* name = string_origin + biased_offset;
      // Separate num_calls from biased_offset so we can add them together.
      uint64_t num_calls = zones_[i].NumCalls();

      // Add any subsequent duplicates to num_calls and total_duration.
      for (size_t j = i + 1; j < num_zones_;) {
        if (!strcmp(name, string_origin + zones_[j].BiasedOffset())) {
          num_calls += zones_[j].NumCalls();
          zones_[i].total_duration += zones_[j].total_duration;
          // Fill hole with last item.
          zones_[j] = zones_[--num_zones_];
        } else {  // Name differed, try next Accumulator.
          ++j;
        }
      }

      assert(num_calls < (1ULL << Accumulator::kNumCallBits));

      // Re-pack regardless of whether any duplicates were found.
      zones_[i].num_calls =
          (biased_offset << Accumulator::kNumCallBits) + num_calls;
    }
  }

  uint64_t analyze_elapsed_ = 0;

  size_t depth_ = 0;      // Number of active zones.
  size_t num_zones_ = 0;  // Number of retired zones.

  alignas(64) Node nodes_[kMaxDepth];         // Stack
  alignas(64) Accumulator zones_[kMaxZones];  // Self-organizing list
};

// Per-thread packet storage, allocated via CacheAligned.
class ThreadSpecific {
  static constexpr size_t kBufferCapacity =
      CacheAligned::kCacheLineSize / sizeof(Packet);

 public:
  // "name" is used to sanity-check offsets fit in kOffsetBits.
  explicit ThreadSpecific(const char* name)
      : buffer_pos_(buffer_),
        begin_(static_cast<Packet*>(
            CacheAligned::Allocate(PROFILER_THREAD_STORAGE << 20))),
        end_(begin_),
        capacity_(end_ + (PROFILER_THREAD_STORAGE << 17)),
        string_origin_(StringOrigin()) {
    // Even in optimized builds (with NDEBUG), verify that this zone's name
    // offset fits within the allotted space. If not, UpdateOrAdd is likely to
    // overrun zones_[]. We also assert(), but users often do not run debug
    // builds. Checking here on the cold path (only reached once per thread)
    // is cheap, but it only covers one zone.
    const size_t biased_offset = name - string_origin_;
    PROFILER_CHECK(biased_offset <= (1ULL << Packet::kOffsetBits));
  }

  ~ThreadSpecific() { CacheAligned::Free(begin_); }

  void WriteEntry(const char* name, const uint64_t timestamp) {
    const size_t biased_offset = name - string_origin_;
    Write(Packet::Make(biased_offset, timestamp));
  }

  void WriteExit(const uint64_t timestamp) {
    const size_t biased_offset = Packet::kOffsetBias;
    Write(Packet::Make(biased_offset, timestamp));
  }

  void AnalyzeRemainingPackets() {
    // Ensures prior weakly-ordered streaming stores are globally visible.
    _mm_sfence();

    const ptrdiff_t num_buffered = buffer_pos_ - buffer_;
    // Storage full => empty it.
    if (end_ + num_buffered > capacity_) {
      results_.AnalyzePackets(begin_, end_);
      end_ = begin_;
    }
    memcpy(end_, buffer_, num_buffered * sizeof(Packet));
    end_ += num_buffered;
    results_.AnalyzePackets(begin_, end_);
    end_ = begin_;
  }

  Results& GetResults() { return results_; }

 private:
  // Write packet to buffer/storage, emptying them as needed.
  void Write(const Packet packet) {
    // Buffer full => copy to storage.
    if (buffer_pos_ == buffer_ + kBufferCapacity) {
      // Storage full => empty it.
      if (end_ + kBufferCapacity > capacity_) {
        results_.AnalyzePackets(begin_, end_);
        end_ = begin_;
      }
      // This buffering halves observer overhead and decreases the overall
      // runtime by about 3%.
      CacheAligned::StreamCacheLine(buffer_, end_);
      end_ += kBufferCapacity;
      buffer_pos_ = buffer_;
    }
    *buffer_pos_ = packet;
    ++buffer_pos_;
  }

  // Write-combining buffer to avoid cache pollution. Must be the first
  // non-static member to ensure cache-line alignment.
  Packet buffer_[kBufferCapacity];
  Packet* buffer_pos_;

  // Contiguous storage for zone enter/exit packets.
  Packet* const begin_;
  Packet* end_;
  Packet* const capacity_;
  // Cached here because we already read this cache line on zone entry/exit.
  const char* string_origin_;
  Results results_;
};

class ThreadList {
 public:
  // Thread-safe.
  void Add(ThreadSpecific* const ts) {
    const uint32_t index = num_threads_.fetch_add(1);
    PROFILER_CHECK(index < kMaxThreads);
    threads_[index] = ts;
  }

  // Single-threaded.
  void PrintResults() {
    const uint32_t num_threads = num_threads_.load();
    for (uint32_t i = 0; i < num_threads; ++i) {
      threads_[i]->AnalyzeRemainingPackets();
    }

    // Combine all threads into a single Result.
    for (uint32_t i = 1; i < num_threads; ++i) {
      threads_[0]->GetResults().Assimilate(threads_[i]->GetResults());
    }

    if (num_threads != 0) {
      threads_[0]->GetResults().Print();
    }
  }

 private:
  // Owning pointers.
  alignas(64) ThreadSpecific* threads_[kMaxThreads];
  std::atomic<uint32_t> num_threads_{0};
};

// RAII zone enter/exit recorder constructed by the ZONE macro; also
// responsible for initializing ThreadSpecific.
class Zone {
 public:
  // "name" must be a string literal (see StringOrigin).
  explicit Zone(const char* name) {
    static thread_local ThreadSpecific* thread_specific;
    if (thread_specific == nullptr) {
      void* mem = CacheAligned::Allocate(sizeof(ThreadSpecific));
      thread_specific = new (mem) ThreadSpecific(name);
      Threads().Add(thread_specific);
    }

    thread_specific_ = thread_specific;

    // (Capture timestamp ASAP, not inside WriteEntry.)
    const uint64_t timestamp = tsc_timer::Start<uint64_t>();
    thread_specific_->WriteEntry(name, timestamp);
  }

  ~Zone() {
    const uint64_t timestamp = tsc_timer::Stop<uint64_t>();
    thread_specific_->WriteExit(timestamp);
  }

  // Call exactly once after all threads have exited all zones.
  static void PrintResults() { Threads().PrintResults(); }

 private:
  // Returns the singleton ThreadList. Non time-critical, function-local static
  // avoids needing a separate definition.
  static ThreadList& Threads() {
    static ThreadList threads_;
    return threads_;
  }

  // Cached so that the static thread_local can be function-local so we don't
  // need to define the static member separately.
  ThreadSpecific* thread_specific_;
};

}  // namespace profiler

// Creates a zone starting from here until the end of the current scope.
// Timestamps will be recorded when entering and exiting the zone.
// "name" must be a string literal, which is ensured by merging with "".
#define PROFILER_ZONE(name) const profiler::Zone zone("" name)

// Creates a zone for an entire function (when placed at its beginning).
// Shorter/more convenient than ZONE.
#define PROFILER_FUNC const profiler::Zone zone(__func__)

#define PROFILER_PRINT_RESULTS profiler::Zone::PrintResults

#else  // !PROFILER_ENABLED
#define PROFILER_ZONE(name)
#define PROFILER_FUNC
#define PROFILER_PRINT_RESULTS()
#endif

#endif  // THIRD_PARTY_BUTTERAUGLI_PROFILER_H_
