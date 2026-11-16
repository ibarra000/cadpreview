#include "render/compose.hpp"
#include "render/image.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace cadpreview {
namespace {

constexpr Rgba WHITE{255, 255, 255, 255};
constexpr Rgba RED{255, 0, 0, 255};

TEST(Compose, FitsWideImageWithMarginAndCentres) {
    const Image source(40, 20, RED);
    const Image out = composeOnCanvas(source, 100);
    ASSERT_EQ(out.width(), 100U);
    ASSERT_EQ(out.height(), 100U);
    // 5% margin: content spans x 5..94; a 2:1 image is 90x45, centred vertically (y 27..71).
    EXPECT_EQ(out.pixel(0, 50), WHITE);
    EXPECT_EQ(out.pixel(4, 50), WHITE);
    EXPECT_EQ(out.pixel(6, 50), RED);
    EXPECT_EQ(out.pixel(93, 50), RED);
    EXPECT_EQ(out.pixel(95, 50), WHITE);
    EXPECT_EQ(out.pixel(50, 20), WHITE);
    EXPECT_EQ(out.pixel(50, 50), RED);
    EXPECT_EQ(out.pixel(50, 80), WHITE);
}

TEST(Compose, DownscalesByAveraging) {
    Image source(400, 400, WHITE);
    source.fillRect(0, 0, 200, 400, {0, 0, 0, 255});
    const Image out = composeOnCanvas(source, 20);
    const Rgba left = out.pixel(5, 10);
    const Rgba right = out.pixel(14, 10);
    EXPECT_LT(left.r, 30);
    EXPECT_GT(right.r, 225);
}

TEST(Compose, BlendsTransparencyOntoWhite) {
    const Image source(10, 10, {0, 0, 0, 0});
    const Image out = composeOnCanvas(source, 50);
    EXPECT_EQ(out.pixel(25, 25), WHITE);
}

TEST(Compose, UpscalesTinyImages) {
    const Image source(1, 1, RED);
    const Image out = composeOnCanvas(source, 64);
    EXPECT_EQ(out.pixel(32, 32), RED);
    EXPECT_EQ(out.pixel(1, 1), WHITE);
}

} // namespace
} // namespace cadpreview
