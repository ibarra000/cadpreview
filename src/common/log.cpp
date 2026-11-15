#include "common/log.hpp"

#include <string>

namespace cadpreview {

void Logger::log(LogLevel level, std::string_view message) const noexcept {
    if (callback_ == nullptr) {
        return;
    }
    try {
        const std::string terminated(message);
        callback_(user_, static_cast<std::int32_t>(level), terminated.c_str());
    } catch (...) { // NOLINT(bugprone-empty-catch): no channel is left to report a logging failure
    }
}

} // namespace cadpreview
