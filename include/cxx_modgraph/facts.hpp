// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/graph.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace cxx_modgraph
{

inline constexpr unsigned int current_facts_version = 1;

struct ProvidedModule
{
    std::string name;
    std::filesystem::path bmi_path;
};

struct TranslationUnit
{
    std::filesystem::path source_path;
    std::filesystem::path object_path;
    std::vector<ProvidedModule> provides;
    std::vector<std::string> required_modules;
};

struct ExternalModule
{
    std::string name;
    std::filesystem::path bmi_path;
};

struct DependencyFacts
{
    unsigned int version = current_facts_version;
    std::filesystem::path source_root = ".";
    std::vector<TranslationUnit> translation_units;
    std::vector<ExternalModule> external_modules;
};

enum class DiagnosticCode
{
    unsupported_version,
    empty_module_name,
    empty_source_path,
    empty_bmi_path,
    duplicate_source,
    duplicate_provider,
    missing_provider,
    dependency_cycle
};

struct Diagnostic
{
    DiagnosticCode code;
    std::string message;
};

struct AnalysisResult
{
    DependencyGraph graph;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] AnalysisResult analyze(const DependencyFacts &facts);

} // namespace cxx_modgraph
