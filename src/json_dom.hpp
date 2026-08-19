// SPDX-License-Identifier: 0BSD

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace cxx_modgraph::detail
{

struct JsonValue
{
    using Object = std::map<std::string, JsonValue>;
    using Array = std::vector<JsonValue>;

    std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object> value;
};

[[nodiscard]] JsonValue parse_json_value(std::string_view input);

} // namespace cxx_modgraph::detail
