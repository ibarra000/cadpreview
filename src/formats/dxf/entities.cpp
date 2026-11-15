#include "formats/dxf/entities.hpp"

#include "common/errors.hpp"
#include "render/vector/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cadpreview::dxf {

namespace {

constexpr double DEGREES = std::numbers::pi / 180.0;
constexpr double MTEXT_LINE_SPACING = 5.0 / 3.0; // AutoCAD's default spacing factor
constexpr long long CLOSED = 1;                  // group 70 bit on polylines and splines
constexpr long long POLYLINE_MESH = 16 | 64;     // 3D mesh or polyface: not drawn
constexpr long long SPLINE_FRAME_VERTEX = 16;
constexpr int PAPER_SPACE = 1; // group 67

enum class Space : std::uint8_t { Model, Paper };

Point pointOf(const Record& record, int xCode) {
    return {record.number(xCode, 0), record.number(xCode + 10, 0)};
}

// Every (x, y) pair that a record lists with the given x code, in order.
std::vector<Point> allPoints(const Record& record, int xCode) {
    std::vector<Point> points;
    for (const Group& group : record.groups) {
        if (group.code == xCode) {
            points.push_back({toDouble(group.value).value_or(0), 0});
        } else if (group.code == xCode + 10 && !points.empty()) {
            points.back().y = toDouble(group.value).value_or(0);
        }
    }
    return points;
}

// Expands vertices with per-vertex bulges into a path; a closed path also bends its last edge.
std::vector<Point> bulgePath(const std::vector<Point>& points, const std::vector<double>& bulges,
                             bool closed) {
    std::vector<Point> path;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const bool last = i + 1 == points.size();
        if (last && !closed) {
            path.push_back(points[i]);
        } else {
            appendBulgeSegment(path, points[i], points[last ? 0 : i + 1], bulges[i]);
        }
    }
    return path;
}

std::vector<double> allNumbers(const Record& record, int code) {
    std::vector<double> numbers;
    for (const Group& group : record.groups) {
        if (group.code == code) {
            numbers.push_back(toDouble(group.value).value_or(0));
        }
    }
    return numbers;
}

class Converter {
public:
    Converter(const Document& document, const ConvertLimits& limits)
        : document_(document), limits_(limits) {}

    Drawing run() {
        convertList(document_.entities, Transform{}, 0, Space::Model);
        if (drawing_.primitiveCount() == 0) {
            convertList(document_.entities, Transform{}, 0, Space::Paper);
        }
        return std::move(drawing_);
    }

private:
    void convertList(const std::vector<Record>& records, const Transform& t, int depth,
                     std::optional<Space> space);
    void convert(const Record& record, const Transform& t, int depth);
    void polylineWithVertices(const std::vector<Record>& records, std::size_t& index,
                              const Transform& t);
    void insert(const Record& record, const Transform& t, int depth);
    void drawBlock(std::string_view name, const Transform& t, int depth);
    void lwpolyline(const Record& record, const Transform& t);
    void spline(const Record& record, const Transform& t);
    void text(const Record& record, const Transform& t);
    void mtext(const Record& record, const Transform& t);

    [[nodiscard]] bool visible(const Record& record) const;
    void charge(std::size_t count);
    void addPolyline(std::vector<Point> points, bool closed, const Transform& t);
    void addArc(const Arc& arc, const Transform& t);
    void addText(TextItem item, const Transform& t);

    const Document& document_;
    const ConvertLimits& limits_;
    Drawing drawing_;
    std::size_t primitives_ = 0;
};

void Converter::charge(std::size_t count) {
    primitives_ += count;
    if (primitives_ > limits_.maxPrimitives) {
        throw FormatError(std::format("drawing exceeds {} primitives", limits_.maxPrimitives));
    }
}

bool Converter::visible(const Record& record) const {
    if (record.integer(60, 0) == 1) {
        return false;
    }
    const auto layer = record.text(8);
    return !layer || !document_.hiddenLayers.contains(upperCase(*layer));
}

void Converter::addPolyline(std::vector<Point> points, bool closed, const Transform& t) {
    if (points.size() < 2) {
        return;
    }
    charge(1);
    for (Point& p : points) {
        p = t.apply(p);
    }
    drawing_.polylines.push_back({std::move(points), closed});
}

void Converter::addArc(const Arc& arc, const Transform& t) {
    if (!t.isSimilarity()) {
        addPolyline(arcPoints(arc), false, t);
        return;
    }
    charge(1);
    const double turn = t.rotation();
    drawing_.arcs.push_back({t.apply(arc.center), arc.radius * t.uniformScale(),
                             arc.startAngle + turn, arc.endAngle + turn});
}

