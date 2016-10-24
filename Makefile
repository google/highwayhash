CXXFLAGS := -std=c++11 -O3 -mavx2 -Wall -I.

HASH_OBJS := $(addprefix highwayhash/, \
	os_specific.o \
	sip_hash.o \
	sip_tree_hash.o \
	scalar_sip_tree_hash.o \
	highway_tree_hash.o \
	scalar_highway_tree_hash.o \
	sse41_highway_tree_hash.o)

HASH_EXE_OBJS := $(addprefix highwayhash/, \
	sip_hash_main.o)

PROFILER_OBJS := $(addprefix highwayhash/, \
	profiler_example.o \
	os_specific.o)

NANOBENCHMARK_OBJS := $(addprefix highwayhash/, \
	nanobenchmark.o \
	nanobenchmark_example.o \
	os_specific.o)

all: sip_hash_main profiler_example nanobenchmark_example

libhighwayhash.a: $(HASH_OBJS)
	$(AR) rcs $@ $^

sip_hash_main: $(HASH_OBJS) $(HASH_EXE_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

profiler_example: $(PROFILER_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

nanobenchmark_example: $(NANOBENCHMARK_OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: clean all
clean:
	$(RM) $(HASH_OBJS) $(HASH_EXE_OBJS) $(PROFILER_OBJS) $(NANOBENCHMARK_OBJS) sip_hash_main profiler_example nanobenchmark_example libhighwayhash.a
