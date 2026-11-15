#pragma once

#include <cstdint>
#include <string_view>

namespace cadpreview {

/// Detected file format. Values match the public CP_FORMAT_* constants.
enum class Format : std::int32_t { Unknown = 0 };

/// Stable short id used in the API and logs, e.g. "unknown".
[[nodiscard]] std::string_view formatId(Format format) noexcept;

/// Human-readable name shown on placeholders.
[[nodiscard]] std::string_view formatDisplayName(Format format) noexcept;

} // namespace cadpreview
