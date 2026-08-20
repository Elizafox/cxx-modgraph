// SPDX-License-Identifier: 0BSD
#include "cxx_modgraph/hermetic.hpp"
#include "cxx_modgraph/json.hpp"
#include <cstdlib>
#include <iostream>
#include <string_view>

void require(bool value, std::string_view message)
{
    if (!value)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

int main()
{
    cxx_modgraph::DependencyFacts facts;
    facts.path_remappings.push_back({"/workspace", "/src"});
    facts.environment_inputs.push_back({"SDKROOT", "sha256:123"});
    facts.tools.push_back({"scanner", "/tools/scan", "sha256:456", "1.0", "host"});
    facts.content_digests.push_back({"sysroot", "/sdk", "sha256:789", "target"});
    facts.graph_digest = cxx_modgraph::graph_record_digest(facts);
    const auto json = cxx_modgraph::to_json(facts);
    const auto parsed = cxx_modgraph::parse_json(json);
    require(parsed.ok(), "hermetic canonical metadata did not round trip");
    require(parsed.facts->tools.front().execution_namespace == "host", "tool namespace was lost");
    require(cxx_modgraph::remap_path("/workspace/lib/a.cpp", facts.path_remappings) ==
                "/src/lib/a.cpp",
            "canonical path remapping failed");
    require(cxx_modgraph::graph_record_digest(*parsed.facts) == facts.graph_digest,
            "content-addressed graph digest was unstable");
}
