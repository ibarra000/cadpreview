#include "render/png.hpp"

#include <png.h>

#include <stdexcept>
#include <string>

namespace cadpreview {

namespace {

png_image describe(const Image& image) {
    png_image description{};
    description.version = PNG_IMAGE_VERSION;
    description.width = image.width();
    description.height = image.height();
    description.format = PNG_FORMAT_RGBA;
    return description;
}

[[noreturn]] void fail(const png_image& description) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay): libpng C string
    throw std::runtime_error(std::string("PNG encoding failed: ") + description.message);
}

} // namespace

// Uses libpng's simplified API, which reports errors through return codes rather than longjmp.
std::vector<std::uint8_t> encodePng(const Image& image) {
    const auto pixels = image.pixels();
    png_image sizing = describe(image);
    png_alloc_size_t size = 0;
    if (png_image_write_to_memory(&sizing, nullptr, &size, 0, pixels.data(), 0, nullptr) == 0) {
        fail(sizing);
    }

    std::vector<std::uint8_t> png(size);
    png_image writing = describe(image);
    if (png_image_write_to_memory(&writing, png.data(), &size, 0, pixels.data(), 0, nullptr) == 0) {
        fail(writing);
    }
    png.resize(size);
    return png;
}

} // namespace cadpreview
