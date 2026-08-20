// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/graph.hpp"

#include <compare>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cxx_modgraph
{

inline constexpr unsigned int current_facts_version = 5;

// A BMI is only reusable inside an identical compatibility scope.  Empty fields are
// deliberately meaningful: cxx-modgraph records policy supplied by the build tool; it
// does not guess which compiler flags happen to affect a particular compiler's BMI.
struct BmiCompatibility
{
    std::string compiler_executable;
    std::string compiler_version;
    std::string target_triple;
    std::string sysroot;
    std::string language_standard;
    std::string standard_library;
    std::string configuration = "default";
    std::string user_key;
    std::vector<std::string> adapter_keys;

    auto operator<=>(const BmiCompatibility &) const = default;
};

struct BmiCacheRecord
{
    std::string module;
    std::string module_set = "default";
    std::string source_digest;
    std::string recipe_digest;
    std::string compatibility_key;
    std::string bmi_digest;
    std::string object_digest;
};

struct InputArtifact
{
    std::filesystem::path path;
    std::string digest;
};

// Inputs that make a graph reproducible outside the machine where it was scanned.
// These are declarations, not instructions to inspect the ambient environment.
struct PathRemapping
{
    std::filesystem::path from;
    std::filesystem::path to;
};

struct EnvironmentInput
{
    std::string name;
    std::string value_digest;
};

struct ToolIdentity
{
    std::string name;
    std::filesystem::path path;
    std::string digest;
    std::string version;
    std::string execution_namespace = "host";
};

struct ContentDigest
{
    std::string kind;
    std::filesystem::path path;
    std::string digest;
    std::string artifact_namespace = "target";
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
    // Portable build context retained for complete build-database emitters.
    std::vector<std::string> arguments;
    std::vector<std::string> local_arguments;
    std::filesystem::path work_directory;
    std::string module_set = "default";
    BmiCompatibility bmi_compatibility;
    bool is_private = false;
};

struct ModuleSet
{
    std::string name;
    std::string family_name;
    std::vector<std::string> baseline_arguments;
    std::vector<std::string> visible_sets;
};

struct ExternalModule
{
    std::string name;
    std::filesystem::path bmi_path;
    std::string module_set = "default";
    BmiCompatibility bmi_compatibility;
};

struct DependencyFacts
{
    unsigned int version = current_facts_version;
    std::filesystem::path source_root = ".";
    std::vector<TranslationUnit> translation_units;
    std::vector<ExternalModule> external_modules;
    std::vector<InputArtifact> inputs;
    std::vector<ModuleSet> module_sets;
    std::vector<BmiCacheRecord> bmi_cache;
    std::vector<PathRemapping> path_remappings;
    std::vector<EnvironmentInput> environment_inputs;
    std::vector<ToolIdentity> tools;
    std::vector<ContentDigest> content_digests;
    std::string graph_digest;
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
