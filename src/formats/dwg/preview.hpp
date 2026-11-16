#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace cadpreview::dwg {

enum class ImageKind : std::uint8_t { Dib, Png };

/// An embedded preview image inside a DWG file; data points into the input.
struct PreviewImage {
    ImageKind kind = ImageKind::Dib;
    std::span<const std::byte> data;
};

/// Locates the largest preview image of an R13-or-later DWG file. Throws FormatError when the
/// file has no usable preview. see notes/dwg.md#preview-block
[[nodiscard]] PreviewImage findPreview(std::span<const std::byte> file);

} // namespace cadpreview::dwg
