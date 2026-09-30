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
standard library. It requires Python 3, Wine and XWayland on Wayland desktops;
it adds no compiler or application runtime dependencies. Tested with Wine 11
on GNOME Wayland at 200% scaling. Physical-device support is separate from UI
compatibility.
