#!/usr/bin/env python3
"""Launch the locally installed Rufus with its own Catppuccin Wine settings."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

from configure import configure


def main():
    parser = argparse.ArgumentParser(description=__doc__, add_help=False)
    parser.add_argument('--help', action='help')
    parser.add_argument('--theme', choices=('mocha', 'latte'), default='mocha')
    parser.add_argument('--dpi', type=int, default=192)
    parser.add_argument('--font', default='Noto Sans CJK SC')
    args, rufus_args = parser.parse_known_args()
    if not 96 <= args.dpi <= 288:
        parser.error('--dpi must be between 96 and 288')
    root = Path.home() / '.local'
    prefix = root / 'share/rufus-zig-dev/wineprefix'
    binary = root / 'opt/rufus-zig-dev/rufus.exe'
    if not binary.is_file():
        parser.exit(1, f'Rufus executable not found: {binary}\n')
    prefix.parent.mkdir(parents=True, exist_ok=True)
    # Keep the lock until the actual app exits; never recolor an active window.
    with (prefix.parent / 'ui.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.exit(1, 'Rufus is already running; close it before switching themes.\n')
        digest = hashlib.sha256()
        for name in ('configure.py', 'palette.json'):
            digest.update(Path(__file__).with_name(name).read_bytes())
        desired = dict(theme=args.theme, dpi=args.dpi, font=args.font, revision=digest.hexdigest())
        stamp = prefix.parent / 'ui.json'
        try:
            current = json.loads(stamp.read_text())
        except (FileNotFoundError, json.JSONDecodeError):
            current = None
        if current != desired:
            try:
                configure(prefix, args.dpi, args.theme, args.font)
            except subprocess.TimeoutExpired:
                parser.exit(1, 'Wine is still running in the Rufus prefix; close it and retry.\n')
            except (ValueError, OSError, subprocess.CalledProcessError) as error:
                parser.exit(1, f'{error}\n')
            temporary = stamp.with_suffix('.tmp')
            temporary.write_text(json.dumps(desired, indent=2) + '\n')
            temporary.replace(stamp)
        env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all')
        sys.exit(subprocess.call(['wine', str(binary), '-g', *rufus_args], env=env))


if __name__ == '__main__':
    main()
