#include "common/log.hpp"
#include "temp_dir.hpp"
#include "tiers/pipeline.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace cadpreview {
namespace {

Preview renderBytes(const std::vector<std::byte>& bytes, const RenderOptions& options = {}) {
    return renderMemory(bytes, options, defaultRegistry(), Logger{});
}

TEST(Pipeline, EmptyInput) {
    const Preview preview = renderBytes({});
    EXPECT_EQ(preview.reason, Reason::EmptyInput);
    EXPECT_EQ(preview.width, DEFAULT_SIZE);
}

TEST(Pipeline, MissingFileCannotOpen) {
    const test::TempDir dir;
    const Preview preview =
        renderFile(dir.path() / "absent.dwg", RenderOptions{}, defaultRegistry(), Logger{});
    EXPECT_EQ(preview.reason, Reason::CannotOpen);
    EXPECT_EQ(preview.reasonText, "file not found");
}

// A source whose size is known but whose reads fail, like a file truncated after opening.
class FailingSource final : public Source {
public:
    [[nodiscard]] std::uint64_t size() const noexcept override { return 100; }
    [[nodiscard]] bool readAt(std::uint64_t /*offset*/, std::span<std::byte> /*out*/) override {
        return false;
    }
};

TEST(Pipeline, ReadFailureGivesCannotOpen) {
    FailingSource source;
    const Preview preview = renderSource(source, RenderOptions{}, defaultRegistry(), Logger{});
    EXPECT_EQ(preview.reason, Reason::CannotOpen);
    EXPECT_EQ(preview.reasonText, "read error");
}

TEST(Pipeline, UnknownBytes) {
    const Preview preview = renderBytes({std::byte{0x42}, std::byte{0x43}}, RenderOptions{32});
    EXPECT_EQ(preview.reason, Reason::UnknownFormat);
    EXPECT_EQ(preview.width, 32U);
    EXPECT_EQ(preview.tier, Tier::Placeholder);
}

TEST(Pipeline, RandomInputNeverThrows) {
    std::mt19937 rng(20261114);
    std::uniform_int_distribution<int> byteDist(0, 255);
    std::uniform_int_distribution<std::size_t> lengthDist(0, 600);
    const std::array<std::array<int, 8>, 2> prefixes{{
        {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1},
        {'P', 'K', 3, 4, 20, 0, 0, 0},
    }};
    for (int i = 0; i < 300; ++i) {
        std::vector<std::byte> bytes(lengthDist(rng));
        for (auto& b : bytes) {
            b = static_cast<std::byte>(byteDist(rng));
        }
        const auto& prefix = prefixes.at(static_cast<std::size_t>(i) % prefixes.size());
        for (std::size_t j = 0; j < bytes.size() && j < prefix.size() && i % 3 != 0; ++j) {
            bytes[j] = static_cast<std::byte>(prefix.at(j));
        }
        const RenderOptions options{16U + static_cast<std::uint32_t>(i % 4) * 16U,
                                    static_cast<Tier>(i % 6)};
        EXPECT_NO_THROW({
            const Preview preview = renderBytes(bytes, options);
            EXPECT_FALSE(preview.png.empty());
        });
    }
}

} // namespace
} // namespace cadpreview
