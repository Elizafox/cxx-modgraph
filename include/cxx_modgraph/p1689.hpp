// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/facts.hpp"
#include "cxx_modgraph/json.hpp"

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace cxx_modgraph
{

struct P1689ImportOptions
{
    std::filesystem::path source_root = ".";
    std::filesystem::path bmi_directory = "build/bmi";
    std::vector<ExternalModule> external_modules;
};

struct P1689ImportResult
{
    std::optional<DependencyFacts> facts;
    std::vector<JsonError> errors;

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] P1689ImportResult import_p1689(std::string_view input,
                                             std::string_view compilation_database,
                                             const P1689ImportOptions &options = {});

} // namespace cxx_modgraph
