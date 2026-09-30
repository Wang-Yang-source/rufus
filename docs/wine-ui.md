# Rufus development UI under Wine

The Linux development launcher uses a dedicated Wine prefix. Wine's default
96 DPI client area can be much smaller than the desktop's scaled title bar.
Use Wine's own decorations and one DPI for the whole window:

```sh
python3 contrib/wine/configure.py \
  --prefix "$HOME/.local/share/rufus-zig-dev/wineprefix" --dpi 192
```

192 DPI matches a 200% desktop, 144 matches 150%, and 96 matches 100%.
Close Rufus before configuring the prefix. The script waits for that prefix's
Wine server to exit so the next launch reloads DPI; it never kills processes.
It refuses the default `~/.wine` prefix. No desktop settings are changed.

This integration reuses Wine's X11 driver and registry support with Python's
standard library. It requires Python 3.9 or newer, Wine and XWayland on Wayland desktops;
it adds no compiler or application runtime dependencies. Tested with Wine 11
on GNOME Wayland at 200% scaling. Physical-device support is separate from UI
compatibility.

## Catppuccin Mocha and Latte

The launcher offers Mocha (dark) and Latte (light), both with mauve accents.
Each switch applies the palette to the dedicated prefix, including the startup
warning, edit fields, menus, disabled controls and title bar. Rufus's opt-in
`SystemColors` setting makes its custom progress bar and toolbar icons follow
these colors too. Without that setting, native Windows appearance is unchanged.

```sh
python3 contrib/wine/install.py --exe /path/to/tested/rufus.exe --dpi 192
rufus-zig-dev --theme mocha
rufus-zig-dev --theme latte
```

The installer creates two application-menu entries. Close Rufus before switching;
the launcher holds a lock while it runs and will not terminate another instance.
It preserves an existing executable as `rufus-before-ui.exe`. Theme changes retain
Rufus's device and application settings. The first launch or a theme change takes
a few seconds while Wine reloads the prefix. DPI can also be overridden on the
command line. The personal launcher uses the separately installed `PingFang SC` font, matching
ProjectGraph. It is not bundled. On another computer supply `--font` with an
installed family that supports Chinese (for example `Noto Sans CJK SC`).
See [the design and asset provenance](projectgraph-ui.md).

The palette data is derived from [Catppuccin palette v1.8.0](https://github.com/catppuccin/palette/tree/07d02aa110ef9eb7e7427afca5c73ba9cf7f8ebd)
under the MIT license, included in `contrib/wine/Catppuccin-LICENSE`. The source
commit and SHA-256 of the original `palette.json` are recorded in the bundled
palette. Only the hex values for the two requested flavors are vendored; no
JavaScript, Python package or runtime download is required.

Wine's built-in classic controls and Win32 system colors cover these surfaces.
The existing Rufus dark-mode code requires Windows theme APIs that Wine does not
fully implement. Desktop GTK themes cover the outside decoration but do not
style Win32 controls. No maintained official Catppuccin Wine `.msstyles` port
was found, so this integration uses Wine's existing color support instead of
introducing another rendering engine.

## UI regression check

Requires MinGW-w64, Wine, Xvfb and the Chinese font. The helper accepts the
unofficial-build warning only inside the explicitly enabled disposable test
session. It changes no drive contents. It captures both Chinese windows, checks
DPI/caption proportions, idle progress background and all three progress states,
and runs Rufus's Ctrl-T hash check before closing the app.

```sh
python3 tests/test_wine_ui.py --exe /path/to/rufus.exe \
  --prefix /tmp/rufus-ui-test --output /tmp/rufus-ui-results
```

The default matrix covers Mocha and Latte at 96, 144 and 192 DPI. Each case writes
`warning.bmp`, `main.bmp` and `result.log` for visual inspection. Do not point this
test at an installed or shared prefix: it sets test-only locale/update settings.
