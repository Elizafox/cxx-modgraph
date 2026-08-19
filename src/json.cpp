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

        output << "]\n    }";
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

    output << (external.empty() ? "]" : "\n  ]") << "\n}\n";
}

std::string to_json(const DependencyFacts &facts)
{
    std::ostringstream output;
    write_json(output, facts);
    return output.str();
}

} // namespace cxx_modgraph
