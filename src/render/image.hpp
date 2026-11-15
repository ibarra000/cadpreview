#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace cadpreview {

struct Rgba {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    friend constexpr bool operator==(const Rgba&, const Rgba&) = default;
};

/// Largest accepted image edge, which bounds allocations driven by untrusted sizes.
inline constexpr std::uint32_t MAX_IMAGE_EDGE = 8192;

/// 8-bit RGBA raster, rows top to bottom.
class Image {
public:
    Image(std::uint32_t width, std::uint32_t height, Rgba fill);

    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept { return rgba_; }
    [[nodiscard]] Rgba pixel(std::uint32_t x, std::uint32_t y) const;

    /// Fills the rectangle, clipped to the image; any part outside is ignored.
    void fillRect(std::int64_t x, std::int64_t y, std::int64_t width, std::int64_t height,
                  Rgba color) noexcept;

private:
    std::uint32_t width_;
    std::uint32_t height_;
    std::vector<std::uint8_t> rgba_;
};

} // namespace cadpreview
