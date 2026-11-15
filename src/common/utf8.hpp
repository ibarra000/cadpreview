#pragma once

#include <cstddef>
#include <filesystem>
#include <string_view>

namespace cadpreview {

/// True when text is well-formed UTF-8 (no overlongs, surrogates, or code points past U+10FFFF).
[[nodiscard]] bool isValidUtf8(std::string_view text) noexcept;

/// Longest prefix of valid UTF-8 text that fits in maxBytes without splitting a code point.
[[nodiscard]] std::size_t utf8PrefixLength(std::string_view text, std::size_t maxBytes) noexcept;

/// Converts UTF-8 text (already validated) to a native path.
[[nodiscard]] std::filesystem::path pathFromUtf8(std::string_view text);

} // namespace cadpreview
