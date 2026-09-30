/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RUFUS_ZIG_HASH_H
#define RUFUS_ZIG_HASH_H
#include <stddef.h>
#include <stdint.h>
/* C calling convention, including on 32-bit Windows. Output must hold the
 * digest size for kind (MD5=0, SHA1=1, SHA256=2, SHA512=3).
 * NULL input is accepted only for len=0. Returns 1 on success, 0 on failure. */
int rufus_hash_buffer(unsigned kind, const uint8_t* input, size_t len, uint8_t* output);
#endif
