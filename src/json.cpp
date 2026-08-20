// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/json.hpp"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <string_view>

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

} // namespace

void write_json(std::ostream &output, const DependencyFacts &facts)
{
    std::vector<TranslationUnit> units = facts.translation_units;
    std::ranges::sort(units, {},
                      [](const TranslationUnit &unit) { return normalized(unit.source_path); });
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
                output << ", \"is-interface\": " << (*module.is_interface ? "true" : "false");
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
    output << (facts.inputs.empty() ? "]" : "\n  ]") << "\n}\n";
}

std::string to_json(const DependencyFacts &facts)
{
    std::ostringstream output;
    write_json(output, facts);
    return output.str();
}

} // namespace cxx_modgraph
