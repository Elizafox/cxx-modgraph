// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/p2977.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <ostream>
#include <sstream>

namespace cxx_modgraph
{
namespace
{
std::string path_string(const std::filesystem::path &path)
{
    return path.lexically_normal().generic_string();
}
} // namespace

void write_p2977(std::ostream &output, const DependencyFacts &facts)
{
    using json = nlohmann::json;
    std::map<std::string, ModuleSet> sets;
    for (const auto &set : facts.module_sets)
        sets.insert_or_assign(set.name, set);
    for (const auto &unit : facts.translation_units)
    {
        const std::string name = unit.module_set.empty() ? "default" : unit.module_set;
        sets.try_emplace(name, ModuleSet{name, name, {}, {}});
    }

    json document = {{"version", 1}, {"revision", 0}, {"sets", json::array()}};
    for (auto &[name, set] : sets)
    {
        json output_set = {{"name", name},
                           {"family-name", set.family_name.empty() ? name : set.family_name},
                           {"baseline-arguments", set.baseline_arguments},
                           {"visible-sets", set.visible_sets},
                           {"translation-units", json::array()}};
        std::vector<const TranslationUnit *> units;
        for (const auto &unit : facts.translation_units)
            if ((unit.module_set.empty() ? "default" : unit.module_set) == name)
                units.push_back(&unit);
        std::ranges::sort(units, {},
                          [](const auto *unit) { return path_string(unit->source_path); });
        for (const auto *unit : units)
        {
            json provides = json::object();
            for (const auto &module : unit->provides)
                provides[module.name] = path_string(module.bmi_path);
            json translation = {
                {"source", path_string(unit->source_path)}, {"arguments", unit->arguments},
                {"local-arguments", unit->local_arguments}, {"private", unit->is_private},
                {"provides", std::move(provides)},          {"requires", unit->required_modules}};
            if (!unit->object_path.empty())
                translation["object"] = path_string(unit->object_path);
            if (!unit->work_directory.empty())
                translation["work-directory"] = path_string(unit->work_directory);
            output_set["translation-units"].push_back(std::move(translation));
        }
        document["sets"].push_back(std::move(output_set));
    }
    output << document.dump(2) << '\n';
}

std::string to_p2977(const DependencyFacts &facts)
{
    std::ostringstream output;
    write_p2977(output, facts);
    return output.str();
}
} // namespace cxx_modgraph
