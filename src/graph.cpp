// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/graph.hpp"

#include <utility>

namespace cxx_modgraph
{

bool SortResult::has_cycle() const noexcept
{
    return !remaining_nodes.empty();
}

void DependencyGraph::add_node(NodeId node)
{
    dependents_.try_emplace(std::move(node));
}

void DependencyGraph::add_dependency(const NodeId &dependent, const NodeId &dependency)
{
    add_node(dependent);
    add_node(dependency);
    dependents_.at(dependency).insert(dependent);
}

bool DependencyGraph::contains(const NodeId &node) const
{
    return dependents_.contains(node);
}

std::size_t DependencyGraph::size() const noexcept
{
    return dependents_.size();
}

SortResult DependencyGraph::topological_sort() const
{
    std::map<NodeId, std::size_t> indegrees;
    for (const auto &[node, unused] : dependents_)
    {
        static_cast<void>(unused);
        indegrees.emplace(node, 0);
    }

    for (const auto &[unused, dependents] : dependents_)
    {
        static_cast<void>(unused);
        for (const NodeId &dependent : dependents)
        {
            ++indegrees.at(dependent);
        }
    }

    std::set<NodeId> ready;
    for (const auto &[node, indegree] : indegrees)
    {
        if (indegree == 0)
        {
            ready.insert(node);
        }
    }

    SortResult result;
    while (!ready.empty())
    {
        std::vector<NodeId> level(ready.begin(), ready.end());
        ready.clear();

        for (const NodeId &node : level)
        {
            result.plan.topological_order.push_back(node);
            for (const NodeId &dependent : dependents_.at(node))
            {
                if (--indegrees.at(dependent) == 0)
                {
                    ready.insert(dependent);
                }
            }
        }

        result.plan.parallel_levels.push_back(std::move(level));
    }

    for (const auto &[node, indegree] : indegrees)
    {
        if (indegree != 0)
        {
            result.remaining_nodes.push_back(node);
        }
    }

    return result;
}

} // namespace cxx_modgraph
