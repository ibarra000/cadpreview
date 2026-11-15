#include "render/vector/drawing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace cadpreview {

namespace {

// Rough advance of one character relative to the cap height, for estimating text boxes.
constexpr double CHARACTER_WIDTH = 0.8;

class BoxBuilder {
public:
    void add(Point p) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
            return;
        }
        if (!box_) {
            box_ = Box{p, p};
            return;
        }
        box_->min = {std::min(box_->min.x, p.x), std::min(box_->min.y, p.y)};
        box_->max = {std::max(box_->max.x, p.x), std::max(box_->max.y, p.y)};
    }

    [[nodiscard]] std::optional<Box> result() const { return box_; }

private:
    std::optional<Box> box_;
};

bool angleInSweep(double angle, double start, double end) {
    constexpr double TAU = 2 * std::numbers::pi;
    const double sweep = std::fmod(std::fmod(end - start, TAU) + TAU, TAU);
    const double offset = std::fmod(std::fmod(angle - start, TAU) + TAU, TAU);
    return sweep == 0 || offset <= sweep;
}

void addArc(BoxBuilder& builder, const Arc& arc) {
    if (!std::isfinite(arc.radius) || !std::isfinite(arc.startAngle) ||
        !std::isfinite(arc.endAngle)) {
        return;
    }
    const auto at = [&](double angle) {
        return Point{arc.center.x + arc.radius * std::cos(angle),
                     arc.center.y + arc.radius * std::sin(angle)};
    };
    builder.add(at(arc.startAngle));
    builder.add(at(arc.endAngle));
    for (int quadrant = 0; quadrant < 4; ++quadrant) {
        const double angle = quadrant * std::numbers::pi / 2;
        if (angleInSweep(angle, arc.startAngle, arc.endAngle)) {
            builder.add(at(angle));
        }
    }
}

void addText(BoxBuilder& builder, const TextItem& text) {
    const double width =
        text.height * CHARACTER_WIDTH * text.widthFactor * static_cast<double>(text.text.size());
    const double c = std::cos(text.rotation);
    const double s = std::sin(text.rotation);
    const std::array<Point, 4> corners{
        {{0, 0}, {width, 0}, {0, text.height}, {width, text.height}}};
    for (const Point& corner : corners) {
        builder.add({text.position.x + corner.x * c - corner.y * s,
                     text.position.y + corner.x * s + corner.y * c});
    }
}

} // namespace

std::optional<Box> extentsOf(const Drawing& drawing) {
    BoxBuilder builder;
    for (const Polyline& polyline : drawing.polylines) {
        for (const Point& point : polyline.points) {
            builder.add(point);
        }
    }
    for (const Arc& arc : drawing.arcs) {
        addArc(builder, arc);
    }
    for (const TextItem& text : drawing.texts) {
        addText(builder, text);
    }
    return builder.result();
}

} // namespace cadpreview
