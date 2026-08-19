// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/facts.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cxx_modgraph
{

void write_json(std::ostream &output, const DependencyFacts &facts);
[[nodiscard]] std::string to_json(const DependencyFacts &facts);

struct JsonError
{
    std::size_t line;
    std::size_t column;
    std::string message;
};

struct JsonParseResult
{
    std::optional<DependencyFacts> facts;
    std::vector<JsonError> errors;

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] JsonParseResult parse_json(std::string_view input);
[[nodiscard]] JsonParseResult read_json(std::istream &input);

} // namespace cxx_modgraph
