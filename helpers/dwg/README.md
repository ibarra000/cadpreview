# cadpreview-dwg

`cadpreview-dwg` is a small separate program that lets cadpreview draw the 2D content of DWG
files (tier 4). It decodes the file with [LibreDWG](https://www.gnu.org/software/libredwg/)
and writes the drawing to standard output as ASCII DXF.

```sh
cadpreview-dwg [--max-memory-mb N] drawing.dwg > drawing.dxf
```

| Exit status | Meaning |
|---|---|
| 0 | Drawing written |
| 2 | Usage error |
| 3 | Not a DWG file, or LibreDWG could not decode it |
| 4 | Internal error |
| 5 | Out of memory |

## License

This program links LibreDWG, so the `cadpreview-dwg` binary is licensed under the
**GNU General Public License, version 3 or later** (see `LIBREDWG-COPYING`). Its own source
files are Apache-2.0, like the rest of cadpreview, which is compatible with GPL-3.0.

The cadpreview library (`cadpreview.dll`, `libcadpreview.so`, the `CadPreview` NuGet package)
does not link this program or LibreDWG. It starts `cadpreview-dwg` as a separate process when
it is installed and reads its output, so the library stays Apache-2.0. Without the helper,
DWG files still get their embedded preview (tier 1).

## Where the library looks for it

1. The path in `cp_options.dwg_helper_path` (`RenderOptions.DwgHelperPath` in .NET,
   `--dwg-helper` on the command line), if given.
2. Otherwise, next to the cadpreview library, or in `../bin` relative to it.
