#include "common/errors.hpp"
#include "common/log.hpp"
#include "containers/source.hpp"
#include "formats/detect.hpp"
#include "render/image.hpp"
#include "tiers/ladder.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace cadpreview {
namespace {

// Formats with tiers do not exist yet, so the tests use a value outside the enumeration.
constexpr auto FAKE_FORMAT = static_cast<Format>(900);

TierFunction succeedWith(Rgba color, std::vector<Tier>* calls, Tier tier) {
    return [=](const TierContext& context) {
        calls->push_back(tier);
        return TierOutcome{Image(context.size, context.size, color), {}, {"note from tier"}};
    };
}

TierFunction failWith(const std::string& message, std::vector<Tier>* calls, Tier tier) {
    return [=](const TierContext& /*context*/) {
        calls->push_back(tier);
        return TierOutcome{std::nullopt, message, {}};
    };
}

TierFunction throwing(std::vector<Tier>* calls, Tier tier) {
    return [=](const TierContext& /*context*/) -> TierOutcome {
        calls->push_back(tier);
        throw FormatError("stream truncated");
    };
}

class LadderTest : public testing::Test {
protected:
    Preview run(const TierRegistry& registry, Tier forced = Tier::Auto,
                Format format = FAKE_FORMAT) {
        Detection detection;
        detection.format = format;
        detection.detail = "unrecognized file format";
        MemorySource source(bytes_);
        return runLadder(source, detection, RenderOptions{64, forced}, registry, Logger{});
    }

    std::vector<Tier> calls_;

private:
    std::vector<std::byte> bytes_{std::byte{1}, std::byte{2}};
};

TEST_F(LadderTest, FirstSuccessfulTierWins) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Brep, succeedWith({1, 2, 3, 255}, &calls_, Tier::Brep)},
        {FAKE_FORMAT, Tier::Thumbnail, failWith("no preview stream", &calls_, Tier::Thumbnail)},
        {FAKE_FORMAT, Tier::Mesh, succeedWith({4, 5, 6, 255}, &calls_, Tier::Mesh)},
    });
    const Preview preview = run(registry);
    EXPECT_TRUE(preview.ok);
    EXPECT_EQ(preview.tier, Tier::Mesh);
    EXPECT_EQ(preview.reason, Reason::None);
    EXPECT_EQ(preview.width, 64U);
    EXPECT_EQ(preview.warnings, "note from tier");
    EXPECT_EQ(calls_, (std::vector<Tier>{Tier::Thumbnail, Tier::Mesh}));
    EXPECT_FALSE(preview.png.empty());
}

TEST_F(LadderTest, ExceptionsFallThroughToTheNextTier) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Thumbnail, throwing(&calls_, Tier::Thumbnail)},
        {FAKE_FORMAT, Tier::Mesh, succeedWith({4, 5, 6, 255}, &calls_, Tier::Mesh)},
    });
    const Preview preview = run(registry);
    EXPECT_TRUE(preview.ok);
    EXPECT_EQ(preview.tier, Tier::Mesh);
}

TEST_F(LadderTest, AllFailuresAreReported) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Thumbnail, failWith("no preview stream", &calls_, Tier::Thumbnail)},
        {FAKE_FORMAT, Tier::Brep, throwing(&calls_, Tier::Brep)},
    });
    const Preview preview = run(registry);
    EXPECT_FALSE(preview.ok);
    EXPECT_EQ(preview.tier, Tier::Placeholder);
    EXPECT_EQ(preview.reason, Reason::TiersFailed);
    EXPECT_NE(preview.reasonText.find("tier 1: no preview stream"), std::string::npos);
    EXPECT_NE(preview.reasonText.find("tier 3: stream truncated"), std::string::npos);
}

TEST_F(LadderTest, OtherExceptionsNeverReachThePlaceholderText) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Thumbnail,
         [](const TierContext& /*context*/) -> TierOutcome {
             throw std::runtime_error("cannot open C:/secret/project.prt");
         }},
    });
    const Preview preview = run(registry);
    EXPECT_EQ(preview.reason, Reason::TiersFailed);
    EXPECT_EQ(preview.reasonText, "tier 1: internal error");
}

TEST_F(LadderTest, AllocationFailureInATierFallsThrough) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Thumbnail,
         [](const TierContext& /*context*/) -> TierOutcome { throw std::bad_alloc(); }},
    });
    const Preview preview = run(registry);
    EXPECT_EQ(preview.reason, Reason::TiersFailed);
    EXPECT_NE(preview.reasonText.find("out of memory"), std::string::npos);
}

