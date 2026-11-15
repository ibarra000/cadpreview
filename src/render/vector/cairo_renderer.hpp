#pragma once

#include "render/image.hpp"
#include "render/vector/drawing.hpp"

#include <cstdint>

namespace cadpreview {

/// Draws the drawing with cairo onto a size x size white canvas: fitted to the 5% margin, y
/// pointing up, dark hairline strokes, and text in the embedded font. Throws FormatError when
/// the drawing has nothing finite to show, std::runtime_error if cairo itself fails.
[[nodiscard]] Image renderDrawing(const Drawing& drawing, std::uint32_t size);

} // namespace cadpreview
