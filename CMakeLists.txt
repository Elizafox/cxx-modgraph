# SPDX-License-Identifier: 0BSD

cmake_minimum_required(VERSION 3.20)

project(
    cxx-modgraph
    VERSION 0.1.0
    DESCRIPTION "Process C++ module dependency data for build systems"
    LANGUAGES CXX
)

option(CXX_MODGRAPH_BUILD_TESTS "Build cxx-modgraph tests" ON)
option(CXX_MODGRAPH_BUILD_HOST_INTEGRATION_TESTS
    "Build optional tests using host C++ standard library modules"
    OFF
)
option(CXX_MODGRAPH_USE_BUNDLED_NLOHMANN_JSON
    "Use the bundled nlohmann/json even when a system package is available"
    OFF
)

if(NOT CXX_MODGRAPH_USE_BUNDLED_NLOHMANN_JSON)
    find_package(nlohmann_json 3.11.3 QUIET)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(vendor/nlohmann-json EXCLUDE_FROM_ALL)
endif()

add_library(cxx-modgraph-lib
    src/facts.cpp
    src/graph.cpp
    src/json.cpp
    src/json_reader.cpp
    src/make.cpp
    src/ninja.cpp
    src/p1689.cpp
)

add_library(cxx-modgraph::lib ALIAS cxx-modgraph-lib)

target_compile_features(cxx-modgraph-lib PUBLIC cxx_std_20)
target_link_libraries(cxx-modgraph-lib PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(cxx-modgraph-lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
set_target_properties(cxx-modgraph-lib PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME cxx-modgraph
)

add_executable(cxx-modgraph
    src/main.cpp
)

target_compile_features(cxx-modgraph PRIVATE cxx_std_20)
target_link_libraries(cxx-modgraph PRIVATE cxx-modgraph::lib)

if(CXX_MODGRAPH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
