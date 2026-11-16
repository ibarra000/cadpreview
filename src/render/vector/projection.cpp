#include "render/vector/projection.hpp"

#include "render/vector/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace cadpreview {

namespace {

// Depth below this fraction of the plan size is treated as noise (for example elevations
// left on 2D linework), so such drawings stay in the top view.
constexpr double MIN_DEPTH_RATIO = 0.01;
// The isometric view turns each arc into a polyline of up to 73 points. Above this many arcs it
// would cost more memory than the work limits allow for, so such drawings stay in the top view.
constexpr std::size_t MAX_ISOMETRIC_ARCS = 50'000;

constexpr double INV_ROOT2 = 1 / std::numbers::sqrt2;
constexpr double INV_ROOT6 = 1 / (std::numbers::sqrt2 * std::numbers::sqrt3);

class Range {
public:
    void add(double value) noexcept {
        low_ = std::min(low_, value);
        high_ = std::max(high_, value);
    }
    [[nodiscard]] bool empty() const noexcept { return high_ < low_; }
    [[nodiscard]] double size() const noexcept { return empty() ? 0 : high_ - low_; }

private:
    double low_ = std::numeric_limits<double>::infinity();
    double high_ = -std::numeric_limits<double>::infinity();
};

bool isFinite(const Point3& p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

std::vector<Point> projectAll(const std::vector<Point3>& points, View view) {
    std::vector<Point> projected;
    projected.reserve(points.size());
    for (const Point3& p : points) {
        projected.push_back(project(p, view));
    }
    return projected;
}

std::vector<Point3> onGround(const std::vector<Point>& points) {
    std::vector<Point3> lifted;
    lifted.reserve(points.size());
    for (const Point& p : points) {
        lifted.push_back({p.x, p.y, 0});
    }
    return lifted;
}

// Projects the 2D content (taken to lie at z = 0) into the isometric view.
void projectPlanContent(Drawing& drawing) {
    for (Polyline& polyline : drawing.polylines) {
        polyline.points = projectAll(onGround(polyline.points), View::Isometric);
    }
    for (const Arc& arc : drawing.arcs) {
        drawing.polylines.push_back(
            {projectAll(onGround(arcPoints(arc)), View::Isometric), false});
    }
    drawing.arcs.clear();
    for (TextItem& text : drawing.texts) {
        text.position = project({text.position.x, text.position.y, 0}, View::Isometric);
    }
}

} // namespace

View chooseView(const Drawing& drawing) {
    if (drawing.arcs.size() > MAX_ISOMETRIC_ARCS) {
        return View::Top;
    }
    Range x;
    Range y;
    Range z; // depth of body edges only: stray z values on lines must not tilt a 2D drawing
    for (const Wire& wire : drawing.wires) {
        for (const Point3& p : wire.points) {
            if (isFinite(p)) {
                x.add(p.x);
                y.add(p.y);
                if (wire.body) {
                    z.add(p.z);
                }
            }
        }
    }
    if (z.size() == 0) {
        return View::Top;
    }
    if (const auto box = extentsOf(drawing)) {
        x.add(box->min.x);
        x.add(box->max.x);
        y.add(box->min.y);
        y.add(box->max.y);
    }
    const double plan = std::max(x.size(), y.size());
    return z.size() > MIN_DEPTH_RATIO * plan ? View::Isometric : View::Top;
}

Point project(Point3 point, View view) noexcept {
    if (view == View::Top) {
        return {point.x, point.y};
    }
    return {(point.x + point.y) * INV_ROOT2, (point.y - point.x + 2 * point.z) * INV_ROOT6};
}

Drawing flatten(Drawing drawing) {
    const View view = chooseView(drawing);
    if (view == View::Isometric) {
        projectPlanContent(drawing);
    }
    // Merge wires back among the polylines in their recorded order (wires are in order too).
    std::vector<Polyline> merged;
    merged.reserve(drawing.polylines.size() + drawing.wires.size());
    std::size_t next = 0;
    for (const Wire& wire : drawing.wires) {
        const std::size_t until = std::clamp(wire.order, next, drawing.polylines.size());
        std::move(drawing.polylines.begin() + static_cast<std::ptrdiff_t>(next),
                  drawing.polylines.begin() + static_cast<std::ptrdiff_t>(until),
                  std::back_inserter(merged));
        next = until;
        merged.push_back({projectAll(wire.points, view), wire.closed});
    }
    std::move(drawing.polylines.begin() + static_cast<std::ptrdiff_t>(next),
              drawing.polylines.end(), std::back_inserter(merged));
    drawing.polylines = std::move(merged);
    drawing.wires.clear();
    return drawing;
}

} // namespace cadpreview