TEST_F(LadderTest, ForcedTierRunsOnlyThatTier) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Thumbnail, succeedWith({1, 2, 3, 255}, &calls_, Tier::Thumbnail)},
        {FAKE_FORMAT, Tier::Mesh, succeedWith({4, 5, 6, 255}, &calls_, Tier::Mesh)},
    });
    const Preview preview = run(registry, Tier::Mesh);
    EXPECT_EQ(preview.tier, Tier::Mesh);
    EXPECT_EQ(calls_, (std::vector<Tier>{Tier::Mesh}));
}

TEST_F(LadderTest, ForcedMissingTierIsUnavailable) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Thumbnail, succeedWith({1, 2, 3, 255}, &calls_, Tier::Thumbnail)},
    });
    const Preview preview = run(registry, Tier::Vector);
    EXPECT_EQ(preview.reason, Reason::TierUnavailable);
    EXPECT_NE(preview.reasonText.find("tier 4"), std::string::npos);
    EXPECT_TRUE(calls_.empty());
}

TEST_F(LadderTest, ForcedPlaceholderSkipsAllTiers) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Thumbnail, succeedWith({1, 2, 3, 255}, &calls_, Tier::Thumbnail)},
    });
    const Preview preview = run(registry, Tier::Placeholder);
    EXPECT_EQ(preview.reason, Reason::PlaceholderRequested);
    EXPECT_TRUE(calls_.empty());
}

TEST_F(LadderTest, UnknownFormatUsesDetectionDetail) {
    const Preview preview = run(TierRegistry{}, Tier::Auto, Format::Unknown);
    EXPECT_EQ(preview.reason, Reason::UnknownFormat);
    EXPECT_EQ(preview.reasonText, "unrecognized file format");
    EXPECT_EQ(preview.formatName, "unknown");
}

TEST_F(LadderTest, KnownFormatWithoutTiersIsUnavailable) {
    const Preview preview = run(TierRegistry{});
    EXPECT_EQ(preview.reason, Reason::TierUnavailable);
}

TEST_F(LadderTest, UnavailableTiersWarnInAutoAndExplainWhenForced) {
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Thumbnail, failWith("no preview stream", &calls_, Tier::Thumbnail)},
        {FAKE_FORMAT, Tier::Vector, failWith("unused", &calls_, Tier::Vector), "needs option X"},
    });
    const Preview automatic = run(registry);
    EXPECT_EQ(automatic.reason, Reason::TiersFailed);
    EXPECT_EQ(automatic.warnings, "needs option X");
    EXPECT_EQ(calls_, (std::vector<Tier>{Tier::Thumbnail}));
    const Preview forced = run(registry, Tier::Vector);
    EXPECT_EQ(forced.reason, Reason::TierUnavailable);
    EXPECT_EQ(forced.reasonText, "needs option X");
}

TEST(TierRegistry, OrdersTiersAscending) {
    const auto noop = [](const TierContext& /*context*/) {
        return TierOutcome{};
    };
    const TierRegistry registry({
        {FAKE_FORMAT, Tier::Brep, noop},
        {FAKE_FORMAT, Tier::Thumbnail, noop},
        {Format::Unknown, Tier::Mesh, noop},
    });
    const auto tiers = registry.tiersFor(FAKE_FORMAT);
    ASSERT_EQ(tiers.size(), 2U);
    EXPECT_EQ(tiers[0]->tier, Tier::Thumbnail);
    EXPECT_EQ(tiers[1]->tier, Tier::Brep);
    EXPECT_NE(registry.find(FAKE_FORMAT, Tier::Brep), nullptr);
    EXPECT_EQ(registry.find(FAKE_FORMAT, Tier::Mesh), nullptr);
    EXPECT_TRUE(defaultRegistry().tiersFor(Format::Unknown).empty());
}

TEST(Names, ReasonAndTierIds) {
    EXPECT_EQ(reasonId(Reason::CannotOpen), "cannot_open");
    EXPECT_EQ(reasonId(static_cast<Reason>(77)), "unrecognized");
    EXPECT_EQ(tierName(Tier::Brep), "brep");
    EXPECT_EQ(tierName(Tier::Placeholder), "placeholder");
}

} // namespace
} // namespace cadpreview
