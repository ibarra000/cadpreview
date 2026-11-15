#include "render/image.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace cadpreview {

namespace {

constexpr std::size_t CHANNELS = 4;

std::uint32_t checkedEdge(std::uint32_t edge) {
    if (edge == 0 || edge > MAX_IMAGE_EDGE) {
        throw std::invalid_argument("image edge out of range");
    }
    return edge;
}

} // namespace

Image::Image(std::uint32_t width, std::uint32_t height, Rgba fill)
    : width_(checkedEdge(width)), height_(checkedEdge(height)),
      rgba_(static_cast<std::size_t>(width_) * height_ * CHANNELS) {
    for (std::size_t i = 0; i < rgba_.size(); i += CHANNELS) {
        rgba_[i] = fill.r;
        rgba_[i + 1] = fill.g;
        rgba_[i + 2] = fill.b;
        rgba_[i + 3] = fill.a;
    }
}

Image Image::fromRgba(std::uint32_t width, std::uint32_t height, std::vector<std::uint8_t> rgba) {
    const std::size_t expected =
        static_cast<std::size_t>(checkedEdge(width)) * checkedEdge(height) * CHANNELS;
    if (rgba.size() != expected) {
        throw std::invalid_argument("pixel buffer size does not match the image");
    }
    return Image(width, height, std::move(rgba), Adopt{});
}

// Takes a buffer whose size fromRgba has already checked, so pixels are never allocated twice.
Image::Image(std::uint32_t width, std::uint32_t height, std::vector<std::uint8_t> rgba,
             Adopt /*tag*/) noexcept
    : width_(width), height_(height), rgba_(std::move(rgba)) {}

void Image::setPixel(std::uint32_t x, std::uint32_t y, Rgba color) {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("pixel outside image");
    }
    const std::size_t offset = (static_cast<std::size_t>(y) * width_ + x) * CHANNELS;
    rgba_[offset] = color.r;
    rgba_[offset + 1] = color.g;
    rgba_[offset + 2] = color.b;
    rgba_[offset + 3] = color.a;
}

Rgba Image::pixel(std::uint32_t x, std::uint32_t y) const {
    if (x >= width_ || y >= height_) {
        throw std::out_of_range("pixel outside image");
    }
    const std::size_t offset = (static_cast<std::size_t>(y) * width_ + x) * CHANNELS;
    return {rgba_[offset], rgba_[offset + 1], rgba_[offset + 2], rgba_[offset + 3]};
}

void Image::fillRect(std::int64_t x, std::int64_t y, std::int64_t width, std::int64_t height,
                     Rgba color) noexcept {
    const std::int64_t left = std::max<std::int64_t>(x, 0);
    const std::int64_t top = std::max<std::int64_t>(y, 0);
    const std::int64_t right = std::min<std::int64_t>(x + width, width_);
    const std::int64_t bottom = std::min<std::int64_t>(y + height, height_);
    for (std::int64_t row = top; row < bottom; ++row) {
        for (std::int64_t column = left; column < right; ++column) {
            const auto offset =
                (static_cast<std::size_t>(row) * width_ + static_cast<std::size_t>(column)) *
                CHANNELS;
            rgba_[offset] = color.r;
            rgba_[offset + 1] = color.g;
            rgba_[offset + 2] = color.b;
            rgba_[offset + 3] = color.a;
        }
    }
}

} // namespace cadpreview
