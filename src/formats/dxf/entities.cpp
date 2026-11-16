#include "formats/dxf/entities.hpp"

#include "common/errors.hpp"
#include "render/vector/geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
constexpr std::uint64_t CLOSED = 1U;             // group 70 bit on polylines and splines
constexpr std::uint64_t POLYLINE_3D = 8U;        // POLYLINE group 70 bits, see notes/dxf.md#3d
constexpr std::uint64_t POLYGON_MESH = 16U;
constexpr std::uint64_t MESH_CLOSED_N = 32U;
constexpr std::uint64_t POLYFACE_MESH = 64U;
constexpr std::uint64_t SPLINE_FRAME_VERTEX = 16U; // VERTEX group 70 bits
constexpr std::uint64_t FACE_COORDINATES = 64U;
constexpr std::uint64_t FACE_RECORD = 128U;
constexpr long long MAX_MESH_SIDE = 32767; // M and N are 16-bit counts
constexpr int PAPER_SPACE = 1;             // group 67

// Entities whose geometry is a 3D body (ACIS data or a subdivision mesh) that tier 4 does not
// read from DXF; they are counted so an empty result can say why.
constexpr std::array<std::string_view, 10> BODY_TYPES{
    "3DSOLID",       "REGION",      "BODY",         "PLANESURFACE",    "EXTRUDEDSURFACE",
    "LOFTEDSURFACE", "NURBSURFACE", "SWEPTSURFACE", "REVOLVEDSURFACE", "MESH"};

enum class Space : std::uint8_t { Model, Paper };

// True when any of `bits` is set in a group 70 style flag value.
bool hasBits(long long flags, std::uint64_t bits) {
    return (static_cast<std::uint64_t>(flags) & bits) != 0;
}

// TEXT alignment from groups 72 (horizontal) and 73 (vertical); see notes/dxf.md#text-alignment.
HAlign textHAlign(long long horizontal) {
    switch (horizontal) {
    case 1:
    case 4:
        return HAlign::Center;
    case 2:
        return HAlign::Right;
    default:
        return HAlign::Left;
    }
}

VAlign textVAlign(long long horizontal, long long vertical) {
    if (horizontal == 4) {
        return VAlign::Middle;
    }
    switch (vertical) {
    case 1:
        return VAlign::Bottom;
    case 2:
        return VAlign::Middle;
    case 3:
        return VAlign::Top;
    default:
        return VAlign::Baseline;
    }
}

// MTEXT attachment column (0 left, 1 centre, 2 right).
HAlign mtextHAlign(long long column) {
    switch (column) {
    case 0:
        return HAlign::Left;
    case 1:
        return HAlign::Center;
    default:
        return HAlign::Right;
    }
}

Point pointOf(const Record& record, int xCode) {
    return {record.number(xCode, 0), record.number(xCode + 10, 0)};
}

Point3 point3Of(const Record& record, int xCode) {
    return {record.number(xCode, 0), record.number(xCode + 10, 0), record.number(xCode + 20, 0)};
}

bool isFlat(const std::vector<Point3>& points) {
    return std::ranges::all_of(points, [](const Point3& p) { return p.z == 0; });
}

