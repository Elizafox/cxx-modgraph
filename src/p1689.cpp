// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/p1689.hpp"
#include "json_dom.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace cxx_modgraph
{
namespace
{

using detail::JsonValue;

class Reader
{
public:
    const JsonValue::Object *object(const JsonValue *value, std::string_view context)
    {
        if (value == nullptr)
        {
            return nullptr;
        }

        const auto *result = std::get_if<JsonValue::Object>(&value->value);
        if (result == nullptr)
        {
            error(std::string(context) + " must be an object");
        }

        return result;
    }

    const JsonValue::Array *array(const JsonValue *value, std::string_view context)
    {
        if (value == nullptr)
        {
            return nullptr;
        }

        const auto *result = std::get_if<JsonValue::Array>(&value->value);
        if (result == nullptr)
        {
            error(std::string(context) + " must be an array");
        }

        return result;
    }

    const std::string *string(const JsonValue *value, std::string_view context)
    {
        if (value == nullptr)
        {
            return nullptr;
        }

        const auto *result = std::get_if<std::string>(&value->value);
        if (result == nullptr)
        {
            error(std::string(context) + " must be a string");
        }

        return result;
    }

    const std::int64_t *integer(const JsonValue *value, std::string_view context)
    {
        if (value == nullptr)
        {
            return nullptr;
        }

        const auto *result = std::get_if<std::int64_t>(&value->value);
        if (result == nullptr)
        {
            error(std::string(context) + " must be an integer");
        }

        return result;
    }

    const JsonValue *property(const JsonValue::Object &value, std::string_view name,
                              std::string_view context, bool required = true)
    {
        const auto found = value.find(std::string(name));
        if (found != value.end())
        {
            return &found->second;
        }

        if (required)
        {
            error(std::string(context) + " is missing required property '" + std::string(name) +
                  "'");
        }

        return nullptr;
    }

    void error(std::string message)
    {
        errors.push_back({1, 1, std::move(message)});
    }

    std::vector<JsonError> errors;
};

std::map<std::string, std::filesystem::path> read_compilation_database(const JsonValue &root,
                                                                       Reader &reader)
{
    std::map<std::string, std::filesystem::path> result;
    const JsonValue::Array *entries = reader.array(&root, "compilation database");
    if (entries == nullptr)
    {
        return result;
    }
    for (std::size_t index = 0; index < entries->size(); ++index)
    {
        const std::string context = "compilation database[" + std::to_string(index) + "]";
        const JsonValue::Object *entry = reader.object(&(*entries)[index], context);
        if (entry == nullptr)
        {
            continue;
        }

        const std::string *file =
            reader.string(reader.property(*entry, "file", context), context + ".file");
        const std::string *output =
            reader.string(reader.property(*entry, "output", context), context + ".output");
        if (file != nullptr && output != nullptr && !result.emplace(*output, *file).second)
        {
            reader.error("compilation database output '" + *output + "' occurs more than once");
        }
    }
    return result;
}

std::filesystem::path bmi_path(std::string module_name, const P1689ImportOptions &options)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string filename;
    filename.reserve(module_name.size());
    for (const char value : module_name)
    {
        const auto character = static_cast<unsigned char>(value);
        if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '-' || character == '_' ||
            character == '.')
        {
            filename.push_back(static_cast<char>(character));
        }
        else
        {
            filename.push_back('%');
            filename.push_back(hex[character >> 4]);
            filename.push_back(hex[character & 0x0f]);
        }
    }

    return options.bmi_directory / (filename + ".pcm");
}

void read_provides(const JsonValue::Object &rule, const std::string &context,
                   const P1689ImportOptions &options, TranslationUnit &unit, Reader &reader)
{
    const JsonValue *value = reader.property(rule, "provides", context, false);
    if (value == nullptr)
    {
        return;
    }

    const JsonValue::Array *provides = reader.array(value, context + ".provides");
    if (provides == nullptr)
    {
        return;
    }

    for (std::size_t index = 0; index < provides->size(); ++index)
    {
        const std::string item_context = context + ".provides[" + std::to_string(index) + "]";
        const JsonValue::Object *provided = reader.object(&(*provides)[index], item_context);
        if (provided == nullptr)
        {
            continue;
        }

        const std::string *name =
            reader.string(reader.property(*provided, "logical-name", item_context),
                          item_context + ".logical-name");
        if (name != nullptr)
        {
            unit.provides.push_back({*name, bmi_path(*name, options)});
        }
    }
}

