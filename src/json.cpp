// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/json.hpp"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <string_view>
#include <tuple>

namespace cxx_modgraph
{
namespace
{

void write_escaped(std::ostream &output, std::string_view value)
{
    output.put('"');
    for (const char raw_character : value)
    {
        const auto character = static_cast<unsigned char>(raw_character);
        switch (character)
        {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20)
            {
                constexpr char digits[] = "0123456789abcdef";
                output << "\\u00" << digits[character >> 4] << digits[character & 0x0f];
            }
            else
            {
                output.put(static_cast<char>(character));
            }
        }
    }
    output.put('"');
}

std::string normalized(const std::filesystem::path &path)
{
    return path.lexically_normal().generic_string();
}

void write_compatibility(std::ostream &output, const BmiCompatibility &value)
{
    output << "{\"compiler-executable\": ";
    write_escaped(output, value.compiler_executable);
    output << ", \"compiler-version\": ";
    write_escaped(output, value.compiler_version);
    output << ", \"target-triple\": ";
    write_escaped(output, value.target_triple);
    output << ", \"sysroot\": ";
    write_escaped(output, value.sysroot);
    output << ", \"language-standard\": ";
    write_escaped(output, value.language_standard);
    output << ", \"standard-library\": ";
    write_escaped(output, value.standard_library);
    output << ", \"configuration\": ";
    write_escaped(output, value.configuration);
    output << ", \"user-key\": ";
    write_escaped(output, value.user_key);
    output << ", \"adapter-keys\": [";
    for (std::size_t i = 0; i < value.adapter_keys.size(); ++i)
    {
        if (i)
        {
            output << ", ";
        }

        write_escaped(output, value.adapter_keys[i]);
    }
    output << "]}";
}

} // namespace

