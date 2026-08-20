# SPDX-License-Identifier: 0BSD

# Auto-detecting entry point for the cxx-modgraph GNU Make adapters.

CXX ?= c++
CXX_MODGRAPH_COMPILER ?= auto

cxx_modgraph_adapter_directory := $(dir $(lastword $(MAKEFILE_LIST)))

ifeq ($(CXX_MODGRAPH_COMPILER),auto)
ifneq ($(filter cl cl.exe,$(notdir $(CXX))),)
CXX_MODGRAPH_COMPILER := msvc
else
cxx_modgraph_compiler_macros := \
    $(shell $(CXX) $(CXX_MODGRAPH_CXXFLAGS) -dM -E -x c++ /dev/null 2>/dev/null)
ifneq ($(findstring __clang__,$(cxx_modgraph_compiler_macros)),)
CXX_MODGRAPH_COMPILER := clang
else ifneq ($(findstring __GNUC__,$(cxx_modgraph_compiler_macros)),)
CXX_MODGRAPH_COMPILER := gcc
else
$(error unable to identify CXX='$(CXX)' as Clang or GCC; set CXX_MODGRAPH_COMPILER explicitly)
endif
endif
endif

ifeq ($(CXX_MODGRAPH_COMPILER),clang)
include $(cxx_modgraph_adapter_directory)clang.mk
else ifeq ($(CXX_MODGRAPH_COMPILER),gcc)
include $(cxx_modgraph_adapter_directory)gcc.mk
else ifeq ($(CXX_MODGRAPH_COMPILER),msvc)
include $(cxx_modgraph_adapter_directory)msvc.mk
else
$(error unsupported CXX_MODGRAPH_COMPILER='$(CXX_MODGRAPH_COMPILER)'; expected auto, clang, gcc, or msvc)
endif
