#include "render/image.hpp"
#include "tiers/placeholder.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace cadpreview {
namespace {

constexpr Rgba WHITE{255, 255, 255, 255};

std::size_t inkIn(const Image& image, std::uint32_t x0, std::uint32_t y0, std::uint32_t x1,
                  std::uint32_t y1) {
    std::size_t count = 0;
    for (std::uint32_t y = y0; y < y1; ++y) {
        for (std::uint32_t x = x0; x < x1; ++x) {
            count += image.pixel(x, y) == WHITE ? 0U : 1U;
        }
    }
    return count;
}

class PlaceholderSizes : public testing::TestWithParam<std::uint32_t> {};

TEST_P(PlaceholderSizes, HasRequestedSizeAndFrame) {
    const std::uint32_t size = GetParam();
    const Image image = drawPlaceholder("Unknown format", "unrecognized file format", size);
    EXPECT_EQ(image.width(), size);
    EXPECT_EQ(image.height(), size);
    EXPECT_EQ(image.pixel(0, 0), WHITE);
    EXPECT_GT(inkIn(image, 0, 0, size, size), 0U);
}

INSTANTIATE_TEST_SUITE_P(Sizes, PlaceholderSizes, testing::Values(16U, 17U, 64U, 512U, 4096U));

TEST(Placeholder, TextSitsInTheMiddle) {
    const Image image = drawPlaceholder("Unknown format", "unrecognized file format", 512);
    EXPECT_GT(inkIn(image, 128, 200, 384, 312), 200U);
}

TEST(Placeholder, LongReasonsStayInsideTheFrame) {
    const std::string longText(2000, 'x');
    const Image image = drawPlaceholder(longText, longText, 256);
    EXPECT_EQ(inkIn(image, 0, 0, 256, 6), 0U);
    EXPECT_EQ(inkIn(image, 0, 250, 256, 256), 0U);
}

} // namespace
} // namespace cadpreview
