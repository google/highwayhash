# We assume X64 unless HH_POWER or HH_AARCH64 are defined.

override CPPFLAGS += -I.
override CXXFLAGS +=-std=c++11 -Wall -O3

SIP_OBJS := $(addprefix obj/, \
	sip_hash.o \
	sip_tree_hash.o \
	scalar_sip_tree_hash.o \
)

DISPATCHER_OBJS := $(addprefix obj/, \
	arch_specific.o \
	instruction_sets.o \
	nanobenchmark.o \
	os_specific.o \
)

HIGHWAYHASH_OBJS := $(DISPATCHER_OBJS) obj/hh_portable.o
HIGHWAYHASH_TEST_OBJS := $(DISPATCHER_OBJS) obj/highwayhash_test_portable.o
VECTOR_TEST_OBJS := $(DISPATCHER_OBJS) obj/vector_test_portable.o

ifdef HH_AARCH64
HH_X64 =
else
ifdef HH_POWER
HH_X64 =
else
HH_X64 = 1
HIGHWAYHASH_OBJS += obj/hh_avx2.o obj/hh_sse41.o
HIGHWAYHASH_TEST_OBJS += obj/highwayhash_test_avx2.o obj/highwayhash_test_sse41.o
VECTOR_TEST_OBJS += obj/vector_test_avx2.o obj/vector_test_sse41.o
endif
endif

all: $(addprefix bin/, \
	profiler_example nanobenchmark_example vector_test sip_hash_test \
	highwayhash_test benchmark) lib/libhighwayhash.a

obj/%.o: highwayhash/%.cc
	@mkdir -p -- $(dir $@)
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

bin/%: obj/%.o
	@mkdir -p -- $(dir $@)
	$(CXX) $(LDFLAGS) $^ -o $@

.DELETE_ON_ERROR:
deps.mk: $(wildcard highwayhash/*.cc) $(wildcard highwayhash/*.h) Makefile
	set -eu; for file in highwayhash/*.cc; do \
		target=obj/$${file##*/}; target=$${target%.*}.o; \
		[ "$$target" = "obj/highwayhash_target.o" ] || \
		[ "$$target" = "obj/data_parallel_benchmark.o" ] || \
		[ "$$target" = "obj/data_parallel_test.o" ] || \
		$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -DHH_DISABLE_TARGET_SPECIFIC -MM -MT \
		"$$target" "$$file"; \
	done | sed -e ':b' -e 's-../[^./]*/--' -e 'tb' >$@
-include deps.mk

bin/profiler_example: $(DISPATCHER_OBJS)

bin/nanobenchmark_example: $(DISPATCHER_OBJS) obj/nanobenchmark.o

ifdef HH_X64
obj/sip_tree_hash.o: CXXFLAGS+=-mavx2
# (Compiled from same source file with different compiler flags)
obj/highwayhash_test_avx2.o: CXXFLAGS+=-mavx2
obj/highwayhash_test_sse41.o: CXXFLAGS+=-msse4.1
obj/hh_avx2.o: CXXFLAGS+=-mavx2
obj/hh_sse41.o: CXXFLAGS+=-msse4.1
obj/vector_test_avx2.o: CXXFLAGS+=-mavx2
obj/vector_test_sse41.o: CXXFLAGS+=-msse4.1

obj/benchmark.o: CXXFLAGS+=-mavx2
endif

lib/libhighwayhash.a: $(SIP_OBJS) $(HIGHWAYHASH_OBJS) obj/c_bindings.o
	@mkdir -p -- $(dir $@)
	$(AR) rcs $@ $^
	./test_exports.sh $@

bin/highwayhash_test: $(HIGHWAYHASH_TEST_OBJS)
bin/vector_test: $(VECTOR_TEST_OBJS)

bin/benchmark: obj/benchmark.o $(HIGHWAYHASH_TEST_OBJS)
bin/benchmark: $(SIP_OBJS) $(HIGHWAYHASH_OBJS)

clean:
	[ ! -d obj ] || $(RM) -r -- obj/

distclean: clean
	[ ! -d bin ] || $(RM) -r -- bin/
	[ ! -d lib ] || $(RM) -r -- lib/

.PHONY: clean distclean all
