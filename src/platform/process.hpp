#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cadpreview {

/// Bounds on a child process started by runProcess.
struct ProcessLimits {
    std::chrono::milliseconds timeout{30'000};
    std::size_t maxOutput = std::size_t{256} * 1024 * 1024; ///< standard output, in bytes
    std::size_t maxErrorOutput = 4096; ///< standard error beyond this is read and dropped
};

enum class ProcessEnd : std::uint8_t {
    Exited,         ///< exited normally; exitCode holds its status
    Crashed,        ///< killed by a signal (POSIX) or an exception status (Windows)
    TimedOut,       ///< killed by runProcess after limits.timeout
    OutputTooLarge, ///< killed by runProcess after writing more than limits.maxOutput
};

struct ProcessResult {
    ProcessEnd end = ProcessEnd::Exited;
    int exitCode = 0;
    std::string output;      ///< standard output, at most limits.maxOutput bytes
    std::string errorOutput; ///< standard error, at most limits.maxErrorOutput bytes
};

/// Runs `program` (a full path; no search path is used) with UTF-8 `arguments`, standard input
/// from the null device, and both output streams captured. Only the pipes are inherited, so
/// concurrent calls from several threads do not leak handles into each other's children. Waits
/// for the process to end, killing it when a limit is exceeded. Throws std::system_error if
/// the program cannot be started.
[[nodiscard]] ProcessResult runProcess(const std::filesystem::path& program,
                                       const std::vector<std::string>& arguments,
                                       const ProcessLimits& limits);

} // namespace cadpreview
