#include "common/errors.hpp"
#include "render/decode/dib.hpp"
#include "render/decode/png_decode.hpp"
#include "render/image.hpp"
#include "render/png.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace cadpreview {
namespace {

void put16(std::vector<std::byte>& out, std::uint32_t v) {
    out.push_back(static_cast<std::byte>(v & 0xFFU));
    out.push_back(static_cast<std::byte>((v >> 8U) & 0xFFU));
}

void put32(std::vector<std::byte>& out, std::uint32_t v) {
    put16(out, v & 0xFFFFU);
    put16(out, v >> 16U);
}

void putBytes(std::vector<std::byte>& out, std::initializer_list<int> values) {
    for (const int v : values) {
        out.push_back(static_cast<std::byte>(v));
    }
}

// BITMAPINFOHEADER (40 bytes) as documented by Microsoft.
std::vector<std::byte> infoHeader(std::int32_t width, std::int32_t height, std::uint16_t bpp,
                                  std::uint32_t compression = 0, std::uint32_t colorsUsed = 0) {
    std::vector<std::byte> out;
    put32(out, 40);
    put32(out, static_cast<std::uint32_t>(width));
    put32(out, static_cast<std::uint32_t>(height));
    put16(out, 1);
    put16(out, bpp);
    put32(out, compression);
    put32(out, 0);
    put32(out, 0);
    put32(out, 0);
    put32(out, colorsUsed);
    put32(out, 0);
    return out;
}

// 2x2, 24 bpp, bottom-up: bottom row red, blue; top row green, white. Rows pad to 8 bytes.
std::vector<std::byte> dib24() {
    auto dib = infoHeader(2, 2, 24);
    putBytes(dib, {0, 0, 255, 255, 0, 0, 0, 0});     // bottom row: red, blue (BGR order)
    putBytes(dib, {0, 255, 0, 255, 255, 255, 0, 0}); // top row: green, white
    return dib;
}

constexpr Rgba RED{255, 0, 0, 255};
constexpr Rgba GREEN{0, 255, 0, 255};
constexpr Rgba BLUE{0, 0, 255, 255};
constexpr Rgba WHITE{255, 255, 255, 255};

TEST(Dib, Decodes24BitBottomUp) {
    const Image image = decodeDib(dib24());
    ASSERT_EQ(image.width(), 2U);
    ASSERT_EQ(image.height(), 2U);
    EXPECT_EQ(image.pixel(0, 0), GREEN);
    EXPECT_EQ(image.pixel(1, 0), WHITE);
    EXPECT_EQ(image.pixel(0, 1), RED);
    EXPECT_EQ(image.pixel(1, 1), BLUE);
}

TEST(Dib, AcceptsAFullBmpFile) {
    std::vector<std::byte> file;
    putBytes(file, {'B', 'M'});
    put32(file, 0);
    put32(file, 0);
    put32(file, 54);
    const auto dib = dib24();
    file.insert(file.end(), dib.begin(), dib.end());
    EXPECT_EQ(decodeDib(file).pixel(1, 1), BLUE);
}

TEST(Dib, DecodesTopDown8BitPalette) {
    auto dib = infoHeader(3, -1, 8, 0, 2);
    putBytes(dib, {0, 0, 0, 0, 255, 255, 255, 0}); // palette: black, white
    putBytes(dib, {1, 0, 1, 0});                   // one row, padded to 4
    const Image image = decodeDib(dib);
    EXPECT_EQ(image.pixel(0, 0), WHITE);
    EXPECT_EQ(image.pixel(1, 0), (Rgba{0, 0, 0, 255}));
}

TEST(Dib, Decodes1BitAnd4Bit) {
    auto mono = infoHeader(9, 1, 1);
    putBytes(mono, {0, 0, 0, 0, 255, 255, 255, 0});
    putBytes(mono, {0b10000000, 0b10000000, 0, 0}); // pixels 0 and 8 white
    const Image monoImage = decodeDib(mono);
    EXPECT_EQ(monoImage.pixel(0, 0), WHITE);
    EXPECT_EQ(monoImage.pixel(1, 0), (Rgba{0, 0, 0, 255}));
    EXPECT_EQ(monoImage.pixel(8, 0), WHITE);

    auto nibble = infoHeader(2, 1, 4, 0, 2);
    putBytes(nibble, {0, 0, 255, 0, 0, 255, 0, 0}); // palette: red, green
    putBytes(nibble, {0x10, 0, 0, 0});
    const Image nibbleImage = decodeDib(nibble);
    EXPECT_EQ(nibbleImage.pixel(0, 0), GREEN);
    EXPECT_EQ(nibbleImage.pixel(1, 0), RED);
}

TEST(Dib, Decodes32BitAsOpaque) {
    auto dib = infoHeader(1, 1, 32);
    putBytes(dib, {255, 0, 0, 0}); // blue, alpha byte unused
    EXPECT_EQ(decodeDib(dib).pixel(0, 0), BLUE);
}

TEST(Dib, RejectsMalformedInput) {
    auto truncated = dib24();
    truncated.resize(truncated.size() - 1);
    EXPECT_THROW(static_cast<void>(decodeDib(truncated)), FormatError);
    EXPECT_THROW(static_cast<void>(decodeDib(infoHeader(0, 1, 24))), FormatError);
    EXPECT_THROW(static_cast<void>(decodeDib(infoHeader(100000, 1, 24))), FormatError);
    EXPECT_THROW(static_cast<void>(decodeDib(infoHeader(1, 1, 7))), FormatError);
    EXPECT_THROW(static_cast<void>(decodeDib(infoHeader(1, 1, 24, 1))), FormatError);
    auto badIndex = infoHeader(1, 1, 8, 0, 1);
    putBytes(badIndex, {0, 0, 0, 0, 5, 0, 0, 0});
    EXPECT_THROW(static_cast<void>(decodeDib(badIndex)), FormatError);
    EXPECT_THROW(static_cast<void>(decodeDib({})), FormatError);
}

TEST(Dib, RejectsAHeaderThatClaimsMoreRowsThanArePresent) {
    // 8192 x 8192 x 24 bpp would need 192 MiB of rows; only the 40-byte header is present.
    EXPECT_THROW(static_cast<void>(decodeDib(infoHeader(8192, 8192, 24))), FormatError);
}

TEST(PngDecode, RoundTripsOurEncoder) {
    Image source(3, 2, WHITE);
    source.fillRect(0, 1, 1, 1, RED);
    const auto png = encodePng(source);
    std::vector<std::byte> bytes(png.size());
    for (std::size_t i = 0; i < png.size(); ++i) {
        bytes[i] = static_cast<std::byte>(png[i]);
    }
    const Image decoded = decodePngImage(bytes);
    EXPECT_EQ(decoded.width(), 3U);
    EXPECT_EQ(decoded.pixel(0, 1), RED);
    EXPECT_EQ(decoded.pixel(2, 0), WHITE);
}

TEST(PngDecode, RejectsGarbage) {
    std::vector<std::byte> garbage(64, std::byte{0x42});
    EXPECT_THROW(static_cast<void>(decodePngImage(garbage)), FormatError);
}

} // namespace
} // namespace cadpreview
