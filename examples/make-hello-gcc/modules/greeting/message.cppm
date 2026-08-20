// SPDX-License-Identifier: 0BSD

export module greeting:message;

import :detail;

export const char *message()
{
    return audience();
}
