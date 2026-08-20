// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/facts.hpp"

#include <iosfwd>
#include <string>

namespace cxx_modgraph
{

// Emit the standalone build database proposed by P2977R1 (version 1 revision 0).
void write_p2977(std::ostream &output, const DependencyFacts &facts);
[[nodiscard]] std::string to_p2977(const DependencyFacts &facts);

} // namespace cxx_modgraph
