// SPDX-License-Identifier: 0BSD

export module greeting:detail;

import std;

export constexpr const char *audience()
{
    return "GCC module graph";
}
