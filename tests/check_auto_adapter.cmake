# SPDX-License-Identifier: 0BSD

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY "${work_directory}")
file(WRITE "${work_directory}/Makefile" [=[
include __CXX_MODGRAPH_ADAPTER__
.PHONY: clean
clean:
	@echo selected=$(CXX_MODGRAPH_COMPILER)
]=])
file(READ "${work_directory}/Makefile" makefile)
string(REPLACE "__CXX_MODGRAPH_ADAPTER__" "${adapter}" makefile "${makefile}")
file(WRITE "${work_directory}/Makefile" "${makefile}")

function(check_selection compiler expected)
    execute_process(
        COMMAND "${make_program}" --no-print-directory clean "CXX=${compiler}"
        WORKING_DIRECTORY "${work_directory}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0 OR NOT output STREQUAL "selected=${expected}\n")
        message(FATAL_ERROR
            "Adapter did not select ${expected} for ${compiler}:\n${output}${error}")
    endif()
endfunction()

check_selection("${clang_compiler}" clang)
check_selection("${gcc_compiler}" gcc)

execute_process(
    COMMAND "${make_program}" --no-print-directory clean
        CXX=/bin/false CXX_MODGRAPH_COMPILER=gcc
    WORKING_DIRECTORY "${work_directory}"
    RESULT_VARIABLE override_result
    OUTPUT_VARIABLE override_output
    ERROR_VARIABLE override_error
)
if(NOT override_result EQUAL 0 OR NOT override_output STREQUAL "selected=gcc\n")
    message(FATAL_ERROR "Explicit adapter override failed:\n${override_output}${override_error}")
endif()

execute_process(
    COMMAND "${make_program}" --no-print-directory clean CXX=/bin/false
    WORKING_DIRECTORY "${work_directory}"
    RESULT_VARIABLE unsupported_result
    OUTPUT_VARIABLE unsupported_output
    ERROR_VARIABLE unsupported_error
)
if(unsupported_result EQUAL 0 OR
   NOT unsupported_error MATCHES "unable to identify CXX='/bin/false'")
    message(FATAL_ERROR
        "Unsupported compiler was not diagnosed:\n${unsupported_output}${unsupported_error}")
endif()
