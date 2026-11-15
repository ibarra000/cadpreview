#pragma once

#include "render/image.hpp"

#include <cstdint>

namespace cadpreview {

/// Fraction of the canvas edge left empty on each side, as the output spec requires.
inline constexpr double CANVAS_MARGIN = 0.05;

/// Returns the margin in pixels for a canvas edge (at least one pixel).
[[nodiscard]] int canvasMargin(std::uint32_t size) noexcept;

/// Scales source to fit a size x size white canvas inside the margin, keeping its aspect ratio
/// and centring it. Shrinking averages source pixels; enlarging interpolates bilinearly.
/// Transparent pixels are blended onto white.
[[nodiscard]] Image composeOnCanvas(const Image& source, std::uint32_t size);

} // namespace cadpreview
