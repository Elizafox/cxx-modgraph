// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/facts.hpp"
#include "cxx_modgraph/json.hpp"

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
    facts.source_root = "..";
    facts.translation_units = {{.source_path = "src/geometry.cppm",
                                .object_path = "build/obj/geometry.o",
                                .provides = {{"geometry", "build/bmi/geometry.pcm"}},
                                .required_modules = {"math"}},
                               {.source_path = "src/math.cppm",
                                .object_path = "build/obj/math.o",
                                .provides = {{"math", "build/bmi/math.pcm"}},
                                .required_modules = {}}};
    facts.external_modules = {{"std", "toolchain/bmi/std.pcm"}};
    return facts;
}

void analyzes_module_providers()
{
    const cxx_modgraph::AnalysisResult result = cxx_modgraph::analyze(example_facts());

    require(result.ok(), "valid facts produced diagnostics");
    require(result.graph.topological_sort().plan.topological_order ==
                std::vector<cxx_modgraph::NodeId>{"src/math.cppm", "src/geometry.cppm"},
            "provider dependency was not represented in the graph");
}

void diagnoses_invalid_facts()
{
    cxx_modgraph::DependencyFacts facts = example_facts();
    facts.translation_units[1].provides[0].name = "geometry";
    facts.translation_units[0].required_modules = {"missing"};

    const cxx_modgraph::AnalysisResult result = cxx_modgraph::analyze(facts);

    require(!result.ok(), "invalid facts were accepted");
    require(result.diagnostics.size() == 2, "unexpected diagnostic count");
    require(result.diagnostics[0].code == cxx_modgraph::DiagnosticCode::duplicate_provider,
            "duplicate provider was not diagnosed first");
    require(result.diagnostics[1].code == cxx_modgraph::DiagnosticCode::missing_provider,
            "missing provider was not diagnosed");
}

void emits_deterministic_json()
{
    cxx_modgraph::DependencyFacts first = example_facts();
    cxx_modgraph::DependencyFacts second = example_facts();
    std::ranges::reverse(second.translation_units);

    const std::string json = cxx_modgraph::to_json(first);
    require(json == cxx_modgraph::to_json(second), "JSON depends on translation unit order");
    require(json.find("\"source-root\": \"..\"") != std::string::npos,
            "source root was not emitted");
    require(json.find("src/geometry.cppm") < json.find("src/math.cppm"),
            "translation units were not sorted by source path");
}

void parses_emitted_json()
{
    const cxx_modgraph::DependencyFacts original = example_facts();
    const cxx_modgraph::JsonParseResult parsed =
        cxx_modgraph::parse_json(cxx_modgraph::to_json(original));

    require(parsed.ok(), "emitted JSON could not be parsed");
    require(parsed.facts->translation_units.size() == 2, "translation units were not parsed");
    require(parsed.facts->translation_units[0].source_path == "src/geometry.cppm",
            "source path was not parsed");
    require(parsed.facts->translation_units[0].required_modules == std::vector<std::string>{"math"},
            "required modules were not parsed");
}

void diagnoses_json_errors()
{
    const cxx_modgraph::JsonParseResult syntax = cxx_modgraph::parse_json("{\n  nope\n}");
    require(!syntax.ok(), "invalid JSON syntax was accepted");
    require(syntax.errors[0].line == 2, "syntax error line was incorrect");

    const cxx_modgraph::JsonParseResult shape = cxx_modgraph::parse_json("{}");
    require(!shape.ok(), "JSON with missing properties was accepted");
    require(shape.errors.size() == 4, "missing properties were not diagnosed");

    const cxx_modgraph::JsonParseResult duplicate = cxx_modgraph::parse_json(
        R"({"version":1,"version":1,"source-root":".","translation-units":[],"external-modules":[]})");
    require(!duplicate.ok(), "duplicate JSON property was accepted");
    require(duplicate.errors[0].message.find("duplicate object property 'version'") !=
                std::string::npos,
            "duplicate JSON property diagnostic was incorrect");
}

