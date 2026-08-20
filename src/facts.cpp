// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/facts.hpp"

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

} // namespace

bool AnalysisResult::ok() const noexcept
{
    return diagnostics.empty();
}

AnalysisResult analyze(const DependencyFacts &facts)
{
    AnalysisResult result;
    if (facts.version != 1 && facts.version != current_facts_version)
    {
        add_diagnostic(result, DiagnosticCode::unsupported_version,
                       "unsupported dependency facts version " + std::to_string(facts.version));
    }

    std::map<std::string, std::string> providers;
    std::set<std::string> sources;

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

        const auto [existing, inserted] = providers.emplace(module.name, std::string{});
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

        if (!sources.insert(source).second)
        {
            add_diagnostic(result, DiagnosticCode::duplicate_source,
                           "translation unit source '" + source + "' occurs more than once");
        }
        result.graph.add_node(source);

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

            const auto [existing, inserted] = providers.emplace(module.name, source);
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
        for (const std::string &required : unit.required_modules)
        {
            if (required.empty())
            {
                add_diagnostic(result, DiagnosticCode::empty_module_name,
                               "translation unit '" + source + "' requires an empty module name");
                continue;
            }

            const auto provider = providers.find(required);
            if (provider == providers.end())
            {
                add_diagnostic(result, DiagnosticCode::missing_provider,
                               "module '" + required + "' required by '" + source +
                                   "' has no provider");
            }
            else if (!provider->second.empty() && provider->second != source)
            {
                result.graph.add_dependency(source, provider->second);
            }
        }
    }

    const SortResult sorted = result.graph.topological_sort();
    if (sorted.has_cycle())
    {
        const std::vector<NodeId> witness = result.graph.cycle_witness();
        std::string chain;
        for (const NodeId &node : witness)
            chain += (chain.empty() ? "" : " -> ") + node;
        add_diagnostic(result, DiagnosticCode::dependency_cycle,
                       "module dependencies contain a cycle: " + chain);
    }

    return result;
}

} // namespace cxx_modgraph
