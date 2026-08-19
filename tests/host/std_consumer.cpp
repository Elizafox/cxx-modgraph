// SPDX-License-Identifier: 0BSD

import std.compat;

int main()
{
    std::vector<int> values{1, 2, 3};
    return std::accumulate(values.begin(), values.end(), 0) == 6 ? 0 : 1;
}
