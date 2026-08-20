// SPDX-License-Identifier: 0BSD
#pragma once

#include "cxx_modgraph/facts.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace cxx_modgraph
{

[[nodiscard]] std::filesystem::path remap_path(const std::filesystem::path &path,
                                               const std::vector<PathRemapping> &remappings);
[[nodiscard]] std::string content_digest(std::string_view contents);
// Hashes canonical facts with graph_digest cleared, making the result stable and self-independent.
[[nodiscard]] std::string graph_record_digest(const DependencyFacts &facts);

} // namespace cxx_modgraph
