# SPDX-License-Identifier: 0BSD

add_library(cxx-modgraph-nlohmann-json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS cxx-modgraph-nlohmann-json)

target_include_directories(cxx-modgraph-nlohmann-json
    SYSTEM INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
