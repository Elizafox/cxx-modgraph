# SPDX-License-Identifier: 0BSD

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY
    "${work_directory}/examples"
    "${work_directory}/adapters"
)
file(COPY "${source_root}/examples/make-hello"
    DESTINATION "${work_directory}/examples")
file(COPY "${source_root}/adapters/make"
    DESTINATION "${work_directory}/adapters")

set(example_directory "${work_directory}/examples/make-hello")
execute_process(
    COMMAND "${make_program}" --no-print-directory
        CXX=${compiler}
        CXX_MODGRAPH=${modgraph}
    WORKING_DIRECTORY "${example_directory}"
    RESULT_VARIABLE make_result
    OUTPUT_VARIABLE make_output
    ERROR_VARIABLE make_error
)
if(NOT make_result EQUAL 0)
    message(FATAL_ERROR "Make hello example failed:\n${make_output}${make_error}")
endif()

execute_process(
    COMMAND "${example_directory}/hello"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Make hello executable failed:\n${run_output}${run_error}")
endif()
if(NOT run_output STREQUAL "Hello, world!\n")
    message(FATAL_ERROR "Unexpected Make hello output: ${run_output}")
endif()