void read_requires(const JsonValue::Object &rule, const std::string &context, TranslationUnit &unit,
                   Reader &reader)
{
    const JsonValue *value = reader.property(rule, "requires", context, false);
    if (value == nullptr)
    {
        return;
    }

    const JsonValue::Array *requirements = reader.array(value, context + ".requires");
    if (requirements == nullptr)
    {
        return;
    }

    for (std::size_t index = 0; index < requirements->size(); ++index)
    {
        const std::string item_context = context + ".requires[" + std::to_string(index) + "]";
        const JsonValue::Object *required = reader.object(&(*requirements)[index], item_context);
        if (required == nullptr)
        {
            continue;
        }

        const std::string *name =
            reader.string(reader.property(*required, "logical-name", item_context),
                          item_context + ".logical-name");
        if (name != nullptr)
        {
            unit.required_modules.push_back(*name);
        }
    }
}

DependencyFacts read_p1689(const JsonValue &root,
                           const std::map<std::string, std::filesystem::path> &sources,
                           const P1689ImportOptions &options, Reader &reader)
{
    DependencyFacts facts;
    facts.source_root = options.source_root;
    facts.external_modules = options.external_modules;

    const JsonValue::Object *document = reader.object(&root, "P1689 document");
    if (document == nullptr)
    {
        return facts;
    }

    const std::int64_t *version = reader.integer(
        reader.property(*document, "version", "P1689 document"), "P1689 document.version");
    if (version != nullptr && *version != 1)
    {
        reader.error("unsupported P1689 version " + std::to_string(*version));
    }

    const JsonValue::Array *rules =
        reader.array(reader.property(*document, "rules", "P1689 document"), "P1689 document.rules");
    if (rules == nullptr)
    {
        return facts;
    }

    for (std::size_t index = 0; index < rules->size(); ++index)
    {
        const std::string context = "rules[" + std::to_string(index) + "]";
        const JsonValue::Object *rule = reader.object(&(*rules)[index], context);
        if (rule == nullptr)
        {
            continue;
        }

        const std::string *output = reader.string(reader.property(*rule, "primary-output", context),
                                                  context + ".primary-output");
        if (output == nullptr)
        {
            continue;
        }

        const auto source = sources.find(*output);
        if (source == sources.end())
        {
            reader.error("P1689 primary output '" + *output +
                         "' has no matching compilation database entry");
            continue;
        }

        TranslationUnit unit;
        unit.source_path = source->second;
        unit.object_path = *output;
        read_provides(*rule, context, options, unit, reader);
        read_requires(*rule, context, unit, reader);
        facts.translation_units.push_back(std::move(unit));
    }
    return facts;
}

} // namespace

bool P1689ImportResult::ok() const noexcept
{
    return facts.has_value() && errors.empty();
}

P1689ImportResult import_p1689(std::string_view input, std::string_view compilation_database,
                               const P1689ImportOptions &options)
{
    try
    {
        const JsonValue p1689 = detail::parse_json_value(input);
        const JsonValue commands = detail::parse_json_value(compilation_database);
        Reader reader;
        const auto sources = read_compilation_database(commands, reader);
        DependencyFacts facts = read_p1689(p1689, sources, options, reader);
        if (!reader.errors.empty())
        {
            return {.facts = std::nullopt, .errors = std::move(reader.errors)};
        }

        return {.facts = std::move(facts), .errors = {}};
    }
    catch (const JsonError &error)
    {
        return {.facts = std::nullopt, .errors = {error}};
    }
}

} // namespace cxx_modgraph
