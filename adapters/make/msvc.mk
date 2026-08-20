# SPDX-License-Identifier: 0BSD

# Reusable MSVC consumer.  cl emits an IFC and object in one interface compile;
# the object rule therefore copies the side output produced with the IFC.
CXX ?= cl
CXX_MODGRAPH ?= cxx-modgraph
CXX_MODGRAPH_RULES ?= build/modules.mk
CXX_MODGRAPH_BMI_DIRECTORY ?= build/bmi
CXX_MODGRAPH_OBJECT_DIRECTORY ?= build/obj
CXX_MODGRAPH_SCAN_DIRECTORY ?= build/scan
CXX_MODGRAPH_COMPILATION_DATABASE ?= build/compile_commands.json
CXX_MODGRAPH_SOURCES ?= $(SOURCES)
CXX_MODGRAPH_CXXFLAGS ?= /nologo /std:c++latest /EHsc

cxx_modgraph_object = $(CXX_MODGRAPH_OBJECT_DIRECTORY)/$(basename $(1)).obj
cxx_modgraph_scan = $(CXX_MODGRAPH_SCAN_DIRECTORY)/$(basename $(1)).module.json
cxx_modgraph_comma := ,
define cxx_modgraph_compdb_entry
{"directory":"$(CURDIR)","arguments":["$(CXX)"$(foreach flag,$(CXX_MODGRAPH_CXXFLAGS),$(cxx_modgraph_comma)"$(flag)"),"/c","$(1)","/Fo$(call cxx_modgraph_object,$(1))"],"file":"$(1)","output":"$(call cxx_modgraph_object,$(1))"}
endef
CXX_MODGRAPH_COMPDB_ENTRIES = $(foreach source,$(CXX_MODGRAPH_SOURCES),$(call cxx_modgraph_compdb_entry,$(source)))
CXX_MODGRAPH_COMPDB_JSON = [$(subst } {,}$(cxx_modgraph_comma) {,$(strip $(CXX_MODGRAPH_COMPDB_ENTRIES)))]
CXX_MODGRAPH_SCAN_OUTPUTS = $(foreach source,$(CXX_MODGRAPH_SOURCES),$(call cxx_modgraph_scan,$(source)))

ifeq ($(filter clean,$(MAKECMDGOALS)),)
include $(CXX_MODGRAPH_RULES)
endif

ifneq ($(strip $(CXX_MODGRAPH_SOURCES)),)
$(CXX_MODGRAPH_COMPILATION_DATABASE):
	@cmake -E make_directory "$(dir $@)"
	@$(file >$@,$(CXX_MODGRAPH_COMPDB_JSON))

define cxx_modgraph_scan_rule
$(call cxx_modgraph_scan,$(1)): $(1)
	@cmake -E make_directory "$$(dir $$@)"
	$(CXX) $(CXX_MODGRAPH_CXXFLAGS) /TP /scanDependencies "$$@" /c "$$<" /Fo"$(call cxx_modgraph_object,$(1))"
endef
$(foreach source,$(CXX_MODGRAPH_SOURCES),$(eval $(call cxx_modgraph_scan_rule,$(source))))

$(CXX_MODGRAPH_RULES): $(CXX_MODGRAPH_SCAN_OUTPUTS) $(CXX_MODGRAPH_COMPILATION_DATABASE)
	@cmake -E make_directory "$(dir $@)"
	$(CXX_MODGRAPH) $(foreach scan,$(CXX_MODGRAPH_SCAN_OUTPUTS),--input "$(scan)") \
		--input-format p1689 --compdb "$(CXX_MODGRAPH_COMPILATION_DATABASE)" \
		--bmi-dir "$(CXX_MODGRAPH_BMI_DIRECTORY)" --bmi-extension .ifc --emit make --output "$@"
endif

$(CXX_MODGRAPH_BMI_TARGETS):
	@cmake -E make_directory "$(dir $@)"
	$(CXX) $(CXX_MODGRAPH_CXXFLAGS) \
		$(foreach module,$(CXX_MODGRAPH_IMPORTS),/reference "$(module)") \
		/TP /interface /c "$(CXX_MODGRAPH_SOURCE)" /ifcOutput"$@" /Fo"$@.obj"

$(CXX_MODGRAPH_OBJECT_TARGETS):
	@cmake -E make_directory "$(dir $@)"
	@if [ -n "$(CXX_MODGRAPH_PROVIDED_BMIS)" ]; then \
		cmake -E copy "$(firstword $(CXX_MODGRAPH_PROVIDED_BMIS)).obj" "$@"; \
	else \
		$(CXX) $(CXX_MODGRAPH_CXXFLAGS) \
			$(foreach module,$(CXX_MODGRAPH_IMPORTS),/reference "$(module)") \
			/c "$(CXX_MODGRAPH_SOURCE)" /Fo"$@"; \
	fi
