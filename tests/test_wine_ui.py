#!/usr/bin/env python3
"""Test the actual Chinese Rufus UI in a dedicated headless Wine prefix."""
import argparse
import importlib.util
import os
import re
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('wine_configure', ROOT / 'contrib/wine/configure.py')
configuration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(configuration)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--prefix', type=Path, required=True, help='Disposable test prefix only')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cc', default='x86_64-w64-mingw32-gcc')
    parser.add_argument('--font', default='Noto Sans CJK SC')
    parser.add_argument('--dpi', type=int, nargs='+', default=[96, 144, 192])
    parser.add_argument('--themes', nargs='+', choices=['mocha', 'latte'], default=['mocha', 'latte'])
    args = parser.parse_args()
    prefix = args.prefix.resolve()
    if prefix == Path.home() / '.wine' or 'test' not in prefix.name:
        parser.error('Use a disposable prefix whose name includes "test"')
    binary = args.exe.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all', RUFUS_UI_TEST='1')
    with tempfile.TemporaryDirectory(prefix='rufus-ui-harness-') as directory:
        harness = Path(directory) / 'smoke.exe'
        subprocess.run([args.cc, '-Wall', '-Wextra', '-Werror',
                        str(ROOT / 'tests/wine_ui_smoke.c'), '-o', str(harness),
                        '-lgdi32', '-ladvapi32'], check=True)
        for dpi in args.dpi:
            if not 96 <= dpi <= 288:
                parser.error('DPI must be between 96 and 288')
            for theme in args.themes:
                configuration.configure(prefix, dpi, theme, args.font)
                output = (args.output / f'{theme}-{dpi}').resolve()
                output.mkdir(exist_ok=True)
                logs = list((prefix / 'drive_c/users').glob('*/AppData/Local/Rufus/rufus.log'))
                for log in logs:
                    log.unlink()
                command = ['xvfb-run', '-a', '-s', '-screen 0 1800x1600x24',
                           'wine', str(harness), 'Z:' + str(binary).replace('/', '\\'), str(dpi)]
                result = subprocess.run(command, cwd=output, env=environment,
                                        capture_output=True, text=True, timeout=45)
                (output / 'result.log').write_text(result.stdout + result.stderr)
                print(f'{theme} {dpi} DPI: {"PASS" if result.returncode == 0 else "FAIL"}', flush=True)
                if result.returncode:
                    raise RuntimeError(result.stdout + result.stderr)
                logs = list((prefix / 'drive_c/users').glob('*/AppData/Local/Rufus/rufus.log'))
                if not logs:
                    raise RuntimeError('Rufus did not produce its persistent test log')
                content = max(logs, key=lambda path: path.stat().st_mtime).read_text(errors='replace')
                checks = re.findall(r'Test (?:MD5|SHA1|SHA256|SHA512)\s+[0-3]: (PASS|FAIL)', content)
                (output / 'rufus.log').write_text(content)
                if checks != ['PASS'] * 16:
                    raise RuntimeError(f'Ctrl-T hash self-test failed: {checks}')


if __name__ == '__main__':
    main()
