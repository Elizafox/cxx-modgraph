// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/make.hpp"

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
    facts.source_root = "/project";
    facts.translation_units = {{.source_path = "modules/std.compat.cppm",
                                .object_path = "build/obj/std.compat.o",
                                .provides = {{"std.compat", "build/bmi/std.compat.pcm"}},
                                .required_modules = {"std"}},
                               {.source_path = "modules/std.cppm",
                                .object_path = "build/obj/std.o",
                                .provides = {{"std", "build/bmi/std.pcm"}},
                                .required_modules = {}},
                               {.source_path = "src/main.cpp",
                                .object_path = "build/obj/main.o",
                                .provides = {},
                                .required_modules = {"std.compat", "vendor.logging"}}};
    facts.external_modules = {{"vendor.logging", "vendor/bmi/logging.pcm"}};
    return facts;
}

void emits_make_prerequisites_and_metadata()
{
    const std::string output = cxx_modgraph::to_make(example_facts());

    require(output.find("CXX_MODGRAPH_BMI_TARGETS := \\\n"
                        "    build/bmi/std.compat.pcm \\\n"
                        "    build/bmi/std.pcm") != std::string::npos,
            "BMI target list was not emitted deterministically");
    require(output.find("CXX_MODGRAPH_OUTPUT_GROUPS := \\\n"
                        "    build/bmi/std.compat.pcm=build/obj/std.compat.o \\\n"
                        "    build/bmi/std.pcm=build/obj/std.o") != std::string::npos,
            "combined BMI and object output groups were not emitted");
    require(output.find("build/bmi/std.compat.pcm: CXX_MODGRAPH_MODULE := std.compat") !=
                std::string::npos,
            "module metadata was not emitted");
    require(output.find("build/bmi/std.compat.pcm: CXX_MODGRAPH_IMPORTS := "
                        "std=build/bmi/std.pcm") != std::string::npos,
            "named import metadata was not emitted");
    require(output.find("build/obj/main.o: CXX_MODGRAPH_IMPORTS := "
                        "std=build/bmi/std.pcm "
                        "std.compat=build/bmi/std.compat.pcm "
                        "vendor.logging=/project/vendor/bmi/logging.pcm") != std::string::npos,
            "transitive named import metadata was not emitted");
    require(output.find("build/bmi/std.compat.pcm: \\\n"
                        "    /project/modules/std.compat.cppm \\\n"
                        "    build/bmi/std.pcm") != std::string::npos,
            "std.compat BMI prerequisites were incorrect");
    require(output.find("build/obj/main.o: \\\n"
                        "    /project/src/main.cpp \\\n"
                        "    /project/vendor/bmi/logging.pcm \\\n"
                        "    build/bmi/std.compat.pcm") != std::string::npos,
            "consumer object prerequisites were incorrect");
}

void emits_deterministically()
{
    cxx_modgraph::DependencyFacts first = example_facts();
    cxx_modgraph::DependencyFacts second = example_facts();
    std::ranges::reverse(second.translation_units);
    std::ranges::reverse(second.external_modules);

    require(cxx_modgraph::to_make(first) == cxx_modgraph::to_make(second),
            "Make output depends on fact insertion order");
}

void emits_explicit_partition_mapping()
{
    cxx_modgraph::DependencyFacts facts;
    facts.translation_units = {{.source_path = "modules/detail.cppm",
                                .object_path = "build/obj/modules/detail.o",
                                .provides = {{"hello:detail", "build/bmi/hello@3Adetail.pcm"}},
                                .required_modules = {}},
                               {.source_path = "modules/hello.cppm",
                                .object_path = "build/obj/modules/hello.o",
                                .provides = {{"hello", "build/bmi/hello.pcm"}},
                                .required_modules = {"hello:detail"}}};

    const std::string output = cxx_modgraph::to_make(facts);
    require(output.find("build/bmi/hello.pcm: CXX_MODGRAPH_IMPORTS := "
                        "hello\\:detail=build/bmi/hello@3Adetail.pcm") != std::string::npos,
            "partition import was not emitted as an explicit name-to-BMI mapping");
}

} // namespace

int main()
{
    emits_make_prerequisites_and_metadata();
    emits_deterministically();
    emits_explicit_partition_mapping();
}
