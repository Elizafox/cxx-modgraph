# SPDX-License-Identifier: 0BSD

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY
    "${work_directory}/modules/one"
    "${work_directory}/modules/two"
    "${work_directory}/src/one"
    "${work_directory}/src/two"
    "${work_directory}/build")
file(WRITE "${work_directory}/modules/one/shared.cppm" "export module one.shared;\n")
file(WRITE "${work_directory}/modules/two/shared.cppm" "export module two.shared;\n")
file(WRITE "${work_directory}/src/one/main.cpp" "int one();\n")
file(WRITE "${work_directory}/src/two/main.cpp" "int two();\n")
file(WRITE "${work_directory}/build/modules.mk" "")
file(WRITE "${work_directory}/Makefile" [=[
CXX_MODGRAPH_RULES := build/modules.mk
CXX_MODGRAPH_COMPILATION_DATABASE := build/compile_commands.json
CXX_MODGRAPH_MODULE_PATHS := modules
CXX_MODGRAPH_SOURCES := src/one/main.cpp src/two/main.cpp
include @adapter@
.PHONY: clean
clean: build/compile_commands.json
]=])
file(READ "${work_directory}/Makefile" makefile)
string(REPLACE "@adapter@" "${adapter}" makefile "${makefile}")
file(WRITE "${work_directory}/Makefile" "${makefile}")

execute_process(
    COMMAND "${make_program}" clean
    WORKING_DIRECTORY "${work_directory}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Make adapter fixture failed:\n${output}${error}")
endif()

file(READ "${work_directory}/build/compile_commands.json" compdb)
foreach(expected IN ITEMS
    "modules/one/shared.cppm"
    "modules/two/shared.cppm"
    "build/obj/modules/one/shared.o"
    "build/obj/modules/two/shared.o"
    "build/obj/src/one/main.o"
    "build/obj/src/two/main.o")
    string(FIND "${compdb}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Compilation database does not contain '${expected}':\n${compdb}")
    endif()
endforeach()
