# SPDX-License-Identifier: 0BSD

foreach(required IN ITEMS
        make_program ninja_program modgraph source_root work_directory cross_cxx)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY "${work_directory}/modules" "${work_directory}/src")
file(WRITE "${work_directory}/modules/hello.cppm" [=[
export module hello;

export int answer()
{
    return 42;
}
]=])
file(WRITE "${work_directory}/src/main.cpp" [=[
import hello;

int main()
{
    return answer() == 42 ? 0 : 1;
}
]=])

set(makefile [=[
CXX := @cross_cxx@
.DEFAULT_GOAL := all
CXX_MODGRAPH := @modgraph@
CXX_MODGRAPH_SOURCES := src/main.cpp
CXX_MODGRAPH_MODULE_PATHS := modules
CXX_MODGRAPH_CXXFLAGS := -std=c++23 -fmodules
CXX_MODGRAPH_BMI_DIRECTORY := build/mingw64/bmi
CXX_MODGRAPH_OBJECT_DIRECTORY := build/mingw64/obj
CXX_MODGRAPH_SCAN_DIRECTORY := build/mingw64/scan
CXX_MODGRAPH_COMPILATION_DATABASE := build/mingw64/compile_commands.json
CXX_MODGRAPH_RULES := build/mingw64/modules.mk
include @source_root@/adapters/make/cxx-modgraph.mk
.PHONY: all
all: make-hello.exe
make-hello.exe: $(CXX_MODGRAPH_LINK_OBJECTS)
	$(CXX) $^ -o $@
]=])
string(CONFIGURE "${makefile}" makefile @ONLY)
file(WRITE "${work_directory}/Makefile" "${makefile}")

execute_process(
    COMMAND "${make_program}" --no-print-directory
    WORKING_DIRECTORY "${work_directory}"
    RESULT_VARIABLE make_result
    OUTPUT_VARIABLE make_output
    ERROR_VARIABLE make_error
)
if(NOT make_result EQUAL 0)
    message(FATAL_ERROR "MinGW Make smoke test failed:\n${make_output}${make_error}")
endif()

execute_process(
    COMMAND "${modgraph}"
        --input build/mingw64/scan/modules/hello.p1689.json
        --input build/mingw64/scan/src/main.p1689.json
        --input-format p1689
        --compdb build/mingw64/compile_commands.json
        --source-root "${work_directory}"
        --bmi-dir ninja-build/bmi
        --bmi-extension .gcm
        --emit ninja
        --output modules.ninja
    WORKING_DIRECTORY "${work_directory}"
    RESULT_VARIABLE emit_result
    OUTPUT_VARIABLE emit_output
    ERROR_VARIABLE emit_error
)
if(NOT emit_result EQUAL 0)
    message(FATAL_ERROR "MinGW Ninja emission failed:\n${emit_output}${emit_error}")
endif()

# Force Ninja to compile every edge instead of reusing Make's objects.
file(REMOVE_RECURSE
    "${work_directory}/build/mingw64/obj"
    "${work_directory}/ninja-build")
set(ninja_file [=[
include @source_root@/adapters/ninja/gcc.ninja
cxx = @cross_cxx@
cxxflags = -std=c++23 -fmodules
include modules.ninja
rule link
  command = $cxx $in -o $out
build ninja-hello.exe: link build/mingw64/obj/modules/hello.o build/mingw64/obj/src/main.o
default ninja-hello.exe
]=])
string(CONFIGURE "${ninja_file}" ninja_file @ONLY)
file(WRITE "${work_directory}/build.ninja" "${ninja_file}")

execute_process(
    COMMAND "${ninja_program}" -v
    WORKING_DIRECTORY "${work_directory}"
    RESULT_VARIABLE ninja_result
    OUTPUT_VARIABLE ninja_output
    ERROR_VARIABLE ninja_error
)
if(NOT ninja_result EQUAL 0)
    message(FATAL_ERROR "MinGW Ninja smoke test failed:\n${ninja_output}${ninja_error}")
endif()

foreach(executable IN ITEMS make-hello.exe ninja-hello.exe)
    file(READ "${work_directory}/${executable}" magic LIMIT 2 HEX)
    string(TOLOWER "${magic}" magic)
    if(NOT magic STREQUAL "4d5a")
        message(FATAL_ERROR "${executable} is not a Windows PE executable")
    endif()
endforeach()
