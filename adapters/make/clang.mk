# SPDX-License-Identifier: 0BSD

# Reusable Clang consumer for cxx-modgraph dependency facts.

CXX_MODGRAPH ?= cxx-modgraph
CXX_MODGRAPH_RULES ?= build/modules.mk
CXX_MODGRAPH_BMI_DIRECTORY ?= build/bmi
CXX_MODGRAPH_OBJECT_DIRECTORY ?= build/obj
CXX_MODGRAPH_CXXFLAGS ?= -std=c++23 -stdlib=libc++
CXX_MODGRAPH_USE_LIBCXX_STD ?= 0
CXX_MODGRAPH_SCANNER ?= clang-scan-deps
CXX_MODGRAPH_COMPILATION_DATABASE ?= build/compile_commands.json
CXX_MODGRAPH_SCAN_OUTPUT ?= build/dependencies.p1689.json
CXX_MODGRAPH_EXTERNAL_MODULES ?=
CXX_MODGRAPH_SOURCES ?= $(SOURCES)
CXX_MODGRAPH_MODULE_PATHS ?=
CXX_MODGRAPH_MODULE_EXTENSIONS ?= cppm ixx mpp

cxx_modgraph_walk = $(foreach entry,$(wildcard $(1)/*), \
    $(if $(wildcard $(entry)/.),$(call cxx_modgraph_walk,$(entry)),$(entry)))
CXX_MODGRAPH_DISCOVERED_MODULE_SOURCES := $(sort \
    $(filter $(foreach extension,$(CXX_MODGRAPH_MODULE_EXTENSIONS),%.$(extension)), \
        $(foreach directory,$(CXX_MODGRAPH_MODULE_PATHS), \
            $(call cxx_modgraph_walk,$(directory)))))
CXX_MODGRAPH_SCAN_SOURCES := $(sort \
    $(CXX_MODGRAPH_SOURCES) $(CXX_MODGRAPH_DISCOVERED_MODULE_SOURCES))

CXX_MODGRAPH_PROJECT_MAKEFILE := $(firstword $(MAKEFILE_LIST))
CXX_MODGRAPH_ADAPTER_MAKEFILE := $(lastword $(MAKEFILE_LIST))
cxx_modgraph_comma := ,

cxx_modgraph_object = \
    $(CXX_MODGRAPH_OBJECT_DIRECTORY)/$(patsubst ./%,%,$(basename $(1))).o

define cxx_modgraph_compdb_entry
{"directory":"$(CURDIR)","command":"$(CXX) $(CXX_MODGRAPH_CXXFLAGS) -c $(1) -o $(call cxx_modgraph_object,$(1))","file":"$(1)","output":"$(call cxx_modgraph_object,$(1))"}
endef

CXX_MODGRAPH_COMPDB_ENTRIES = \
    $(foreach source,$(CXX_MODGRAPH_SCAN_SOURCES),$(call cxx_modgraph_compdb_entry,$(source)))
CXX_MODGRAPH_COMPDB_JSON = \
    [$(subst } {,}$(cxx_modgraph_comma) {,$(strip $(CXX_MODGRAPH_COMPDB_ENTRIES)))]

ifeq ($(filter clean,$(MAKECMDGOALS)),)
include $(CXX_MODGRAPH_RULES)
endif

ifneq ($(strip $(CXX_MODGRAPH_SCAN_SOURCES)),)
$(CXX_MODGRAPH_COMPILATION_DATABASE): \
        $(CXX_MODGRAPH_PROJECT_MAKEFILE) $(CXX_MODGRAPH_ADAPTER_MAKEFILE) \
        | $(dir $(CXX_MODGRAPH_COMPILATION_DATABASE))
	@$(file >$@,$(CXX_MODGRAPH_COMPDB_JSON))

$(dir $(CXX_MODGRAPH_COMPILATION_DATABASE)):
	@mkdir -p $@

$(CXX_MODGRAPH_SCAN_OUTPUT): \
        $(CXX_MODGRAPH_COMPILATION_DATABASE) $(CXX_MODGRAPH_SCAN_SOURCES)
	@mkdir -p $(dir $@)
	$(CXX_MODGRAPH_SCANNER) -format=p1689 \
		-compilation-database=$(CXX_MODGRAPH_COMPILATION_DATABASE) -o $@

$(CXX_MODGRAPH_RULES): \
        $(CXX_MODGRAPH_SCAN_OUTPUT) $(CXX_MODGRAPH_COMPILATION_DATABASE) $(CXX_MODGRAPH)
	@mkdir -p $(dir $@)
	$(CXX_MODGRAPH) --input $(CXX_MODGRAPH_SCAN_OUTPUT) --input-format p1689 \
		--compdb $(CXX_MODGRAPH_COMPILATION_DATABASE) \
		--bmi-dir $(CXX_MODGRAPH_BMI_DIRECTORY) \
		$(foreach module,$(CXX_MODGRAPH_EXTERNAL_MODULES),--external-module $(module)) \
		--emit make --output $@
else
CXX_MODGRAPH_FACTS ?= dependencies.json
$(CXX_MODGRAPH_RULES): $(CXX_MODGRAPH_FACTS) $(CXX_MODGRAPH)
	@mkdir -p $(dir $@)
	$(CXX_MODGRAPH) --input $(CXX_MODGRAPH_FACTS) --emit make --output $@
endif

$(CXX_MODGRAPH_BMI_TARGETS):
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_MODGRAPH_CXXFLAGS) \
		-fprebuilt-module-path=$(CXX_MODGRAPH_BMI_DIRECTORY) \
		$(foreach module,$(CXX_MODGRAPH_IMPORTS),-fmodule-file=$(module)) \
		--precompile $(CXX_MODGRAPH_SOURCE) -o $@

.SECONDEXPANSION:
$(CXX_MODGRAPH_OBJECT_TARGETS): $$(CXX_MODGRAPH_PROVIDED_BMIS)
	@mkdir -p $(dir $@)
	@if test -n "$(CXX_MODGRAPH_PROVIDED_BMIS)"; then \
		$(CXX) $(CXX_MODGRAPH_CXXFLAGS) \
			-fprebuilt-module-path=$(CXX_MODGRAPH_BMI_DIRECTORY) \
			$(foreach module,$(CXX_MODGRAPH_IMPORTS),-fmodule-file=$(module)) \
			-c $(firstword $(CXX_MODGRAPH_PROVIDED_BMIS)) -o $@; \
	else \
		$(CXX) $(CXX_MODGRAPH_CXXFLAGS) \
			-fprebuilt-module-path=$(CXX_MODGRAPH_BMI_DIRECTORY) \
			$(foreach module,$(CXX_MODGRAPH_IMPORTS),-fmodule-file=$(module)) \
			-c $(CXX_MODGRAPH_SOURCE) -o $@; \
	fi

ifeq ($(CXX_MODGRAPH_USE_LIBCXX_STD),1)
CXX_MODGRAPH_LIBCXX_MANIFEST := $(shell $(CXX) -print-file-name=libc++.modules.json)
CXX_MODGRAPH_LIBCXX_MODULE_DIRECTORY := \
    $(abspath $(dir $(CXX_MODGRAPH_LIBCXX_MANIFEST))/../share/libc++/v1)
CXX_MODGRAPH_STD_SOURCE ?= $(CXX_MODGRAPH_LIBCXX_MODULE_DIRECTORY)/std.cppm
CXX_MODGRAPH_STD_BMI ?= $(CXX_MODGRAPH_BMI_DIRECTORY)/std.pcm
CXX_MODGRAPH_STD_OBJECT ?= $(CXX_MODGRAPH_OBJECT_DIRECTORY)/std.o
CXX_MODGRAPH_EXTRA_OBJECTS += $(CXX_MODGRAPH_STD_OBJECT)
CXX_MODGRAPH_EXTERNAL_MODULES += std=$(CXX_MODGRAPH_STD_BMI)

$(CXX_MODGRAPH_STD_BMI): $(CXX_MODGRAPH_STD_SOURCE)
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_MODGRAPH_CXXFLAGS) --precompile $< -o $@

$(CXX_MODGRAPH_STD_OBJECT): $(CXX_MODGRAPH_STD_BMI)
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_MODGRAPH_CXXFLAGS) -c $< -o $@
endif

CXX_MODGRAPH_LINK_OBJECTS = \
    $(CXX_MODGRAPH_EXTRA_OBJECTS) $(CXX_MODGRAPH_OBJECT_TARGETS)
