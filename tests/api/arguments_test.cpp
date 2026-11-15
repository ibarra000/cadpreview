#include "api_test_util.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace cadpreview::test {
namespace {

const std::array<std::uint8_t, 3> SAMPLE{1, 2, 3};

TEST(Arguments, RenderMemoryRejectsNullOut) {
    EXPECT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), nullptr, nullptr),
              CP_ERROR_INVALID_ARGUMENT);
}

TEST(Arguments, RenderMemoryRejectsNullDataWithLength) {
    int sentinel = 0;
    cp_preview* out = reinterpret_cast<cp_preview*>(&sentinel); // NOLINT: must be reset to null
    EXPECT_EQ(cp_render_memory(nullptr, 4, nullptr, &out), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(out, nullptr);
}

TEST(Arguments, RenderMemoryAcceptsNullDataWithZeroLength) {
    cp_preview* raw = nullptr;
    ASSERT_EQ(cp_render_memory(nullptr, 0, nullptr, &raw), CP_OK);
    const PreviewPtr preview(raw);
    EXPECT_EQ(infoOf(preview.get()).reason_code, CP_REASON_EMPTY_INPUT);
}

TEST(Arguments, RenderFileRejectsNullPath) {
    cp_preview* out = nullptr;
    EXPECT_EQ(cp_render_file(nullptr, nullptr, &out), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(out, nullptr);
}

TEST(Arguments, RenderFileRejectsNullOut) {
    EXPECT_EQ(cp_render_file("missing.bin", nullptr, nullptr), CP_ERROR_INVALID_ARGUMENT);
}

TEST(Arguments, RenderFileRejectsInvalidUtf8Path) {
    cp_preview* out = nullptr;
    EXPECT_EQ(cp_render_file("bad\xC3\x28name", nullptr, &out), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(cp_render_file("overlong\xC0\xAF", nullptr, &out), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(out, nullptr);
}

TEST(Arguments, SizeOutsideRangeIsRejected) {
    for (const std::uint32_t size : {1U, 15U, 4097U, 100000U}) {
        const cp_options options = makeOptions(size);
        cp_preview* out = nullptr;
        EXPECT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options, &out),
                  CP_ERROR_INVALID_ARGUMENT)
            << "size " << size;
    }
}

TEST(Arguments, SizeLimitsAreAccepted) {
    for (const std::uint32_t size : {16U, 4096U}) {
        const cp_options options = makeOptions(size);
        const PreviewPtr preview = renderBytes(SAMPLE, &options);
        ASSERT_NE(preview, nullptr) << "size " << size;
        EXPECT_EQ(infoOf(preview.get()).width, size);
    }
}

TEST(Arguments, TierAboveFiveIsRejected) {
    const cp_options options = makeOptions(0, 6);
    cp_preview* out = nullptr;
    EXPECT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options, &out),
              CP_ERROR_INVALID_ARGUMENT);
}

TEST(Arguments, NonZeroReservedIsRejected) {
    cp_options options = makeOptions();
    options.reserved = 1;
    cp_preview* out = nullptr;
    EXPECT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options, &out),
              CP_ERROR_INVALID_ARGUMENT);
}

TEST(Arguments, PreviewGettersRejectNulls) {
    const PreviewPtr preview = renderBytes(SAMPLE, nullptr);
    ASSERT_NE(preview, nullptr);
    const void* data = nullptr;
    std::size_t len = 0;
    EXPECT_EQ(cp_preview_png(nullptr, &data, &len), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(cp_preview_png(preview.get(), nullptr, &len), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(cp_preview_png(preview.get(), &data, nullptr), CP_ERROR_INVALID_ARGUMENT);
    struct cp_preview_info info{};
    info.struct_size = sizeof(info);
    EXPECT_EQ(cp_preview_info(nullptr, &info), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(cp_preview_info(preview.get(), nullptr), CP_ERROR_INVALID_ARGUMENT);
}

TEST(Arguments, DetectRejectsNulls) {
    cp_format_info info{};
    info.struct_size = sizeof(info);
    EXPECT_EQ(cp_detect_format(nullptr, &info), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(cp_detect_format("missing.bin", nullptr), CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(cp_detect_format("bad\xFF", &info), CP_ERROR_INVALID_ARGUMENT);
}

TEST(Arguments, FreeNullIsNoOp) {
    cp_preview_free(nullptr);
    SUCCEED();
}

} // namespace
} // namespace cadpreview::test
