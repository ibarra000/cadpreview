#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cadpreview::test {

struct DecodedImage {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;

    [[nodiscard]] std::array<std::uint8_t, 4> pixel(std::uint32_t x, std::uint32_t y) const;
    [[nodiscard]] std::size_t countPixelsNot(std::array<std::uint8_t, 4> color) const;
};

[[nodiscard]] std::optional<DecodedImage> decodePng(std::span<const std::uint8_t> png);

} // namespace cadpreview::test
