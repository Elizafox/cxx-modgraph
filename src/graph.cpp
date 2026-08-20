// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/graph.hpp"

#include <algorithm>
#include <queue>
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

const std::map<NodeId, std::set<NodeId>> &DependencyGraph::dependents() const noexcept
{
    return dependents_;
}

std::vector<NodeId> DependencyGraph::cycle_witness() const
{
    std::vector<NodeId> best;
    for (const auto &[start, unused] : dependents_)
    {
        static_cast<void>(unused);
        std::queue<std::vector<NodeId>> pending;
        pending.push({start});
        std::map<NodeId, std::size_t> shortest{{start, 0}};
        while (!pending.empty())
        {
            std::vector<NodeId> path = std::move(pending.front());
            pending.pop();
            if (!best.empty() && path.size() + 1 > best.size())
                continue;
            for (const NodeId &next : dependents_.at(path.back()))
            {
                if (next == start)
                {
                    auto candidate = path;
                    candidate.push_back(start);
                    if (best.empty() || candidate.size() < best.size() ||
                        (candidate.size() == best.size() && candidate < best))
                        best = std::move(candidate);
                    continue;
                }
                const std::size_t distance = path.size();
                const auto found = shortest.find(next);
                if (found != shortest.end() && found->second <= distance)
                    continue;
                shortest[next] = distance;
                auto candidate = path;
                candidate.push_back(next);
                pending.push(std::move(candidate));
            }
        }
    }
    return best;
}

std::vector<NodeId> DependencyGraph::critical_path() const
{
    const SortResult sorted = topological_sort();
    if (sorted.has_cycle())
        return {};
    std::map<NodeId, std::vector<NodeId>> best;
    for (const NodeId &node : sorted.plan.topological_order)
    {
        if (best[node].empty())
            best[node] = {node};
        for (const NodeId &next : dependents_.at(node))
        {
            auto candidate = best[node];
            candidate.push_back(next);
            if (candidate.size() > best[next].size() ||
                (candidate.size() == best[next].size() && candidate < best[next]))
                best[next] = std::move(candidate);
        }
    }
    std::vector<NodeId> result;
    for (const auto &[unused, path] : best)
        if (path.size() > result.size() || (path.size() == result.size() && path < result))
            result = path;
    return result;
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
