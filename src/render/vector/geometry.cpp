#include "render/vector/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace cadpreview {

namespace {

constexpr double TAU = 2 * std::numbers::pi;
constexpr int SEGMENTS_PER_TURN = 72;
constexpr int SPLINE_SAMPLES_PER_SPAN = 8;
constexpr int MAX_SPLINE_SAMPLES = 4096;
constexpr double EPSILON = 1e-9;

int segmentsFor(double sweep) {
    return std::clamp(static_cast<int>(std::ceil(std::abs(sweep) / TAU * SEGMENTS_PER_TURN)), 2,
                      SEGMENTS_PER_TURN);
}

double positiveSweep(double start, double end) {
    double sweep = std::fmod(end - start, TAU);
    if (sweep <= 0) {
        sweep += TAU;
    }
    return sweep;
}

} // namespace

Transform Transform::after(const Transform& inner) const noexcept {
    return {a * inner.a + c * inner.b,        b * inner.a + d * inner.b,
            a * inner.c + c * inner.d,        b * inner.c + d * inner.d,
            a * inner.tx + c * inner.ty + tx, b * inner.tx + d * inner.ty + ty};
}

bool Transform::isSimilarity() const noexcept {
    const double scaleX = std::hypot(a, b);
    const double scaleY = std::hypot(c, d);
    const double determinant = a * d - b * c;
    return determinant > 0 && std::abs(scaleX - scaleY) <= EPSILON * std::max(scaleX, 1.0) &&
           std::abs(a * c + b * d) <= EPSILON * std::max(scaleX * scaleY, 1.0);
}

double Transform::uniformScale() const noexcept {
    return std::sqrt(std::abs(a * d - b * c));
}

double Transform::rotation() const noexcept {
    return std::atan2(b, a);
}

Transform Transform::insert(Point insertion, double scaleX, double scaleY, double rotation,
                            Point base) noexcept {
    const double cosR = std::cos(rotation);
    const double sinR = std::sin(rotation);
    Transform t{cosR * scaleX, sinR * scaleX, -sinR * scaleY, cosR * scaleY, 0, 0};
    const Point shifted = t.apply({-base.x, -base.y});
    t.tx = insertion.x + shifted.x;
    t.ty = insertion.y + shifted.y;
    return t;
}

void appendBulgeSegment(std::vector<Point>& out, Point from, Point to, double bulge) {
    out.push_back(from);
    const double chord = std::hypot(to.x - from.x, to.y - from.y);
    if (!std::isfinite(bulge) || std::abs(bulge) < EPSILON || chord < EPSILON) {
        return;
    }
    const double angle = 4 * std::atan(bulge); // signed included angle
    const double radius = chord / (2 * std::sin(std::abs(angle) / 2));
    // The radius at `from` meets the chord at (pi - |angle|) / 2, on the left for
    // counter-clockwise arcs and on the right for clockwise ones.
    const double side = angle > 0 ? 1.0 : -1.0;
    const double toCenter =
        std::atan2(to.y - from.y, to.x - from.x) + side * (std::numbers::pi - std::abs(angle)) / 2;
    const Point center{from.x + radius * std::cos(toCenter), from.y + radius * std::sin(toCenter)};
    const double start = std::atan2(from.y - center.y, from.x - center.x);
    const int segments = segmentsFor(angle);
    for (int i = 1; i < segments; ++i) {
        const double t = start + angle * i / segments;
        out.push_back({center.x + radius * std::cos(t), center.y + radius * std::sin(t)});
    }
}

std::vector<Point> ellipsePoints(Point center, Point majorAxis, double ratio, double startParameter,
                                 double endParameter) {
    const double sweep = positiveSweep(startParameter, endParameter);
    const Point minorAxis{-majorAxis.y * ratio, majorAxis.x * ratio};
    const int segments = segmentsFor(sweep);
    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const double t = startParameter + sweep * i / segments;
        points.push_back({center.x + majorAxis.x * std::cos(t) + minorAxis.x * std::sin(t),
                          center.y + majorAxis.y * std::cos(t) + minorAxis.y * std::sin(t)});
    }
    return points;
}

std::vector<Point> arcPoints(const Arc& arc) {
    return ellipsePoints(arc.center, {arc.radius, 0}, 1.0, arc.startAngle, arc.endAngle);
}

namespace {

// de Boor's algorithm for one parameter value; weights make it rational.
Point deBoor(int degree, std::span<const double> knots, std::span<const Point> controls,
             std::span<const double> weights, std::size_t span, double t) {
    struct Homogeneous {
        double x;
        double y;
        double w;
    };
    std::vector<Homogeneous> d;
    for (std::size_t j = 0; j <= static_cast<std::size_t>(degree); ++j) {
        const std::size_t index = span - static_cast<std::size_t>(degree) + j;
        const double w = weights.size() == controls.size() ? weights[index] : 1.0;
        d.push_back({controls[index].x * w, controls[index].y * w, w});
    }
    for (int r = 1; r <= degree; ++r) {
        for (int j = degree; j >= r; --j) {
            const std::size_t i =
                span - static_cast<std::size_t>(degree) + static_cast<std::size_t>(j);
            const double denominator =
                knots[i + static_cast<std::size_t>(degree - r) + 1] - knots[i];
            const double alpha =
                std::abs(denominator) < EPSILON ? 0.0 : (t - knots[i]) / denominator;
            auto& current = d[static_cast<std::size_t>(j)];
            const auto& previous = d[static_cast<std::size_t>(j - 1)];
            current = {(1 - alpha) * previous.x + alpha * current.x,
                       (1 - alpha) * previous.y + alpha * current.y,
                       (1 - alpha) * previous.w + alpha * current.w};
        }
    }
    const auto& result = d[static_cast<std::size_t>(degree)];
    return std::abs(result.w) < EPSILON ? Point{result.x, result.y}
                                        : Point{result.x / result.w, result.y / result.w};
}

} // namespace

std::vector<Point> splinePoints(int degree, std::span<const double> knots,
                                std::span<const Point> controls, std::span<const double> weights) {
    const std::vector<Point> polygon(controls.begin(), controls.end());
    if (degree < 1 || controls.size() <= static_cast<std::size_t>(degree) ||
        knots.size() != controls.size() + static_cast<std::size_t>(degree) + 1 ||
        !std::ranges::is_sorted(knots)) {
        return polygon;
    }
    const auto p = static_cast<std::size_t>(degree);
    const double first = knots[p];
    const double last = knots[controls.size()];
    if (!(last > first)) {
        return polygon;
    }
    const int samples =
        std::min(MAX_SPLINE_SAMPLES, static_cast<int>(controls.size()) * SPLINE_SAMPLES_PER_SPAN);
    std::vector<Point> points;
    std::size_t span = p;
    for (int i = 0; i <= samples; ++i) {
        const double t = first + (last - first) * i / samples;
        while (span + 1 < controls.size() && t >= knots[span + 1]) {
            ++span;
        }
        points.push_back(deBoor(degree, knots, controls, weights, span, t));
    }
    return points;
}

} // namespace cadpreview
