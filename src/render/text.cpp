#include "render/text.hpp"

#include <algorithm>

namespace cadpreview {

namespace {

constexpr std::string_view ELLIPSIS = "...";

bool isContinuationByte(char c) noexcept {
    return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U;
}

void drawGlyph(Image& image, int x, int y, char c, int scale, Rgba color) {
    const auto rows = glyphRows(c);
    for (int row = 0; row < GLYPH_ROWS; ++row) {
        const auto bits = rows.at(static_cast<std::size_t>(row));
        for (int column = 0; column < GLYPH_COLUMNS; ++column) {
            const unsigned mask = 1U << static_cast<unsigned>(GLYPH_COLUMNS - 1 - column);
            if ((bits & mask) != 0U) {
                image.fillRect(x + static_cast<std::int64_t>(column) * scale,
                               y + static_cast<std::int64_t>(row) * scale, scale, scale, color);
            }
        }
    }
}

std::vector<std::string> splitWords(std::string_view text, std::size_t maxChars) {
    std::vector<std::string> words;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = std::min(text.find(' ', start), text.size());
        for (std::size_t piece = start; piece < end; piece += maxChars) {
            words.emplace_back(text.substr(piece, std::min(maxChars, end - piece)));
        }
        start = end + 1;
    }
    return words;
}

std::string withEllipsis(std::string line, std::size_t maxChars) {
    const std::size_t keep = maxChars > ELLIPSIS.size() ? maxChars - ELLIPSIS.size() : 0;
    if (line.size() > keep) {
        line.resize(keep);
    }
    line.append(ELLIPSIS.substr(0, std::min(ELLIPSIS.size(), maxChars)));
    return line;
}

} // namespace

std::string toDisplayAscii(std::string_view utf8) {
    std::string ascii;
    ascii.reserve(utf8.size());
    for (const char c : utf8) {
        if (isContinuationByte(c)) {
            continue;
        }
        const bool printable = c >= ' ' && c <= '~';
        ascii.push_back(printable ? c : '?');
    }
    return ascii;
}

int textWidth(std::size_t characters, int scale) noexcept {
    if (characters == 0) {
        return 0;
    }
    return (static_cast<int>(characters) * GLYPH_ADVANCE - (GLYPH_ADVANCE - GLYPH_COLUMNS)) * scale;
}

void drawText(Image& image, int x, int y, std::string_view ascii, int scale, Rgba color) {
    int cursor = x;
    for (const char c : ascii) {
        drawGlyph(image, cursor, y, c, scale, color);
        cursor += GLYPH_ADVANCE * scale;
    }
}

std::vector<std::string> wrapText(std::string_view text, std::size_t maxChars,
                                  std::size_t maxLines) {
    std::vector<std::string> lines;
    if (maxChars == 0 || maxLines == 0) {
        return lines;
    }
    for (const std::string& word : splitWords(text, maxChars)) {
        if (!lines.empty() && lines.back().size() + 1 + word.size() <= maxChars) {
            lines.back().append(" ").append(word);
            continue;
        }
        if (lines.size() == maxLines) {
            lines.back() = withEllipsis(lines.back(), maxChars);
            return lines;
        }
        lines.push_back(word);
    }
    return lines;
}

} // namespace cadpreview
