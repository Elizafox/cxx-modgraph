// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/facts.hpp"

#include <functional>
#include <map>
#include <set>
#include <utility>

namespace cxx_modgraph
{
namespace
{

void add_diagnostic(AnalysisResult &result, DiagnosticCode code, std::string message)
{
    result.diagnostics.push_back({code, std::move(message)});
}

std::string path_key(const std::filesystem::path &path)
{
    return path.lexically_normal().generic_string();
}

std::string set_name(const std::string &name)
{
    return name.empty() ? "default" : name;
}

std::string provider_key(std::string_view module, std::string_view set)
{
    return std::string(set) + '\0' + std::string(module);
}

std::string compatibility_scope(const std::string &set, const BmiCompatibility &compatibility)
{
    std::string result = set_name(set);
    const auto add = [&](std::string_view value)
    {
        result.push_back('\0');
        result += value;
    };

    add(compatibility.compiler_executable);
    add(compatibility.compiler_version);
    add(compatibility.target_triple);
    add(compatibility.sysroot);
    add(compatibility.language_standard);
    add(compatibility.standard_library);
    add(compatibility.configuration.empty() ? "default" : compatibility.configuration);
    add(compatibility.user_key);
    for (const auto &key : compatibility.adapter_keys)
    {
        add(key);
    }

    return result;
}

std::string node_key(const TranslationUnit &unit, const std::set<std::string> &repeated)
{
    const std::string source = path_key(unit.source_path);
    if (!repeated.contains(source))
    {
        return source;
    }

    const std::string configuration = unit.bmi_compatibility.configuration.empty()
                                          ? "default"
                                          : unit.bmi_compatibility.configuration;
    const auto fingerprint =
        std::hash<std::string>{}(compatibility_scope(unit.module_set, unit.bmi_compatibility));
    return source + " [" + set_name(unit.module_set) + "/" + configuration + "/" +
           std::to_string(fingerprint) + "]";
}

} // namespace

bool AnalysisResult::ok() const noexcept
{
    return diagnostics.empty();
}

AnalysisResult analyze(const DependencyFacts &facts)
{
    AnalysisResult result;
    if (facts.version < 1 || facts.version > current_facts_version)
    {
        add_diagnostic(result, DiagnosticCode::unsupported_version,
                       "unsupported dependency facts version " + std::to_string(facts.version));
    }

    std::map<std::string, std::string> providers;
    std::map<std::string, std::size_t> source_counts;
    for (const auto &unit : facts.translation_units)
    {
        ++source_counts[path_key(unit.source_path)];
    }
    std::set<std::string> repeated_sources;
    for (const auto &[source, count] : source_counts)
    {
        if (count > 1)
        {
            repeated_sources.insert(source);
        }
    }
    std::set<std::string> source_scopes;

    for (const ExternalModule &module : facts.external_modules)
    {
        if (module.name.empty())
        {
            add_diagnostic(result, DiagnosticCode::empty_module_name,
                           "external module has an empty name");
            continue;
        }

        if (module.bmi_path.empty())
        {
            add_diagnostic(result, DiagnosticCode::empty_bmi_path,
                           "external module '" + module.name + "' has no BMI path");
        }

        const auto [existing, inserted] = providers.emplace(
            provider_key(module.name,
                         compatibility_scope(module.module_set, module.bmi_compatibility)),
            std::string{});
        if (!inserted)
        {
            add_diagnostic(result, DiagnosticCode::duplicate_provider,
                           "module '" + module.name + "' has duplicate providers (external and '" +
                               existing->second + "')");
        }
    }

    for (const TranslationUnit &unit : facts.translation_units)
    {
        const std::string source = path_key(unit.source_path);
        if (unit.source_path.empty())
        {
            add_diagnostic(result, DiagnosticCode::empty_source_path,
                           "translation unit has an empty source path");
            continue;
        }

        const std::string set = set_name(unit.module_set);
        const std::string scope = compatibility_scope(set, unit.bmi_compatibility);
        if (!source_scopes.insert(provider_key(source, scope)).second)
        {
            add_diagnostic(result, DiagnosticCode::duplicate_source,
                           "translation unit source '" + source + "' occurs more than once");
        }
        const std::string node = node_key(unit, repeated_sources);
        result.graph.add_node(node);

        for (const ProvidedModule &module : unit.provides)
        {
            if (module.name.empty())
            {
                add_diagnostic(result, DiagnosticCode::empty_module_name,
                               "translation unit '" + source + "' provides an empty module name");
                continue;
            }

            if (module.bmi_path.empty())
            {
                add_diagnostic(result, DiagnosticCode::empty_bmi_path,
                               "module '" + module.name + "' has no BMI path");
            }

            const auto [existing, inserted] =
                providers.emplace(provider_key(module.name, scope), node);
            if (!inserted)
            {
                add_diagnostic(result, DiagnosticCode::duplicate_provider,
                               "module '" + module.name + "' has duplicate providers: '" +
                                   existing->second + "' and '" + source + "'");
            }
        }
    }

    for (const TranslationUnit &unit : facts.translation_units)
    {
        if (unit.source_path.empty())
        {
            continue;
        }
        const std::string source = path_key(unit.source_path);
        const std::string node = node_key(unit, repeated_sources);
        const std::string set = set_name(unit.module_set);
        const std::string scope = compatibility_scope(set, unit.bmi_compatibility);
        for (const std::string &required : unit.required_modules)
        {
            if (required.empty())
            {
                add_diagnostic(result, DiagnosticCode::empty_module_name,
                               "translation unit '" + source + "' requires an empty module name");
                continue;
            }

            auto provider = providers.find(provider_key(required, scope));
            if (provider == providers.end() && set != "default")
            {
                provider = providers.find(
                    provider_key(required, compatibility_scope("default", unit.bmi_compatibility)));
            }
            if (provider == providers.end())
            {
                add_diagnostic(result, DiagnosticCode::missing_provider,
                               "module '" + required + "' required by '" + source +
                                   "' has no provider");
            }
            else if (!provider->second.empty() && provider->second != node)
            {
                result.graph.add_dependency(node, provider->second);
            }
        }
    }

    const SortResult sorted = result.graph.topological_sort();
    if (sorted.has_cycle())
    {
        const std::vector<NodeId> witness = result.graph.cycle_witness();
        std::string chain;
        for (const NodeId &node : witness)
        {
            chain += (chain.empty() ? "" : " -> ") + node;
        }
        add_diagnostic(result, DiagnosticCode::dependency_cycle,
                       "module dependencies contain a cycle: " + chain);
    }

    return result;
}

} // namespace cxx_modgraph
