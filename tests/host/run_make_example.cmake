# SPDX-License-Identifier: 0BSD

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY
    "${work_directory}/examples"
    "${work_directory}/adapters"
)
file(COPY
    "${source_root}/examples/make-hello-simple"
    "${source_root}/examples/make-hello-complex"
    DESTINATION "${work_directory}/examples")
file(COPY "${source_root}/adapters/make"
    DESTINATION "${work_directory}/adapters")

foreach(example IN ITEMS simple complex)
    set(example_directory "${work_directory}/examples/make-hello-${example}")
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
        message(FATAL_ERROR
            "Make hello ${example} example failed:\n${make_output}${make_error}")
    endif()

    execute_process(
        COMMAND "${example_directory}/hello"
        RESULT_VARIABLE run_result
        OUTPUT_VARIABLE run_output
        ERROR_VARIABLE run_error
    )
    if(NOT run_result EQUAL 0)
        message(FATAL_ERROR
            "Make hello ${example} executable failed:\n${run_output}${run_error}")
    endif()
    if(example STREQUAL "simple")
        set(expected_output "Hello, world!\n")
    else()
        set(expected_output "[demo] Hello, module graph!\n")
    endif()
    if(NOT run_output STREQUAL expected_output)
        message(FATAL_ERROR
            "Unexpected Make hello ${example} output: ${run_output}")
    endif()
endforeach()
