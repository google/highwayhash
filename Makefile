CXXFLAGS := -std=c++11 -O3 -mavx2 -Wall -I.

OBJS := $(addprefix highwayhash/, \
	sip_hash.o \
	sip_tree_hash.o \
	scalar_sip_tree_hash.o \
	highway_tree_hash.o \
	scalar_highway_tree_hash.o \
	sse41_highway_tree_hash.o \
	sip_hash_main.o)

all: sip_hash_main profiler_example nanobenchmark_example

sip_hash_main: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@

profiler_example: highwayhash/profiler_example.o
	$(CXX) $(CXXFLAGS) $^ -o $@

nanobenchmark_example: highwayhash/nanobenchmark.o highwayhash/nanobenchmark_example.o
	$(CXX) $(CXXFLAGS) $^ -o $@

.PHONY: clean all
clean:
	$(RM) $(OBJS) sip_hash_main profiler_example nanobenchmark_example
