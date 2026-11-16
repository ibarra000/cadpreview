#include "api_test_util.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace cadpreview::test {
namespace {

const std::array<std::uint8_t, 3> SAMPLE{1, 2, 3};

// Version 1 of cp_options ends before dwg_helper_path, which version 2 (0.2.0) appended.
constexpr std::uint32_t OPTIONS_V1_SIZE = offsetof(cp_options, dwg_helper_path);

TEST(Structs, V1SizesAreFrozen) {
    if constexpr (sizeof(void*) == 8) {
        EXPECT_EQ(OPTIONS_V1_SIZE, 32U);
        EXPECT_EQ(sizeof(cp_options), 40U);
        EXPECT_EQ(sizeof(struct cp_preview_info), 72U);
    }
    EXPECT_EQ(sizeof(cp_format_info), 104U);
}

TEST(Structs, OptionsSmallerThanV1AreRejected) {
    cp_options options = makeOptions();
    options.struct_size = OPTIONS_V1_SIZE - 1;
    cp_preview* out = nullptr;
    EXPECT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options, &out),
              CP_ERROR_INVALID_ARGUMENT);
}

TEST(Structs, V1OptionsWithoutTheHelperPathAreAccepted) {
    cp_options options = makeOptions(64);
    options.struct_size = OPTIONS_V1_SIZE;
    options.dwg_helper_path = "ignored: beyond the caller's declared size";
    cp_preview* raw = nullptr;
    ASSERT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options, &raw), CP_OK);
    const PreviewPtr preview(raw);
    EXPECT_EQ(infoOf(preview.get()).width, 64U);
}

TEST(Structs, AFieldOnlyPartlyInsideStructSizeIsIgnored) {
    // A size that ends inside dwg_helper_path would copy half of the pointer, giving a wild
    // pointer. Fields not wholly inside struct_size must count as absent.
    static constexpr std::string_view HELPER = "no-such-cadpreview-dwg";
    cp_options options = makeOptions(64);
    options.dwg_helper_path = HELPER.data(); // a literal, so NUL-terminated
    for (std::uint32_t size = OPTIONS_V1_SIZE + 1; size < sizeof(cp_options); ++size) {
        options.struct_size = size;
        cp_preview* raw = nullptr;
        ASSERT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options, &raw), CP_OK) << size;
        const PreviewPtr preview(raw);
        EXPECT_EQ(infoOf(preview.get()).width, 64U);
    }
}

struct NewerOptions {
    cp_options v1;
    std::uint64_t futureField;
};

TEST(Structs, LargerOptionsWithZeroTailAreAccepted) {
    NewerOptions options{};
    options.v1 = makeOptions(64);
    options.v1.struct_size = sizeof(NewerOptions);
    cp_preview* raw = nullptr;
    ASSERT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options.v1, &raw), CP_OK);
    const PreviewPtr preview(raw);
    EXPECT_EQ(infoOf(preview.get()).width, 64U);
}

TEST(Structs, LargerOptionsWithUnknownFieldSetAreRejected) {
    NewerOptions options{};
    options.v1 = makeOptions(64);
    options.v1.struct_size = sizeof(NewerOptions);
    options.futureField = 1;
    cp_preview* out = nullptr;
    EXPECT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options.v1, &out),
              CP_ERROR_INVALID_ARGUMENT);
}

TEST(Structs, AbsurdStructSizeIsRejected) {
    cp_options options = makeOptions();
    options.struct_size = 1U << 20U;
    cp_preview* out = nullptr;
    EXPECT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options, &out),
              CP_ERROR_INVALID_ARGUMENT);
}

struct NewerInfo {
    struct cp_preview_info v1;
    std::uint64_t futureField;
};

TEST(Structs, InfoWriteBackReportsFilledBytesAndZeroesTail) {
    const PreviewPtr preview = renderBytes(SAMPLE, nullptr);
    ASSERT_NE(preview, nullptr);
    NewerInfo info{};
    std::memset(&info, 0xAB, sizeof(info));
    info.v1.struct_size = sizeof(NewerInfo);
    ASSERT_EQ(cp_preview_info(preview.get(), &info.v1), CP_OK);
    EXPECT_EQ(info.v1.struct_size, sizeof(struct cp_preview_info));
    EXPECT_EQ(info.futureField, 0U);
    EXPECT_EQ(info.v1.reserved, 0U);
}

TEST(Structs, InfoSmallerThanV1IsRejected) {
    const PreviewPtr preview = renderBytes(SAMPLE, nullptr);
    ASSERT_NE(preview, nullptr);
    struct cp_preview_info info{};
    info.struct_size = 8;
    EXPECT_EQ(cp_preview_info(preview.get(), &info), CP_ERROR_INVALID_ARGUMENT);
}

TEST(Structs, StructSizeLimitIs4096) {
    struct Big {
        cp_options v1;
        std::array<std::uint8_t, 4096 - sizeof(cp_options) + 1> tail;
    };
    Big options{};
    options.v1 = makeOptions(32);
    options.v1.struct_size = 4096;
    cp_preview* raw = nullptr;
    ASSERT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options.v1, &raw), CP_OK);
    cp_preview_free(raw);
    options.v1.struct_size = 4097;
    EXPECT_EQ(cp_render_memory(SAMPLE.data(), SAMPLE.size(), &options.v1, &raw),
              CP_ERROR_INVALID_ARGUMENT);
}

TEST(Structs, FormatInfoWriteBackReportsFilledBytesAndZeroesTail) {
    struct NewerFormatInfo {
        cp_format_info v1;
        std::uint64_t futureField;
    };
    NewerFormatInfo info{};
    std::memset(&info, 0xCD, sizeof(info));
    info.v1.struct_size = sizeof(NewerFormatInfo);
    ASSERT_EQ(cp_detect_format("missing.bin", &info.v1), CP_OK);
    EXPECT_EQ(info.v1.struct_size, sizeof(cp_format_info));
    EXPECT_EQ(info.futureField, 0U);
    EXPECT_STREQ(static_cast<const char*>(info.v1.name), "unknown");
}

TEST(Structs, FormatInfoSmallerThanV1IsRejected) {
    cp_format_info info{};
    info.struct_size = 4;
    EXPECT_EQ(cp_detect_format("missing.bin", &info), CP_ERROR_INVALID_ARGUMENT);
}

} // namespace
} // namespace cadpreview::test
