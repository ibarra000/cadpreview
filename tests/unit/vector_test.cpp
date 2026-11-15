#include "common/errors.hpp"
#include "render/image.hpp"
#include "render/vector/cairo_renderer.hpp"
#include "render/vector/drawing.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace cadpreview {
namespace {

constexpr Rgba WHITE{255, 255, 255, 255};

bool isInk(Rgba c) {
    return c.r < 200;
}

std::size_t inkCount(const Image& image) {
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < image.height(); ++y) {
        for (std::uint32_t x = 0; x < image.width(); ++x) {
            count += isInk(image.pixel(x, y)) ? 1U : 0U;
        }
    }
    return count;
}

TEST(Extents, CoverPolylinesArcsAndText) {
    Drawing drawing;
    drawing.polylines.push_back({{{0, 0}, {10, 5}}, false});
    drawing.arcs.push_back({{20, 0}, 2, 0, std::numbers::pi / 2});
    drawing.texts.push_back({"AB", {-5, -5}, 1.0, 0.0, 1.0, HAlign::Left, VAlign::Baseline});
    const auto box = extentsOf(drawing);
    ASSERT_TRUE(box.has_value());
    EXPECT_DOUBLE_EQ(box->min.x, -5);
    EXPECT_DOUBLE_EQ(box->min.y, -5);
    EXPECT_NEAR(box->max.x, 22, 1e-9);
    EXPECT_DOUBLE_EQ(box->max.y, 5);
}

TEST(Extents, ArcBoundsFollowTheSweep) {
    Drawing drawing;
    drawing.arcs.push_back({{0, 0}, 1, 0, std::numbers::pi / 2}); // first quadrant only
    const auto box = extentsOf(drawing);
    ASSERT_TRUE(box.has_value());
    EXPECT_NEAR(box->min.x, 0, 1e-9);
    EXPECT_NEAR(box->min.y, 0, 1e-9);
    EXPECT_NEAR(box->max.x, 1, 1e-9);
    EXPECT_NEAR(box->max.y, 1, 1e-9);
}

TEST(Extents, IgnoreNonFiniteCoordinates) {
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    Drawing drawing;
    drawing.polylines.push_back(
        {{{NaN, 0}, {1, 1}, {2, std::numeric_limits<double>::infinity()}}, false});
    const auto box = extentsOf(drawing);
    ASSERT_TRUE(box.has_value());
    EXPECT_DOUBLE_EQ(box->min.x, 1);
    EXPECT_DOUBLE_EQ(box->max.x, 1);
    EXPECT_FALSE(extentsOf(Drawing{}).has_value());
}

TEST(CairoRenderer, DrawsALineAcrossTheMiddle) {
    Drawing drawing;
    drawing.polylines.push_back({{{0, 0}, {100, 0}}, false});
    const Image image = renderDrawing(drawing, 128);
    ASSERT_EQ(image.width(), 128U);
    EXPECT_EQ(image.pixel(0, 0), WHITE);
    EXPECT_EQ(image.pixel(64, 10), WHITE);
    EXPECT_TRUE(isInk(image.pixel(64, 64)) || isInk(image.pixel(64, 63)));
    EXPECT_EQ(image.pixel(2, 64), WHITE); // inside the 5% margin
}

TEST(CairoRenderer, DrawsCircleOutlineNotItsCentre) {
    Drawing drawing;
    drawing.arcs.push_back({{0, 0}, 10, 0, 2 * std::numbers::pi});
    const Image image = renderDrawing(drawing, 100);
    EXPECT_EQ(image.pixel(50, 50), WHITE);
    EXPECT_GT(inkCount(image), 100U);
}

TEST(CairoRenderer, DrawsTextWithTheEmbeddedFont) {
    Drawing drawing;
    drawing.texts.push_back({"HELLO", {0, 0}, 10.0, 0.0, 1.0, HAlign::Left, VAlign::Baseline});
    EXPECT_GT(inkCount(renderDrawing(drawing, 256)), 200U);
}

TEST(CairoRenderer, HandlesASinglePointAndRejectsEmptyDrawings) {
    Drawing point;
    point.polylines.push_back({{{3, 3}, {3, 3}}, false});
    EXPECT_NO_THROW(static_cast<void>(renderDrawing(point, 64)));
    EXPECT_THROW(static_cast<void>(renderDrawing(Drawing{}, 64)), FormatError);
}

} // namespace
} // namespace cadpreview
