# Zig buffer hash integration

The optional backend replaces only `HashBuffer` with Zig standard-library
MD5, SHA-1, SHA-256 and SHA-512. File hashing, streaming hash contexts, PE/Authenticode
region selection, Windows UI and bundled C libraries retain their existing implementations.
No hash algorithm or general-purpose library is reimplemented.

## Dependency decision

- Use `std.crypto.hash`: all four required algorithms are shipped with Zig,
  with no extra package or allocator needed for this operation.
- Keep existing C libraries for WIM, ISO/UDF, ext filesystems and boot installation.
  Their project-specific behavior is not covered by the hash migration.
- Consider zigwin32 for later Windows migrations; a binding is unnecessary here.
- Consider zig-xml for later XML work, subject to compatibility testing; its lack
  of DTD support prevents treating it as a direct ezxml replacement.

Zig's license is MIT (Expat). Preserve its required notices when distributing
standard-library code. Project integration files remain GPL-3.0-or-later.
See [Zig's license](zig-LICENSE.txt) and https://ziglang.org/documentation/0.16.0/.
Include `zig-LICENSE.txt` with distributions using this backend.

## Toolchain

Supported toolchains are **Zig 0.16.0** and the exact development build
**0.17.0-dev.2294+71403f299**, matching the locally installed compiler.
Other development snapshots require a new compatibility check and hash pin.
Official archives and SHA-256 hashes are pinned in `zig-toolchain.json`,
using the release index at https://ziglang.org/download/index.json and
versioned development downloads at https://ziglang.org/builds/. Verify the downloaded
archive's SHA-256 before extraction. No third-party Zig packages are introduced.

## Building

Use the existing MinGW configure step, then enable the optional backend:

```sh
./configure --disable-debug
make -j4 ZIG_HASH=1 ZIG=/absolute/path/to/zig
```

`ZIG_HASH=0` (the default) builds the original C implementation. A mode stamp
recompiles the adapter when switching backends. Pass `ZIG_HASH=1` on subsequent
make invocations too. The C compiler's `-dumpmachine` selects the Zig target;
32-bit x86, x86-64 and ARM64 MinGW targets are supported by the module. The
Visual Studio build continues to use C; it does not enable this backend yet.

The backend uses `ReleaseSafe`, embeds compiler runtime support in its static
archive, and exports a C calling-convention function returning an `int` rather
than Zig `bool`. Digest IDs match `enum hash_type`. Output is caller-owned and
must have space for the requested digest. NULL input with zero length hashes
an empty buffer; invalid IDs, NULL output or NULL input with nonzero length
return failure without writing output. Input and output must not overlap.

## Validation

```sh
ZIG=/absolute/path/to/zig python3 tests/test_zig_hash.py
```

The test compares the exported function with Python `hashlib` for all four
algorithms, padding/block boundaries, binary data and a larger buffer. It checks
unaligned output, guard bytes, empty input and invalid arguments. It also runs
a native C caller and cross-links C callers for all three Windows targets.
Cross-linking does not substitute for full Rufus builds or Windows runtime
regression testing. Existing SHA acceleration makes performance equivalence
something to benchmark separately before making the Zig backend the default.

## Development builds

The `Zig hash backend` GitHub Actions workflow supports manual runs and checks
both pinned compilers on Linux and Windows. Windows jobs build Test1 executables
and upload `rufus-zig-development-x64` and `rufus-zig-stable-x64` artifacts,
including license notices and the toolchain manifest.

The local Linux installation uses Wine and a separate prefix. Launch it with
`rufus-zig-dev` or the **Rufus Zig Dev** desktop entry. The executable, notices,
build metadata and test results are under `~/.local/opt/rufus-zig-dev/`.
Wine validation covers startup and the application's 16 built-in hash vectors;
it does not establish that USB formatting works under Wine. Use Windows for
physical-drive workflows, which depend on Windows storage services.
