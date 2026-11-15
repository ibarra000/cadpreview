#include "api_test_util.hpp"
#include "temp_dir.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace cadpreview::test {
namespace {

constexpr int THREADS = 8;
constexpr int ITERATIONS = 100;

void countLog(void* user, cp_log_level /*level*/, const char* /*message*/) {
    static_cast<std::atomic<int>*>(user)->fetch_add(1, std::memory_order_relaxed);
}

TEST(Threads, ConcurrentRendersAreIndependent) {
    const TempDir dir;
    const std::array<std::uint8_t, 6> bytes{'n', 'o', 't', 'c', 'a', 'd'};
    const std::string file = utf8(dir.write("shared.bin", bytes));
    std::atomic<int> failures{0};
    std::atomic<int> logs{0};

    std::vector<std::jthread> workers;
    workers.reserve(THREADS);
    for (int t = 0; t < THREADS; ++t) {
        workers.emplace_back([&, t] {
            cp_options options = makeOptions(16U + static_cast<std::uint32_t>(t) * 8U);
            options.log = &countLog;
            options.log_user = &logs;
            for (int i = 0; i < ITERATIONS; ++i) {
                const PreviewPtr preview =
                    (i % 2 == 0) ? renderBytes(bytes, &options) : renderPath(file, &options);
                if (!preview || infoOf(preview.get()).width != options.size ||
                    pngOf(preview.get()).empty()) {
                    failures.fetch_add(1);
                }
            }
        });
    }
    workers.clear();

    EXPECT_EQ(failures.load(), 0);
    EXPECT_GT(logs.load(), THREADS * ITERATIONS - 1);
}

TEST(Threads, ConcurrentReadsOfOneSharedPreview) {
    const std::array<std::uint8_t, 3> bytes{1, 2, 3};
    const PreviewPtr preview = renderBytes(bytes, nullptr);
    ASSERT_NE(preview, nullptr);
    const auto expected = pngOf(preview.get());
    std::atomic<int> mismatches{0};
    {
        std::vector<std::jthread> readers;
        readers.reserve(THREADS);
        for (int t = 0; t < THREADS; ++t) {
            readers.emplace_back([&] {
                for (int i = 0; i < ITERATIONS; ++i) {
                    const auto png = pngOf(preview.get());
                    const auto info = infoOf(preview.get());
                    if (png.data() != expected.data() || info.width != 512) {
                        mismatches.fetch_add(1);
                    }
                }
            });
        }
    }
    EXPECT_EQ(mismatches.load(), 0);
}

} // namespace
} // namespace cadpreview::test
