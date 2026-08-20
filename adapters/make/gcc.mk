# SPDX-License-Identifier: 0BSD

# Reusable GCC 16 consumer for cxx-modgraph dependency facts.

CXX_MODGRAPH ?= cxx-modgraph
CXX_MODGRAPH_RULES ?= build/modules.mk
CXX_MODGRAPH_BMI_DIRECTORY ?= build/bmi
CXX_MODGRAPH_OBJECT_DIRECTORY ?= build/obj
CXX_MODGRAPH_SCAN_DIRECTORY ?= build/scan
CXX_MODGRAPH_CXXFLAGS ?= -std=c++23 -fmodules
CXX_MODGRAPH_EXTERNAL_MODULES ?=
CXX_MODGRAPH_EXTRA_OBJECTS ?=
CXX_MODGRAPH_SOURCES ?= $(SOURCES)
CXX_MODGRAPH_MODULE_PATHS ?=
CXX_MODGRAPH_MODULE_EXTENSIONS ?= cppm ixx mpp
CXX_MODGRAPH_USE_LIBSTDCXX_STD ?= 0
CXX_MODGRAPH_LIBSTDCXX_BUILD_DIRECTORY ?= build/libstdc++
CXX_MODGRAPH_LIBSTDCXX_CXXFLAGS ?= $(CXX_MODGRAPH_CXXFLAGS)
CXX_MODGRAPH_MAPPER_ROOT ?=

ifeq ($(CXX_MODGRAPH_USE_LIBSTDCXX_STD),1)
CXX_MODGRAPH_STD_BMI := \
    $(CXX_MODGRAPH_LIBSTDCXX_BUILD_DIRECTORY)/gcm.cache/std.gcm
CXX_MODGRAPH_STD_COMPAT_BMI := \
    $(CXX_MODGRAPH_LIBSTDCXX_BUILD_DIRECTORY)/gcm.cache/std.compat.gcm
CXX_MODGRAPH_STD_OBJECT := $(CXX_MODGRAPH_LIBSTDCXX_BUILD_DIRECTORY)/std.o
CXX_MODGRAPH_STD_COMPAT_OBJECT := \
    $(CXX_MODGRAPH_LIBSTDCXX_BUILD_DIRECTORY)/std.compat.o
CXX_MODGRAPH_EXTERNAL_MODULES += \
    std=$(CXX_MODGRAPH_STD_BMI) \
    std.compat=$(CXX_MODGRAPH_STD_COMPAT_BMI)
CXX_MODGRAPH_EXTRA_OBJECTS += \
    $(CXX_MODGRAPH_STD_OBJECT) $(CXX_MODGRAPH_STD_COMPAT_OBJECT)
CXX_MODGRAPH_MAPPER_ROOT := \
    $(abspath $(CXX_MODGRAPH_LIBSTDCXX_BUILD_DIRECTORY)/gcm.cache)

$(CXX_MODGRAPH_STD_BMI) $(CXX_MODGRAPH_STD_COMPAT_BMI) \
        $(CXX_MODGRAPH_STD_OBJECT) $(CXX_MODGRAPH_STD_COMPAT_OBJECT) &:
	@mkdir -p $(CXX_MODGRAPH_LIBSTDCXX_BUILD_DIRECTORY)
	cd $(CXX_MODGRAPH_LIBSTDCXX_BUILD_DIRECTORY) && \
		$(CXX) $(CXX_MODGRAPH_LIBSTDCXX_CXXFLAGS) -c --compile-std-module
endif

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
cxx_modgraph_scan = \
    $(CXX_MODGRAPH_SCAN_DIRECTORY)/$(patsubst ./%,%,$(basename $(1))).p1689.json

define cxx_modgraph_compdb_entry
{"directory":"$(CURDIR)","command":"$(CXX) $(CXX_MODGRAPH_CXXFLAGS) -c $(1) -o $(call cxx_modgraph_object,$(1))","file":"$(1)","output":"$(call cxx_modgraph_object,$(1))"}
endef

CXX_MODGRAPH_COMPDB_ENTRIES = \
    $(foreach source,$(CXX_MODGRAPH_SCAN_SOURCES),$(call cxx_modgraph_compdb_entry,$(source)))
CXX_MODGRAPH_COMPDB_JSON = \
    [$(subst } {,}$(cxx_modgraph_comma) {,$(strip $(CXX_MODGRAPH_COMPDB_ENTRIES)))]
CXX_MODGRAPH_COMPILATION_DATABASE ?= build/compile_commands.json
CXX_MODGRAPH_SCAN_OUTPUTS := \
    $(foreach source,$(CXX_MODGRAPH_SCAN_SOURCES),$(call cxx_modgraph_scan,$(source)))

