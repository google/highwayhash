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
HighwayHash is faster than SipHash for all input sizes, with about 7 times
higher throughput at 1 KiB.

## Applications

Expected applications include DOS-proof hash tables and random generators.

SipHash is immune to hash flooding because multi-collisions are infeasible to
compute. This makes it suitable for hash tables storing user-controlled data.

The output is also indistinguishable from a uniform random function, which
means it can be used for choosing random subsets (e.g. for A/B experiments).
Such generators are idempotent (repeatable/deterministic), which is useful
in parallel algorithms and for testing/verification.

We have verified the bit distribution and avalanche properties of HighwayHash,
though new cryptanalysis tools may need to be developed to analyze it.

## SipHash-AVX2

Our SipHash implementation aims for maximum efficiency while remaining
compatible with the reference C code. Outputs are identical for the given
test cases (messages between 0 and 63 bytes).

Compared to Bernstein's SSE4.1 implementation (https://goo.gl/80GBSD),
it is about 1.5x faster on the same machine. About 10% of that is tuning
and the rest is achieved with AVX-2 independent-shift instructions.

The implementation uses custom vector classes with overloaded operators
(e.g. const V2x64U a = b + c) for type-safety and improved readability
vs. compiler intrinsics (e.g. const __m128i a = _mm_add_epi64(b, c)).

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

By making full use of AVX-2 vectors, this leads to a 3x speedup vs. our
compatible implementation. However, the output no longer matches SipHash.

## HighwayHash

We have devised a new way of mixing inputs with AVX-2 multiply and permute
instructions. The multiplications are 32x32 -> 64 bits and therefore infeasible
to reverse. Permuting equalizes the distribution of the resulting bytes.

The internal state occupies two 256-bit AVX-2 registers, v0 and v1. Due to
limitations of the instruction set, the registers are partitioned into two
halves that remain independent until the reduce phase. Operations are 64-bit,
so each register half contains two lanes. To improve distribution, four bits
of each v0 lane are hardwired to 1. Therefore, the effective size of each state
half is 248 bits. The algorithm outputs 64 bit digests, which can easily be
extended to 256 bits by no longer discarding the remaining bits.

In addition to high throughput, the algorithm is designed for low finalization
cost. This enables a 2-3x speedup versus SipTreeHash, especially for smaller
inputs.

## Results

Performance is measured as throughput for 1 KiB messages. The benchmark
measures the minimum elapsed time among many repetitions (thus ensuring
the inputs are cache-resident) but also updates an output variable to
ensure the compiler does not elide anything. The C++ implementations are
compiled with GCC 4.8.4 and run on a single core of a desktop Xeon E5-1650 v3
clocked at 3.5 GHz.

SipHash:      1.7 GB/s
SipTreeHash:  4.8 GB/s
HighwayHash: 11.3 GB/s

## Requirements

The software requires AVX-2-capable CPUs (Intel Haswell or upcoming AMD).

## Build instructions

To build with Bazel (http://bazel.io/) :
    bazel build :all -c opt --copt=-mavx2

A simple Makefile is also provided.


## Modules

* sip_hash.cc is the compatible implementation of SipHash, and also
  provides the final reduction for sip_tree_hash.
* sip_tree_hash.cc is the faster but incompatible SIMD j-lanes tree hash.
* highway_tree_hash.cc is our new, fast AVX-2 mixing algorithm.
* scalar_sip_tree_hash.cc and scalar_highway_tree_hash.cc are non-SIMD versions.
* vec2.h contains a wrapper class for 256-bit AVX-2 vectors with 64-bit lanes.
* vec.h provides a similar class for 128-bit vectors.
* code_annotation.h defines some compiler-dependent language extensions.

By Jan Wassenberg <jan.wassenberg@gmail.com> and Jyrki Alakuijala
<jyrki.alakuijala@gmail.com>, 2016-03-01

This is not an official Google product.
