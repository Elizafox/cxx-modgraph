// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/facts.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cxx_modgraph
{

struct IncrementalUpdate
{
    bool topology_changed = false;
    std::vector<NodeId> affected_translation_units;
    std::vector<std::vector<NodeId>> parallel_levels;
};

// In-memory state for an optional long-running frontend. The batch CLI does not use
// this type unless daemon mode is explicitly selected.
class IncrementalState
{
public:
    explicit IncrementalState(DependencyFacts facts = {});

    [[nodiscard]] const DependencyFacts &facts() const noexcept;
    [[nodiscard]] const AnalysisResult &analysis() const noexcept;
    [[nodiscard]] const std::map<std::string, std::string> &provider_index() const noexcept;
    [[nodiscard]] const std::map<NodeId, std::vector<NodeId>> &reverse_edges() const noexcept;

    IncrementalUpdate update(TranslationUnit unit, std::string p1689_digest = {},
                             std::string compile_command_digest = {});
    IncrementalUpdate erase(const std::filesystem::path &source_path);

    void cache_backend_fragment(std::string key, std::string fragment);
    [[nodiscard]] const std::string *backend_fragment(std::string_view key) const;
    [[nodiscard]] const std::map<std::string, std::string> &p1689_digests() const noexcept;
    [[nodiscard]] const std::map<std::string, std::string> &
    compile_command_digests() const noexcept;

private:
    IncrementalUpdate rebuild(std::set<NodeId> seeds, std::string old_topology);
    [[nodiscard]] std::string topology_key() const;

    DependencyFacts facts_;
    AnalysisResult analysis_;
    std::map<std::string, std::string> providers_;
    std::map<NodeId, std::vector<NodeId>> reverse_edges_;
    std::map<std::string, std::string> p1689_digests_;
    std::map<std::string, std::string> compile_command_digests_;
    std::map<std::string, std::string> backend_fragments_;
};

} // namespace cxx_modgraph