void write_json(std::ostream &output, const DependencyFacts &facts)
{
    std::vector<TranslationUnit> units = facts.translation_units;
    std::ranges::sort(units,
                      [](const TranslationUnit &a, const TranslationUnit &b)
                      {
                          return std::tie(a.source_path, a.module_set, a.bmi_compatibility) <
                                 std::tie(b.source_path, b.module_set, b.bmi_compatibility);
                      });

    std::vector<ExternalModule> external = facts.external_modules;
    std::ranges::sort(external, {}, &ExternalModule::name);

    output << "{\n  \"version\": " << facts.version << ",\n  \"source-root\": ";
    write_escaped(output, normalized(facts.source_root));
    output << ",\n  \"translation-units\": [";

    for (std::size_t index = 0; index < units.size(); ++index)
    {
        TranslationUnit unit = units[index];
        std::ranges::sort(unit.provides, {}, &ProvidedModule::name);
        std::ranges::sort(unit.required_modules);

        output << (index == 0 ? "\n" : ",\n") << "    {\n      \"source\": ";
        write_escaped(output, normalized(unit.source_path));
        output << ",\n      \"object\": ";
        write_escaped(output, normalized(unit.object_path));
        output << ",\n      \"provides\": [";
        for (std::size_t module_index = 0; module_index < unit.provides.size(); ++module_index)
        {
            const ProvidedModule &module = unit.provides[module_index];

            output << (module_index == 0 ? "\n" : ",\n") << "        {\"name\": ";
            write_escaped(output, module.name);

            output << ", \"bmi\": ";
            write_escaped(output, normalized(module.bmi_path));

            if (module.is_interface)
            {
                output << ", \"is-interface\": " << (*module.is_interface ? "true" : "false");
            }

            if (!module.lookup_method.empty())
            {
                output << ", \"lookup-method\": ";
                write_escaped(output, module.lookup_method);
            }

            output << '}';
        }

        output << (unit.provides.empty() ? "]" : "\n      ]") << ",\n      \"requires\": [";
        for (std::size_t required_index = 0; required_index < unit.required_modules.size();
             ++required_index)
        {
            if (required_index != 0)
            {
                output << ", ";
            }

            write_escaped(output, unit.required_modules[required_index]);
        }

        output << "]";

        if (!unit.arguments.empty())
        {
            output << ",\n      \"arguments\": [";

            for (std::size_t i = 0; i < unit.arguments.size(); ++i)
            {
                if (i)
                {
                    output << ", ";
                }

                write_escaped(output, unit.arguments[i]);
            }

            output << ']';
        }

        if (!unit.local_arguments.empty())
        {
            output << ",\n      \"local-arguments\": [";

            for (std::size_t i = 0; i < unit.local_arguments.size(); ++i)
            {
                if (i)
                {
                    output << ", ";
                }

                write_escaped(output, unit.local_arguments[i]);
            }

            output << ']';
        }

        if (!unit.work_directory.empty())
        {
            output << ",\n      \"work-directory\": ";
            write_escaped(output, normalized(unit.work_directory));
        }

        if (!unit.module_set.empty() && unit.module_set != "default")
        {
            output << ",\n      \"module-set\": ";
            write_escaped(output, unit.module_set);
        }

        output << ",\n      \"bmi-compatibility\": ";
        write_compatibility(output, unit.bmi_compatibility);
        if (unit.is_private)
        {
            output << ",\n      \"private\": true";
        }

        if (!unit.dependency_reasons.empty())
        {
            output << ",\n      \"dependency-reasons\": [";
            for (std::size_t i = 0; i < unit.dependency_reasons.size(); ++i)
            {
                const auto &r = unit.dependency_reasons[i];

                output << (i ? ",\n" : "\n") << "        {\"module\": ";
                write_escaped(output, r.module);
                output << ", \"reason\": ";
                write_escaped(output, r.reason);
                output << ", \"lookup-method\": ";
                write_escaped(output, r.lookup_method);

                if (!r.raw_requirement_json.empty())
                {
                    output << ", \"raw-requirement\": ";
                    write_escaped(output, r.raw_requirement_json);
                }

                output << '}';
            }

            output << "\n      ]";
        }

        if (unit.provenance)
        {
            const auto &p = *unit.provenance;
            output << ",\n      \"provenance\": {\"rule-source\": ";
            write_escaped(output, normalized(p.rule_source));
            output << ", \"scanner\": ";
            write_escaped(output, p.scanner);
            output << ", \"scanner-version\": ";
            write_escaped(output, p.scanner_version);
            output << ", \"original-output\": ";
            write_escaped(output, normalized(p.original_output));
            output << ", \"source-lookup\": ";
            write_escaped(output, p.source_lookup);
            output << ", \"source-path-unique\": " << (p.source_path_unique ? "true" : "false");

            if (!p.raw_rule_json.empty())
            {
                output << ", \"raw-rule\": ";
                write_escaped(output, p.raw_rule_json);
            }

            output << '}';
        }
        output << "\n    }";
    }

    output << (units.empty() ? "]" : "\n  ]") << ",\n  \"external-modules\": [";

    for (std::size_t index = 0; index < external.size(); ++index)
    {
        output << (index == 0 ? "\n" : ",\n") << "    {\"name\": ";
        write_escaped(output, external[index].name);
        output << ", \"bmi\": ";
        write_escaped(output, normalized(external[index].bmi_path));
        output << ", \"module-set\": ";
        write_escaped(output, external[index].module_set);
        output << ", \"bmi-compatibility\": ";
        write_compatibility(output, external[index].bmi_compatibility);
        output << '}';
    }

    output << (external.empty() ? "]" : "\n  ]") << ",\n  \"inputs\": [";
    for (std::size_t i = 0; i < facts.inputs.size(); ++i)
    {
        output << (i ? ",\n" : "\n") << "    {\"path\": ";
        write_escaped(output, normalized(facts.inputs[i].path));
        output << ", \"digest\": ";
        write_escaped(output, facts.inputs[i].digest);
        output << '}';
    }

    output << (facts.inputs.empty() ? "]" : "\n  ]") << ",\n  \"module-sets\": [";
    std::vector<ModuleSet> sets = facts.module_sets;
    std::ranges::sort(sets, {}, &ModuleSet::name);
    for (std::size_t i = 0; i < sets.size(); ++i)
    {
        const auto &set = sets[i];
        output << (i ? ",\n" : "\n") << "    {\"name\": ";
        write_escaped(output, set.name);
        output << ", \"family-name\": ";
        write_escaped(output, set.family_name);
        output << ", \"baseline-arguments\": [";
        for (std::size_t j = 0; j < set.baseline_arguments.size(); ++j)
        {
            if (j)
            {
                output << ", ";
            }

            write_escaped(output, set.baseline_arguments[j]);
        }

        output << "], \"visible-sets\": [";
        for (std::size_t j = 0; j < set.visible_sets.size(); ++j)
        {
            if (j)
            {
                output << ", ";
            }

            write_escaped(output, set.visible_sets[j]);
        }

        output << "]}";
    }

    output << (sets.empty() ? "]" : "\n  ]") << ",\n  \"bmi-cache\": [";
    auto cache = facts.bmi_cache;
    std::ranges::sort(cache, {}, &BmiCacheRecord::module);
    for (std::size_t i = 0; i < cache.size(); ++i)
    {
        const auto &r = cache[i];
        output << (i ? ",\n" : "\n") << "    {\"module\": ";
        write_escaped(output, r.module);
        output << ", \"module-set\": ";
        write_escaped(output, r.module_set);
        output << ", \"source-digest\": ";
        write_escaped(output, r.source_digest);
        output << ", \"recipe-digest\": ";
        write_escaped(output, r.recipe_digest);
        output << ", \"compatibility-key\": ";
        write_escaped(output, r.compatibility_key);
        output << ", \"bmi-digest\": ";
        write_escaped(output, r.bmi_digest);
        output << ", \"object-digest\": ";
        write_escaped(output, r.object_digest);
        output << '}';
    }

    output << (cache.empty() ? "]" : "\n  ]") << ",\n  \"path-remappings\": [";
    auto remappings = facts.path_remappings;
    std::ranges::sort(remappings, {}, &PathRemapping::from);
    for (std::size_t i = 0; i < remappings.size(); ++i)
    {
        output << (i ? ",\n" : "\n") << "    {\"from\": ";
        write_escaped(output, normalized(remappings[i].from));
        output << ", \"to\": ";
        write_escaped(output, normalized(remappings[i].to));
        output << '}';
    }

    output << (remappings.empty() ? "]" : "\n  ]") << ",\n  \"environment-inputs\": [";
    auto environment = facts.environment_inputs;
    std::ranges::sort(environment, {}, &EnvironmentInput::name);
    for (std::size_t i = 0; i < environment.size(); ++i)
    {
        output << (i ? ",\n" : "\n") << "    {\"name\": ";
        write_escaped(output, environment[i].name);
        output << ", \"value-digest\": ";
        write_escaped(output, environment[i].value_digest);
        output << '}';
    }

    output << (environment.empty() ? "]" : "\n  ]") << ",\n  \"tools\": [";
    auto tools = facts.tools;
    std::ranges::sort(tools, {}, &ToolIdentity::name);
    for (std::size_t i = 0; i < tools.size(); ++i)
    {
        const auto &tool = tools[i];
        output << (i ? ",\n" : "\n") << "    {\"name\": ";
        write_escaped(output, tool.name);
        output << ", \"path\": ";
        write_escaped(output, normalized(tool.path));
        output << ", \"digest\": ";
        write_escaped(output, tool.digest);
        output << ", \"version\": ";
        write_escaped(output, tool.version);
        output << ", \"namespace\": ";
        write_escaped(output, tool.execution_namespace);
        output << '}';
    }

    output << (tools.empty() ? "]" : "\n  ]") << ",\n  \"content-digests\": [";
    auto digests = facts.content_digests;
    std::ranges::sort(digests, [](const ContentDigest &a, const ContentDigest &b)
                      { return std::tie(a.kind, a.path) < std::tie(b.kind, b.path); });
    for (std::size_t i = 0; i < digests.size(); ++i)
    {
        const auto &item = digests[i];
        output << (i ? ",\n" : "\n") << "    {\"kind\": ";
        write_escaped(output, item.kind);
        output << ", \"path\": ";
        write_escaped(output, normalized(item.path));
        output << ", \"digest\": ";
        write_escaped(output, item.digest);
        output << ", \"namespace\": ";
        write_escaped(output, item.artifact_namespace);
        output << '}';
    }

    output << (digests.empty() ? "]" : "\n  ]") << ",\n  \"graph-digest\": ";
    write_escaped(output, facts.graph_digest);
    output << "\n}\n";
}

std::string to_json(const DependencyFacts &facts)
{
    std::ostringstream output;
    write_json(output, facts);
    return output.str();
}

} // namespace cxx_modgraph
