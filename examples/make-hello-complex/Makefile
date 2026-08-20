# SPDX-License-Identifier: 0BSD

CXX := clang++

.DEFAULT_GOAL := all

CXX_MODGRAPH := ../../build/cxx-modgraph
CXX_MODGRAPH_SOURCES := src/greeting.cpp src/main.cpp
CXX_MODGRAPH_MODULE_PATHS := modules
CXX_MODGRAPH_USE_LIBCXX_STD := 1

include ../../adapters/make/clang.mk

.PHONY: all clean
all: hello

hello: $(CXX_MODGRAPH_LINK_OBJECTS)
	$(CXX) $(CXX_MODGRAPH_CXXFLAGS) $^ -o $@

clean:
	rm -rf build hello
