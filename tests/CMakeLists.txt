# SPDX-License-Identifier: 0BSD

add_executable(cxx-modgraph-graph-tests
    graph_test.cpp
)

target_compile_features(cxx-modgraph-graph-tests PRIVATE cxx_std_20)
target_link_libraries(cxx-modgraph-graph-tests PRIVATE cxx-modgraph::lib)

add_test(NAME graph COMMAND cxx-modgraph-graph-tests)

add_executable(cxx-modgraph-facts-tests
    facts_test.cpp
)

target_compile_features(cxx-modgraph-facts-tests PRIVATE cxx_std_20)
target_link_libraries(cxx-modgraph-facts-tests PRIVATE cxx-modgraph::lib)

add_test(NAME facts COMMAND cxx-modgraph-facts-tests)

add_executable(cxx-modgraph-make-tests
    make_test.cpp
)

target_compile_features(cxx-modgraph-make-tests PRIVATE cxx_std_20)
target_link_libraries(cxx-modgraph-make-tests PRIVATE cxx-modgraph::lib)

add_test(NAME make-emitter COMMAND cxx-modgraph-make-tests)

add_executable(cxx-modgraph-ninja-tests
    ninja_test.cpp
)

target_compile_features(cxx-modgraph-ninja-tests PRIVATE cxx_std_20)
target_link_libraries(cxx-modgraph-ninja-tests PRIVATE cxx-modgraph::lib)

add_test(NAME ninja-emitter COMMAND cxx-modgraph-ninja-tests)

add_executable(cxx-modgraph-p1689-tests
    p1689_test.cpp
)

target_compile_features(cxx-modgraph-p1689-tests PRIVATE cxx_std_20)
target_link_libraries(cxx-modgraph-p1689-tests PRIVATE cxx-modgraph::lib)

add_test(NAME p1689-import COMMAND cxx-modgraph-p1689-tests)

add_test(
    NAME cli-normalize
    COMMAND cxx-modgraph
        --input ${CMAKE_CURRENT_SOURCE_DIR}/data/valid-facts.json
        --emit json
)

add_test(
    NAME cli-reject-invalid
    COMMAND cxx-modgraph
        --input ${CMAKE_CURRENT_SOURCE_DIR}/data/invalid-facts.json
        --emit json
)
set_tests_properties(cli-reject-invalid PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME cli-std-modules
    COMMAND cxx-modgraph
        --input ${CMAKE_CURRENT_SOURCE_DIR}/data/std-modules.json
        --emit json
)

add_test(
    NAME cli-emit-make
    COMMAND cxx-modgraph
        --input ${CMAKE_CURRENT_SOURCE_DIR}/data/std-modules.json
        --emit make
)

add_test(
    NAME cli-emit-ninja
    COMMAND cxx-modgraph
        --input ${CMAKE_CURRENT_SOURCE_DIR}/data/std-modules.json
        --emit ninja
)

find_program(CXX_MODGRAPH_NINJA_EXECUTABLE NAMES ninja ninja-build)
if(CXX_MODGRAPH_NINJA_EXECUTABLE)
    add_test(
        NAME ninja-include
        COMMAND ${CMAKE_COMMAND}
            -Dninja_program=${CXX_MODGRAPH_NINJA_EXECUTABLE}
            -Dmodgraph=$<TARGET_FILE:cxx-modgraph>
            -Dinput=${CMAKE_CURRENT_SOURCE_DIR}/data/std-modules.json
            -Dadapter=${PROJECT_SOURCE_DIR}/adapters/ninja/clang.ninja
            -Dwork_directory=${CMAKE_CURRENT_BINARY_DIR}/ninja-include
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ninja.cmake
    )
endif()

add_test(
    NAME cli-p1689-to-make
    COMMAND cxx-modgraph
        --input ${CMAKE_CURRENT_SOURCE_DIR}/data/modules.p1689.json
        --input-format p1689
        --compdb ${CMAKE_CURRENT_SOURCE_DIR}/data/compile_commands.json
        --external-module std=build/bmi/std.pcm
        --emit make
)

find_program(CXX_MODGRAPH_MAKE_EXECUTABLE NAMES gmake make)
if(CXX_MODGRAPH_MAKE_EXECUTABLE)
    execute_process(
        COMMAND ${CXX_MODGRAPH_MAKE_EXECUTABLE} --version
        OUTPUT_VARIABLE make_version
        ERROR_QUIET
    )
    if(make_version MATCHES "GNU Make")
        add_test(
            NAME make-include
            COMMAND ${CMAKE_COMMAND}
                -Dmake_program=${CXX_MODGRAPH_MAKE_EXECUTABLE}
                -Dmodgraph=$<TARGET_FILE:cxx-modgraph>
                -Dinput=${CMAKE_CURRENT_SOURCE_DIR}/data/std-modules.json
                -Dwork_directory=${CMAKE_CURRENT_BINARY_DIR}/make-include
                -P ${CMAKE_CURRENT_SOURCE_DIR}/check_make.cmake
        )
        add_test(
            NAME make-adapter-discovery
            COMMAND ${CMAKE_COMMAND}
                -Dmake_program=${CXX_MODGRAPH_MAKE_EXECUTABLE}
                -Dadapter=${PROJECT_SOURCE_DIR}/adapters/make/clang.mk
                -Dwork_directory=${CMAKE_CURRENT_BINARY_DIR}/make-adapter
                -P ${CMAKE_CURRENT_SOURCE_DIR}/check_make_adapter.cmake
        )
        find_program(CXX_MODGRAPH_TEST_CLANGXX NAMES clang++)
        find_program(CXX_MODGRAPH_TEST_GXX NAMES g++)
        if(CXX_MODGRAPH_TEST_CLANGXX AND CXX_MODGRAPH_TEST_GXX)
            add_test(
                NAME make-adapter-auto-detect
                COMMAND ${CMAKE_COMMAND}
                    -Dmake_program=${CXX_MODGRAPH_MAKE_EXECUTABLE}
                    -Dadapter=${PROJECT_SOURCE_DIR}/adapters/make/cxx-modgraph.mk
                    -Dclang_compiler=${CXX_MODGRAPH_TEST_CLANGXX}
                    -Dgcc_compiler=${CXX_MODGRAPH_TEST_GXX}
                    -Dwork_directory=${CMAKE_CURRENT_BINARY_DIR}/make-adapter-auto
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/check_auto_adapter.cmake
            )
        endif()
    endif()
endif()

if(CXX_MODGRAPH_BUILD_HOST_INTEGRATION_TESTS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/host/configure.cmake)
endif()
