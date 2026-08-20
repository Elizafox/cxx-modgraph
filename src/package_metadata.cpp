// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/package_metadata.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace cxx_modgraph
{
namespace
{
std::filesystem::path resolve(const std::filesystem::path &base, const std::string &value)
{
    std::filesystem::path path(value);
    return path.is_absolute() ? path.lexically_normal() : (base / path).lexically_normal();
}

JsonError error(std::string message)
{
    return {1, 1, std::move(message)};
}
} // namespace

bool PackageImportResult::ok() const noexcept
{
    return facts.has_value() && errors.empty();
}

PackageImportResult import_package_metadata(std::string_view input,
                                            const std::filesystem::path &manifest_path)
{
    using json = nlohmann::json;
    try
    {
        const json root = json::parse(input);
        if (!root.is_object() || root.value("version", 0) != 1 || !root.contains("package") ||
            !root["package"].is_string() || !root.contains("modules") ||
            !root["modules"].is_array())
            return {.errors = {error("package metadata requires version 1, package, and modules")}};

        DependencyFacts facts;
        const std::filesystem::path base = manifest_path.parent_path();
        const std::string package = root["package"].get<std::string>();
        for (std::size_t i = 0; i < root["modules"].size(); ++i)
        {
            const json &item = root["modules"][i];
            const std::string context = "modules[" + std::to_string(i) + "]";
            if (!item.is_object() || !item.contains("logical-name") ||
                !item["logical-name"].is_string() || !item.contains("source") ||
                !item["source"].is_string() || !item.contains("arguments") ||
                !item["arguments"].is_array())
                return {.errors = {error(
                            context + " requires logical-name, source, and an arguments array")}};
            TranslationUnit unit;
            unit.source_path = resolve(base, item["source"].get<std::string>());
            unit.object_path = item.contains("object")
                                   ? resolve(base, item["object"].get<std::string>())
                                   : std::filesystem::path{};
            const std::string name = item["logical-name"].get<std::string>();
            std::filesystem::path bmi;
            if (item.contains("compatible-bmi") && item["compatible-bmi"].is_string())
                bmi = resolve(base, item["compatible-bmi"].get<std::string>());
            else if (item.contains("bmi") && item["bmi"].is_string())
                bmi = resolve(base, item["bmi"].get<std::string>());
            else
                bmi = (base / "bmi" / (name + ".pcm")).lexically_normal();
            unit.provides.push_back({name, bmi, true, "installed package metadata"});
            unit.arguments = item["arguments"].get<std::vector<std::string>>();
            unit.local_arguments = item.value("local-arguments", std::vector<std::string>{});
            unit.required_modules = item.value("requires", std::vector<std::string>{});
            unit.work_directory = item.contains("working-directory")
                                      ? resolve(base, item["working-directory"].get<std::string>())
                                      : base;
            unit.module_set = item.value("module-set", package);
            unit.is_private = item.value("private", false);
            facts.translation_units.push_back(std::move(unit));
        }
        auto visible = root.value("visible-sets", std::vector<std::string>{});
        const auto dependencies = root.value("dependencies", std::vector<std::string>{});
        visible.insert(visible.end(), dependencies.begin(), dependencies.end());
        ModuleSet set{package, root.value("family-name", package),
                      root.value("baseline-arguments", std::vector<std::string>{}),
                      std::move(visible)};
        facts.module_sets.push_back(std::move(set));
        return {.facts = std::move(facts), .errors = {}};
    }
    catch (const json::exception &exception)
    {
        return {.errors = {error(std::string("invalid package metadata: ") + exception.what())}};
    }
}
} // namespace cxx_modgraph
