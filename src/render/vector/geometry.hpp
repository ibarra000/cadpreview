#pragma once

#include "render/vector/drawing.hpp"

#include <span>
#include <vector>

namespace cadpreview {

/// 2D affine transform: (x, y) -> (a*x + c*y + tx, b*x + d*y + ty).
struct Transform {
    double a = 1;
    double b = 0;
    double c = 0;
    double d = 1;
    double tx = 0;
    double ty = 0;

    [[nodiscard]] Point apply(Point p) const noexcept {
        return {a * p.x + c * p.y + tx, b * p.x + d * p.y + ty};
    }
    /// This transform applied after inner.
    [[nodiscard]] Transform after(const Transform& inner) const noexcept;
    /// True when it only rotates, scales uniformly and translates (circles stay circles).
    [[nodiscard]] bool isSimilarity() const noexcept;
    [[nodiscard]] double uniformScale() const noexcept;
    [[nodiscard]] double rotation() const noexcept;

    /// Translate by insertion point, rotate, scale, then move the block base to the origin.
    [[nodiscard]] static Transform insert(Point insertion, double scaleX, double scaleY,
                                          double rotation, Point base) noexcept;
};

/// Points along a polyline segment with a DXF bulge (tan of a quarter of the included angle;
/// positive is counter-clockwise), excluding the end point. see notes/dxf.md#bulge
void appendBulgeSegment(std::vector<Point>& out, Point from, Point to, double bulge);

/// Points on an elliptical arc; majorAxis is relative to center, parameters in radians.
[[nodiscard]] std::vector<Point> ellipsePoints(Point center, Point majorAxis, double ratio,
                                               double startParameter, double endParameter);

/// Points on an arc, for arcs that a transform turns into ellipses.
[[nodiscard]] std::vector<Point> arcPoints(const Arc& arc);

/// Evaluates a B-spline with the given knots and optional weights; returns the control polygon
/// when the knot vector does not fit the degree and control points.
[[nodiscard]] std::vector<Point> splinePoints(int degree, std::span<const double> knots,
                                              std::span<const Point> controls,
                                              std::span<const double> weights);

} // namespace cadpreview
