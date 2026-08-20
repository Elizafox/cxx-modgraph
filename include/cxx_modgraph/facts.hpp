// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/graph.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cxx_modgraph
{

inline constexpr unsigned int current_facts_version = 2;

struct InputArtifact
{
    std::filesystem::path path;
    std::string digest;
};

struct ScanProvenance
{
    std::filesystem::path rule_source;
    std::string scanner;
    std::string scanner_version;
    std::filesystem::path original_output;
    std::string source_lookup;
    bool source_path_unique = true;
    std::string raw_rule_json;
};

struct DependencyReason
{
    std::string module;
    std::string reason;
    std::string lookup_method;
    std::string raw_requirement_json;
};

struct ProvidedModule
{
    std::string name;
    std::filesystem::path bmi_path;
    std::optional<bool> is_interface;
    std::string lookup_method;
};

struct TranslationUnit
{
    std::filesystem::path source_path;
    std::filesystem::path object_path;
    std::vector<ProvidedModule> provides;
    std::vector<std::string> required_modules;
    std::vector<DependencyReason> dependency_reasons;
    std::optional<ScanProvenance> provenance;
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
    std::vector<InputArtifact> inputs;
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
