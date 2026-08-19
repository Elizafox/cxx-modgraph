# SPDX-License-Identifier: 0BSD

file(REMOVE_RECURSE "${work_directory}")
file(MAKE_DIRECTORY "${work_directory}")

function(run_checked description)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${description} failed:\n${output}${error}")
    endif()
endfunction()

run_checked("normalizing host module facts"
    "${modgraph}" --input "${facts}" --emit json
    --output "${work_directory}/normalized.json"
)

run_checked("precompiling std"
    "${compiler}" -std=c++23 -stdlib=libc++ --precompile
    "${std_source}" -o "${work_directory}/std.pcm"
)
run_checked("precompiling std.compat"
    "${compiler}" -std=c++23 -stdlib=libc++ --precompile
    -fmodule-file=std=${work_directory}/std.pcm
    "${std_compat_source}" -o "${work_directory}/std.compat.pcm"
)
run_checked("compiling std module object"
    "${compiler}" -std=c++23 -stdlib=libc++ -c
    "${work_directory}/std.pcm" -o "${work_directory}/std.o"
)
run_checked("compiling std.compat module object"
    "${compiler}" -std=c++23 -stdlib=libc++ -c
    -fmodule-file=std=${work_directory}/std.pcm
    "${work_directory}/std.compat.pcm" -o "${work_directory}/std.compat.o"
)
run_checked("compiling standard module consumer"
    "${compiler}" -std=c++23 -stdlib=libc++ -c
    -fmodule-file=std=${work_directory}/std.pcm
    -fmodule-file=std.compat=${work_directory}/std.compat.pcm
    "${consumer_source}" -o "${work_directory}/consumer.o"
)
run_checked("linking standard module consumer"
    "${compiler}" -std=c++23 -stdlib=libc++
    "${work_directory}/std.o"
    "${work_directory}/std.compat.o"
    "${work_directory}/consumer.o"
    -o "${work_directory}/consumer"
)
run_checked("running standard module consumer" "${work_directory}/consumer")
