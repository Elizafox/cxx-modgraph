// SPDX-License-Identifier: 0BSD

export module greeting:message;

import :audience;
import :punctuation;
import support.logging;
import std;

export std::string message()
{
    return log_prefix() + "Hello, " + audience() + punctuation();
}
