#pragma once

#include "render/image.hpp"

#include <cstddef>
#include <span>

namespace cadpreview {

/// Decodes a PNG with libpng, refusing images larger than MAX_IMAGE_EDGE. Throws FormatError.
[[nodiscard]] Image decodePngImage(std::span<const std::byte> data);

} // namespace cadpreview
