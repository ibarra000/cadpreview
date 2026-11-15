#include "common/errors.hpp"
#include "containers/byte_reader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace cadpreview {
namespace {

constexpr std::array<std::byte, 10> DATA{
    std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}, std::byte{0x05},
    std::byte{0x06}, std::byte{0x07}, std::byte{0x08}, std::byte{0xFF}, std::byte{0x7F}};

TEST(ByteReader, ReadsLittleEndianIntegers) {
    ByteReader reader(DATA);
    EXPECT_EQ(reader.u8(), 0x01U);
    EXPECT_EQ(reader.u16(), 0x0302U);
    EXPECT_EQ(reader.u32(), 0x07060504U);
    EXPECT_EQ(reader.position(), 7U);
    EXPECT_EQ(reader.remaining(), 3U);
}

TEST(ByteReader, ReadsSignedValues) {
    ByteReader reader(DATA);
    reader.seek(8);
    EXPECT_EQ(reader.i16(), 0x7FFF);
    reader.seek(6);
    EXPECT_EQ(reader.i32(), static_cast<std::int32_t>(0x7FFF0807U));
}

TEST(ByteReader, ReadsU64AndBytes) {
    ByteReader reader(DATA);
    EXPECT_EQ(reader.u64(), 0x0807060504030201ULL);
    const auto rest = reader.bytes(2);
    EXPECT_EQ(rest.size(), 2U);
    EXPECT_EQ(rest[0], std::byte{0xFF});
    EXPECT_EQ(reader.remaining(), 0U);
}

TEST(ByteReader, OverrunsThrowFormatError) {
    ByteReader reader(DATA);
    reader.seek(9);
    EXPECT_THROW(static_cast<void>(reader.u16()), FormatError);
    EXPECT_EQ(reader.position(), 9U);
    EXPECT_THROW(reader.seek(11), FormatError);
    EXPECT_THROW(static_cast<void>(reader.bytes(2)), FormatError);
    EXPECT_THROW(reader.skip(std::numeric_limits<std::size_t>::max()), FormatError);
    EXPECT_NO_THROW(reader.seek(10));
    EXPECT_THROW(static_cast<void>(reader.u8()), FormatError);
}

TEST(ByteReader, SubReaderIsBounded) {
    ByteReader reader(DATA);
    reader.seek(2);
    ByteReader sub = reader.sub(3);
    EXPECT_EQ(reader.position(), 5U);
    EXPECT_EQ(sub.remaining(), 3U);
    EXPECT_EQ(sub.u8(), 0x03U);
    EXPECT_THROW(static_cast<void>(sub.u32()), FormatError);
}

} // namespace
} // namespace cadpreview
