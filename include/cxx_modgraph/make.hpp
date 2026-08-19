// SPDX-License-Identifier: 0BSD

#pragma once

#include "cxx_modgraph/facts.hpp"

#include <iosfwd>
#include <string>

namespace cxx_modgraph
{

void write_make(std::ostream &output, const DependencyFacts &facts);
[[nodiscard]] std::string to_make(const DependencyFacts &facts);

} // namespace cxx_modgraph
