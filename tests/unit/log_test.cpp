#include "common/log.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cadpreview {
namespace {

using Captured = std::vector<std::pair<std::int32_t, std::string>>;

void capture(void* user, std::int32_t level, const char* message) {
    static_cast<Captured*>(user)->emplace_back(level, message);
}

TEST(Logger, DefaultLoggerIsSilent) {
    const Logger log;
    log.info("nobody listens");
    SUCCEED();
}

TEST(Logger, ForwardsLevelsAndMessages) {
    Captured captured;
    const Logger log(&capture, &captured);
    log.debug("d");
    log.info("i");
    log.warning("w");
    log.error(std::string_view("error text with trailing junk").substr(0, 10));
    ASSERT_EQ(captured.size(), 4U);
    EXPECT_EQ(captured[0], (std::pair<std::int32_t, std::string>{0, "d"}));
    EXPECT_EQ(captured[1].first, 1);
    EXPECT_EQ(captured[2].first, 2);
    EXPECT_EQ(captured[3], (std::pair<std::int32_t, std::string>{3, "error text"}));
}

void throwingCallback(void* /*user*/, std::int32_t /*level*/, const char* /*message*/) {
    throw std::runtime_error("callbacks must not throw, but the library survives if one does");
}

TEST(Logger, SurvivesAThrowingCallback) {
    const Logger log(&throwingCallback, nullptr);
    EXPECT_TRUE(log.enabled());
    EXPECT_NO_THROW(log.warning("message"));
    EXPECT_FALSE(Logger{}.enabled());
}

} // namespace
} // namespace cadpreview
