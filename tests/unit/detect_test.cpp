#include "containers/source.hpp"
#include "formats/detect.hpp"
#include "formats/format.hpp"
#include "temp_dir.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace cadpreview {
namespace {

std::vector<std::byte> bytesOf(std::initializer_list<int> values, std::size_t padTo = 0) {
    std::vector<std::byte> bytes;
    for (const int v : values) {
        bytes.push_back(static_cast<std::byte>(v));
    }
    if (bytes.size() < padTo) {
        bytes.resize(padTo, std::byte{0});
    }
    return bytes;
}

Detection detectBytes(const std::vector<std::byte>& bytes) {
    MemorySource source(bytes);
    return detect(source);
}

TEST(Detect, CompoundFileSignature) {
    const Detection d = detectBytes(bytesOf({0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}, 512));
    EXPECT_EQ(d.container, Container::CompoundFile);
    EXPECT_EQ(d.format, Format::Unknown);
    EXPECT_FALSE(d.readFailed);
    EXPECT_NE(d.detail.find("compound file"), std::string::npos);
}

TEST(Detect, TruncatedCompoundSignatureIsNotAContainer) {
    const Detection d = detectBytes(bytesOf({0xD0, 0xCF, 0x11, 0xE0}));
    EXPECT_EQ(d.container, Container::None);
}

TEST(Detect, ZipLocalHeader) {
    const Detection d = detectBytes(bytesOf({'P', 'K', 3, 4, 20, 0}, 30));
    EXPECT_EQ(d.container, Container::Zip);
    EXPECT_NE(d.detail.find("ZIP"), std::string::npos);
}

TEST(Detect, ArbitraryBytesAreUnrecognized) {
    const Detection d = detectBytes(bytesOf({'h', 'e', 'l', 'l', 'o'}));
    EXPECT_EQ(d.container, Container::None);
    EXPECT_EQ(d.format, Format::Unknown);
    EXPECT_EQ(d.detail, "unrecognized file format");
}

TEST(Detect, MissingFileIsUnknown) {
    const test::TempDir dir;
    const Detection d = detectFile(dir.path() / "missing.prt");
    EXPECT_EQ(d.format, Format::Unknown);
    EXPECT_TRUE(d.readFailed);
}

TEST(Format, IdsAndNames) {
    EXPECT_EQ(formatId(Format::Unknown), "unknown");
    EXPECT_EQ(formatDisplayName(Format::Unknown), "Unknown format");
    EXPECT_EQ(formatId(static_cast<Format>(12345)), "unknown");
}

} // namespace
} // namespace cadpreview
