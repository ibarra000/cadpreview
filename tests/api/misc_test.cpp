#include "api_test_util.hpp"
#include "temp_dir.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace cadpreview::test {
namespace {

TEST(Version, MatchesHeader) {
    EXPECT_STREQ(cp_version(), CP_VERSION_STRING);
}

TEST(ReasonName, KnownCodesHaveStableIds) {
    EXPECT_STREQ(cp_reason_name(CP_REASON_NONE), "none");
    EXPECT_STREQ(cp_reason_name(CP_REASON_CANNOT_OPEN), "cannot_open");
    EXPECT_STREQ(cp_reason_name(CP_REASON_EMPTY_INPUT), "empty_input");
    EXPECT_STREQ(cp_reason_name(CP_REASON_UNKNOWN_FORMAT), "unknown_format");
    EXPECT_STREQ(cp_reason_name(CP_REASON_TIER_UNAVAILABLE), "tier_unavailable");
    EXPECT_STREQ(cp_reason_name(CP_REASON_TIERS_FAILED), "tiers_failed");
    EXPECT_STREQ(cp_reason_name(CP_REASON_PLACEHOLDER_REQUESTED), "placeholder_requested");
}

TEST(ReasonName, UnknownCodesAreNeverNull) {
    EXPECT_STREQ(cp_reason_name(-1), "unrecognized");
    EXPECT_STREQ(cp_reason_name(1000), "unrecognized");
}

TEST(Detect, MissingFileIsUnknownNotAnError) {
    const TempDir dir;
    cp_format_info info{};
    info.struct_size = sizeof(info);
    ASSERT_EQ(cp_detect_format(utf8(dir.path() / "nope.dwg").c_str(), &info), CP_OK);
    EXPECT_EQ(info.format, CP_FORMAT_UNKNOWN);
    EXPECT_STREQ(static_cast<const char*>(info.name), "unknown");
    EXPECT_STREQ(static_cast<const char*>(info.version), "");
    EXPECT_EQ(info.struct_size, sizeof(cp_format_info));
}

TEST(Detect, CompoundFileIsUnknownInThisVersion) {
    const TempDir dir;
    const auto file = dir.write("container.bin", cfbSignatureFile());
    cp_format_info info{};
    info.struct_size = sizeof(info);
    ASSERT_EQ(cp_detect_format(utf8(file).c_str(), &info), CP_OK);
    EXPECT_EQ(info.format, CP_FORMAT_UNKNOWN);
}

TEST(Png, StartsWithSignature) {
    const std::array<std::uint8_t, 1> byte{0};
    const PreviewPtr preview = renderBytes(byte, nullptr);
    ASSERT_NE(preview, nullptr);
    const auto png = pngOf(preview.get());
    ASSERT_GE(png.size(), 8U);
    const std::array<std::uint8_t, 8> signature{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    EXPECT_TRUE(std::equal(signature.begin(), signature.end(), png.begin()));
}

} // namespace
} // namespace cadpreview::test
