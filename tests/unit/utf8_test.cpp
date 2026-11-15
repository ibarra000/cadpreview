#include "common/utf8.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace cadpreview {
namespace {

TEST(Utf8, AcceptsAsciiAndMultibyte) {
    EXPECT_TRUE(isValidUtf8(""));
    EXPECT_TRUE(isValidUtf8("plain/path.dwg"));
    EXPECT_TRUE(isValidUtf8("pi\xC3\xA8"
                            "ce"));                       // U+00E8
    EXPECT_TRUE(isValidUtf8("\xE9\x9B\xB6\xE4\xBB\xB6")); // U+96F6 U+4EF6
    EXPECT_TRUE(isValidUtf8("\xF0\x9F\x93\x90"));         // U+1F4D0
    EXPECT_TRUE(isValidUtf8("\xF4\x8F\xBF\xBF"));         // U+10FFFF
}

TEST(Utf8, RejectsMalformedSequences) {
    EXPECT_FALSE(isValidUtf8("\x80"));             // lone continuation
    EXPECT_FALSE(isValidUtf8("\xC3"));             // truncated
    EXPECT_FALSE(isValidUtf8("\xE9\x9B"));         // truncated
    EXPECT_FALSE(isValidUtf8("\xC3\x28"));         // bad continuation
    EXPECT_FALSE(isValidUtf8("\xC0\xAF"));         // overlong
    EXPECT_FALSE(isValidUtf8("\xE0\x80\xAF"));     // overlong
    EXPECT_FALSE(isValidUtf8("\xF0\x80\x80\xAF")); // overlong
    EXPECT_FALSE(isValidUtf8("\xED\xA0\x80"));     // surrogate
    EXPECT_FALSE(isValidUtf8("\xF4\x90\x80\x80")); // above U+10FFFF
    EXPECT_FALSE(isValidUtf8("\xF5\x80\x80\x80")); // invalid lead
    EXPECT_FALSE(isValidUtf8("\xFF"));
}

TEST(Utf8, TruncationKeepsWholeCodePoints) {
    const std::string_view text = "ab\xE9\x9B\xB6"
                                  "c";
    EXPECT_EQ(utf8PrefixLength(text, 10), text.size());
    EXPECT_EQ(utf8PrefixLength(text, 5), 5U);
    EXPECT_EQ(utf8PrefixLength(text, 4), 2U);
    EXPECT_EQ(utf8PrefixLength(text, 3), 2U);
    EXPECT_EQ(utf8PrefixLength(text, 0), 0U);
}

TEST(Utf8, PathRoundTrips) {
    const std::string_view text = "dir/pi\xC3\xA8"
                                  "ce-\xE9\x9B\xB6.bin";
    const std::u8string expected(u8"dir/pièce-零.bin");
    // gtest is built without char8_t printers, so compare without EXPECT_EQ.
    EXPECT_TRUE(pathFromUtf8(text).generic_u8string() == expected);
}

} // namespace
} // namespace cadpreview
