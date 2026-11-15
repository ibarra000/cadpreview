#pragma once

#include "render/image.hpp"

#include <cstdint>
#include <string_view>

namespace cadpreview {

/// Draws the neutral tier-5 image: a light frame at a 5% margin with the title and the detail
/// text centred inside it. Text is dropped when the image is too small to hold it.
[[nodiscard]] Image drawPlaceholder(std::string_view title, std::string_view detail,
                                    std::uint32_t size);

} // namespace cadpreview
