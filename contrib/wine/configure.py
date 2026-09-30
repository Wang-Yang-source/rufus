#!/usr/bin/env python3
"""Configure the dedicated Rufus Wine prefix, without changing the desktop."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def display_registry(dpi):
    return rf'''Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\Wine\Drivers]
"Graphics"="x11"

[HKEY_CURRENT_USER\Software\Wine\X11 Driver]
"Decorated"="N"

[HKEY_CURRENT_USER\Control Panel\Desktop]
"LogPixels"=dword:{dpi:08x}

[HKEY_LOCAL_MACHINE\System\CurrentControlSet\Hardware Profiles\Current\Software\Fonts]
"LogPixels"=dword:{dpi:08x}
'''


def configure(prefix, dpi):
    prefix = prefix.expanduser().resolve()
    if prefix == Path.home() / '.wine':
        raise ValueError('Use a dedicated Rufus prefix, not ~/.wine')
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all')
    # Never terminate Wine processes: a busy prefix must be closed by its owner.
    subprocess.run(['wineserver', '-w'], env=env, check=True, timeout=15)
    prefix.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rufus-display-') as directory:
        registry = Path(directory) / 'display.reg'
        registry.write_text(display_registry(dpi), encoding='utf-16')
        subprocess.run(['wine', 'regedit', '/S', str(registry)],
                       env=env, check=True, timeout=60)
    # Wine caches DPI and decorations. Start Rufus only after this server exits.
    subprocess.run(['wineserver', '-w'], env=env, check=True, timeout=30)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--dpi', type=int, required=True,
                        help='96 = 100%%, 144 = 150%%, 192 = 200%%')
    args = parser.parse_args()
    if not 96 <= args.dpi <= 288:
        parser.error('--dpi must be between 96 and 288')
    try:
        configure(args.prefix, args.dpi)
    except subprocess.TimeoutExpired:
        parser.exit(1, 'Wine is still running in this prefix; close it and retry.\n')
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'{error}\n')
    print(f'Configured {args.prefix}: {args.dpi} DPI, Wine window decorations')


if __name__ == '__main__':
    main()
