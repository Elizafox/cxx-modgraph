# SPDX-License-Identifier: 0BSD

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY "${work_directory}")

set(fragment "${work_directory}/modules.mk")
execute_process(
    COMMAND "${modgraph}" --input "${input}" --emit make --output "${fragment}"
    RESULT_VARIABLE emit_result
    OUTPUT_VARIABLE emit_output
    ERROR_VARIABLE emit_error
)
if(NOT emit_result EQUAL 0)
    message(FATAL_ERROR "Make emission failed:\n${emit_output}${emit_error}")
endif()

set(wrapper "${work_directory}/Makefile")
file(WRITE "${wrapper}"
    "include ${fragment}\n"
    ".PHONY: check\n"
    "check:\n"
    "\t@:\n"
)

execute_process(
    COMMAND "${make_program}" --no-print-directory -f "${wrapper}" -n check
    RESULT_VARIABLE make_result
    OUTPUT_VARIABLE make_output
    ERROR_VARIABLE make_error
)
if(NOT make_result EQUAL 0)
    message(FATAL_ERROR "GNU Make rejected the emitted fragment:\n${make_output}${make_error}")
endif()
