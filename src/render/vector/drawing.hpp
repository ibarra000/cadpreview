#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cadpreview {

/// A 2D point in drawing (world) units; y points up, as in CAD.
struct Point {
    double x = 0;
    double y = 0;
};

struct Polyline {
    std::vector<Point> points;
    bool closed = false;
};

/// Counter-clockwise arc from startAngle to endAngle, in radians. A full circle spans 2*pi.
struct Arc {
    Point center;
    double radius = 0;
    double startAngle = 0;
    double endAngle = 0;
};

enum class HAlign : std::uint8_t { Left, Center, Right };
enum class VAlign : std::uint8_t { Baseline, Bottom, Middle, Top };

/// Single-line text. height is the cap height in drawing units; rotation is in radians.
struct TextItem {
    std::string text; ///< UTF-8
    Point position;
    double height = 0;
    double rotation = 0;
    double widthFactor = 1;
    HAlign hAlign = HAlign::Left;
    VAlign vAlign = VAlign::Baseline;
};

/// Format-neutral 2D content produced by the DXF and DWG readers for tier 4.
struct Drawing {
    std::vector<Polyline> polylines;
    std::vector<Arc> arcs;
    std::vector<TextItem> texts;

    [[nodiscard]] std::size_t primitiveCount() const noexcept {
        return polylines.size() + arcs.size() + texts.size();
    }
};

struct Box {
    Point min;
    Point max;
};

/// Bounding box of every finite coordinate, or nullopt when nothing finite was drawn. Text is
/// estimated from its height, length and rotation.
[[nodiscard]] std::optional<Box> extentsOf(const Drawing& drawing);

} // namespace cadpreview
