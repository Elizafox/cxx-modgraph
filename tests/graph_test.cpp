// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/graph.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{

void require(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void sorts_dependencies_into_deterministic_levels()
{
    cxx_modgraph::DependencyGraph graph;
    graph.add_dependency("geometry", "math");
    graph.add_dependency("app.core", "geometry");
    graph.add_dependency("app.core", "logging");

    const cxx_modgraph::SortResult result = graph.topological_sort();

    require(!result.has_cycle(), "acyclic graph was reported as cyclic");
    require(result.plan.parallel_levels ==
                std::vector<std::vector<cxx_modgraph::NodeId>>{
                    {"logging", "math"}, {"geometry"}, {"app.core"}},
            "parallel levels were incorrect");
    require(result.plan.topological_order ==
                std::vector<cxx_modgraph::NodeId>{"logging", "math", "geometry", "app.core"},
            "topological order was incorrect");
}

void reports_nodes_remaining_in_a_cycle()
{
    cxx_modgraph::DependencyGraph graph;
    graph.add_dependency("a", "b");
    graph.add_dependency("b", "a");
    graph.add_dependency("consumer", "a");

    const cxx_modgraph::SortResult result = graph.topological_sort();

    require(result.has_cycle(), "cycle was not reported");
    require(result.remaining_nodes == std::vector<cxx_modgraph::NodeId>{"a", "b", "consumer"},
            "nodes remaining after cycle detection were incorrect");
    require(graph.cycle_witness() == std::vector<cxx_modgraph::NodeId>({"a", "b", "a"}),
            "concrete cycle witness was incorrect");
}

void finds_deterministic_critical_path()
{
    cxx_modgraph::DependencyGraph graph;
    graph.add_dependency("b", "a");
    graph.add_dependency("c", "b");
    graph.add_dependency("d", "a");
    require(graph.critical_path() == std::vector<cxx_modgraph::NodeId>({"a", "b", "c"}),
            "critical path was incorrect");
}

} // namespace

int main()
{
    sorts_dependencies_into_deterministic_levels();
    reports_nodes_remaining_in_a_cycle();
    finds_deterministic_critical_path();
}
