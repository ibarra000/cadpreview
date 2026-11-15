#pragma once

#include "render/image.hpp"

#include <cstddef>
#include <span>

namespace cadpreview {

/// Decodes a device-independent bitmap (a BITMAPINFOHEADER followed by an optional palette and
/// the pixels), with or without the 14-byte "BM" file header. Supports uncompressed 1, 4, 8, 24
/// and 32 bits per pixel. Throws FormatError for anything malformed or unsupported.
/// see notes/images.md#dib
[[nodiscard]] Image decodeDib(std::span<const std::byte> data);

} // namespace cadpreview