void plans_std_compat_after_std()
{
    cxx_modgraph::DependencyFacts facts;
    facts.translation_units = {{.source_path = "toolchain/std.cppm",
                                .object_path = "build/obj/std.o",
                                .provides = {{"std", "build/bmi/std.pcm"}},
                                .required_modules = {}},
                               {.source_path = "toolchain/std.compat.cppm",
                                .object_path = "build/obj/std.compat.o",
                                .provides = {{"std.compat", "build/bmi/std.compat.pcm"}},
                                .required_modules = {"std"}},
                               {.source_path = "src/main.cpp",
                                .object_path = "build/obj/main.o",
                                .provides = {},
                                .required_modules = {"std.compat"}}};

    const cxx_modgraph::AnalysisResult result = cxx_modgraph::analyze(facts);

    require(result.ok(), "std and std.compat facts produced diagnostics");
    require(result.graph.topological_sort().plan.parallel_levels ==
                std::vector<std::vector<cxx_modgraph::NodeId>>{
                    {"toolchain/std.cppm"}, {"toolchain/std.compat.cppm"}, {"src/main.cpp"}},
            "std.compat was not planned after std");
}

void separates_provider_compatibility_scopes()
{
    cxx_modgraph::TranslationUnit debug{.source_path = "src/foo.cppm",
                                        .object_path = "debug/foo.o",
                                        .provides = {{"foo", "debug/foo.pcm"}}};
    debug.bmi_compatibility.configuration = "debug";
    cxx_modgraph::TranslationUnit asan = debug;
    asan.object_path = "asan/foo.o";
    asan.provides.front().bmi_path = "asan/foo.pcm";
    asan.bmi_compatibility.configuration = "asan";
    cxx_modgraph::TranslationUnit consumer{
        .source_path = "src/main.cpp", .object_path = "asan/main.o", .required_modules = {"foo"}};
    consumer.bmi_compatibility.configuration = "asan";
    cxx_modgraph::DependencyFacts facts;
    facts.translation_units = {debug, asan, consumer};
    const auto result = cxx_modgraph::analyze(facts);
    require(result.ok(), "configuration-specific translations were rejected");
    require(result.graph.topological_sort().plan.topological_order.back() == "src/main.cpp",
            "consumer did not use the provider in its compatibility scope");
}

void round_trips_compatibility_and_cache_records()
{
    auto facts = example_facts();
    auto &compat = facts.translation_units.front().bmi_compatibility;
    compat.compiler_executable = "/usr/bin/clang++";
    compat.compiler_version = "20.1";
    compat.target_triple = "x86_64-linux-gnu";
    compat.language_standard = "c++23";
    compat.configuration = "asan";
    compat.user_key = "vendor-policy-v2";
    compat.adapter_keys = {"clang-modules-revision=7"};
    facts.bmi_cache.push_back({"geometry", "default", "sha256:source", "sha256:recipe",
                               "sha256:compat", "sha256:bmi", "sha256:object"});
    const auto parsed = cxx_modgraph::parse_json(cxx_modgraph::to_json(facts));
    require(parsed.ok(), "compatibility/cache JSON did not parse");
    require(parsed.facts->translation_units.front().bmi_compatibility == compat,
            "BMI compatibility scope did not round trip");
    require(parsed.facts->bmi_cache.size() == 1 &&
                parsed.facts->bmi_cache.front().recipe_digest == "sha256:recipe",
            "BMI cache record did not round trip");
}

} // namespace

int main()
{
    analyzes_module_providers();
    diagnoses_invalid_facts();
    emits_deterministic_json();
    parses_emitted_json();
    diagnoses_json_errors();
    plans_std_compat_after_std();
    separates_provider_compatibility_scopes();
    round_trips_compatibility_and_cache_records();
}