void Converter::addText(TextItem item, const Transform& t) {
    if (item.text.empty()) {
        return;
    }
    charge(1);
    item.position = t.apply(item.position);
    item.height *= t.uniformScale();
    item.rotation += t.rotation();
    drawing_.texts.push_back(std::move(item));
}

void Converter::convertList(const std::vector<Record>& records, const Transform& t, int depth,
                            std::optional<Space> space) {
    for (std::size_t i = 0; i < records.size(); ++i) {
        const Record& record = records[i];
        if (space) {
            const bool paper = record.integer(67, 0) == PAPER_SPACE;
            if (paper != (*space == Space::Paper)) {
                continue;
            }
        }
        if (record.type == "POLYLINE") {
            polylineWithVertices(records, i, t);
        } else if (visible(record)) {
            convert(record, t, depth);
        }
    }
}

void Converter::convert(const Record& r, const Transform& t, int depth) {
    const std::string_view type = r.type;
    if (type == "LINE") {
        addPolyline({pointOf(r, 10), pointOf(r, 11)}, false, t);
    } else if (type == "CIRCLE") {
        addArc({pointOf(r, 10), r.number(40, 0), 0, 2 * std::numbers::pi}, t);
    } else if (type == "ARC") {
        addArc({pointOf(r, 10), r.number(40, 0), r.number(50, 0) * DEGREES,
                r.number(51, 360) * DEGREES},
               t);
    } else if (type == "LWPOLYLINE") {
        lwpolyline(r, t);
    } else if (type == "ELLIPSE") {
        addPolyline(ellipsePoints(pointOf(r, 10), pointOf(r, 11), r.number(40, 1), r.number(41, 0),
                                  r.number(42, 2 * std::numbers::pi)),
                    false, t);
    } else if (type == "SPLINE") {
        spline(r, t);
    } else if (type == "SOLID" || type == "TRACE") {
        addPolyline({pointOf(r, 10), pointOf(r, 11), pointOf(r, 13), pointOf(r, 12)}, true, t);
    } else if (type == "3DFACE") {
        addPolyline({pointOf(r, 10), pointOf(r, 11), pointOf(r, 12), pointOf(r, 13)}, true, t);
    } else if (type == "LEADER") {
        addPolyline(allPoints(r, 10), false, t);
    } else if (type == "TEXT" || type == "ATTRIB") {
        text(r, t);
    } else if (type == "MTEXT") {
        mtext(r, t);
    } else if (type == "INSERT") {
        insert(r, t, depth);
    } else if (type == "DIMENSION") {
        drawBlock(r.text(2).value_or(""), t, depth);
    }
}

void Converter::polylineWithVertices(const std::vector<Record>& records, std::size_t& index,
                                     const Transform& t) {
    const Record& head = records[index];
    std::vector<Point> points;
    std::vector<double> bulges;
    while (index + 1 < records.size() && records[index + 1].type == "VERTEX") {
        const Record& vertex = records[++index];
        if ((vertex.integer(70, 0) & SPLINE_FRAME_VERTEX) == 0) {
            points.push_back(pointOf(vertex, 10));
            bulges.push_back(vertex.number(42, 0));
        }
    }
    if (index + 1 < records.size() && records[index + 1].type == "SEQEND") {
        ++index;
    }
    const long long flags = head.integer(70, 0);
    if ((flags & POLYLINE_MESH) != 0 || !visible(head) || points.empty()) {
        return;
    }
    const bool closed = (flags & CLOSED) != 0;
    addPolyline(bulgePath(points, bulges, closed), closed, t);
}

void Converter::lwpolyline(const Record& r, const Transform& t) {
    std::vector<Point> points;
    std::vector<double> bulges;
    for (const Group& group : r.groups) {
        if (group.code == 10) {
            points.push_back({toDouble(group.value).value_or(0), 0});
            bulges.push_back(0);
        } else if (group.code == 20 && !points.empty()) {
            points.back().y = toDouble(group.value).value_or(0);
        } else if (group.code == 42 && !bulges.empty()) {
            bulges.back() = toDouble(group.value).value_or(0);
        }
    }
    const bool closed = (r.integer(70, 0) & CLOSED) != 0;
    addPolyline(bulgePath(points, bulges, closed), closed, t);
}

void Converter::spline(const Record& r, const Transform& t) {
    const auto controls = allPoints(r, 10);
    const bool closed = (r.integer(70, 0) & CLOSED) != 0;
    if (controls.size() >= 2) {
        const auto degree = static_cast<int>(r.integer(71, 3));
        addPolyline(splinePoints(degree, allNumbers(r, 40), controls, allNumbers(r, 41)), closed,
                    t);
    } else {
        addPolyline(allPoints(r, 11), closed, t);
    }
}

