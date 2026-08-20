// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/incremental.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace cxx_modgraph
{
namespace
{
std::string key(const std::filesystem::path &path)
{
    return path.lexically_normal().generic_string();
}
} // namespace

IncrementalState::IncrementalState(DependencyFacts facts) : facts_(std::move(facts))
{
    rebuild({}, {});
}

const DependencyFacts &IncrementalState::facts() const noexcept
{
    return facts_;
}
const AnalysisResult &IncrementalState::analysis() const noexcept
{
    return analysis_;
}
const std::map<std::string, std::string> &IncrementalState::provider_index() const noexcept
{
    return providers_;
}
const std::map<NodeId, std::vector<NodeId>> &IncrementalState::reverse_edges() const noexcept
{
    return reverse_edges_;
}
const std::map<std::string, std::string> &IncrementalState::p1689_digests() const noexcept
{
    return p1689_digests_;
}
const std::map<std::string, std::string> &IncrementalState::compile_command_digests() const noexcept
{
    return compile_command_digests_;
}

std::string IncrementalState::topology_key() const
{
    std::ostringstream out;
    for (const auto &unit : facts_.translation_units)
    {
        out << key(unit.source_path) << '\0' << unit.module_set << '\0';
        auto provides = unit.provides;
        auto requirements = unit.required_modules;
        std::ranges::sort(provides, {}, &ProvidedModule::name);
        std::ranges::sort(requirements);
        for (const auto &module : provides)
        {
            out << 'P' << module.name << '\0';
        }
        for (const auto &module : requirements)
        {
            out << 'R' << module << '\0';
        }
    }
    return out.str();
}

IncrementalUpdate IncrementalState::update(TranslationUnit unit, std::string p1689_digest,
                                           std::string compile_command_digest)
{
    const std::string source = key(unit.source_path);
    const std::string old_topology = topology_key();
    std::set<NodeId> seeds{source};
    for (const auto &[dependency, dependents] : reverse_edges_)
    {
        if (dependency == source)
        {
            seeds.insert(dependents.begin(), dependents.end());
        }
    }
    auto found = std::ranges::find_if(facts_.translation_units, [&](const auto &item)
                                      { return key(item.source_path) == source; });
    if (found == facts_.translation_units.end())
    {
        facts_.translation_units.push_back(std::move(unit));
    }
    else
    {
        *found = std::move(unit);
    }
    if (!p1689_digest.empty())
    {
        p1689_digests_[source] = std::move(p1689_digest);
    }
    if (!compile_command_digest.empty())
    {
        compile_command_digests_[source] = std::move(compile_command_digest);
    }
    return rebuild(std::move(seeds), old_topology);
}

IncrementalUpdate IncrementalState::erase(const std::filesystem::path &source_path)
{
    const std::string source = key(source_path);
    const std::string old_topology = topology_key();
    std::set<NodeId> seeds{source};
    if (auto it = reverse_edges_.find(source); it != reverse_edges_.end())
    {
        seeds.insert(it->second.begin(), it->second.end());
    }
    std::erase_if(facts_.translation_units,
                  [&](const auto &unit) { return key(unit.source_path) == source; });
    p1689_digests_.erase(source);
    compile_command_digests_.erase(source);
    return rebuild(std::move(seeds), old_topology);
}

IncrementalUpdate IncrementalState::rebuild(std::set<NodeId> affected, std::string old_topology)
{
    analysis_ = analyze(facts_);
    providers_.clear();
    reverse_edges_.clear();
    for (const auto &unit : facts_.translation_units)
    {
        for (const auto &module : unit.provides)
        {
            providers_[module.name] = key(unit.source_path);
        }
    }
    for (const auto &unit : facts_.translation_units)
    {
        for (const auto &required : unit.required_modules)
        {
            if (auto provider = providers_.find(required); provider != providers_.end())
            {
                reverse_edges_[provider->second].push_back(key(unit.source_path));
            }
        }
    }
    for (auto &[unused, values] : reverse_edges_)
    {
        std::ranges::sort(values);
        values.erase(std::unique(values.begin(), values.end()), values.end());
    }
    std::vector<NodeId> queue(affected.begin(), affected.end());
    for (std::size_t i = 0; i < queue.size(); ++i)
    {
        if (auto it = reverse_edges_.find(queue[i]); it != reverse_edges_.end())
        {
            for (const auto &dependent : it->second)
            {
                if (affected.insert(dependent).second)
                {
                    queue.push_back(dependent);
                }
            }
        }
    }
    IncrementalUpdate result;
    result.topology_changed = old_topology != topology_key();
    result.affected_translation_units.assign(affected.begin(), affected.end());
    result.parallel_levels = analysis_.graph.topological_sort().plan.parallel_levels;
    return result;
}

void IncrementalState::cache_backend_fragment(std::string key, std::string fragment)
{
    backend_fragments_[std::move(key)] = std::move(fragment);
}
const std::string *IncrementalState::backend_fragment(std::string_view key) const
{
    auto found = backend_fragments_.find(std::string(key));
    return found == backend_fragments_.end() ? nullptr : &found->second;
}

} // namespace cxx_modgraph
