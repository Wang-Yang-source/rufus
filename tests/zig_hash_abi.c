/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../src/zig/hash.h"
#include <string.h>

int main(void)
{
    static const uint8_t expected[16] = {
        0x90, 0x01, 0x50, 0x98, 0x3c, 0xd2, 0x4f, 0xb0,
        0xd6, 0x96, 0x3f, 0x7d, 0x28, 0xe1, 0x7f, 0x72
    };
    uint8_t output[16];
    if (!rufus_hash_buffer(0, (const uint8_t*)"abc", 3, output))
        return 1;
    return memcmp(output, expected, sizeof(output)) != 0;
}
