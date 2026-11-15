#pragma once

#include <cstdint>
#include <string_view>

namespace cadpreview {

enum class LogLevel : std::int32_t { Debug = 0, Info = 1, Warning = 2, Error = 3 };

/// Same signature as the public cp_log_fn.
using LogCallback = void (*)(void* user, std::int32_t level, const char* message);

/// Forwards messages to an optional callback. Copyable and cheap; holds no state of its own.
class Logger {
public:
    Logger() = default;
    Logger(LogCallback callback, void* user) noexcept : callback_(callback), user_(user) {}

    /// False when no callback is set, so callers can skip building messages.
    [[nodiscard]] bool enabled() const noexcept { return callback_ != nullptr; }

    void log(LogLevel level, std::string_view message) const noexcept;
    void debug(std::string_view message) const noexcept { log(LogLevel::Debug, message); }
    void info(std::string_view message) const noexcept { log(LogLevel::Info, message); }
    void warning(std::string_view message) const noexcept { log(LogLevel::Warning, message); }
    void error(std::string_view message) const noexcept { log(LogLevel::Error, message); }

private:
    LogCallback callback_ = nullptr;
    void* user_ = nullptr;
};

} // namespace cadpreview
