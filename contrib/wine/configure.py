#!/usr/bin/env python3
"""Configure the dedicated Rufus Wine prefix, without changing the desktop."""
import argparse
import os
import json
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


# Win32 color roles, mapped to the upstream palette rather than a new theme engine.
COLOR_ROLES = {
    'ActiveBorder': 'surface1', 'ActiveTitle': 'mantle', 'AppWorkSpace': 'crust',
    'Background': 'base', 'ButtonAlternateFace': 'surface0',
    'ButtonDkShadow': 'surface0', 'ButtonFace': 'mantle',
    'ButtonHilight': 'surface1', 'ButtonLight': 'surface0',
    'ButtonShadow': 'surface1', 'ButtonText': 'text',
    'GradientActiveTitle': 'mantle', 'GradientInactiveTitle': 'mantle',
    'GrayText': 'overlay0', 'Hilight': 'mauve', 'HilightText': 'crust',
    'HotTrackingColor': 'mauve', 'InactiveBorder': 'surface0',
    'InactiveTitle': 'mantle', 'InactiveTitleText': 'subtext0',
    'InfoText': 'text', 'InfoWindow': 'mantle', 'Menu': 'base',
    'MenuBar': 'base', 'MenuHilight': 'mauve', 'MenuText': 'text',
    'Scrollbar': 'surface0', 'TitleText': 'text', 'Window': 'base',
    'WindowFrame': 'surface1', 'WindowText': 'text',
}


def theme_registry(flavor, font=None):
    palette = json.loads(Path(__file__).with_name('palette.json').read_text())
    colors = palette['flavors'][flavor]

    def rgb(name):
        value = colors[name].removeprefix('#')
        return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))

    lines = [r'[HKEY_CURRENT_USER\Control Panel\Colors]']
    for key, role in COLOR_ROLES.items():
        lines.append(f'"{key}"="' + ' '.join(map(str, rgb(role))) + '"')
    lines.extend([
        '', r'[HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\ThemeManager]',
        '"ThemeActive"="0"',
        '', r'[HKEY_CURRENT_USER\Software\Akeo Consulting\Rufus]',
        '"SystemColors"=dword:00000001',
        '"ProjectGraphUI"=dword:00000001',
    ])
    for state, role in [('Normal', 'green'), ('Paused', 'yellow'), ('Error', 'red')]:
        r, g, b = rgb(role)
        lines.append(f'"ProgressColor{state}"=dword:{r | g << 8 | b << 16:08x}')
    if font:
        if len(font) > 31 or any(c in font for c in '\\"\r\n'):
            raise ValueError('Invalid Win32 font family name')
        lines.extend(['', r'[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes]'])
        for name in ('Segoe UI', 'Segoe UI Symbol', 'MS Shell Dlg', 'MS Shell Dlg 2', 'Tahoma'):
            lines.append(f'"{name}"="{font}"')
    return '\n' + '\n'.join(lines) + '\n'


def configure(prefix, dpi, theme=None, font=None):
    prefix = prefix.expanduser().resolve()
    if prefix == Path.home() / '.wine':
        raise ValueError('Use a dedicated Rufus prefix, not ~/.wine')
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all')
    # Never terminate Wine processes: a busy prefix must be closed by its owner.
    subprocess.run(['wineserver', '-w'], env=env, check=True, timeout=15)
    prefix.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rufus-display-') as directory:
        registry = Path(directory) / 'display.reg'
        content = display_registry(dpi)
        if theme:
            content += theme_registry(theme, font)
        registry.write_text(content, encoding='utf-16')
        subprocess.run(['wine', 'regedit', '/S', str(registry)],
                       env=env, check=True, timeout=60)
    # Wine caches DPI and decorations. Start Rufus only after this server exits.
    subprocess.run(['wineserver', '-w'], env=env, check=True, timeout=30)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--dpi', type=int, required=True,
                        help='96 = 100%%, 144 = 150%%, 192 = 200%%')
    parser.add_argument('--theme', choices=('mocha', 'latte'))
    parser.add_argument('--font', help='Installed sans serif font family with Chinese glyphs')
    args = parser.parse_args()
    if not 96 <= args.dpi <= 288:
        parser.error('--dpi must be between 96 and 288')
    try:
        configure(args.prefix, args.dpi, args.theme, args.font)
    except subprocess.TimeoutExpired:
        parser.exit(1, 'Wine is still running in this prefix; close it and retry.\n')
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'{error}\n')
    print(f'Configured {args.prefix}: {args.dpi} DPI, {args.theme or "existing theme"}')


if __name__ == '__main__':
    main()
