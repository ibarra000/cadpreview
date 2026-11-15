#include "tiers/placeholder.hpp"

#include "render/text.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace cadpreview {

namespace {

constexpr Rgba BACKGROUND{255, 255, 255, 255};
constexpr Rgba FRAME{218, 220, 224, 255};
constexpr Rgba TITLE{95, 99, 104, 255};
constexpr Rgba DETAIL{128, 134, 139, 255};

constexpr std::size_t MAX_TITLE_LINES = 2;
constexpr std::size_t MAX_DETAIL_LINES = 6;
constexpr std::size_t MIN_CHARS_FOR_TEXT = 6;

struct Layout {
    int margin = 0;  ///< 5% of the edge, where the frame sits.
    int frame = 0;   ///< Frame line thickness.
    int padding = 0; ///< Space between the frame and the text.
    int titleScale = 1;
    int detailScale = 1;
};

Layout layoutFor(int size) {
    Layout layout;
    layout.margin = std::max(1, (size * 5 + 50) / 100);
    layout.frame = std::max(1, size / 256);
    layout.padding = std::max(1, size / 32);
    layout.titleScale = std::max(1, size / 128);
    layout.detailScale = std::max(1, size / 256);
    return layout;
}

void drawFrame(Image& image, int size, const Layout& layout) {
    const int inner = size - 2 * layout.margin;
    if (inner <= 0) {
        return;
    }
    image.fillRect(layout.margin, layout.margin, inner, layout.frame, FRAME);
    image.fillRect(layout.margin, size - layout.margin - layout.frame, inner, layout.frame, FRAME);
    image.fillRect(layout.margin, layout.margin, layout.frame, inner, FRAME);
    image.fillRect(size - layout.margin - layout.frame, layout.margin, layout.frame, inner, FRAME);
}

struct TextBlock {
    std::vector<std::string> lines;
    int scale = 1;
    Rgba color;

    [[nodiscard]] int height() const {
        return static_cast<int>(lines.size()) * LINE_ADVANCE * scale;
    }
};

std::size_t charsPerLine(int width, int scale) {
    return width > 0 ? static_cast<std::size_t>(width / (GLYPH_ADVANCE * scale)) : 0;
}

void drawBlock(Image& image, int size, int top, const TextBlock& block) {
    int y = top;
    for (const std::string& line : block.lines) {
        const int x = (size - textWidth(line.size(), block.scale)) / 2;
        drawText(image, x, y, line, block.scale, block.color);
        y += LINE_ADVANCE * block.scale;
    }
}

} // namespace

Image drawPlaceholder(std::string_view title, std::string_view detail, std::uint32_t size) {
    Image image(size, size, BACKGROUND);
    const int edge = static_cast<int>(size);
    const Layout layout = layoutFor(edge);
    drawFrame(image, edge, layout);

    const int textArea = edge - 2 * (layout.margin + layout.frame + layout.padding);
    const std::size_t titleChars = charsPerLine(textArea, layout.titleScale);
    if (titleChars < MIN_CHARS_FOR_TEXT) {
        return image;
    }
    const TextBlock titleBlock{wrapText(toDisplayAscii(title), titleChars, MAX_TITLE_LINES),
                               layout.titleScale, TITLE};
    const int gap = LINE_ADVANCE * layout.detailScale;
    const int detailRoom = textArea - titleBlock.height() - gap;
    const auto detailLines =
        static_cast<std::size_t>(std::max(0, detailRoom / (LINE_ADVANCE * layout.detailScale)));
    const TextBlock detailBlock{wrapText(toDisplayAscii(detail),
                                         charsPerLine(textArea, layout.detailScale),
                                         std::min(detailLines, MAX_DETAIL_LINES)),
                                layout.detailScale, DETAIL};

    const int total =
        titleBlock.height() + (detailBlock.lines.empty() ? 0 : gap + detailBlock.height());
    const int top = (edge - total) / 2;
    drawBlock(image, edge, top, titleBlock);
    drawBlock(image, edge, top + titleBlock.height() + gap, detailBlock);
    return image;
}

} // namespace cadpreview
