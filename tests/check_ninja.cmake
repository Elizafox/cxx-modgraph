# SPDX-License-Identifier: 0BSD

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY "${work_directory}")

set(fragment "${work_directory}/modules.ninja")
execute_process(
    COMMAND "${modgraph}" --input "${input}" --emit ninja --output "${fragment}"
    RESULT_VARIABLE emit_result
    OUTPUT_VARIABLE emit_output
    ERROR_VARIABLE emit_error
)
if(NOT emit_result EQUAL 0)
    message(FATAL_ERROR "Ninja emission failed:\n${emit_output}${emit_error}")
endif()

set(wrapper "${work_directory}/build.ninja")
file(WRITE "${wrapper}"
    "include ${adapter}\n"
    "include ${fragment}\n"
    "default cxx_modgraph_outputs\n"
)
execute_process(
    COMMAND "${ninja_program}" -f "${wrapper}" -t targets
    RESULT_VARIABLE ninja_result
    OUTPUT_VARIABLE ninja_output
    ERROR_VARIABLE ninja_error
)
if(NOT ninja_result EQUAL 0)
    message(FATAL_ERROR "Ninja rejected the emitted fragment:\n${ninja_output}${ninja_error}")
endif()
