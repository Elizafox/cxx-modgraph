// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/json.hpp"

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace cxx_modgraph
{

struct PackageImportResult
{
    std::optional<DependencyFacts> facts;
    std::vector<JsonError> errors;
    [[nodiscard]] bool ok() const noexcept;
};

// Read installed module-source metadata. Relative paths are resolved against
// the manifest directory, never against ambient package search paths.
[[nodiscard]] PackageImportResult
import_package_metadata(std::string_view input, const std::filesystem::path &manifest_path);

} // namespace cxx_modgraph