std::vector<Point> planOf(const std::vector<Point3>& points) {
    std::vector<Point> plan;
    plan.reserve(points.size());
    for (const Point3& p : points) {
        plan.push_back({p.x, p.y});
    }
    return plan;
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

std::vector<std::pair<std::size_t, std::size_t>> faceEdges(const Record& faceRecord,
                                                           std::size_t corners);

class Converter {
public:
    Converter(const Document& document, const ConvertLimits& limits)
        : document_(&document), limits_(limits) {}

    Drawing run() {
        convertList(document_->entities, Transform{}, 0, Space::Model);
        if (drawing_.primitiveCount() == 0) {
            convertList(document_->entities, Transform{}, 0, Space::Paper);
        }
        return std::move(drawing_);
    }

private:
    void convertList(const std::vector<Record>& records, const Transform& t, int depth,
                     std::optional<Space> space);
    void convert(const Record& record, const Transform& t, int depth);
    void polylineWithVertices(const std::vector<Record>& records, std::size_t& index,
                              const Transform& t);
    void polyline2d(const Record& head, const std::vector<const Record*>& vertices,
                    const Transform& t);
    void polygonMesh(const Record& head, const std::vector<const Record*>& vertices,
                     const Transform& t);
    void polyfaceMesh(const std::vector<const Record*>& vertices, const Transform& t);
    void lineOrWire(std::vector<Point3> points, bool closed, const Transform& t);
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
    void addWire(std::vector<Point3> points, bool closed, const Transform& t, bool body = false);
    void addText(TextItem item, const Transform& t);

    const Document* document_; // never null; a pointer keeps the class assignable
    ConvertLimits limits_;
    Drawing drawing_;
    std::size_t work_ = 0;
};

void Converter::charge(std::size_t count) {
    if (count > limits_.maxWork - work_) {
        throw FormatError(std::format("drawing needs more than {} work units", limits_.maxWork));
    }
    work_ += count;
}

bool Converter::visible(const Record& record) const {
    if (record.integer(60, 0) == 1) {
        return false;
    }
    const auto layer = record.text(8);
    return !layer || !document_->hiddenLayers.contains(upperCase(*layer));
}

void Converter::addPolyline(std::vector<Point> points, bool closed, const Transform& t) {
    charge(points.size());
    if (points.size() < 2) {
        return;
    }
    for (Point& p : points) {
        p = t.apply(p);
    }
    drawing_.polylines.push_back({std::move(points), closed});
}

// Block transforms are 2D: they move x and y, and z is kept. see notes/dxf.md#3d
void Converter::addWire(std::vector<Point3> points, bool closed, const Transform& t, bool body) {
    charge(points.size());
    if (points.size() < 2) {
        return;
    }
    for (Point3& p : points) {
        const Point moved = t.apply({p.x, p.y});
        p = {moved.x, moved.y, p.z};
    }
    drawing_.wires.push_back({std::move(points), closed, body, drawing_.polylines.size()});
}

// Entities without depth stay 2D, so flat drawings render exactly as before.
void Converter::lineOrWire(std::vector<Point3> points, bool closed, const Transform& t) {
    if (isFlat(points)) {
        addPolyline(planOf(points), closed, t);
    } else {
        addWire(std::move(points), closed, t);
    }
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
    charge(1 + (item.text.size() / TEXT_BYTES_PER_WORK));
    if (item.text.empty()) {
        return;
    }
    item.position = t.apply(item.position);
    item.height *= t.uniformScale();
    item.rotation += t.rotation();
    drawing_.texts.push_back(std::move(item));
}

// NOLINTNEXTLINE(misc-no-recursion): block nesting is bounded by limits_.maxBlockDepth
void Converter::convertList(const std::vector<Record>& records, const Transform& t, int depth,
                            std::optional<Space> space) {
    for (std::size_t i = 0; i < records.size(); ++i) {
        charge(1);
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

// NOLINTNEXTLINE(misc-no-recursion): block nesting is bounded by limits_.maxBlockDepth
void Converter::convert(const Record& r, const Transform& t, int depth) {
    const std::string_view type = r.type;
    if (type == "LINE") {
        lineOrWire({point3Of(r, 10), point3Of(r, 11)}, false, t);
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
        lineOrWire({point3Of(r, 10), point3Of(r, 11), point3Of(r, 12), point3Of(r, 13)}, true, t);
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
    } else if (std::ranges::find(BODY_TYPES, type) != BODY_TYPES.end()) {
        ++drawing_.undrawnBodies;
    }
}

void Converter::polylineWithVertices(const std::vector<Record>& records, std::size_t& index,
                                     const Transform& t) {
    const Record& head = records[index];
    std::vector<const Record*> vertices;
    while (index + 1 < records.size() && records[index + 1].type == "VERTEX") {
        vertices.push_back(&records[++index]);
    }
    if (index + 1 < records.size() && records[index + 1].type == "SEQEND") {
        ++index;
    }
    charge(vertices.size()); // read on every block placement, even when nothing is drawn
    if (!visible(head) || vertices.empty()) {
        return;
    }
    const long long flags = head.integer(70, 0);
    if (hasBits(flags, POLYFACE_MESH)) {
        polyfaceMesh(vertices, t);
    } else if (hasBits(flags, POLYGON_MESH)) {
        polygonMesh(head, vertices, t);
    } else if (hasBits(flags, POLYLINE_3D)) {
        std::vector<Point3> points;
        for (const Record* vertex : vertices) {
            if (!hasBits(vertex->integer(70, 0), SPLINE_FRAME_VERTEX)) {
                points.push_back(point3Of(*vertex, 10));
            }
        }
        addWire(std::move(points), hasBits(flags, CLOSED), t);
    } else {
        polyline2d(head, vertices, t);
    }
}

void Converter::polyline2d(const Record& head, const std::vector<const Record*>& vertices,
                           const Transform& t) {
    std::vector<Point> points;
    std::vector<double> bulges;
    for (const Record* vertex : vertices) {
        if (!hasBits(vertex->integer(70, 0), SPLINE_FRAME_VERTEX)) {
            points.push_back(pointOf(*vertex, 10));
            bulges.push_back(vertex->number(42, 0));
        }
    }
    if (points.empty()) {
        return;
    }
    const bool closed = hasBits(head.integer(70, 0), CLOSED);
    addPolyline(bulgePath(points, bulges, closed), closed, t);
}

// An M x N grid of vertices, listed row by row; rows and columns are drawn as wires. Bit 1
// closes the M direction (columns), bit 32 the N direction (rows). see notes/dxf.md#3d
void Converter::polygonMesh(const Record& head, const std::vector<const Record*>& vertices,
                            const Transform& t) {
    const long long m = head.integer(71, 0);
    const long long n = head.integer(72, 0);
    if (m < 1 || n < 1 || m > MAX_MESH_SIDE || n > MAX_MESH_SIDE ||
        static_cast<std::size_t>(m * n) != vertices.size()) {
        return;
    }
    const auto rows = static_cast<std::size_t>(m);
    const auto columns = static_cast<std::size_t>(n);
    const long long flags = head.integer(70, 0);
    for (std::size_t row = 0; row < rows; ++row) {
        std::vector<Point3> points;
        points.reserve(columns);
        for (std::size_t column = 0; column < columns; ++column) {
            points.push_back(point3Of(*vertices[row * columns + column], 10));
        }
        addWire(std::move(points), hasBits(flags, MESH_CLOSED_N), t, true);
    }
    for (std::size_t column = 0; column < columns; ++column) {
        std::vector<Point3> points;
        points.reserve(rows);
        for (std::size_t row = 0; row < rows; ++row) {
            points.push_back(point3Of(*vertices[row * columns + column], 10));
        }
        addWire(std::move(points), hasBits(flags, CLOSED), t, true);
    }
}

// Coordinate vertices (bit 64) come first; face records (bit 128 only) list up to four
// 1-based vertex indices in groups 71-74, negative where the edge starting there is hidden.
// Edges that continue where the previous one ended are joined into one wire, so a polyline
// written as consecutive two-vertex faces (as the DWG helper writes solid edges) costs one wire.
void Converter::polyfaceMesh(const std::vector<const Record*>& vertices, const Transform& t) {
    std::vector<Point3> corners;
    std::vector<Point3> chain;
    std::size_t chainEnd = 0;
    for (const Record* vertex : vertices) {
        const long long flags = vertex->integer(70, 0);
        if (hasBits(flags, FACE_COORDINATES)) {
            charge(1);
            corners.push_back(point3Of(*vertex, 10));
            continue;
        }
        if (!hasBits(flags, FACE_RECORD)) {
            continue;
        }
        for (const auto& [from, to] : faceEdges(*vertex, corners.size())) {
            if (chain.empty() || from != chainEnd) {
                addWire(std::exchange(chain, {}), false, t, true);
                chain.push_back(corners[from]);
            }
            chain.push_back(corners[to]);
            chainEnd = to;
        }
    }
    addWire(std::move(chain), false, t, true);
}

// The visible edges of a polyface face record, as 0-based corner pairs; none when an index
// is out of range.
std::vector<std::pair<std::size_t, std::size_t>> faceEdges(const Record& faceRecord,
                                                           std::size_t corners) {
    const auto count = static_cast<long long>(corners);
    std::array<long long, 4> indices{};
    std::size_t used = 0;
    for (int code = 71; code <= 74; ++code) {
        const long long index = faceRecord.integer(code, 0);
        if (index == 0) {
            break;
        }
        if (index < -count || index > count) {
            return {};
        }
        indices.at(used++) = index;
    }
    std::vector<std::pair<std::size_t, std::size_t>> edges;
    // Two indices describe one edge; three or four describe a closed outline.
    const std::size_t sides = used == 2 ? 1 : used;
    for (std::size_t i = 0; used >= 2 && i < sides; ++i) {
        const long long from = indices.at(i);
        const long long to = std::llabs(indices.at((i + 1) % used));
        if (from > 0) {
            edges.emplace_back(static_cast<std::size_t>(from - 1),
                               static_cast<std::size_t>(to - 1));
        }
    }
    return edges;
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
    const bool closed = hasBits(r.integer(70, 0), CLOSED);
    addPolyline(bulgePath(points, bulges, closed), closed, t);
}

void Converter::spline(const Record& r, const Transform& t) {
    const auto controls = allPoints(r, 10);
    const bool closed = hasBits(r.integer(70, 0), CLOSED);
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
    item.hAlign = textHAlign(horizontal);
    item.vAlign = textVAlign(horizontal, vertical);
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
    charge(raw.size() / TEXT_BYTES_PER_WORK);
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
    const double shift = static_cast<double>(row) * blockHeight / 2;
    const Point origin = pointOf(r, 10);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const double offset = shift - height - static_cast<double>(i) * spacing;
        TextItem item;
        item.text = std::string(lines[i]);
        item.height = height;
        item.rotation = rotation;
        item.position = {origin.x - std::sin(rotation) * offset,
                         origin.y + std::cos(rotation) * offset};
        item.hAlign = mtextHAlign(column);
        addText(std::move(item), t);
    }
}

// NOLINTNEXTLINE(misc-no-recursion): block nesting is bounded by limits_.maxBlockDepth
void Converter::insert(const Record& r, const Transform& t, int depth) {
    const auto found = document_->blocks.find(upperCase(r.text(2).value_or("")));
    if (found == document_->blocks.end()) {
        return;
    }
    const Point insertion = pointOf(r, 10);
    const double rotation = r.number(50, 0) * DEGREES;
    const auto columns = std::max<long long>(1, r.integer(70, 1));
    const auto rows = std::max<long long>(1, r.integer(71, 1));
    const auto budget = static_cast<long long>(limits_.maxWork);
    if (columns > budget || rows > budget / columns) {
        throw FormatError("block array is too large");
    }
    charge(static_cast<std::size_t>(columns * rows));
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

// NOLINTNEXTLINE(misc-no-recursion): block nesting is bounded by limits_.maxBlockDepth
void Converter::drawBlock(std::string_view name, const Transform& t, int depth) {
    if (depth + 1 > limits_.maxBlockDepth) {
        throw FormatError("blocks are nested too deeply");
    }
    const auto found = document_->blocks.find(upperCase(name));
    if (found != document_->blocks.end()) {
        convertList(found->second.entities, t, depth + 1, std::nullopt);
    }
}

} // namespace

Drawing toDrawing(const Document& document, const ConvertLimits& limits) {
    return Converter(document, limits).run();
}

} // namespace cadpreview::dxf
