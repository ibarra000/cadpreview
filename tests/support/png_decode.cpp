#include "png_decode.hpp"

#include <png.h>

#include <algorithm>
#include <array>

namespace cadpreview::test {

std::array<std::uint8_t, 4> DecodedImage::pixel(std::uint32_t x, std::uint32_t y) const {
    const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
    return {rgba.at(offset), rgba.at(offset + 1), rgba.at(offset + 2), rgba.at(offset + 3)};
}

std::size_t DecodedImage::countPixelsNot(std::array<std::uint8_t, 4> color) const {
    std::size_t count = 0;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4) {
        if (!std::equal(color.begin(), color.end(),
                        rgba.begin() + static_cast<std::ptrdiff_t>(i))) {
            ++count;
        }
    }
    return count;
}

std::optional<DecodedImage> decodePng(std::span<const std::uint8_t> png) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, png.data(), png.size()) == 0) {
        return std::nullopt;
    }
    image.format = PNG_FORMAT_RGBA;
    DecodedImage decoded;
    decoded.width = image.width;
    decoded.height = image.height;
    decoded.rgba.resize(PNG_IMAGE_SIZE(image));
    if (png_image_finish_read(&image, nullptr, decoded.rgba.data(), 0, nullptr) == 0) {
        png_image_free(&image);
        return std::nullopt;
    }
    return decoded;
}

} // namespace cadpreview::test
