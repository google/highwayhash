Hash functions are widely used, so it is desirable to increase their speed and
security. This package provides three 'strong' (well-distributed and
unpredictable) hash functions: a faster version of SipHash, a data-parallel
variant of SipHash using tree hashing, and an even faster algorithm we call
HighwayHash.

SipHash is a fast but 'cryptographically strong' pseudo-random function by
Aumasson and Bernstein [https://www.131002.net/siphash/siphash.pdf].

SipTreeHash slices inputs into 8-byte packets and computes their SipHash in
parallel, which is faster when processing at least 96 bytes.

HighwayHash is a new way of mixing inputs which may inspire new
cryptographically strong hashes. Large inputs are processed at a rate of
0.3 cycles per byte, and latency remains low even for small inputs.
HighwayHash is faster than SipHash for all input sizes, with about 3.8 times
higher throughput at 1 KiB. We discuss design choices and provide statistical
analysis and preliminary cryptanalysis in https://arxiv.org/abs/1612.06257.

## Applications

Unlike prior strong hashes, these functions are fast enough to be recommended
as safer replacements for weak hashes in many applications. The additional CPU
cost appears affordable, based on profiling data indicating C++ hash functions
account for less than 0.25% of CPU usage.

Hash-based selection of random subsets is useful for A/B experiments and similar
applications. Such random generators are idempotent (repeatable and
deterministic), which is helpful for parallel algorithms and testing. To avoid
bias, it is important that the hash function be unpredictable and
indistinguishable from a uniform random generator. We have verified the bit
distribution and avalanche properties of SipHash and HighwayHash.

64-bit hashes are also useful for authenticating short-lived messages such as
network/RPC packets. This requires that the hash function withstand
differential, length extension and other attacks. We have undertaken such a
formal security analysis for HighwayHash (pending publication). New
cryptanalysis tools may still need to be developed for further analysis.

Strong hashes are also important parts of methods for protecting hash tables
against unacceptable worst-case behavior and denial of service attacks
(see "hash flooding" below).

## SipHash

Our SipHash implementation is a fast and portable drop-in replacement for
the reference C code. Outputs are identical for the given test cases (messages
between 0 and 63 bytes).

Interestingly, it is about twice as fast as a SIMD implementation using SSE4.1
(https://goo.gl/80GBSD). This is presumably due to the lack of SIMD bit rotate
instructions.

SipHash13 is a faster but weaker variant with one mixing round per update and
three during finalization.

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
cost. The result is about 1.5 times as fast as SipTreeHash.

For older CPUs, we also provide an SSE4.1 version (about 95% as fast) and a
portable fall-back (about 10% as fast).

Statistical analyses and preliminary cryptanalysis are given in
https://arxiv.org/abs/1612.06257.

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
HighwayTreeHash (AVX-2) | 45.86 | 34.58 | 19.69 | 17.29 | 15.31 | 13.79 |  0.35
SSE41HighwayTreeHash | 47.37 | 35.88 | 20.81 | 17.82 | 15.92 | 14.41 |  0.40
SipTreeHash | 67.66 | 51.12 | 29.03 | 25.57 | 22.42 | 20.15 |  0.64
SipTreeHash13 | 58.39 | 42.63 | 25.03 | 21.34 | 18.92 | 17.15 |  0.40
SipHash | 42.44 | 32.79 | 18.69 | 18.17 | 16.27 | 14.68 |  1.33
SipHash13 | 39.83 | 30.64 | 17.53 | 16.69 | 15.04 | 13.63 |  0.76

The tree hashes are slightly slower for <= 8 byte inputs, but up to 3.8 times
as fast for large inputs. The SSE4.1 variant is nearly as fast as the AVX-2
version because it also processes 32 bytes at a time.

## Requirements

SipTreeHash[13] and HighwayTreeHash require an AVX-2-capable CPU (e.g. Haswell).
SSE41HighwayTreeHash requires SSE4.1. SipHash[13] and ScalarHighwayTreeHash have
no particular CPU requirements.

## Defending against hash flooding

We wish to defend (web) services that utilize hash sets/maps against
denial-of-service attacks. Such data structures assign attacker-controlled
input messages `m` to bin `H(s, m) % p` using a seed `s`, hash function `H`, and
table size `p`. Choosing a prime `p` ensures all hash bits are used; the costly
division can be avoided by multiplying with the inverse (https://goo.gl/l7ASm8).

Attackers may attempt to trigger 'flooding' (excessive work in insertions or
lookups) by finding 'collisions', i.e. many `m` assigned to the same bin. If the
attacker has local access, they can do far worse, so we assume the attacker can
only issue remote requests. If the attacker is able to send large numbers of
requests, they can already deny service, so we need only ensure the attacker's
cost is sufficiently large compared to the service's provisioning.

If the hash function is 'weak' (e.g. CityHash/Murmur), attackers can easily
generate collisions regardless of the seed. This causes `n^2` work for `n`
requests to an unprotected hash table, which is unacceptable. If the seed is
known, the attacker can find collisions for any `H` by computing `H(s, m) % p`
for various `m`. This raises the attacker's cost by a factor of `p` (typically
10^3..10^5), but we need a further increase in the cost/work ratio to be safe.

It is reasonable to assume `s` is a secret property of the service generated on
startup or even per-connection, and therefore initially unknown to remote
attackers. A timing attack by Wool/Bar-Yosef recovers 13-bit seeds by testing
all 8K possibilities using millions of requests, which takes several days (even
assuming unrealistic 150 us round-trip times). It appears infeasible to recover
64-bit seeds in this way.

If the seed remains secret, the security claims of 'strong' hashes such as
SipHash or HighwayHash imply attackers need 2^32 guesses of `m` before
expecting a collision (birthday paradox), and 2^63 requests to guess the seed.
These costs are large enough to consider the service safe, even when using a
conventional hash table.

Even if the seed is somehow revealed and/or attackers manage to find collisions,
there are two ways to prevent denial of service by limiting the work per
request.

1. Use augmented/de-amortized cuckoo hash tables (https://goo.gl/PFwwkx).
These guarantee worst-case `log n` bounds, but only if the hash function is
'indistinguishable from random', which is claimed for SipHash and HighwayHash
but certainly not for weak hashes.

2. Use conventional separate chaining for collision resolution, but with trees
instead of linked lists. Indexing the tree with `H(s, m)` rather than `m` avoids
the space and time overhead of self-balancing algorithms such as
AVL/splay/red-black/a,b trees. This relies on the equidistribution property of
strong hashes. (Thanks to funny-falcon for proposing this!)

In both cases, attackers pay a high cost (likely at least proportional to `p`)
to trigger only modest additional work (a factor of `log n`).

In summary, a strong hash function is not, by itself, sufficient to protect a
chained hash table from flooding attacks. However, strong hash functions are
important parts of two schemes for preventing denial of service. Using weak hash
functions can slightly accelerate the best-case and average-case performance of
a service, but at the risk of greatly reduced attack costs and worst-case
performance.

## Build instructions

To build with blaze/[Bazel](http://bazel.io/):
`bazel build -c opt --copt=-mavx2 -- :all`

`sip_hash_test` and other tests are omitted by default; to compile them,
please first install [GTest](https://github.com/google/googletest).

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
Damian Gryski | Go/SSE | https://github.com/dgryski/go-highway/
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
<jyrki.alakuijala@gmail.com>, updated 2017-01-06

This is not an official Google product.
