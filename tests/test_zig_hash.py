#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the exported C ABI against Python's independent hashlib backend."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ZIG = os.environ.get('ZIG', 'zig')


def main():
    version = subprocess.check_output([ZIG, 'version'], text=True).strip()
    pins = json.loads((ROOT / 'docs/zig-toolchain.json').read_text())
    versions = {pins['version'], pins['development']['version']}
    if version not in versions:
        raise SystemExit(f'Expected a pinned Zig version {sorted(versions)}, got {version}')
    with tempfile.TemporaryDirectory(prefix='rufus-zig-hash-') as directory:
        suffix = '.dll' if sys.platform == 'win32' else '.dylib' if sys.platform == 'darwin' else '.so'
        library = Path(directory) / ('rufushash' + suffix)
        subprocess.run([ZIG, 'build-lib', str(ROOT / 'src/zig/hash.zig'),
                        '-dynamic', '-O', 'ReleaseSafe', '-fcompiler-rt',
                        f'-femit-bin={library}'], check=True, cwd=directory)
        module = ctypes.CDLL(str(library))
        hash_buffer = module.rufus_hash_buffer
        hash_buffer.argtypes = [ctypes.c_uint, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
        hash_buffer.restype = ctypes.c_int
        cases = 0
        for kind, algorithm in enumerate(('md5', 'sha1', 'sha256', 'sha512')):
            for length in (0, 1, 3, 55, 56, 63, 64, 65, 111, 112, 127, 128, 129, 4096, 65537):
                data = bytes((i * 37 + 11) % 256 for i in range(length))
                expected = hashlib.new(algorithm, data).digest()
                source = ctypes.create_string_buffer(data)
                dest = (ctypes.c_ubyte * 80)(*([0xA5] * 80))
                assert hash_buffer(kind, source, length, ctypes.byref(dest, 1)) == 1
                assert bytes(dest)[1:1 + len(expected)] == expected
                assert dest[0] == 0xA5
                assert bytes(dest)[1 + len(expected):] == bytes([0xA5]) * (79 - len(expected))
                cases += 1
            dest = (ctypes.c_ubyte * 64)()
            assert hash_buffer(kind, None, 0, dest) == 1
            assert bytes(dest)[:len(expected)] == hashlib.new(algorithm).digest()
        for kind, source, length, output in ((4, b'abc', 3, True),
                                              (0xFFFFFFFF, b'abc', 3, True),
                                              (0, None, 1, True),
                                              (0, b'abc', 3, False)):
            dest = (ctypes.c_ubyte * 64)(*([0xA5] * 64))
            assert hash_buffer(kind, source, length, dest if output else None) == 0
            assert bytes(dest) == bytes([0xA5]) * 64
        native_archive = Path(directory) / 'native.a'
        subprocess.run([ZIG, 'build-lib', str(ROOT / 'src/zig/hash.zig'),
                        '-O', 'ReleaseSafe', '-fcompiler-rt', f'-femit-bin={native_archive}'],
                       check=True, cwd=directory)
        native_exe = Path(directory) / ('abi.exe' if sys.platform == 'win32' else 'abi')
        subprocess.run([ZIG, 'cc', str(ROOT / 'tests/zig_hash_abi.c'),
                        str(native_archive), '-o', str(native_exe)], check=True, cwd=directory)
        subprocess.run([str(native_exe)], check=True, cwd=directory)
        # Compile and link a real C caller for each supported Windows architecture.
        for target in ('x86-windows-gnu', 'x86_64-windows-gnu', 'aarch64-windows-gnu'):
            archive = Path(directory) / (target + '.a')
            subprocess.run([ZIG, 'build-lib', str(ROOT / 'src/zig/hash.zig'),
                            '-target', target, '-O', 'ReleaseSafe', '-fcompiler-rt',
                            f'-femit-bin={archive}'], check=True, cwd=directory)
            subprocess.run([ZIG, 'cc', '-target', target,
                            str(ROOT / 'tests/zig_hash_abi.c'), str(archive),
                            '-o', str(Path(directory) / (target + '.exe'))],
                           check=True, cwd=directory)
        print(f'Passed {cases} digest comparisons, NULL/error/guard checks, and 3 Windows C links')


if __name__ == '__main__':
    main()
