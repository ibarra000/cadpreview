#include "formats/dwg/preview.hpp"

#include "common/errors.hpp"
#include "containers/byte_reader.hpp"
#include "containers/source.hpp"

#include <algorithm>
#include <array>
#include <optional>

namespace cadpreview::dwg {

namespace {

// see notes/dwg.md#image-seeker and notes/dwg.md#preview-block
constexpr std::size_t IMAGE_SEEKER_OFFSET = 0x0D;
constexpr std::array<std::uint8_t, 16> START_SENTINEL{
    0x1F, 0x25, 0x6D, 0x07, 0xD4, 0x36, 0x28, 0x28, 0x9D, 0x57, 0xCA, 0x3F, 0x9D, 0x44, 0x10, 0x2B};
constexpr std::uint8_t TYPE_DIB = 2;
constexpr std::uint8_t TYPE_WMF = 3;
constexpr std::uint8_t TYPE_PNG = 6;
constexpr std::array<std::uint8_t, 8> PNG_SIGNATURE{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};

bool startsWith(std::span<const std::byte> data, std::span<const std::uint8_t> prefix) {
    return data.size() >= prefix.size() &&
           std::equal(prefix.begin(), prefix.end(), data.begin(), [](std::uint8_t a, std::byte b) {
               return a == static_cast<std::uint8_t>(b);
           });
}

struct Entry {
    std::uint8_t type = 0;
    std::span<const std::byte> data;
};

Entry readEntry(ByteReader& table, std::span<const std::byte> file) {
    Entry entry;
    entry.type = table.u8();
    const std::uint32_t start = table.u32();
    const std::uint32_t size = table.u32();
    if (!inRange(start, size, file.size())) {
        throw FormatError("DWG preview entry lies outside the file");
    }
    entry.data = file.subspan(start, size);
    return entry;
}

std::optional<PreviewImage> asImage(const Entry& entry) {
    if (entry.type == TYPE_DIB) {
        return PreviewImage{ImageKind::Dib, entry.data};
    }
    if (entry.type == TYPE_PNG && startsWith(entry.data, PNG_SIGNATURE)) {
        return PreviewImage{ImageKind::Png, entry.data};
    }
    return std::nullopt;
}

} // namespace

PreviewImage findPreview(std::span<const std::byte> file) {
    ByteReader reader(file);
    reader.seek(IMAGE_SEEKER_OFFSET);
    reader.seek(reader.u32());
    if (!startsWith(reader.bytes(START_SENTINEL.size()), START_SENTINEL)) {
        throw FormatError("no DWG preview block");
    }
    static_cast<void>(reader.u32()); // overall size; entries are bounds-checked individually
    const std::uint8_t count = reader.u8();
    std::optional<PreviewImage> best;
    bool sawWmf = false;
    for (std::uint8_t i = 0; i < count; ++i) {
        const Entry entry = readEntry(reader, file);
        sawWmf = sawWmf || entry.type == TYPE_WMF;
        const auto image = asImage(entry);
        if (image && (!best || image->data.size() > best->data.size())) {
            best = image;
        }
    }
    if (!best) {
        throw FormatError(sawWmf ? "WMF preview not supported" : "no embedded preview");
    }
    return *best;
}

} // namespace cadpreview::dwg
