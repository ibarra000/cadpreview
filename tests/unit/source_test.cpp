#include "containers/source.hpp"
#include "temp_dir.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace cadpreview {
namespace {

using test::TempDir;

constexpr auto U64_MAX = std::numeric_limits<std::uint64_t>::max();

std::vector<std::byte> toBytes(std::initializer_list<int> values) {
    std::vector<std::byte> bytes;
    for (const int v : values) {
        bytes.push_back(static_cast<std::byte>(v));
    }
    return bytes;
}

TEST(InRange, HandlesEdgesAndOverflow) {
    EXPECT_TRUE(inRange(0, 0, 0));
    EXPECT_TRUE(inRange(0, 10, 10));
    EXPECT_TRUE(inRange(10, 0, 10));
    EXPECT_FALSE(inRange(11, 0, 10));
    EXPECT_FALSE(inRange(5, 6, 10));
    EXPECT_FALSE(inRange(U64_MAX, 1, 10));
    EXPECT_FALSE(inRange(1, U64_MAX, 10));
    EXPECT_FALSE(inRange(U64_MAX, U64_MAX, U64_MAX));
}

TEST(MemorySource, ReadsWithinBoundsOnly) {
    const auto data = toBytes({1, 2, 3, 4});
    MemorySource source(data);
    EXPECT_EQ(source.size(), 4U);
    std::array<std::byte, 2> out{};
    ASSERT_TRUE(source.readAt(2, out));
    EXPECT_EQ(out[0], std::byte{3});
    EXPECT_EQ(out[1], std::byte{4});
    EXPECT_FALSE(source.readAt(3, out));
    EXPECT_FALSE(source.readAt(U64_MAX, out));
    EXPECT_TRUE(source.readAt(4, std::span<std::byte>{}));
}

TEST(OpenFile, MissingFileReportsError) {
    const TempDir dir;
    const FileOpenResult result = openFile(dir.path() / "missing.bin");
    EXPECT_EQ(result.source, nullptr);
    EXPECT_FALSE(result.error.empty());
}

TEST(OpenFile, DirectoryIsNotARegularFile) {
    const TempDir dir;
    const FileOpenResult result = openFile(dir.path());
    EXPECT_EQ(result.source, nullptr);
    EXPECT_EQ(result.error, "not a regular file");
}

TEST(OpenFile, ReadsRegularFile) {
    const TempDir dir;
    const std::array<std::uint8_t, 5> bytes{10, 20, 30, 40, 50};
    const FileOpenResult result = openFile(dir.write("five.bin", bytes));
    ASSERT_NE(result.source, nullptr);
    EXPECT_EQ(result.source->size(), 5U);
    std::array<std::byte, 3> out{};
    ASSERT_TRUE(result.source->readAt(1, out));
    EXPECT_EQ(out[0], std::byte{20});
    EXPECT_EQ(out[2], std::byte{40});
    EXPECT_FALSE(result.source->readAt(3, out));
    ASSERT_TRUE(result.source->readAt(0, out));
    EXPECT_EQ(out[0], std::byte{10});
}

TEST(ReadPrefix, ClampsToSourceSize) {
    const auto data = toBytes({7, 8, 9});
    MemorySource source(data);
    const auto prefix = readPrefix(source, 8);
    ASSERT_TRUE(prefix.has_value());
    EXPECT_EQ(prefix->size(), 3U);
    const auto shorter = readPrefix(source, 2);
    ASSERT_TRUE(shorter.has_value());
    EXPECT_EQ(shorter->size(), 2U);
}

} // namespace
} // namespace cadpreview
