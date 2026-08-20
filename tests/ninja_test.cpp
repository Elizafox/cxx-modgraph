// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/ninja.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace
{

void require(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

cxx_modgraph::DependencyFacts example_facts()
{
    cxx_modgraph::DependencyFacts facts;
    facts.source_root = "/project root";
    facts.translation_units = {{.source_path = "modules/detail.cppm",
                                .object_path = "build/obj/detail.o",
                                .provides = {{"hello:detail", "build/bmi/hello@3Adetail.pcm"}},
                                .required_modules = {}},
                               {.source_path = "modules/hello.cppm",
                                .object_path = "build/obj/hello.o",
                                .provides = {{"hello", "build/bmi/hello.pcm"}},
                                .required_modules = {"hello:detail"}},
                               {.source_path = "src/main.cpp",
                                .object_path = "build/obj/main.o",
                                .provides = {},
                                .required_modules = {"hello"}}};
    return facts;
}

} // namespace

int main()
{
    const std::string output = cxx_modgraph::to_ninja(example_facts());
    require(output.find("build build/bmi/hello.pcm: cxx_modgraph_bmi "
                        "/project$ root/modules/hello.cppm | build/bmi/hello@3Adetail.pcm") !=
                std::string::npos,
            "BMI edge or Ninja path escaping was incorrect");
    require(output.find("module_flags = -fmodule-file='hello$:detail="
                        "build/bmi/hello@3Adetail.pcm'") != std::string::npos,
            "partition mapping was not escaped and emitted");
    require(output.find("build build/obj/hello.o: cxx_modgraph_module_object ") !=
                    std::string::npos &&
                output.find("  provided_bmi = build/bmi/hello.pcm") != std::string::npos,
            "module object edge was not emitted");
    require(output.find("build cxx_modgraph_outputs: phony") != std::string::npos,
            "aggregate output target was not emitted");

    auto reversed = example_facts();
    std::ranges::reverse(reversed.translation_units);
    require(output == cxx_modgraph::to_ninja(reversed),
            "Ninja output depends on fact insertion order");
}
