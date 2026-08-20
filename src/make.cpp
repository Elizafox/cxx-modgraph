// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/make.hpp"

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
        else
        {
            if (character == ' ' || character == '\t' || character == '#' || character == ':' ||
                character == '%' || character == '\\')
            {
                result.push_back('\\');
            }

            result.push_back(character);
        }
    }

    return result;
}

std::string escaped_path(const std::filesystem::path &path)
{
    return escape(normalized(path));
}

void write_variable(std::ostream &output, std::string_view name,
                    const std::vector<std::filesystem::path> &values)
{
    output << name << " :=";
    for (const std::filesystem::path &value : values)
    {
        output << " \\\n    " << escaped_path(value);
    }
    output << "\n\n";
}

void write_target_variable(std::ostream &output, const std::filesystem::path &target,
                           std::string_view name, std::string_view value)
{
    output << escaped_path(target) << ": " << name << " := " << escape(value) << '\n';
}

void write_target_variable(std::ostream &output, const std::filesystem::path &target,
                           std::string_view name, const std::vector<std::filesystem::path> &values)
{
    output << escaped_path(target) << ": " << name << " :=";
    for (const std::filesystem::path &value : values)
    {
        output << ' ' << escaped_path(value);
    }
    output << '\n';
}

void write_prerequisites(std::ostream &output, const std::filesystem::path &target,
                         const std::vector<std::filesystem::path> &prerequisites)
{
    output << escaped_path(target) << ':';
    for (const std::filesystem::path &prerequisite : prerequisites)
    {
        output << " \\\n    " << escaped_path(prerequisite);
    }
    output << "\n\n";
}

std::vector<ModuleImport>
import_bmis(const TranslationUnit &unit, const std::map<std::string, ModuleProvider> &providers,
            const DependencyFacts &facts)
{
    std::set<std::string> seen;
    std::vector<ModuleImport> result;
    for (const std::string &required : unit.required_modules)
    {
        const auto provider = providers.find(required);
        if (provider == providers.end())
        {
            continue;
        }

        std::filesystem::path path = provider->second.bmi_path;
        if (provider->second.external)
        {
            path = resolve_input(facts, path);
        }

        if (seen.insert(required).second)
        {
            result.push_back({required, std::move(path)});
        }
    }

    std::ranges::sort(result, {}, &ModuleImport::name);
    return result;
}

std::vector<std::filesystem::path> import_paths(const std::vector<ModuleImport> &imports)
{
    std::vector<std::filesystem::path> result;
    for (const ModuleImport &module : imports)
    {
        result.push_back(module.bmi_path);
    }

    std::ranges::sort(result, {}, normalized);
    return result;
}

void write_imports(std::ostream &output, const std::filesystem::path &target,
                   const std::vector<ModuleImport> &imports)
{
    output << escaped_path(target) << ": CXX_MODGRAPH_IMPORTS :=";
    for (const ModuleImport &module : imports)
    {
        output << ' ' << escape(module.name + "=" + normalized(module.bmi_path));
    }
    output << '\n';
}

} // namespace

void write_make(std::ostream &output, const DependencyFacts &facts)
{
    std::map<std::string, ModuleProvider> providers;
    for (const ExternalModule &module : facts.external_modules)
    {
        providers.try_emplace(module.name, ModuleProvider{module.bmi_path, true});
    }
    for (const TranslationUnit &unit : facts.translation_units)
    {
        for (const ProvidedModule &module : unit.provides)
        {
            providers.try_emplace(module.name, ModuleProvider{module.bmi_path, false});
        }
    }

    std::vector<std::filesystem::path> bmi_targets;
    std::vector<std::filesystem::path> object_targets;
    for (const TranslationUnit &unit : facts.translation_units)
    {
        for (const ProvidedModule &module : unit.provides)
        {
            bmi_targets.push_back(module.bmi_path);
        }
        if (!unit.object_path.empty())
        {
            object_targets.push_back(unit.object_path);
        }
    }
    std::ranges::sort(bmi_targets, {}, normalized);
    std::ranges::sort(object_targets, {}, normalized);

    output << "# Generated by cxx-modgraph. Do not edit.\n\n";
    write_variable(output, "CXX_MODGRAPH_BMI_TARGETS", bmi_targets);
    write_variable(output, "CXX_MODGRAPH_OBJECT_TARGETS", object_targets);

    std::vector<const TranslationUnit *> units;
    for (const TranslationUnit &unit : facts.translation_units)
    {
        units.push_back(&unit);
    }
    std::ranges::sort(units, {},
                      [](const TranslationUnit *unit) { return normalized(unit->source_path); });

    for (const TranslationUnit *unit : units)
    {
        const std::filesystem::path source = resolve_input(facts, unit->source_path);
        const std::vector<ModuleImport> imports = import_bmis(*unit, providers, facts);
        const std::vector<std::filesystem::path> imported_bmis = import_paths(imports);

        std::vector<ProvidedModule> provided = unit->provides;
        std::ranges::sort(provided, {}, &ProvidedModule::name);
        std::vector<std::filesystem::path> own_bmis;
        for (const ProvidedModule &module : provided)
        {
            own_bmis.push_back(module.bmi_path);
            write_target_variable(output, module.bmi_path, "CXX_MODGRAPH_SOURCE",
                                  normalized(source));
            write_target_variable(output, module.bmi_path, "CXX_MODGRAPH_MODULE", module.name);
            write_target_variable(output, module.bmi_path, "CXX_MODGRAPH_IMPORT_BMIS",
                                  imported_bmis);
            write_imports(output, module.bmi_path, imports);

            std::vector<std::filesystem::path> prerequisites{source};
            prerequisites.insert(prerequisites.end(), imported_bmis.begin(), imported_bmis.end());
            write_prerequisites(output, module.bmi_path, prerequisites);
        }

        if (!unit->object_path.empty())
        {
            write_target_variable(output, unit->object_path, "CXX_MODGRAPH_SOURCE",
                                  normalized(source));
            write_target_variable(output, unit->object_path, "CXX_MODGRAPH_PROVIDED_BMIS",
                                  own_bmis);
            write_target_variable(output, unit->object_path, "CXX_MODGRAPH_IMPORT_BMIS",
                                  imported_bmis);
            write_imports(output, unit->object_path, imports);

            std::vector<std::filesystem::path> prerequisites{source};
            prerequisites.insert(prerequisites.end(), own_bmis.begin(), own_bmis.end());
            prerequisites.insert(prerequisites.end(), imported_bmis.begin(), imported_bmis.end());
            write_prerequisites(output, unit->object_path, prerequisites);
        }
    }
}

std::string to_make(const DependencyFacts &facts)
{
    std::ostringstream output;
    write_make(output, facts);
    return output.str();
}

} // namespace cxx_modgraph
