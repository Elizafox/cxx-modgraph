// SPDX-License-Identifier: 0BSD

#include "cxx_modgraph/p1689.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{

constexpr std::string_view p1689 = R"({
  "version": 1,
  "revision": 0,
  "rules": [
    {
      "primary-output": "build/obj/hello.o",
      "provides": [{"logical-name": "hello:detail", "is-interface": true}],
      "requires": [{"logical-name": "std"}]
    },
    {
      "primary-output": "build/obj/main.o",
      "requires": [{"logical-name": "hello:detail"}]
    }
  ]
})";

constexpr std::string_view commands = R"([
  {"directory": "/project", "file": "src/hello.cppm",
   "output": "build/obj/hello.o", "command": "clang++ -c src/hello.cppm"},
  {"directory": "/project", "file": "src/main.cpp",
   "output": "build/obj/main.o", "command": "clang++ -c src/main.cpp"}
])";

void require(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void imports_p1689_with_compilation_database_sources()
{
    cxx_modgraph::P1689ImportOptions options;
    options.bmi_directory = "out/bmi";
    options.external_modules = {{"std", "toolchain/std.pcm"}};

    const cxx_modgraph::P1689ImportResult result =
        cxx_modgraph::import_p1689(p1689, commands, options);

    require(result.ok(), "valid P1689 input was rejected");
    require(result.facts->translation_units.size() == 2, "P1689 rules were not imported");
    require(result.facts->translation_units[0].source_path == "src/hello.cppm",
            "source was not obtained from the compilation database");
    require(result.facts->translation_units[0].provides[0].bmi_path == "out/bmi/hello-detail.pcm",
            "Clang partition BMI name was not derived correctly");
    require(cxx_modgraph::analyze(*result.facts).ok(), "imported facts did not form a valid graph");
}

void requires_matching_compilation_database_output()
{
    const cxx_modgraph::P1689ImportResult result = cxx_modgraph::import_p1689(p1689, "[]");

    require(!result.ok(), "unmapped P1689 outputs were accepted");
    require(result.errors.size() == 2, "unmapped outputs were not diagnosed");
}

} // namespace

int main()
{
    imports_p1689_with_compilation_database_sources();
    requires_matching_compilation_database_output();
}
