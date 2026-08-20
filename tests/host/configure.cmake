# SPDX-License-Identifier: 0BSD

set(host_test_reason "the configured compiler is not Clang")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libc++.modules.json
        OUTPUT_VARIABLE libcxx_modules_manifest
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(EXISTS "${libcxx_modules_manifest}")
        file(READ "${libcxx_modules_manifest}" libcxx_modules_json)
        string(JSON module_count ERROR_VARIABLE manifest_error
            LENGTH "${libcxx_modules_json}" modules)

        if(NOT manifest_error AND module_count GREATER 0)
            get_filename_component(manifest_directory "${libcxx_modules_manifest}" DIRECTORY)
            math(EXPR last_module_index "${module_count} - 1")
            foreach(module_index RANGE ${last_module_index})
                string(JSON module_name GET "${libcxx_modules_json}"
                    modules ${module_index} logical-name)
                string(JSON module_source GET "${libcxx_modules_json}"
                    modules ${module_index} source-path)
                cmake_path(ABSOLUTE_PATH module_source
                    BASE_DIRECTORY "${manifest_directory}"
                    NORMALIZE
                )
                if(module_name STREQUAL "std")
                    set(host_std_source "${module_source}")
                elseif(module_name STREQUAL "std.compat")
                    set(host_std_compat_source "${module_source}")
                endif()
            endforeach()
        endif()

        if(EXISTS "${host_std_source}" AND EXISTS "${host_std_compat_source}")
            set(host_test_directory "${CMAKE_CURRENT_BINARY_DIR}/host-std-modules")
            file(MAKE_DIRECTORY "${host_test_directory}")
            configure_file(
                ${CMAKE_CURRENT_SOURCE_DIR}/host/std-modules.json.in
                ${host_test_directory}/std-modules.json
                @ONLY
            )
            add_test(
                NAME host-std-modules
                COMMAND ${CMAKE_COMMAND} -E env
                    CCACHE_DIR=${host_test_directory}/ccache
                    ${CMAKE_COMMAND}
                    -Dcompiler=${CMAKE_CXX_COMPILER}
                    -Dmodgraph=$<TARGET_FILE:cxx-modgraph>
                    -Dfacts=${host_test_directory}/std-modules.json
                    -Dstd_source=${host_std_source}
                    -Dstd_compat_source=${host_std_compat_source}
                    -Dconsumer_source=${CMAKE_CURRENT_SOURCE_DIR}/host/std_consumer.cpp
                    -Dwork_directory=${host_test_directory}/work
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/host/run.cmake
            )
            find_program(host_make_program NAMES gmake make)
            if(host_make_program)
                execute_process(
                    COMMAND ${host_make_program} --version
                    OUTPUT_VARIABLE host_make_version
                    ERROR_QUIET
                )
                if(host_make_version MATCHES "GNU Make")
                    add_test(
                        NAME host-make-hello
                        COMMAND ${CMAKE_COMMAND} -E env
                            CCACHE_DIR=${host_test_directory}/make-ccache
                            ${CMAKE_COMMAND}
                            -Dcompiler=${CMAKE_CXX_COMPILER}
                            -Dmake_program=${host_make_program}
                            -Dmodgraph=$<TARGET_FILE:cxx-modgraph>
                            -Dsource_root=${PROJECT_SOURCE_DIR}
                            -Dwork_directory=${host_test_directory}/make-example
                            -P ${CMAKE_CURRENT_SOURCE_DIR}/host/run_make_example.cmake
                    )
                endif()
            endif()
            set(host_test_configured TRUE)
        else()
            set(host_test_reason "libc++ manifest does not provide std and std.compat sources")
        endif()
    else()
        set(host_test_reason "the configured Clang does not expose libc++.modules.json")
    endif()
endif()

if(NOT host_test_configured)
    add_test(
        NAME host-std-modules
        COMMAND ${CMAKE_COMMAND} -E echo "SKIP: ${host_test_reason}"
    )
    set_tests_properties(host-std-modules PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP:")
endif()

find_program(host_gcc_compiler NAMES g++-16 g++)
find_program(host_gcc_make_program NAMES gmake make)
if(host_gcc_compiler AND host_gcc_make_program)
    execute_process(
        COMMAND ${host_gcc_compiler} -dumpfullversion
        OUTPUT_VARIABLE host_gcc_version
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(host_gcc_version VERSION_GREATER_EQUAL 16)
    add_test(
        NAME host-make-gcc
        COMMAND ${CMAKE_COMMAND} -E env
            CCACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/host-make-gcc/ccache
            ${CMAKE_COMMAND}
            -Dcompiler=${host_gcc_compiler}
            -Dmake_program=${host_gcc_make_program}
            -Dmodgraph=$<TARGET_FILE:cxx-modgraph>
            -Dsource_root=${PROJECT_SOURCE_DIR}
            -Dwork_directory=${CMAKE_CURRENT_BINARY_DIR}/host-make-gcc/work
            -P ${CMAKE_CURRENT_SOURCE_DIR}/host/run_make_gcc_example.cmake
    )
else()
    add_test(
        NAME host-make-gcc
        COMMAND ${CMAKE_COMMAND} -E echo "SKIP: GCC 16 or newer is unavailable"
    )
    set_tests_properties(host-make-gcc PROPERTIES SKIP_REGULAR_EXPRESSION "^SKIP:")
endif()
