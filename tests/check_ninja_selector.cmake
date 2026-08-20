# SPDX-License-Identifier: 0BSD

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY "${work_directory}")

foreach(compiler IN ITEMS clang gcc)
    file(WRITE "${work_directory}/${compiler}.ninja"
        "cxx_modgraph_compiler = ${compiler}\n"
        "cxx_modgraph_adapter_directory = ${adapter_directory}\n"
        "include ${adapter_directory}/cxx-modgraph.ninja\n"
        "build probe: cxx_modgraph_object source.cpp\n"
    )
    execute_process(
        COMMAND "${ninja_program}" -f "${work_directory}/${compiler}.ninja" -t targets
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Ninja ${compiler} selector failed:\n${output}${error}")
    endif()
endforeach()
