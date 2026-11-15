#pragma once

#include "render/image.hpp"

#include <cstdint>
#include <vector>

namespace cadpreview {

/// Encodes an RGBA image as PNG. Throws std::bad_alloc or std::runtime_error on failure.
[[nodiscard]] std::vector<std::uint8_t> encodePng(const Image& image);

} // namespace cadpreview
