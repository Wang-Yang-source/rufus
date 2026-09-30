# ProjectGraph visual direction for the Wine development UI

Reference: the local ProjectGraph design documents `typography.md`, `icons.md`,
`ui-style.md` and `color-system.md`. Its Godot resources are not needed for this
Win32 integration. The reference specifies PingFang SC Regular, 14 logical-pixel
body text, 16-pixel titles, Lucide line icons (18–20 logical pixels), mauve accents,
and small rounded controls. This is a visual adaptation, not a Godot integration.

Reuse decisions:

- Keep Win32 dialogs, controls, keyboard navigation and accessibility names.
  Reuse the existing Rufus color adapter and icon recoloring, rather than add a
  second GUI framework or a new input implementation.
- Vendor selected SVGs from Lucide 1.49.0, commit
  `5a92b9ba262de5bf10e864219883267672c05db8`. `res/icons/lucide/manifest.json`
  records source and generated PNG hashes. Keep the complete upstream license.
  Render the original geometry with ImageMagick/librsvg at build-preparation time;
  application builds consume PNG resources without an SVG runtime dependency.
- Use the same PingFang SC font as the reference on this personal computer.
  It is installed separately in the user's font directory and is **not bundled
  with the application or redistributed in this repository**. Reference mirror:
  `https://github.com/zongren/font`, commit
  `f73a1000d623a3570a2cd9cfbe4c31f0d1a43d2f`, `PingFang-SC-Regular.ttf`, SHA-256
  `7c31780a74b296162818aa5891c395d6ecdfcf1e71d9e1e4c3326869ad901ab6`.
  That mirror does not establish a font redistribution license. For another
  installation supply `--font` with an installed, licensed CJK font.

Check both themes at 100%, 150% and 200%. Text must remain readable, glyphs must
not fall back to a visibly different typeface, and all icons must have the same
stroke weight. Confirm dropdown text, long paths and keyboard focus remain
visible. Open both entries from the application menu to check the actual desktop
rather than judging only headless captures.

## Rounded controls and window frame

The optional adapter retains native Win32 controls and uses the standard
`HTCAPTION`, `HTMINBUTTON`, `HTCLOSE` and `WM_SYSCOMMAND` roles for the window.
It does not expose a maximize button for Rufus's fixed-size dialog.

GDI+ was evaluated for the small control surfaces, but actual Wine screenshots
showed coarse edges and native border remnants. ProjectGraph already documents
SVG textures with nine-slice scaling. Use the same approach here: librsvg renders
small theme surfaces at 100%, 150% and 200%, and Windows/Wine's native `AlphaBlend`
composes the slices. The adapter changes presentation only; keyboard, edit and
combo behavior stays in the original controls. No GUI toolkit, SVG interpreter
or external rendering DLL is shipped with the executable.

The same frame compositor runs inside Rufus's existing progress painter, since
that painter handles `WM_PAINT` before the common-controls subclass chain. Native
edit borders are removed and their content remains native. Cached premultiplied
32-bit DIBs are decoded with `CreateIconFromResourceEx`, sliced at the rounded
corners and released when the main window is destroyed. `AlphaBlend` is loaded
from the system directory through Rufus's existing loader. Surface resources are
build prerequisites in the MinGW resource rule and are included by the shared
Windows resource file for Visual Studio too.
