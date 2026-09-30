#!/usr/bin/env python3
"""Install the development launcher and both Catppuccin entries for this user."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import sys


def desktop_quote(value):
    for char in ('\\', '"', '`', '$'):
        value = value.replace(char, '\\' + char)
    return '"' + value + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--dpi', type=int, default=192)
    args = parser.parse_args()
    if not args.exe.is_file():
        parser.error('The development executable must already exist')
    if not 96 <= args.dpi <= 288:
        parser.error('--dpi must be between 96 and 288')
    root = Path.home() / '.local'
    install = root / 'opt/rufus-zig-dev'
    install.mkdir(parents=True, exist_ok=True)
    binary = install / 'rufus.exe'
    if args.exe.resolve() != binary.resolve():
        if binary.exists():
            backup = install / 'rufus-before-ui.exe'
            if not backup.exists():
                shutil.copy2(binary, backup)
        temporary = binary.with_suffix('.exe.tmp')
        shutil.copy2(args.exe, temporary)
        temporary.replace(binary)
    scripts = install / 'wine-ui'
    scripts.mkdir(exist_ok=True)
    for name in ('configure.py', 'launch.py', 'palette.json', 'Catppuccin-LICENSE'):
        shutil.copy2(Path(__file__).with_name(name), scripts / name)
    launcher = root / 'bin/rufus-zig-dev'
    launcher.parent.mkdir(parents=True, exist_ok=True)
    launcher.write_text('#!/bin/sh\nexec ' + shlex.quote(sys.executable) + ' ' +
                        shlex.quote(str(scripts / 'launch.py')) +
                        f' --dpi {args.dpi} "$@"\n')
    launcher.chmod(0o755)
    applications = root / 'share/applications'
    applications.mkdir(parents=True, exist_ok=True)
    for flavor, chinese in [('mocha', '深色'), ('latte', '浅色')]:
        name = 'rufus-zig-dev' + ('-latte' if flavor == 'latte' else '') + '.desktop'
        text = f'''[Desktop Entry]
Type=Application
Name=Rufus Zig Dev — {flavor.title()}
Name[zh_CN]=Rufus Zig Dev · {flavor.title()} {chinese}
Comment=Catppuccin {flavor.title()} development build under Wine
Exec={desktop_quote(str(launcher))} --theme {flavor}
Icon={install / 'rufus.png'}
Terminal=false
Categories=Utility;
StartupNotify=true
StartupWMClass=rufus.exe
'''
        (applications / name).write_text(text)
    provenance = dict(executable_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                      dpi=args.dpi, themes=['mocha', 'latte'])
    (install / 'ui-install.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print(f'Installed Mocha and Latte launchers: {launcher}')


if __name__ == '__main__':
    main()
