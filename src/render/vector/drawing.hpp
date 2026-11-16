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

/// A 3D point in drawing (world) units; z points towards the viewer of the top view.
struct Point3 {
    double x = 0;
    double y = 0;
    double z = 0;
};

/// A 3D polyline: edges of meshes, 3D polylines and solids. flatten() (projection.hpp) turns
/// wires into polylines before rendering.
struct Wire {
    std::vector<Point3> points;
    bool closed = false;
    /// An edge of a solid, surface or mesh. Only these switch the view to isometric, since 2D
    /// drawings often carry lines and faces with stray z values.
    bool body = false;
    /// Drawing order: the number of polylines drawn before this wire in the source, so the top
    /// view keeps the file's order (and its exact pixels).
    std::size_t order = 0;
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

/// Format-neutral content produced by the DXF and DWG readers for tier 4: 2D primitives in
/// the top view, plus 3D wires.
struct Drawing {
    std::vector<Polyline> polylines;
    std::vector<Arc> arcs;
    std::vector<TextItem> texts;
    std::vector<Wire> wires;
    /// Solids, surfaces and other 3D bodies that were visible but could not be drawn.
    std::size_t undrawnBodies = 0;

    [[nodiscard]] std::size_t primitiveCount() const noexcept {
        return polylines.size() + arcs.size() + texts.size() + wires.size();
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
