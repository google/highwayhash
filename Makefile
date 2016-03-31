CXXFLAGS := -std=c++11 -O2 -mavx2 -Wall -DDEBUG

OBJS := highway_tree_hash.o \
	scalar_highway_tree_hash.o \
	scalar_sip_tree_hash.o \
	sip_hash.o \
	sip_tree_hash.o \
	sse41_sip_hash.o

MAIN := sip_hash_main

all: $(MAIN)

$(MAIN): $(OBJS) $(MAIN).cc
	$(CXX) $(CXXFLAGS) $(OBJS) $(MAIN).cc -o $(MAIN)

.PHONY: clean
clean:
	$(RM) $(OBJS) $(MAIN)
