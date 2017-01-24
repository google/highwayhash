# The -m machine flag here indicates the minimum CPU required to run any of the
# binaries. The instruction_sets dispatcher allows highwayhash_test to test all
# implementations supported by the compiler and CPU regardless of this flag.
# By contrast, benchmark only measures implementations enabled by this flag so
# that it can call HighwayHashT directly, which is slightly faster.
CXXFLAGS := -std=c++11 -O3 -mavx2 -Wall -I.

PROFILER_OBJS := $(addprefix highwayhash/, \
	profiler_example.o \
	os_specific.o \
)

NANOBENCHMARK_OBJS := $(addprefix highwayhash/, \
	nanobenchmark.o \
	nanobenchmark_example.o \
	os_specific.o \
)

VECTOR_TEST_OBJS := $(addprefix highwayhash/, \
	vector_test.o \
)

SIP_OBJS := $(addprefix highwayhash/, \
	sip_hash.o \
	sip_tree_hash.o \
	scalar_sip_tree_hash.o \
)

SIP_TEST_OBJS := $(addprefix highwayhash/, \
	sip_hash_test.o \
)

HIGHWAYHASH_OBJS := $(addprefix highwayhash/, \
	arch_specific.o \
	c_bindings.o \
	hh_avx2.o \
	hh_sse41.o \
	hh_portable.o \
	instruction_sets.o \
)

HIGHWAYHASH_TEST_OBJS := $(addprefix highwayhash/, \
	highwayhash_test.o \
)

BENCHMARK_OBJS := $(addprefix highwayhash/, \
	arch_specific.o \
	benchmark.o \
	os_specific.o \
)

all: profiler_example nanobenchmark_example vector_test sip_hash_test\
	highwayhash_test benchmark

profiler_example: $(PROFILER_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

nanobenchmark_example: $(NANOBENCHMARK_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

vector_test: $(VECTOR_TEST_OBJS)
	$(CXX) $(CXXFLAGS) -mavx2 $^ -o $@

sip_hash_test: $(SIP_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

# CPU-specific implementations (same source file, different compiler flags)
highwayhash/hh_avx2.o: highwayhash/highwayhash_target.cc highwayhash/hh_avx2.h
	$(CXX) $(CXXFLAGS) -mavx2 -DHH_TARGET=TargetAVX2 -DHH_TARGET_AVX2 -c highwayhash/highwayhash_target.cc -o $@

highwayhash/hh_portable.o: highwayhash/highwayhash_target.cc highwayhash/hh_portable.h
	$(CXX) $(CXXFLAGS) -DHH_TARGET=TargetPortable -DHH_TARGET_PORTABLE -c highwayhash/highwayhash_target.cc -o $@

highwayhash/hh_sse41.o: highwayhash/highwayhash_target.cc highwayhash/hh_sse41.h
	$(CXX) $(CXXFLAGS) -msse4.1 -DHH_TARGET=TargetSSE41 -DHH_TARGET_SSE41 -c highwayhash/highwayhash_target.cc -o $@

libhighwayhash.a: $(SIP_OBJS) $(HIGHWAYHASH_OBJS)
	$(AR) rcs $@ $^

highwayhash_test: $(HIGHWAYHASH_OBJS) $(HIGHWAYHASH_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

benchmark: $(SIP_OBJS) $(HIGHWAYHASH_OBJS) $(BENCHMARK_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: clean all
clean:
	$(RM) $(PROFILER_OBJS) $(NANOBENCHMARK_OBJS) $(VECTOR_TEST_OBJS) $(SIP_OBJS) $(SIP_TEST_OBJS) $(HIGHWAYHASH_OBJS) $(HIGHWAYHASH_TEST_OBJS) $(BENCHMARK_OBJS) profiler_example nanobenchmark_example vector_test sip_hash_test highwayhash_test benchmark libhighwayhash.a
