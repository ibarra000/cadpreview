#include "render/decode/png_decode.hpp"

#include "common/errors.hpp"

#include <png.h>

#include <cstdint>
#include <vector>

namespace cadpreview {

namespace {

constexpr std::uint64_t MAX_DEFLATE_RATIO = 1032;

// Frees libpng's read state on every exit path.
class PngRead {
public:
    PngRead() { image_.version = PNG_IMAGE_VERSION; }
    ~PngRead() { png_image_free(&image_); }
    PngRead(const PngRead&) = delete;
    PngRead& operator=(const PngRead&) = delete;
    PngRead(PngRead&&) = delete;
    PngRead& operator=(PngRead&&) = delete;

    png_image& image() noexcept { return image_; }

private:
    png_image image_{};
};

} // namespace

// Uses libpng's simplified API, which reports errors through return codes rather than longjmp.
Image decodePngImage(std::span<const std::byte> data) {
    PngRead read;
    png_image& image = read.image();
    if (png_image_begin_read_from_memory(&image, data.data(), data.size()) == 0) {
        throw FormatError("not a readable PNG");
    }
    if (image.width == 0 || image.height == 0 || image.width > MAX_IMAGE_EDGE ||
        image.height > MAX_IMAGE_EDGE) {
        throw FormatError("PNG size out of range");
    }
    // Deflate expands at most about 1032:1, and each row holds at least a filter byte plus one
    // bit per pixel. A file too small to hold that much compressed data is rejected before the
    // full-size pixel buffer is allocated.
    const std::uint64_t minimumRaw =
        static_cast<std::uint64_t>(image.height) * (1U + ((image.width + 7U) / 8U));
    if (minimumRaw > static_cast<std::uint64_t>(data.size()) * MAX_DEFLATE_RATIO) {
        throw FormatError("PNG is too small for its stated size");
    }
    image.format = PNG_FORMAT_RGBA;
    std::vector<std::uint8_t> rgba(PNG_IMAGE_SIZE(image));
    if (png_image_finish_read(&image, nullptr, rgba.data(), 0, nullptr) == 0) {
        throw FormatError("corrupt PNG data");
    }
    return Image::fromRgba(image.width, image.height, std::move(rgba));
}

} // namespace cadpreview