// see notes/dxf.md#text-alignment
void Converter::text(const Record& r, const Transform& t) {
    const long long horizontal = r.integer(72, 0);
    const long long vertical = r.integer(73, 0);
    TextItem item;
    item.text = plainText(r.text(1).value_or(""), false);
    item.height = r.number(40, 0);
    item.rotation = r.number(50, 0) * DEGREES;
    item.widthFactor = r.number(41, 1);
    item.position = (horizontal != 0 || vertical != 0) ? pointOf(r, 11) : pointOf(r, 10);
    item.hAlign = horizontal == 1 || horizontal == 4 ? HAlign::Center
                  : horizontal == 2                  ? HAlign::Right
                                                     : HAlign::Left;
    item.vAlign = horizontal == 4 ? VAlign::Middle
                  : vertical == 1 ? VAlign::Bottom
                  : vertical == 2 ? VAlign::Middle
                  : vertical == 3 ? VAlign::Top
                                  : VAlign::Baseline;
    addText(std::move(item), t);
}

// see notes/dxf.md#mtext
void Converter::mtext(const Record& r, const Transform& t) {
    std::string raw;
    for (const Group& group : r.groups) {
        if (group.code == 3) {
            raw.append(group.value);
        }
    }
    raw.append(r.text(1).value_or(""));
    const std::string content = plainText(raw, true);
    const double height = r.number(40, 1);
    const double rotation =
        r.text(50) ? r.number(50, 0) * DEGREES : std::atan2(r.number(21, 0), r.number(11, 1));
    const long long attachment = std::clamp<long long>(r.integer(71, 1), 1, 9);
    const auto row = (attachment - 1) / 3;
    const auto column = (attachment - 1) % 3;
    std::vector<std::string_view> lines;
    for (std::size_t start = 0; start <= content.size();) {
        const auto end = std::min(content.find('\n', start), content.size());
        lines.push_back(std::string_view(content).substr(start, end - start));
        start = end + 1;
    }
    const double spacing = height * MTEXT_LINE_SPACING;
    const double blockHeight = static_cast<double>(lines.size() - 1) * spacing + height;
    const double shift = row == 0 ? 0 : row == 1 ? blockHeight / 2 : blockHeight;
    const Point origin = pointOf(r, 10);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const double offset = shift - height - static_cast<double>(i) * spacing;
        TextItem item;
        item.text = std::string(lines[i]);
        item.height = height;
        item.rotation = rotation;
        item.position = {origin.x - std::sin(rotation) * offset,
                         origin.y + std::cos(rotation) * offset};
        item.hAlign = column == 0 ? HAlign::Left : column == 1 ? HAlign::Center : HAlign::Right;
        addText(std::move(item), t);
    }
}

void Converter::insert(const Record& r, const Transform& t, int depth) {
    const auto found = document_.blocks.find(upperCase(r.text(2).value_or("")));
    if (found == document_.blocks.end()) {
        return;
    }
    const Point insertion = pointOf(r, 10);
    const double rotation = r.number(50, 0) * DEGREES;
    const auto columns = std::max<long long>(1, r.integer(70, 1));
    const auto rows = std::max<long long>(1, r.integer(71, 1));
    const auto copies = static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows);
    if (copies > limits_.maxPrimitives) {
        throw FormatError("block array is too large");
    }
    const double columnSpacing = r.number(44, 0);
    const double rowSpacing = r.number(45, 0);
    for (long long row = 0; row < rows; ++row) {
        for (long long column = 0; column < columns; ++column) {
            const double dx = static_cast<double>(column) * columnSpacing;
            const double dy = static_cast<double>(row) * rowSpacing;
            const Point at{insertion.x + dx * std::cos(rotation) - dy * std::sin(rotation),
                           insertion.y + dx * std::sin(rotation) + dy * std::cos(rotation)};
            const Transform local = Transform::insert(at, r.number(41, 1), r.number(42, 1),
                                                      rotation, found->second.base);
            drawBlock(found->first, t.after(local), depth);
        }
    }
}

void Converter::drawBlock(std::string_view name, const Transform& t, int depth) {
    if (depth + 1 > limits_.maxBlockDepth) {
        throw FormatError("blocks are nested too deeply");
    }
    const auto found = document_.blocks.find(upperCase(name));
    if (found != document_.blocks.end()) {
        convertList(found->second.entities, t, depth + 1, std::nullopt);
    }
}

} // namespace

Drawing toDrawing(const Document& document, const ConvertLimits& limits) {
    return Converter(document, limits).run();
}

} // namespace cadpreview::dxf