define cxx_modgraph_scan_rule
$(call cxx_modgraph_scan,$(1)): $(1) $(CXX_MODGRAPH_PROJECT_MAKEFILE) $(CXX_MODGRAPH_ADAPTER_MAKEFILE)
	@mkdir -p $$(dir $$@)
	$(CXX) $(CXX_MODGRAPH_CXXFLAGS) -MMD -MF $$@.d -MT $$@ \
		-fdeps-format=p1689r5 -fdeps-file=$$@ \
		-fdeps-target=$(call cxx_modgraph_object,$(1)) \
		-E -x c++ $(1) -o /dev/null
endef
$(foreach source,$(CXX_MODGRAPH_SCAN_SOURCES), \
    $(eval $(call cxx_modgraph_scan_rule,$(source))))
-include $(addsuffix .d,$(CXX_MODGRAPH_SCAN_OUTPUTS))

ifeq ($(filter clean,$(MAKECMDGOALS)),)
include $(CXX_MODGRAPH_RULES)
endif

$(CXX_MODGRAPH_COMPILATION_DATABASE): \
        $(CXX_MODGRAPH_PROJECT_MAKEFILE) $(CXX_MODGRAPH_ADAPTER_MAKEFILE) \
        | $(dir $(CXX_MODGRAPH_COMPILATION_DATABASE))
	@$(file >$@,$(CXX_MODGRAPH_COMPDB_JSON))

$(dir $(CXX_MODGRAPH_COMPILATION_DATABASE)):
	@mkdir -p $@

$(CXX_MODGRAPH_RULES): \
        $(CXX_MODGRAPH_SCAN_OUTPUTS) $(CXX_MODGRAPH_COMPILATION_DATABASE) $(CXX_MODGRAPH)
	@mkdir -p $(dir $@)
	$(CXX_MODGRAPH) \
		$(foreach scan,$(CXX_MODGRAPH_SCAN_OUTPUTS),--input $(scan)) \
		--input-format p1689 --compdb $(CXX_MODGRAPH_COMPILATION_DATABASE) \
		--bmi-dir $(CXX_MODGRAPH_BMI_DIRECTORY) --bmi-extension .gcm \
		$(foreach module,$(CXX_MODGRAPH_EXTERNAL_MODULES),--external-module $(module)) \
		--emit make --output $@

define cxx_modgraph_write_mapper
	@mkdir -p $(dir $@)
	@: > $@.mapper
	@$(if $(CXX_MODGRAPH_MAPPER_ROOT), \
		printf '$$root %s\n' $(CXX_MODGRAPH_MAPPER_ROOT) >> $@.mapper;)
	@$(foreach module,$(CXX_MODGRAPH_PROVIDES), \
		printf '%s %s\n' $(word 1,$(subst =, ,$(module))) \
		$(abspath $(word 2,$(subst =, ,$(module)))) >> $@.mapper;)
	@$(foreach module,$(CXX_MODGRAPH_IMPORTS), \
		printf '%s %s\n' $(word 1,$(subst =, ,$(module))) \
		$(abspath $(word 2,$(subst =, ,$(module)))) >> $@.mapper;)
endef

define cxx_modgraph_output_group_rule
$(subst =, ,$(1)) &:
	@mkdir -p $(dir $(word 2,$(subst =, ,$(1))))
	$$(cxx_modgraph_write_mapper)
	$$(CXX) $$(CXX_MODGRAPH_CXXFLAGS) -fmodule-mapper=$$@.mapper \
		-c $$(CXX_MODGRAPH_SOURCE) -o $(word 2,$(subst =, ,$(1)))
endef
$(foreach group,$(CXX_MODGRAPH_OUTPUT_GROUPS), \
    $(eval $(call cxx_modgraph_output_group_rule,$(group))))

CXX_MODGRAPH_GROUPED_OBJECT_TARGETS := \
    $(foreach group,$(CXX_MODGRAPH_OUTPUT_GROUPS),$(word 2,$(subst =, ,$(group))))
CXX_MODGRAPH_CONSUMER_OBJECT_TARGETS := \
    $(filter-out $(CXX_MODGRAPH_GROUPED_OBJECT_TARGETS),$(CXX_MODGRAPH_OBJECT_TARGETS))

$(CXX_MODGRAPH_CONSUMER_OBJECT_TARGETS):
	$(cxx_modgraph_write_mapper)
	$(CXX) $(CXX_MODGRAPH_CXXFLAGS) -fmodule-mapper=$@.mapper \
		-c $(CXX_MODGRAPH_SOURCE) -o $@

CXX_MODGRAPH_LINK_OBJECTS = \
    $(CXX_MODGRAPH_EXTRA_OBJECTS) $(CXX_MODGRAPH_OBJECT_TARGETS)
