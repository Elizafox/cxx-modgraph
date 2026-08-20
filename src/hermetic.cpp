// SPDX-License-Identifier: 0BSD
#include "cxx_modgraph/hermetic.hpp"
#include "cxx_modgraph/json.hpp"

#include <iomanip>
#include <sstream>

namespace cxx_modgraph
{

std::filesystem::path remap_path(const std::filesystem::path &path,
                                 const std::vector<PathRemapping> &remappings)
{
    const auto normalized = path.lexically_normal();
    const PathRemapping *best = nullptr;
    std::size_t best_size = 0;
    for (const auto &mapping : remappings)
    {
        const auto from = mapping.from.lexically_normal();
        auto relative = normalized.lexically_relative(from);
        if (!relative.empty() && *relative.begin() != ".." && from.native().size() >= best_size)
        {
            best = &mapping;
            best_size = from.native().size();
        }
    }

    if (!best)
    {
        return normalized;
    }

    return (best->to / normalized.lexically_relative(best->from.lexically_normal()))
        .lexically_normal();
}

std::string content_digest(std::string_view value)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : value)
    {
        hash ^= c;
        hash *= 1099511628211ull;
    }

    std::ostringstream out;
    out << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;

    return out.str();
}

std::string graph_record_digest(const DependencyFacts &facts)
{
    auto record = facts;
    record.graph_digest.clear();
    return content_digest(to_json(record));
}

} // namespace cxx_modgraph
