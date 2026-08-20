// SPDX-License-Identifier: 0BSD

#pragma once

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace cxx_modgraph
{

using NodeId = std::string;

struct BuildPlan
{
    std::vector<NodeId> topological_order;
    std::vector<std::vector<NodeId>> parallel_levels;
};

struct SortResult
{
    BuildPlan plan;
    std::vector<NodeId> remaining_nodes;

    [[nodiscard]] bool has_cycle() const noexcept;
};

class DependencyGraph
{
public:
    void add_node(NodeId node);
    void add_dependency(const NodeId &dependent, const NodeId &dependency);

    [[nodiscard]] bool contains(const NodeId &node) const;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] SortResult topological_sort() const;
    [[nodiscard]] std::vector<NodeId> cycle_witness() const;
    [[nodiscard]] std::vector<NodeId> critical_path() const;
    [[nodiscard]] const std::map<NodeId, std::set<NodeId>> &dependents() const noexcept;

private:
    // Edges point from a prerequisite to the nodes that depend on it.
    std::map<NodeId, std::set<NodeId>> dependents_;
};

} // namespace cxx_modgraph
