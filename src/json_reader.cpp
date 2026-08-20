// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/json.hpp"
#include "json_dom.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <istream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace cxx_modgraph
{
namespace
{

using detail::JsonValue;

class DuplicateProperty : public std::runtime_error
{
public:
    explicit DuplicateProperty(std::string name)
        : std::runtime_error("duplicate object property '" + name + "'")
    {
    }
};

JsonValue convert_json(const nlohmann::json &value)
{
    if (value.is_null())
    {
        return JsonValue{nullptr};
    }

    if (value.is_boolean())
    {
        return JsonValue{value.get<bool>()};
    }

    if (value.is_number_unsigned())
    {
        const std::uint64_t number = value.get<std::uint64_t>();
        if (number > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            throw JsonError{1, 1, "integer is out of range"};
        }

        return JsonValue{static_cast<std::int64_t>(number)};
    }

    if (value.is_number_integer())
    {
        return JsonValue{value.get<std::int64_t>()};
    }

    if (value.is_number_float())
    {
        throw JsonError{1, 1, "dependency facts require integer JSON numbers"};
    }

    if (value.is_string())
    {
        return JsonValue{value.get<std::string>()};
    }

    if (value.is_array())
    {
        JsonValue::Array result;
        result.reserve(value.size());
        for (const nlohmann::json &item : value)
        {
            result.push_back(convert_json(item));
        }
        return JsonValue{std::move(result)};
    }

    JsonValue::Object result;
    for (auto item = value.begin(); item != value.end(); ++item)
    {
        result.emplace(item.key(), convert_json(item.value()));
    }

    return JsonValue{std::move(result)};
}

JsonError syntax_error(std::string_view input, const nlohmann::json::parse_error &error)
{
    const std::size_t offset = error.byte == 0 ? 0 : std::min(error.byte - 1, input.size());
    std::size_t line = 1;
    std::size_t column = 1;
    for (std::size_t index = 0; index < offset; ++index)
    {
        if (input[index] == '\n')
        {
            ++line;
            column = 1;
        }
        else
        {
            ++column;
        }
    }

    return {line, column, error.what()};
}

class Decoder
{
public:
    DependencyFacts decode(const JsonValue &value)
    {
        const JsonValue::Object *root = object(&value, "root");
        if (root == nullptr)
        {
            return {};
        }

        reject_unknown(*root,
                       {"version", "source-root", "translation-units", "external-modules", "inputs",
                        "module-sets", "bmi-cache"},
                       "root");

        DependencyFacts facts;
        if (const std::int64_t *version = integer(property(*root, "version", "root"), "version"))
        {
            if (*version < 0 ||
                static_cast<std::uint64_t>(*version) > std::numeric_limits<unsigned int>::max())
            {
                error("version must fit in an unsigned integer");
            }
            else
            {
                facts.version = static_cast<unsigned int>(*version);
            }
        }

        if (const std::string *root_path =
                string(property(*root, "source-root", "root"), "source-root"))
        {
            facts.source_root = *root_path;
        }

        decode_units(property(*root, "translation-units", "root"), facts);
        decode_external(property(*root, "external-modules", "root"), facts);
        decode_inputs(property(*root, "inputs", "root", false), facts);
        decode_sets(property(*root, "module-sets", "root", false), facts);
        decode_cache(property(*root, "bmi-cache", "root", false), facts);

        return facts;
    }

    std::vector<JsonError> take_errors()
    {
        return std::move(errors_);
    }

private:
    void error(std::string message)
    {
        errors_.push_back({1, 1, std::move(message)});
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
            error(std::string(context) + " is missing required property '" + std::string(name) +
                  "'");
        return nullptr;
    }

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

    const bool *boolean(const JsonValue *value, std::string_view context)
    {
        if (value == nullptr)
            return nullptr;
        const auto *result = std::get_if<bool>(&value->value);
        if (result == nullptr)
            error(std::string(context) + " must be a boolean");
        return result;
    }

    void reject_unknown(const JsonValue::Object &value,
                        std::initializer_list<std::string_view> accepted, std::string_view context)
    {
        for (const auto &[name, unused] : value)
        {
            static_cast<void>(unused);
            if (std::ranges::find(accepted, name) == accepted.end())
            {
                error(std::string(context) + " contains unknown property '" + name + "'");
            }
        }
    }

    void decode_units(const JsonValue *value, DependencyFacts &facts)
    {
        const JsonValue::Array *units = array(value, "translation-units");
        if (units == nullptr)
        {
            return;
        }

        for (std::size_t index = 0; index < units->size(); ++index)
        {
            const std::string context = "translation-units[" + std::to_string(index) + "]";
            const JsonValue::Object *item = object(&(*units)[index], context);
            if (item == nullptr)
            {
                continue;
            }

            reject_unknown(*item,
                           {"source", "object", "provides", "requires", "dependency-reasons",
                            "provenance", "arguments", "local-arguments", "work-directory",
                            "module-set", "private", "bmi-compatibility"},
                           context);
            TranslationUnit unit;
            if (const std::string *source =
                    string(property(*item, "source", context), context + ".source"))
            {
                unit.source_path = *source;
            }

            if (const std::string *object_path =
                    string(property(*item, "object", context), context + ".object"))
            {
                unit.object_path = *object_path;
            }

            decode_provides(property(*item, "provides", context), context, unit);
            decode_requires(property(*item, "requires", context), context, unit);
            decode_reasons(property(*item, "dependency-reasons", context, false), context, unit);
            decode_provenance(property(*item, "provenance", context, false), context, unit);
            decode_strings(property(*item, "arguments", context, false), context + ".arguments",
                           unit.arguments);
            decode_strings(property(*item, "local-arguments", context, false),
                           context + ".local-arguments", unit.local_arguments);
            if (auto x = string(property(*item, "work-directory", context, false),
                                context + ".work-directory"))
                unit.work_directory = *x;
            if (auto x =
                    string(property(*item, "module-set", context, false), context + ".module-set"))
                unit.module_set = *x;
            if (auto x = boolean(property(*item, "private", context, false), context + ".private"))
                unit.is_private = *x;
            decode_compatibility(property(*item, "bmi-compatibility", context, false),
                                 context + ".bmi-compatibility", unit.bmi_compatibility);
            facts.translation_units.push_back(std::move(unit));
        }
    }

    void decode_strings(const JsonValue *value, const std::string &context,
                        std::vector<std::string> &result)
    {
        if (!value)
            return;
        if (const auto *items = array(value, context))
            for (std::size_t i = 0; i < items->size(); ++i)
                if (auto x = string(&(*items)[i], context + "[" + std::to_string(i) + "]"))
                    result.push_back(*x);
    }

    void decode_compatibility(const JsonValue *value, const std::string &context,
                              BmiCompatibility &result)
    {
        if (!value)
            return;
        const auto *item = object(value, context);
        if (!item)
            return;
        reject_unknown(*item,
                       {"compiler-executable", "compiler-version", "target-triple", "sysroot",
                        "language-standard", "standard-library", "configuration", "user-key",
                        "adapter-keys"},
                       context);
        const auto get = [&](std::string_view name, std::string &out)
        {
            if (auto x = string(property(*item, name, context, false),
                                context + "." + std::string(name)))
                out = *x;
        };
        get("compiler-executable", result.compiler_executable);
        get("compiler-version", result.compiler_version);
        get("target-triple", result.target_triple);
        get("sysroot", result.sysroot);
        get("language-standard", result.language_standard);
        get("standard-library", result.standard_library);
        get("configuration", result.configuration);
        get("user-key", result.user_key);
        decode_strings(property(*item, "adapter-keys", context, false), context + ".adapter-keys",
                       result.adapter_keys);
    }

    void decode_cache(const JsonValue *value, DependencyFacts &facts)
    {
        if (!value)
            return;
        const auto *items = array(value, "bmi-cache");
        if (!items)
            return;
        for (std::size_t i = 0; i < items->size(); ++i)
        {
            const std::string c = "bmi-cache[" + std::to_string(i) + "]";
            const auto *item = object(&(*items)[i], c);
            if (!item)
                continue;
            reject_unknown(*item,
                           {"module", "module-set", "source-digest", "recipe-digest",
                            "compatibility-key", "bmi-digest", "object-digest"},
                           c);
            BmiCacheRecord r;
            const auto get = [&](std::string_view name, std::string &out, bool required = true)
            {
                if (auto x =
                        string(property(*item, name, c, required), c + "." + std::string(name)))
                    out = *x;
            };
            get("module", r.module);
            get("module-set", r.module_set, false);
            get("source-digest", r.source_digest);
            get("recipe-digest", r.recipe_digest);
            get("compatibility-key", r.compatibility_key);
            get("bmi-digest", r.bmi_digest);
            get("object-digest", r.object_digest, false);
            facts.bmi_cache.push_back(std::move(r));
        }
    }

    void decode_sets(const JsonValue *value, DependencyFacts &facts)
    {
        if (!value)
            return;
        const auto *items = array(value, "module-sets");
        if (!items)
            return;
        for (std::size_t i = 0; i < items->size(); ++i)
        {
            const std::string context = "module-sets[" + std::to_string(i) + "]";
            const auto *item = object(&(*items)[i], context);
            if (!item)
                continue;
            reject_unknown(*item, {"name", "family-name", "baseline-arguments", "visible-sets"},
                           context);
            ModuleSet set;
            if (auto x = string(property(*item, "name", context), context + ".name"))
                set.name = *x;
            if (auto x = string(property(*item, "family-name", context), context + ".family-name"))
                set.family_name = *x;
            decode_strings(property(*item, "baseline-arguments", context),
                           context + ".baseline-arguments", set.baseline_arguments);
            decode_strings(property(*item, "visible-sets", context, false),
                           context + ".visible-sets", set.visible_sets);
            facts.module_sets.push_back(std::move(set));
        }
    }

    void decode_provides(const JsonValue *value, const std::string &context, TranslationUnit &unit)
    {
        const JsonValue::Array *modules = array(value, context + ".provides");
        if (modules == nullptr)
        {
            return;
        }

        for (std::size_t index = 0; index < modules->size(); ++index)
        {
            const std::string item_context = context + ".provides[" + std::to_string(index) + "]";
            const JsonValue::Object *item = object(&(*modules)[index], item_context);
            if (item == nullptr)
            {
                continue;
            }

            reject_unknown(*item, {"name", "bmi", "is-interface", "lookup-method"}, item_context);
            ProvidedModule module;
            if (const std::string *name =
                    string(property(*item, "name", item_context), item_context + ".name"))
            {
                module.name = *name;
            }

            if (const std::string *bmi =
                    string(property(*item, "bmi", item_context), item_context + ".bmi"))
            {
                module.bmi_path = *bmi;
            }
            if (const bool *is_interface =
                    boolean(property(*item, "is-interface", item_context, false),
                            item_context + ".is-interface"))
                module.is_interface = *is_interface;
            if (const std::string *lookup =
                    string(property(*item, "lookup-method", item_context, false),
                           item_context + ".lookup-method"))
                module.lookup_method = *lookup;

            unit.provides.push_back(std::move(module));
        }
    }

    void decode_reasons(const JsonValue *value, const std::string &context, TranslationUnit &unit)
    {
        if (value == nullptr)
            return;
        const auto *items = array(value, context + ".dependency-reasons");
        if (!items)
            return;
        for (std::size_t i = 0; i < items->size(); ++i)
        {
            const std::string c = context + ".dependency-reasons[" + std::to_string(i) + "]";
            const auto *item = object(&(*items)[i], c);
            if (!item)
                continue;
            reject_unknown(*item, {"module", "reason", "lookup-method", "raw-requirement"}, c);
            DependencyReason reason;
            if (auto p = string(property(*item, "module", c), c + ".module"))
                reason.module = *p;
            if (auto p = string(property(*item, "reason", c), c + ".reason"))
                reason.reason = *p;
            if (auto p = string(property(*item, "lookup-method", c), c + ".lookup-method"))
                reason.lookup_method = *p;
            if (auto p =
                    string(property(*item, "raw-requirement", c, false), c + ".raw-requirement"))
                reason.raw_requirement_json = *p;
            unit.dependency_reasons.push_back(std::move(reason));
        }
    }

    void decode_provenance(const JsonValue *value, const std::string &context,
                           TranslationUnit &unit)
    {
        if (!value)
            return;
        const auto *item = object(value, context + ".provenance");
        if (!item)
            return;
        reject_unknown(*item,
                       {"rule-source", "scanner", "scanner-version", "original-output",
                        "source-lookup", "source-path-unique", "raw-rule"},
                       context + ".provenance");
        ScanProvenance p;
        if (auto x = string(property(*item, "rule-source", context, false), "rule-source"))
            p.rule_source = *x;
        if (auto x = string(property(*item, "scanner", context, false), "scanner"))
            p.scanner = *x;
        if (auto x = string(property(*item, "scanner-version", context, false), "scanner-version"))
            p.scanner_version = *x;
        if (auto x = string(property(*item, "original-output", context, false), "original-output"))
            p.original_output = *x;
        if (auto x = string(property(*item, "source-lookup", context, false), "source-lookup"))
            p.source_lookup = *x;
        if (auto x = boolean(property(*item, "source-path-unique", context, false),
                             "source-path-unique"))
            p.source_path_unique = *x;
        if (auto x = string(property(*item, "raw-rule", context, false), "raw-rule"))
            p.raw_rule_json = *x;
        unit.provenance = std::move(p);
    }

    void decode_inputs(const JsonValue *value, DependencyFacts &facts)
    {
        if (!value)
            return;
        const auto *items = array(value, "inputs");
        if (!items)
            return;
        for (std::size_t i = 0; i < items->size(); ++i)
        {
            const std::string c = "inputs[" + std::to_string(i) + "]";
            const auto *item = object(&(*items)[i], c);
            if (!item)
                continue;
            reject_unknown(*item, {"path", "digest"}, c);
            InputArtifact a;
            if (auto x = string(property(*item, "path", c), c + ".path"))
                a.path = *x;
            if (auto x = string(property(*item, "digest", c), c + ".digest"))
                a.digest = *x;
            facts.inputs.push_back(std::move(a));
        }
    }

    void decode_requires(const JsonValue *value, const std::string &context, TranslationUnit &unit)
    {
        const JsonValue::Array *modules = array(value, context + ".requires");
        if (modules == nullptr)
        {
            return;
        }

        for (std::size_t index = 0; index < modules->size(); ++index)
        {
            const std::string item_context = context + ".requires[" + std::to_string(index) + "]";
            if (const std::string *name = string(&(*modules)[index], item_context))
            {
                unit.required_modules.push_back(*name);
            }
        }
    }

    void decode_external(const JsonValue *value, DependencyFacts &facts)
    {
        const JsonValue::Array *modules = array(value, "external-modules");
        if (modules == nullptr)
        {
            return;
        }

        for (std::size_t index = 0; index < modules->size(); ++index)
        {
            const std::string context = "external-modules[" + std::to_string(index) + "]";
            const JsonValue::Object *item = object(&(*modules)[index], context);
            if (item == nullptr)
            {
                continue;
            }

            reject_unknown(*item, {"name", "bmi", "module-set", "bmi-compatibility"}, context);
            ExternalModule module;
            if (const std::string *name =
                    string(property(*item, "name", context), context + ".name"))
            {
                module.name = *name;
            }

            if (const std::string *bmi = string(property(*item, "bmi", context), context + ".bmi"))
            {
                module.bmi_path = *bmi;
            }
            if (auto x =
                    string(property(*item, "module-set", context, false), context + ".module-set"))
                module.module_set = *x;
            decode_compatibility(property(*item, "bmi-compatibility", context, false),
                                 context + ".bmi-compatibility", module.bmi_compatibility);

            facts.external_modules.push_back(std::move(module));
        }
    }

    std::vector<JsonError> errors_;
};

} // namespace

detail::JsonValue detail::parse_json_value(std::string_view input)
{
    std::vector<std::set<std::string>> object_keys;
    const auto callback =
        [&object_keys](int, nlohmann::json::parse_event_t event, nlohmann::json &parsed)
    {
        if (event == nlohmann::json::parse_event_t::object_start)
        {
            object_keys.emplace_back();
        }
        else if (event == nlohmann::json::parse_event_t::key)
        {
            const std::string name = parsed.get<std::string>();
            if (!object_keys.back().insert(name).second)
            {
                throw DuplicateProperty(name);
            }
        }
        else if (event == nlohmann::json::parse_event_t::object_end)
        {
            object_keys.pop_back();
        }

        return true;
    };

    try
    {
        return convert_json(nlohmann::json::parse(input.begin(), input.end(), callback));
    }
    catch (const nlohmann::json::parse_error &error)
    {
        throw syntax_error(input, error);
    }
    catch (const DuplicateProperty &error)
    {
        throw JsonError{1, 1, error.what()};
    }
}

bool JsonParseResult::ok() const noexcept
{
    return facts.has_value() && errors.empty();
}

JsonParseResult parse_json(std::string_view input)
{
    try
    {
        const JsonValue root = detail::parse_json_value(input);
        Decoder decoder;
        DependencyFacts facts = decoder.decode(root);
        std::vector<JsonError> errors = decoder.take_errors();
        if (!errors.empty())
        {
            return {.facts = std::nullopt, .errors = std::move(errors)};
        }
        return {.facts = std::move(facts), .errors = {}};
    }
    catch (const JsonError &error)
    {
        return {.facts = std::nullopt, .errors = {error}};
    }
}

JsonParseResult read_json(std::istream &input)
{
    std::ostringstream contents;
    contents << input.rdbuf();
    return parse_json(contents.str());
}

} // namespace cxx_modgraph
