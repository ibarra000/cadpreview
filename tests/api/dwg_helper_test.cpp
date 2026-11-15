// DWG tier 4 runs LibreDWG in the separate cadpreview-dwg helper. These tests use the stand-in
// from tests/support/fake_helper.cpp, so they run in every build.

#include "api_test_util.hpp"
#include "temp_dir.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cadpreview::test {
namespace {

std::filesystem::path fakeHelper() {
    return CADPREVIEW_FAKE_HELPER;
}

// A file detected as DWG (AC1024) with no embedded preview, so only tier 4 can draw it. The
// fake helper reads `marker` to decide how to behave.
std::vector<std::uint8_t> dwgLike(std::string_view marker = "") {
    std::vector<std::uint8_t> bytes{'A', 'C', '1', '0', '2', '4'};
    bytes.resize(128, 0);
    bytes.insert(bytes.end(), marker.begin(), marker.end());
    return bytes;
}

struct Rendered {
    PreviewPtr preview;
    struct cp_preview_info info{};
};

Rendered render(const TempDir& dir, std::string_view marker, std::uint32_t tier,
                const std::string& helper) {
    const auto path = dir.write("drawing.dwg", dwgLike(marker));
    cp_options options = makeOptions(64, tier);
    options.dwg_helper_path = helper.c_str();
    Rendered result{renderPath(utf8(path), &options), {}};
    if (result.preview) {
        result.info = infoOf(result.preview.get());
    }
    return result;
}

TEST(DwgHelper, ForcedVectorTierDrawsWhatTheHelperReturns) {
    const TempDir dir;
    const Rendered r = render(dir, "", CP_TIER_VECTOR, utf8(fakeHelper()));
    ASSERT_TRUE(r.preview);
    EXPECT_EQ(r.info.ok, 1) << reasonOf(r.preview.get());
    EXPECT_EQ(r.info.tier, static_cast<std::uint32_t>(CP_TIER_VECTOR));
    EXPECT_EQ(r.info.format, CP_FORMAT_DWG);
}

TEST(DwgHelper, AutoFallsBackToTheHelperWhenThereIsNoEmbeddedPreview) {
    const TempDir dir;
    const Rendered r = render(dir, "", CP_TIER_AUTO, utf8(fakeHelper()));
    ASSERT_TRUE(r.preview);
    EXPECT_EQ(r.info.ok, 1) << reasonOf(r.preview.get());
    EXPECT_EQ(r.info.tier, static_cast<std::uint32_t>(CP_TIER_VECTOR));
}

TEST(DwgHelper, AMissingHelperMakesTheTierUnavailable) {
    const TempDir dir;
    const std::string missing = utf8(dir.path() / "no-such-helper");
    const Rendered forced = render(dir, "", CP_TIER_VECTOR, missing);
    ASSERT_TRUE(forced.preview);
    EXPECT_EQ(forced.info.reason_code, CP_REASON_TIER_UNAVAILABLE);
    EXPECT_NE(reasonOf(forced.preview.get()).find("cadpreview-dwg"), std::string::npos);

    const Rendered automatic = render(dir, "", CP_TIER_AUTO, missing);
    ASSERT_TRUE(automatic.preview);
    EXPECT_EQ(automatic.info.reason_code, CP_REASON_TIERS_FAILED);
    EXPECT_NE(std::string(automatic.info.warnings).find("cadpreview-dwg"), std::string::npos);
}

TEST(DwgHelper, HelperFailuresBecomeTierFailuresWithoutThePath) {
    const TempDir dir;
    for (const std::string_view marker : {"MODE=fail", "MODE=crash"}) {
        const Rendered r = render(dir, marker, CP_TIER_VECTOR, utf8(fakeHelper()));
        ASSERT_TRUE(r.preview);
        EXPECT_EQ(r.info.reason_code, CP_REASON_TIERS_FAILED) << marker;
        const std::string reason = reasonOf(r.preview.get());
        EXPECT_NE(reason.find("helper"), std::string::npos) << reason;
        EXPECT_EQ(reason.find(utf8(dir.path())), std::string::npos) << reason;
    }
}

TEST(DwgHelper, InMemoryInputCannotUseTheHelper) {
    const auto bytes = dwgLike();
    const std::string helper = utf8(fakeHelper());
    cp_options options = makeOptions(64, CP_TIER_VECTOR);
    options.dwg_helper_path = helper.c_str();
    const PreviewPtr preview = renderBytes(bytes, &options);
    ASSERT_TRUE(preview);
    EXPECT_EQ(infoOf(preview.get()).reason_code, CP_REASON_TIER_UNAVAILABLE);
}

TEST(DwgHelper, HelperPathMustBeUtf8) {
    const auto bytes = dwgLike();
    cp_options options = makeOptions(64);
    options.dwg_helper_path = "\xFF\xFE";
    cp_preview* out = nullptr;
    EXPECT_EQ(cp_render_memory(bytes.data(), bytes.size(), &options, &out),
              CP_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(out, nullptr);
}

} // namespace
} // namespace cadpreview::test
