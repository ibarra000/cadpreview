#include "common/errors.hpp"
#include "common/log.hpp"
#include "formats/dwg/preview.hpp"
#include "tiers/pipeline.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cadpreview::dwg {
namespace {

// Builds a DWG prefix with the confirmed preview layout (see notes/dwg.md#preview-block).
class DwgBuilder {
public:
    DwgBuilder() : bytes_(0x40, std::byte{0}) {
        const std::string version = "AC1024";
        for (std::size_t i = 0; i < version.size(); ++i) {
            bytes_[i] = static_cast<std::byte>(version[i]);
        }
    }

    // Each entry is (type, data). Offsets are computed so the table is valid.
    [[nodiscard]] std::vector<std::byte>
    build(const std::vector<std::pair<int, std::vector<std::byte>>>& entries,
          bool endSentinel = true) const {
        std::vector<std::byte> out = bytes_;
        const auto blockStart = static_cast<std::uint32_t>(out.size());
        put32(out, 0x0D, blockStart);
        for (const int b : START) {
            out.push_back(static_cast<std::byte>(b));
        }
        const std::size_t sizeAt = out.size();
        append32(out, 0);
        out.push_back(static_cast<std::byte>(entries.size()));
        auto dataStart = static_cast<std::uint32_t>(out.size() + entries.size() * 9);
        for (const auto& [type, data] : entries) {
            out.push_back(static_cast<std::byte>(type));
            append32(out, dataStart);
            append32(out, static_cast<std::uint32_t>(data.size()));
            dataStart += static_cast<std::uint32_t>(data.size());
        }
        for (const auto& entry : entries) {
            out.insert(out.end(), entry.second.begin(), entry.second.end());
        }
        put32(out, sizeAt, static_cast<std::uint32_t>(out.size() - sizeAt - 4));
        if (endSentinel) {
            for (const int b : START) {
                out.push_back(static_cast<std::byte>(~static_cast<unsigned>(b) & 0xFFU));
            }
        }
        return out;
    }

    static constexpr std::array<int, 16> START{0x1F, 0x25, 0x6D, 0x07, 0xD4, 0x36, 0x28, 0x28,
                                               0x9D, 0x57, 0xCA, 0x3F, 0x9D, 0x44, 0x10, 0x2B};

private:
    static void put32(std::vector<std::byte>& out, std::size_t at, std::uint32_t v) {
        for (std::size_t i = 0; i < 4; ++i) {
            out[at + i] = static_cast<std::byte>((v >> (8 * i)) & 0xFFU);
        }
    }
    static void append32(std::vector<std::byte>& out, std::uint32_t v) {
        for (std::size_t i = 0; i < 4; ++i) {
            out.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFFU));
        }
    }

    std::vector<std::byte> bytes_;
};

// 1x1 24-bit DIB (BITMAPINFOHEADER + one padded pixel), red.
std::vector<std::byte> tinyDib() {
    std::vector<std::byte> dib(44, std::byte{0});
    dib[0] = std::byte{40};
    dib[4] = std::byte{1};
    dib[8] = std::byte{1};
    dib[12] = std::byte{1};
    dib[14] = std::byte{24};
    dib[42] = std::byte{0xFF}; // B G R order: red
    return dib;
}

std::vector<std::byte> header80() {
    return std::vector<std::byte>(80, std::byte{0});
}

TEST(DwgPreview, FindsTheLargestImageEntry) {
    auto big = tinyDib();
    big.resize(100, std::byte{0}); // trailing bytes make it the largest entry
    const auto file = DwgBuilder().build({{1, header80()}, {2, tinyDib()}, {2, big}});
    const PreviewImage image = findPreview(file);
    EXPECT_EQ(image.kind, ImageKind::Dib);
    EXPECT_EQ(image.data.size(), 100U);
}

TEST(DwgPreview, ReportsMissingOrUnsupportedPreviews) {
    EXPECT_THROW(static_cast<void>(findPreview(DwgBuilder().build({{1, header80()}}))),
                 FormatError);
    EXPECT_THROW(static_cast<void>(findPreview(DwgBuilder().build({{3, header80()}}))),
                 FormatError);
    auto noSeeker = DwgBuilder().build({{2, tinyDib()}});
    noSeeker[0x0D] = std::byte{0xFF};
    noSeeker[0x0E] = std::byte{0xFF};
    EXPECT_THROW(static_cast<void>(findPreview(noSeeker)), FormatError);
}

TEST(DwgPreview, RejectsEntriesOutsideTheFile) {
    auto file = DwgBuilder().build({{2, tinyDib()}});
    file.resize(file.size() - 30); // cut through the image and the end sentinel
    EXPECT_THROW(static_cast<void>(findPreview(file)), FormatError);
}

TEST(DwgPreview, AcceptsPngOnlyWithItsSignature) {
    std::vector<std::byte> fakePng(20, std::byte{0x42});
    EXPECT_THROW(static_cast<void>(findPreview(DwgBuilder().build({{6, fakePng}}))), FormatError);
}

TEST(DwgTiers, ThumbnailThroughThePipeline) {
    const auto file = DwgBuilder().build({{1, header80()}, {2, tinyDib()}});
    const Preview preview = renderMemory(file, {64, Tier::Auto}, defaultRegistry(), Logger{});
    EXPECT_TRUE(preview.ok);
    EXPECT_EQ(preview.tier, Tier::Thumbnail);
    EXPECT_EQ(preview.format, Format::Dwg);
    EXPECT_EQ(preview.version, "AC1024");
}

} // namespace
} // namespace cadpreview::dwg
