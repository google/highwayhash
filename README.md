Hash functions are widely used, so it is desirable to increase their speed and
security. This package provides three hash functions that are resistant to
hash flooding and outperform existing algorithms: a faster version of SipHash,
a data-parallel variant of SipHash using tree hashing, and an even faster
algorithm we call HighwayHash.

SipHash is a fast but cryptographically strong pseudo-random function by
Aumasson and Bernstein [https://www.131002.net/siphash/siphash.pdf].

SipTreeHash slices inputs into 8-byte packets and computes their SipHash in
parallel, which is faster when processing at least 96 bytes.

HighwayHash is a new way of mixing inputs which may inspire new
cryptographically strong hashes. Large inputs are processed at a rate of
0.3 cycles per byte, and latency remains low even for small inputs.
HighwayHash is faster than SipHash for all input sizes, with about 5 times
higher throughput at 1 KiB.

## Applications

Expected applications include DoS-proof hash tables and random generators.

SipHash is immune to hash flooding because multi-collisions are infeasible to
compute. This makes it suitable for hash tables storing user-controlled data.

The output is also indistinguishable from a uniform random function, which
means it can be used for choosing random subsets (e.g. for A/B experiments).
Such generators are idempotent (repeatable/deterministic), which is useful
in parallel algorithms and for testing/verification.

We have verified the bit distribution and avalanche properties of HighwayHash.
A formal security analysis is pending publication, though new cryptanalysis
tools may still need to be developed for further analysis.

## SipHash

Our SipHash implementation is a fast and portable drop-in replacement for
the reference C code. Outputs are identical for the given test cases (messages
between 0 and 63 bytes).

Interestingly, it is about twice as fast as a SIMD implementation using SSE4.1
(https://goo.gl/80GBSD). This is presumably due to the lack of SIMD bit rotate
instructions.

## SipTreeHash

Even faster throughput can be achieved by logically partitioning inputs into
interleaved streams and hashing them independently. The resulting hashes are
then combined via original SipHash. Such "tree hash" constructions retain the
safety guarantees of the underlying hash function.

Example: 64 byte input = 8 qwords. Interpret them as four interleaved
streams: A0, A1, A2, A3, B0, B1, B2, B3. Compute four separate hashes of
A0, A1, A2, A3 via four-way SIMD; update each of these with the second qwords
B0, B1, B2, B3. Each independent hash result H_i includes A_i and B_i. Finally,
combine the H_i into a single digest via SipHash.

This is about twice as fast as SipHash, but the outputs differ.

The implementation uses custom AVX-2 vector classes with overloaded operators
(e.g. `const V4x64U a = b + c`) for type-safety and improved readability
vs. compiler intrinsics (e.g. `const __m256i a = _mm256_add_epi64(b, c)`).

## HighwayHash

We have devised a new way of mixing inputs with AVX-2 multiply and permute
instructions. The multiplications are 32x32 -> 64 bits and therefore infeasible
to reverse. Permuting equalizes the distribution of the resulting bytes.

The internal state occupies four 256-bit AVX-2 registers. Due to limitations of
the instruction set, the registers are partitioned into two 512-bit halves that
remain independent until the reduce phase. The algorithm outputs 64 bit digests
or up to 256 bits at no extra cost.

In addition to high throughput, the algorithm is designed for low finalization
cost. This enables a 2-3x speedup versus SipTreeHash, especially for smaller
inputs.

For older CPUs, we also provide an SSE4.1 version (about 95% as fast) and a
portable fall-back (about 10% as fast).

## Performance measurements

To measure the CPU cost of a hash function, we can either create an artificial
'microbenchmark' (easier to control, but probably not representative of the
actual runtime), or insert instrumentation directly into an application (risks
influencing the results through observer overead). We provide novel variants of
both approaches that mitigate their respective disadvantages.

profiler.h uses software write-combining to stream program traces to memory
with minimal overhead. These can be analyzed offline, or when memory is full,
to learn how much time was spent in each (possibly nested) zone.

nanobenchmark.h enables cycle-accurate measurements of very short functions.
It uses CPU fences and robust statistics to minimize variability, and also
avoids unrealistic branch prediction effects.

## Results

We compile the C++ implementations with GCC 4.8.4 and run on a single core of
a Xeon E5-2690 v3 clocked at 2.6 GHz. CPU cost is measured as cycles per byte
for various input sizes:

Algorithm | 3 | 4 | 7 | 8 | 9 | 10 | 1023
--- | --- | --- | --- | --- | --- |--- |--- |
HighwayTreeHash (AVX-2) | 40.75 | 31.59 | 17.48 | 15.80 | 13.76 | 12.41 |  0.38
SSE41HighwayTreeHash | 40.40 | 33.34 | 18.09 | 16.84 | 14.42 | 12.93 |  0.39
SipTreeHash | 55.43 | 42.83 | 23.99 | 21.46 | 18.82 | 16.92 |  0.61
SipHash | 36.57 | 25.82 | 14.69 | 15.20 | 14.49 | 13.16 |  1.32

The tree hashes are slightly slower for <= 8 byte inputs, but up to 3.5 times
as fast for large inputs. The SSE4.1 variant is nearly as fast as the AVX-2
version because it also processes 32 bytes at a time.

## Requirements

SipTreeHash and HighwayTreeHash require an AVX-2-capable CPU (e.g. Haswell).
SSE41HighwayTreeHash requires SSE4.1. SipHash and ScalarHighwayTreeHash have
no particular CPU requirements.

## Build instructions

`sip_hash_test` and `data_parallel_test` use [GTest](https://github.com/google/googletest).
You can avoid the dependency in [Bazel](http://bazel.io/) builds with
`bazel build -c opt --copt=-mavx2 -- :all -:sip_hash_test -:data_parallel_test`

To build with Make : `make`

Note on project structure: the highwayhash/ subdirectory allows users to
`#include "highwayhash/sip_hash.h"` rather than just `"sip_hash.h"`. Keeping
BUILD in the project root directory shortens the Bazel build command line.
This requires "highwayhash/" prefixes in all Bazel and Makefile rules.
Adding "." to the include path enables highwayhash/ prefixes in our cc files.

## Third-party implementations / bindings

Thanks to Damian Gryski for making us aware of these third-party
implementations or bindings. Please feel free to get in touch or
raise an issue and we'll add yours as well.

By | Language | URL
--- | --- | ---
Damian Gryski | Go | https://github.com/dgryski/go-highway/
Lovell Fuller | node.js bindings | https://github.com/lovell/highwayhash
Vinzent Steinberg | Rust bindings | https://github.com/vks/highwayhash-rs

## Modules

### Hashes

* sip_hash.cc is the compatible implementation of SipHash, and also provides the
  final reduction for sip_tree_hash.
* sip_tree_hash.cc is the faster but incompatible SIMD j-lanes tree hash.
* highway_tree_hash.cc is our new, fast AVX-2 mixing algorithm.
* scalar_sip_tree_hash.cc and scalar_highway_tree_hash.cc are non-SIMD versions.
* sse41_highway_tree_hash is a variant that only needs SSE4.1.
* c_bindings.h provides C-callable functions.
* state_helpers.h simplifies the implementation of each hash.

### Infrastructure
* code_annotation.h defines some compiler-dependent language extensions.
* data_parallel.h provides a C++11 ThreadPool and PerThread (similar to OpenMP).
* nanobenchmark.h measures elapsed times with < 1 cycle variability.
* profiler.h is a low-overhead, deterministic hierarchical profiler.
* tsc_timer.h obtains high-resolution timestamps without CPU reordering.
* vec2.h contains a wrapper class for 256-bit AVX-2 vectors with 64-bit lanes.
* vec.h provides a similar class for 128-bit vectors.

By Jan Wassenberg <jan.wassenberg@gmail.com> and Jyrki Alakuijala
<jyrki.alakuijala@gmail.com>, updated 2016-08-05

This is not an official Google product.
