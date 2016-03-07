CXXFLAGS := -g -std=c++11 -Wall -O3 -march=native

OBJS := highway_tree_hash.o \
	scalar_highway_tree_hash.o \
	scalar_sip_tree_hash.o \
	sip_hash.o \
	sip_tree_hash.o

MAIN := sip_hash_main

all: $(MAIN)

$(MAIN): $(OBJS) $(MAIN).cc
	$(CXX) $(CXXFLAGS) $(OBJS) $(MAIN).cc -o $(MAIN)

clean: 
	$(RM) $(OBJS) $(MAIN)

.PHONY: clean
