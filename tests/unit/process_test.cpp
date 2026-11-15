#include "platform/process.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace cadpreview {
namespace {

using std::chrono::milliseconds;

std::filesystem::path fakeHelper() {
    return CADPREVIEW_FAKE_HELPER;
}

ProcessLimits quick() {
    ProcessLimits limits;
    limits.timeout = milliseconds(10'000);
    limits.maxOutput = std::size_t{1024} * 1024;
    return limits;
}

TEST(Process, ArgumentsArriveUnchanged) {
    // Spaces, quotes, trailing backslashes and empty strings are where Windows command-line
    // quoting usually goes wrong.
    const std::vector<std::string> arguments{
        "plain", "with space", R"(quote"inside)",        "trailing\\",    R"(back\\"slash)",
        "",      "tab\there",  R"(C:\Program Files\x\)", "\xC3\x98 utf-8"};
    std::vector<std::string> command{"--echo"};
    command.insert(command.end(), arguments.begin(), arguments.end());
    const ProcessResult result = runProcess(fakeHelper(), command, quick());
    ASSERT_EQ(result.end, ProcessEnd::Exited);
    EXPECT_EQ(result.exitCode, 0);
    std::string expected;
    for (const std::string& argument : arguments) {
        expected += argument + "\n";
    }
    EXPECT_EQ(result.output, expected);
}

TEST(Process, ReportsExitStatusAndKeepsErrorOutput) {
    const ProcessResult result = runProcess(fakeHelper(), {"--fail"}, quick());
    EXPECT_EQ(result.end, ProcessEnd::Exited);
    EXPECT_EQ(result.exitCode, 3);
    EXPECT_NE(result.errorOutput.find("cannot decode"), std::string::npos);
}

TEST(Process, KillsAProcessThatRunsTooLong) {
    ProcessLimits limits = quick();
    limits.timeout = milliseconds(300);
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result = runProcess(fakeHelper(), {"--sleep"}, limits);
    EXPECT_EQ(result.end, ProcessEnd::TimedOut);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(20));
}

TEST(Process, StopsAProcessThatWritesTooMuch) {
    ProcessLimits limits = quick();
    limits.maxOutput = 100'000;
    const ProcessResult result = runProcess(fakeHelper(), {"--flood"}, limits);
    EXPECT_EQ(result.end, ProcessEnd::OutputTooLarge);
    EXPECT_LE(result.output.size(), limits.maxOutput);
}

TEST(Process, AGrandchildHoldingTheOutputCannotHangTheCaller) {
    // The child starts a sleeping grandchild that shares its output pipe, then exits. On
    // Windows the job allows one process, so the grandchild is refused; on POSIX the deadline
    // ends the wait and the grandchild dies with the child's process group.
    ProcessLimits limits = quick();
    limits.timeout = milliseconds(1500);
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result =
        runProcess(fakeHelper(), {"--spawn", fakeHelper().string()}, limits);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(15));
#ifdef _WIN32
    EXPECT_EQ(result.end, ProcessEnd::Exited);
    EXPECT_NE(result.output.find("blocked"), std::string::npos) << result.output;
#else
    EXPECT_EQ(result.end, ProcessEnd::TimedOut);
#endif
}

TEST(Process, ReportsAbnormalTermination) {
    const ProcessResult result = runProcess(fakeHelper(), {"--crash"}, quick());
    EXPECT_EQ(result.end, ProcessEnd::Crashed);
}

TEST(Process, ThrowsWhenTheProgramCannotStart) {
    EXPECT_THROW(
        static_cast<void>(runProcess(fakeHelper().parent_path() / "no-such-program", {}, quick())),
        std::system_error);
}

} // namespace
} // namespace cadpreview
