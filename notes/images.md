# Image formats in previews

## dib

**Claim:** A device-independent bitmap (DIB) starts with a BITMAPINFOHEADER. Its fields are:
- `biSize` (u32, 40 or larger for the V4/V5 headers)
- `biWidth` (i32)
- `biHeight` (i32; negative means rows run top-down)
- `biPlanes` (u16, = 1)
- `biBitCount` (u16)
- `biCompression` (u32; 0 = BI_RGB, 3 = BI_BITFIELDS)
- `biSizeImage`, X and Y resolution, `biClrUsed` and `biClrImportant` (u32 each)

The header is followed by the colour masks (BI_BITFIELDS with a 40-byte header), then a palette
of `biClrUsed` (or 2^bitCount) BGRx entries for depths of 8 or less, then the pixel rows, each
padded to 4 bytes. A `.bmp` file adds a 14-byte BITMAPFILEHEADER that starts with "BM".
**Status:** confirmed. The structure is Microsoft's published documentation (BITMAPINFOHEADER
and BITMAPFILEHEADER on learn.microsoft.com). The DWG previews in `vendor/dwg/` are 40-byte-header
DIBs that decode correctly (see dwg.md#preview-block).
**Method:** decoder written from the documentation (`src/render/decode/dib.cpp`), tested with
hand-built bitmaps (`tests/unit/decode_test.cpp`), and checked by viewing the DWG previews.

cadpreview decodes uncompressed 1, 4, 8, 24 and 32 bits per pixel. It rejects run-length
compression, 16-bit pixels, palette indexes beyond the palette, and edges larger than
`MAX_IMAGE_EDGE`. 32-bit pixels are treated as opaque because DIB previews leave the alpha byte
unused.

## png

PNG previews are decoded with libpng's simplified API, which bounds the image size before
allocating anything (`src/render/decode/png_decode.cpp`).
