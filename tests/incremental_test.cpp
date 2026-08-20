// SPDX-License-Identifier: 0BSD
#include "cxx_modgraph/incremental.hpp"
#include <cstdlib>
#include <iostream>
#include <string_view>

void require(bool value, std::string_view message)
{
    if (!value)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

int main()
{
    cxx_modgraph::DependencyFacts facts;
    facts.translation_units = {
        {.source_path = "a.cppm", .object_path = "a.o", .provides = {{"a", "a.pcm"}}},
        {.source_path = "b.cpp", .object_path = "b.o", .required_modules = {"a"}}};
    cxx_modgraph::IncrementalState state(facts);
    auto unchanged = state.update(
        {.source_path = "a.cppm", .object_path = "new-a.o", .provides = {{"a", "a.pcm"}}}, "scan:1",
        "command:1");
    require(!unchanged.topology_changed, "object-only update changed topology");
    require(unchanged.affected_translation_units.size() == 2,
            "reverse dependent was not invalidated");
    auto changed = state.update(
        {.source_path = "a.cppm", .object_path = "new-a.o", .provides = {{"renamed", "a.pcm"}}});
    require(changed.topology_changed, "provider rename did not change topology");
    require(state.p1689_digests().at("a.cppm") == "scan:1", "scan digest was not cached");
    state.cache_backend_fragment("ninja", "fragment");
    require(*state.backend_fragment("ninja") == "fragment", "backend fragment was not cached");
}
