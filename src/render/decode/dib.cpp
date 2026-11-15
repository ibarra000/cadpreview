#include "render/decode/dib.hpp"

#include "common/errors.hpp"
#include "containers/byte_reader.hpp"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace cadpreview {

namespace {

// Field sizes and values follow Microsoft's BITMAPFILEHEADER and BITMAPINFOHEADER
// documentation; see notes/images.md#dib.
constexpr std::size_t FILE_HEADER_SIZE = 14;
constexpr std::uint32_t INFO_HEADER_SIZE = 40;
constexpr std::uint32_t MAX_INFO_HEADER_SIZE = 124; // BITMAPV5HEADER
constexpr std::uint32_t BI_RGB = 0;
constexpr std::uint32_t BI_BITFIELDS = 3;

struct Header {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool topDown = false;
    std::uint16_t bitsPerPixel = 0;
    std::uint32_t paletteSize = 0;
};

std::uint32_t checkedEdge(std::int64_t value) {
    if (value <= 0 || std::cmp_greater(value, MAX_IMAGE_EDGE)) {
        throw FormatError("bitmap size out of range");
    }
    return static_cast<std::uint32_t>(value);
}

Header readHeader(ByteReader& reader) {
    const std::uint32_t headerSize = reader.u32();
    if (headerSize < INFO_HEADER_SIZE || headerSize > MAX_INFO_HEADER_SIZE) {
        throw FormatError("unsupported bitmap header");
    }
    Header header;
    header.width = checkedEdge(reader.i32());
    const std::int32_t height = reader.i32();
    header.topDown = height < 0;
    header.height = checkedEdge(header.topDown ? -static_cast<std::int64_t>(height) : height);
    if (reader.u16() != 1) {
        throw FormatError("bitmap must have one plane");
    }
    header.bitsPerPixel = reader.u16();
    const std::uint32_t compression = reader.u32();
    const bool bitfields32 = compression == BI_BITFIELDS && header.bitsPerPixel == 32;
    if (compression != BI_RGB && !bitfields32) {
        throw FormatError("compressed bitmaps are not supported");
    }
    reader.skip(12); // image size and resolution
    const std::uint32_t colorsUsed = reader.u32();
    reader.skip(4); // important colours
    reader.skip(headerSize - INFO_HEADER_SIZE);
    if (bitfields32 && headerSize == INFO_HEADER_SIZE) {
        reader.skip(12); // the three masks follow the 40-byte header
    }
    switch (header.bitsPerPixel) {
    case 1:
    case 4:
    case 8:
        header.paletteSize = colorsUsed != 0 ? colorsUsed : (1U << header.bitsPerPixel);
        if (header.paletteSize > (1U << header.bitsPerPixel)) {
            throw FormatError("bitmap palette too large");
        }
        break;
    case 24:
    case 32:
        break;
    default:
        throw FormatError("unsupported bitmap depth");
    }
    return header;
}

std::vector<Rgba> readPalette(ByteReader& reader, std::uint32_t count) {
    std::vector<Rgba> palette;
    palette.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto entry = reader.bytes(4);
        palette.push_back({static_cast<std::uint8_t>(entry[2]), static_cast<std::uint8_t>(entry[1]),
                           static_cast<std::uint8_t>(entry[0]), 255});
    }
    return palette;
}

Rgba indexed(std::span<const std::byte> row, std::uint32_t x, std::uint16_t bits,
             const std::vector<Rgba>& palette) {
    const std::size_t bitOffset = static_cast<std::size_t>(x) * bits;
    const auto byte = static_cast<unsigned>(row[bitOffset / 8]);
    const unsigned shift = 8U - bits - static_cast<unsigned>(bitOffset % 8);
    const unsigned index = (byte >> shift) & ((1U << bits) - 1U);
    if (index >= palette.size()) {
        throw FormatError("bitmap palette index out of range");
    }
    return palette[index];
}

Rgba direct(std::span<const std::byte> row, std::uint32_t x, std::uint16_t bits) {
    const std::size_t offset = static_cast<std::size_t>(x) * (bits / 8U);
    return {static_cast<std::uint8_t>(row[offset + 2]), static_cast<std::uint8_t>(row[offset + 1]),
            static_cast<std::uint8_t>(row[offset]), 255};
}

} // namespace

Image decodeDib(std::span<const std::byte> data) {
    ByteReader reader(data);
    if (data.size() >= FILE_HEADER_SIZE && data[0] == std::byte{'B'} && data[1] == std::byte{'M'}) {
        reader.skip(FILE_HEADER_SIZE);
    }
    const Header header = readHeader(reader);
    const std::vector<Rgba> palette = readPalette(reader, header.paletteSize);

    const std::uint64_t rowBits = static_cast<std::uint64_t>(header.width) * header.bitsPerPixel;
    const auto stride = static_cast<std::size_t>(((rowBits + 31U) / 32U) * 4U);
    // Check that every row is present before allocating the image: a header alone can claim
    // 8192 x 8192 pixels.
    if (reader.remaining() / stride < header.height) {
        throw FormatError("bitmap pixel data is truncated");
    }
    Image image(header.width, header.height, {});
    for (std::uint32_t stored = 0; stored < header.height; ++stored) {
        const auto row = reader.bytes(stride);
        const std::uint32_t y = header.topDown ? stored : header.height - 1 - stored;
        for (std::uint32_t x = 0; x < header.width; ++x) {
            const Rgba color = header.bitsPerPixel <= 8
                                   ? indexed(row, x, header.bitsPerPixel, palette)
                                   : direct(row, x, header.bitsPerPixel);
            image.setPixel(x, y, color);
        }
    }
    return image;
}

} // namespace cadpreview
