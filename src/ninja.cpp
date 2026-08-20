// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/ninja.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

namespace cxx_modgraph
{
namespace
{

struct ModuleProvider
{
    std::filesystem::path bmi_path;
    bool external = false;
    std::vector<std::string> required_modules;
};

struct ModuleImport
{
    std::string name;
    std::filesystem::path bmi_path;
};

std::string normalized(const std::filesystem::path &path)
{
    return path.lexically_normal().generic_string();
}

std::filesystem::path resolve_input(const DependencyFacts &facts, const std::filesystem::path &path)
{
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }

    return (facts.source_root / path).lexically_normal();
}

std::string escape(std::string_view value)
{
    std::string result;
    for (const char character : value)
    {
        if (character == '$')
        {
            result += "$$";
        }
        else if (character == ' ')
        {
            result += "$ ";
        }
        else if (character == ':')
        {
            result += "$:";
        }
        else if (character == '\n')
        {
            result += "$\n";
        }
        else
        {
            result.push_back(character);
        }
    }

    return result;
}

std::string escaped_path(const std::filesystem::path &path)
{
    return escape(normalized(path));
}

std::vector<ModuleImport> imports_for(const TranslationUnit &unit,
                                      const std::map<std::string, ModuleProvider> &providers,
                                      const DependencyFacts &facts, bool transitive)
{
    std::set<std::string> seen;
    std::vector<ModuleImport> result;
    const auto visit = [&](const std::string &name, const auto &visit_module) -> void
    {
        if (!seen.insert(name).second)
        {
            return;
        }

        const auto provider = providers.find(name);
        if (provider == providers.end())
        {
            return;
        }

        std::filesystem::path path = provider->second.bmi_path;
        if (provider->second.external)
        {
            path = resolve_input(facts, path);
        }

        result.push_back({name, std::move(path)});
        if (transitive)
        {
            for (const std::string &dependency : provider->second.required_modules)
            {
                visit_module(dependency, visit_module);
            }
        }
    };

    for (const std::string &required : unit.required_modules)
    {
        visit(required, visit);
    }

    std::ranges::sort(result, {}, &ModuleImport::name);
    return result;
}

void write_edge(std::ostream &output, const std::filesystem::path &target, std::string_view rule,
                const std::filesystem::path &source, const std::vector<ModuleImport> &direct,
                const std::vector<ModuleImport> &imports, const std::filesystem::path &provided_bmi,
                std::string_view provided_module = {})
{
    output << "build " << escaped_path(target) << ": " << rule << ' ' << escaped_path(source);
    if (!direct.empty())
    {
        output << " |";
        std::vector<std::filesystem::path> paths;
        for (const ModuleImport &module : direct)
        {
            paths.push_back(module.bmi_path);
        }

        std::ranges::sort(paths, {}, normalized);
        for (const auto &path : paths)
        {
            output << ' ' << escaped_path(path);
        }
    }

    output << '\n';
    output << "  source = " << escaped_path(source) << '\n';
    output << "  output_dir = "
           << escaped_path(target.parent_path().empty() ? std::filesystem::path{"."}
                                                        : target.parent_path())
           << '\n';
    if (!provided_bmi.empty())
    {
        output << "  provided_bmi = " << escaped_path(provided_bmi) << '\n';
    }

    if (!provided_module.empty())
    {
        output << "  provided_module = " << escape(provided_module) << '\n';
    }

    output << "  module_flags =";
    for (const ModuleImport &module : imports)
    {
        output << " -fmodule-file='" << escape(module.name + "=" + normalized(module.bmi_path))
               << '\'';
    }

    output << "\n  module_mappings =";
    for (const ModuleImport &module : imports)
    {
        output << ' ' << escape(module.name + "=" + normalized(module.bmi_path));
    }
    output << "\n  msvc_module_flags =";
    for (const ModuleImport &module : imports)
        output << " /reference \"" << escape(module.name + "=" + normalized(module.bmi_path))
               << "\"";
    output << "\n\n";
}

} // namespace

void write_ninja(std::ostream &output, const DependencyFacts &facts)
{
    std::map<std::string, ModuleProvider> providers;
    for (const ExternalModule &module : facts.external_modules)
    {
        providers.try_emplace(module.name, ModuleProvider{module.bmi_path, true, {}});
    }

    for (const TranslationUnit &unit : facts.translation_units)
    {
        for (const ProvidedModule &module : unit.provides)
        {
            providers.try_emplace(module.name,
                                  ModuleProvider{module.bmi_path, false, unit.required_modules});
        }
    }

    std::vector<const TranslationUnit *> units;
    for (const TranslationUnit &unit : facts.translation_units)
    {
        units.push_back(&unit);
    }

    std::ranges::sort(units, {},
                      [](const TranslationUnit *unit) { return normalized(unit->source_path); });

    output << "# Generated by cxx-modgraph. Do not edit.\n\n";
    std::vector<std::filesystem::path> all_outputs;
    for (const TranslationUnit *unit : units)
    {
        const std::filesystem::path source = resolve_input(facts, unit->source_path);
        const auto direct = imports_for(*unit, providers, facts, false);
        const auto imports = imports_for(*unit, providers, facts, true);
        std::vector<ProvidedModule> provided = unit->provides;
        std::ranges::sort(provided, {}, &ProvidedModule::name);
        for (const ProvidedModule &module : provided)
        {
            write_edge(output, module.bmi_path, "cxx_modgraph_bmi", source, direct, imports, {},
                       module.name);
            all_outputs.push_back(module.bmi_path);
        }

        if (!unit->object_path.empty())
        {
            if (provided.empty())
            {
                write_edge(output, unit->object_path, "cxx_modgraph_object", source, direct,
                           imports, {});
            }
            else
            {
                std::vector<ModuleImport> object_dependencies = direct;
                object_dependencies.push_back({provided.front().name, provided.front().bmi_path});
                write_edge(output, unit->object_path, "cxx_modgraph_module_object", source,
                           object_dependencies, imports, provided.front().bmi_path);
            }

            all_outputs.push_back(unit->object_path);
        }
    }
    std::ranges::sort(all_outputs, {}, normalized);
    output << "build cxx_modgraph_outputs: phony";
    for (const auto &path : all_outputs)
    {
        output << ' ' << escaped_path(path);
    }

    output << '\n';
}

std::string to_ninja(const DependencyFacts &facts)
{
    std::ostringstream output;
    write_ninja(output, facts);
    return output.str();
}

} // namespace cxx_modgraph
