// SPDX-License-Identifier: GPL-3.0-or-later
const std = @import("std");

/// C ABI: algorithm IDs match enum hash_type in rufus.h. The caller provides
/// 16/20/32/64 writable output bytes. NULL input is valid only for length zero.
/// Returns 1 on success, 0 on invalid arguments; failure leaves output unchanged.
export fn rufus_hash_buffer(kind: c_uint, input: ?[*]const u8, len: usize, output: ?[*]u8) c_int {
    const dest = output orelse return 0;
    if (input == null and len != 0) return 0;
    const bytes: []const u8 = if (input) |ptr| ptr[0..len] else &.{};
    switch (kind) {
        0 => std.crypto.hash.Md5.hash(bytes, dest[0..16], .{}),
        1 => std.crypto.hash.Sha1.hash(bytes, dest[0..20], .{}),
        2 => std.crypto.hash.sha2.Sha256.hash(bytes, dest[0..32], .{}),
        3 => std.crypto.hash.sha2.Sha512.hash(bytes, dest[0..64], .{}),
        else => return 0,
    }
    return 1;
}
