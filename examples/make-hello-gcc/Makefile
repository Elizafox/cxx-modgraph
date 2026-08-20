# SPDX-License-Identifier: 0BSD

CXX := g++

.DEFAULT_GOAL := all

CXX_MODGRAPH := ../../build/cxx-modgraph
CXX_MODGRAPH_SOURCES := src/main.cpp
CXX_MODGRAPH_MODULE_PATHS := modules
CXX_MODGRAPH_USE_LIBSTDCXX_STD := 1

include ../../adapters/make/gcc.mk

.PHONY: all clean
all: hello

hello: $(CXX_MODGRAPH_LINK_OBJECTS)
	$(CXX) $^ -o $@

clean:
	rm -rf build hello
