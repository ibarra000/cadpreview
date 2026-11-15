#include "png_decode.hpp"
#include "render/image.hpp"
#include "render/png.hpp"
#include "render/text.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace cadpreview {
namespace {

constexpr Rgba WHITE{255, 255, 255, 255};
constexpr Rgba RED{200, 10, 10, 255};

TEST(Image, StartsFilledAndClipsRectangles) {
    Image image(4, 3, WHITE);
    EXPECT_EQ(image.width(), 4U);
    EXPECT_EQ(image.height(), 3U);
    EXPECT_EQ(image.pixels().size(), 4U * 3U * 4U);
    image.fillRect(-2, -2, 4, 4, RED);
    image.fillRect(3, 2, 100, 100, RED);
    EXPECT_EQ(image.pixel(0, 0), RED);
    EXPECT_EQ(image.pixel(1, 1), RED);
    EXPECT_EQ(image.pixel(2, 2), WHITE);
    EXPECT_EQ(image.pixel(3, 2), RED);
}

TEST(Image, RejectsBadDimensions) {
    EXPECT_THROW(Image(0, 10, WHITE), std::invalid_argument);
    EXPECT_THROW(Image(10, 0, WHITE), std::invalid_argument);
    EXPECT_THROW(Image(MAX_IMAGE_EDGE + 1, 1, WHITE), std::invalid_argument);
}

TEST(Font, EveryPrintableGlyphHasInk) {
    for (char c = '!'; c <= '~'; ++c) {
        const auto rows = glyphRows(c);
        EXPECT_TRUE(std::ranges::any_of(rows, [](std::uint8_t r) { return r != 0; }))
            << "glyph '" << c << "'";
        for (const std::uint8_t row : rows) {
            EXPECT_EQ(row & 0xE0U, 0U) << "glyph '" << c << "' wider than 5 columns";
        }
    }
    const auto space = glyphRows(' ');
    EXPECT_TRUE(std::ranges::all_of(space, [](std::uint8_t r) { return r == 0; }));
    EXPECT_EQ(glyphRows('\x01'), glyphRows('?'));
}

TEST(Text, NonAsciiBecomesQuestionMarks) {
    EXPECT_EQ(toDisplayAscii("pi\xC3\xA8"
                             "ce \xE9\x9B\xB6"),
              "pi?ce ?");
    EXPECT_EQ(toDisplayAscii("tab\there"), "tab?here");
}

TEST(Text, WrapsWordsAndSplitsLongOnes) {
    EXPECT_EQ(wrapText("one two three", 7, 5), (std::vector<std::string>{"one two", "three"}));
    EXPECT_EQ(wrapText("abcdefghij", 4, 5), (std::vector<std::string>{"abcd", "efgh", "ij"}));
    EXPECT_TRUE(wrapText("", 10, 3).empty());
    EXPECT_TRUE(wrapText("text", 0, 3).empty());
}

TEST(Text, TruncatesWithEllipsis) {
    const auto lines = wrapText("alpha beta gamma delta epsilon", 10, 2);
    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0], "alpha beta");
    EXPECT_LE(lines[1].size(), 10U);
    EXPECT_TRUE(lines[1].ends_with("..."));
}

TEST(Text, DrawsInkInsideBounds) {
    Image image(40, 20, WHITE);
    drawText(image, 1, 1, "Hi", 2, RED);
    EXPECT_EQ(textWidth(2, 2), 22);
    bool hasInk = false;
    for (std::uint32_t y = 0; y < 20; ++y) {
        for (std::uint32_t x = 0; x < 40; ++x) {
            hasInk = hasInk || image.pixel(x, y) == RED;
        }
    }
    EXPECT_TRUE(hasInk);
    drawText(image, 38, 18, "clipped", 3, RED);
}

TEST(Png, RoundTripsPixels) {
    Image image(3, 2, WHITE);
    image.fillRect(1, 0, 1, 2, RED);
    const auto png = encodePng(image);
    const auto decoded = test::decodePng(png);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->width, 3U);
    EXPECT_EQ(decoded->height, 2U);
    EXPECT_EQ(decoded->pixel(1, 1), (std::array<std::uint8_t, 4>{200, 10, 10, 255}));
    EXPECT_EQ(decoded->pixel(0, 0), (std::array<std::uint8_t, 4>{255, 255, 255, 255}));
}

} // namespace
} // namespace cadpreview
