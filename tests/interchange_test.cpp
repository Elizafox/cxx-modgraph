// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/p2977.hpp"
#include "cxx_modgraph/package_metadata.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
void require(bool value, std::string_view message)
{
    if (!value)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void imports_installed_sources_and_emits_complete_database()
{
    constexpr std::string_view metadata = R"({
      "version": 1, "package": "vendor.widgets", "dependencies": ["vendor.base"],
      "baseline-arguments": ["-std=c++23"],
      "modules": [{"logical-name":"widgets", "source":"share/widgets.cppm",
        "object":"lib/widgets.o", "compatible-bmi":"lib/widgets.pcm",
        "arguments":["clang++","-c","share/widgets.cppm"],
        "local-arguments":["-DWIDGETS_BUILD"], "requires":["base"]}]
    })";
    auto imported = cxx_modgraph::import_package_metadata(metadata, "/opt/widgets/package.json");
    require(imported.ok(), "valid package metadata was rejected");
    const auto &unit = imported.facts->translation_units.front();
    require(unit.source_path == "/opt/widgets/share/widgets.cppm",
            "source was not manifest-relative");
    require(unit.provides.front().bmi_path == "/opt/widgets/lib/widgets.pcm",
            "compatible BMI was lost");
    require(imported.facts->module_sets.front().visible_sets.front() == "vendor.base",
            "package dependency was lost");

    const std::string output = cxx_modgraph::to_p2977(*imported.facts);
    require(output.find("\"baseline-arguments\"") != std::string::npos,
            "baseline arguments missing");
    require(output.find("\"local-arguments\"") != std::string::npos, "local arguments missing");
    require(output.find("/opt/widgets/share/widgets.cppm") != std::string::npos, "source missing");
    require(output.find("/opt/widgets/lib/widgets.pcm") != std::string::npos, "BMI missing");
}
} // namespace

int main()
{
    imports_installed_sources_and_emits_complete_database();
}
