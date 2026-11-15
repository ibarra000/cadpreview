#pragma once

#include "render/image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cadpreview {

inline constexpr int GLYPH_COLUMNS = 5;
inline constexpr int GLYPH_ROWS = 7;
inline constexpr int GLYPH_ADVANCE = 6; ///< Columns per character, including spacing.
inline constexpr int LINE_ADVANCE = 9;  ///< Rows per text line, including spacing.

/// Seven rows of the built-in 5x7 font; bit 4 is the leftmost column. Characters outside
/// printable ASCII use the '?' glyph.
[[nodiscard]] std::array<std::uint8_t, GLYPH_ROWS> glyphRows(char c);

/// Maps UTF-8 text to printable ASCII, one '?' per other code point or control character.
[[nodiscard]] std::string toDisplayAscii(std::string_view utf8);

/// Width in pixels of `characters` glyphs at `scale`, without trailing spacing.
[[nodiscard]] int textWidth(std::size_t characters, int scale) noexcept;

/// Draws ASCII text with its top-left corner at (x, y); pixels outside the image are clipped.
void drawText(Image& image, int x, int y, std::string_view ascii, int scale, Rgba color);

/// Greedy word wrap into at most maxLines lines of maxChars characters. Words longer than a line
/// are split; text that does not fit ends with "...".
[[nodiscard]] std::vector<std::string> wrapText(std::string_view text, std::size_t maxChars,
                                                std::size_t maxLines);

} // namespace cadpreview
