#include "api_test_util.hpp"
#include "png_decode.hpp"
#include "temp_dir.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cadpreview::test {
namespace {

const std::array<std::uint8_t, 4> RANDOM_BYTES{0x13, 0x37, 0xC0, 0xDE};

void expectPlaceholder(const cp_preview* preview, cp_reason reason, std::uint32_t size) {
    ASSERT_NE(preview, nullptr);
    const struct cp_preview_info info = infoOf(preview);
    EXPECT_EQ(info.ok, 0);
    EXPECT_EQ(info.tier, static_cast<std::uint32_t>(CP_TIER_PLACEHOLDER));
    EXPECT_EQ(info.reason_code, reason);
    EXPECT_EQ(info.width, size);
    EXPECT_EQ(info.height, size);
    EXPECT_EQ(info.format, CP_FORMAT_UNKNOWN);
    ASSERT_NE(info.reason, nullptr);
    EXPECT_STRNE(info.reason, "");
    EXPECT_STREQ(info.format_name, "unknown");
    EXPECT_STREQ(info.version, "");
    EXPECT_STREQ(info.warnings, "");

    const auto decoded = decodePng(pngOf(preview));
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->width, size);
    EXPECT_EQ(decoded->height, size);
    EXPECT_GT(decoded->countPixelsNot({255, 255, 255, 255}), 0U);
}

TEST(Render, MissingFileGivesCannotOpenPlaceholder) {
    const TempDir dir;
    const PreviewPtr preview = renderPath(utf8(dir.path() / "missing.sldprt"), nullptr);
    expectPlaceholder(preview.get(), CP_REASON_CANNOT_OPEN, 512);
}

TEST(Render, DirectoryGivesCannotOpenPlaceholder) {
    const TempDir dir;
    const PreviewPtr preview = renderPath(utf8(dir.path()), nullptr);
    expectPlaceholder(preview.get(), CP_REASON_CANNOT_OPEN, 512);
}

TEST(Render, EmptyFileGivesEmptyInputPlaceholder) {
    const TempDir dir;
    const auto file = dir.write("empty.dwg", {});
    const PreviewPtr preview = renderPath(utf8(file), nullptr);
    expectPlaceholder(preview.get(), CP_REASON_EMPTY_INPUT, 512);
}

TEST(Render, RandomBytesGiveUnknownFormatPlaceholder) {
    const cp_options options = makeOptions(128);
    const PreviewPtr preview = renderBytes(RANDOM_BYTES, &options);
    expectPlaceholder(preview.get(), CP_REASON_UNKNOWN_FORMAT, 128);
}

TEST(Render, CompoundFileWithoutKnownFormatIsUnknown) {
    const auto bytes = cfbSignatureFile();
    const PreviewPtr preview = renderBytes(bytes, nullptr);
    expectPlaceholder(preview.get(), CP_REASON_UNKNOWN_FORMAT, 512);
    EXPECT_NE(reasonOf(preview.get()).find("compound"), std::string_view::npos);
}

TEST(Render, FileAndMemoryAgree) {
    const TempDir dir;
    const auto file = dir.write("sample.bin", RANDOM_BYTES);
    const cp_options options = makeOptions(64);
    const PreviewPtr fromFile = renderPath(utf8(file), &options);
    const PreviewPtr fromMemory = renderBytes(RANDOM_BYTES, &options);
    ASSERT_NE(fromFile, nullptr);
    ASSERT_NE(fromMemory, nullptr);
    const auto a = pngOf(fromFile.get());
    const auto b = pngOf(fromMemory.get());
    EXPECT_TRUE(std::ranges::equal(a, b));
}

TEST(Render, NonAsciiPathIsOpened) {
    const TempDir dir;
    const auto file = dir.write(std::filesystem::path(u8"pièce-零件.bin"), RANDOM_BYTES);
    const PreviewPtr preview = renderPath(utf8(file), nullptr);
    expectPlaceholder(preview.get(), CP_REASON_UNKNOWN_FORMAT, 512);
}

TEST(Render, ForcedPlaceholderTierReportsRequest) {
    const cp_options options = makeOptions(0, CP_TIER_PLACEHOLDER);
    const PreviewPtr preview = renderBytes(RANDOM_BYTES, &options);
    expectPlaceholder(preview.get(), CP_REASON_PLACEHOLDER_REQUESTED, 512);
}

TEST(Render, ForcedTierOnUnknownFormatReportsUnknownFormat) {
    for (const std::uint32_t tier : {1U, 2U, 3U, 4U}) {
        const cp_options options = makeOptions(0, tier);
        const PreviewPtr preview = renderBytes(RANDOM_BYTES, &options);
        expectPlaceholder(preview.get(), CP_REASON_UNKNOWN_FORMAT, 512);
    }
}

TEST(Render, ReasonTextNeverContainsThePath) {
    const TempDir dir;
    const std::string path = utf8(dir.path() / "secret-project-name.prt");
    const PreviewPtr preview = renderPath(path, nullptr);
    ASSERT_NE(preview, nullptr);
    EXPECT_EQ(reasonOf(preview.get()).find("secret-project-name"), std::string_view::npos);
}

struct LogCapture {
    std::vector<std::pair<cp_log_level, std::string>> messages;
};

void captureLog(void* user, cp_log_level level, const char* message) {
    static_cast<LogCapture*>(user)->messages.emplace_back(level, message);
}

TEST(Render, LogCallbackReceivesMessagesWithUserPointer) {
    LogCapture capture;
    cp_options options = makeOptions();
    options.log = &captureLog;
    options.log_user = &capture;
    const PreviewPtr preview = renderBytes(RANDOM_BYTES, &options);
    ASSERT_NE(preview, nullptr);
    ASSERT_FALSE(capture.messages.empty());
    for (const auto& [level, message] : capture.messages) {
        EXPECT_GE(level, CP_LOG_DEBUG);
        EXPECT_LE(level, CP_LOG_ERROR);
        EXPECT_FALSE(message.empty());
    }
}

TEST(Render, RenderTimeIsReported) {
    const PreviewPtr preview = renderBytes(RANDOM_BYTES, nullptr);
    ASSERT_NE(preview, nullptr);
    EXPECT_LT(infoOf(preview.get()).render_time_us, 60'000'000U);
}

} // namespace
} // namespace cadpreview::test
