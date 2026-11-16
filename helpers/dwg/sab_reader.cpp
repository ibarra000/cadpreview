#include "sab_reader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace cadpreview::dwghelper {

namespace {

// see notes/acis.md#sab-layout
constexpr std::string_view MAGIC = "ACIS BinaryFile";
constexpr std::size_t HEADER_INTEGERS = 4;
constexpr std::size_t MAX_NAME_BYTES = 256;
constexpr std::string_view END_RECORD = "End-of-ACIS-data";
constexpr std::uint8_t KEPT_VALUES = 9; // the records used need at most 9 values
constexpr int TURN_SEGMENTS = 32;       // segments for a full turn of an ellipse or circle
constexpr double TAU = 2 * std::numbers::pi;
constexpr double MAX_SWEEP = 2 * TAU; // larger parameter ranges are not trusted
constexpr double MATCH_TOLERANCE = 1e-4;

enum class Tag : std::uint8_t {
    Integer = 0x04,
    Double = 0x06,
    Text = 0x07,
    TrueFlag = 0x0A,
    FalseFlag = 0x0B,
    Pointer = 0x0C,
    Identifier = 0x0D,
    SubIdentifier = 0x0E,
    RecordEnd = 0x11,
    Position = 0x13,
    Vector = 0x14,
};

enum class Kind : std::uint8_t { Integer, Double, Text, Flag, Pointer, Position, Vector };

/// One tagged value: an integer or pointer in `integer`, a double in values[0], a position or
/// vector in values.
struct Value {
    Kind kind = Kind::Flag;
    std::int32_t integer = 0;
    std::array<double, 3> values{};

    [[nodiscard]] Vec3 vector() const { return {values[0], values[1], values[2]}; }
};

/// The record types the wireframe uses; values are kept only for these.
enum class Type : std::uint8_t { Other, Point, Vertex, Edge, Ellipse, Sphere };

Type typeOf(std::string_view name) {
    if (name == "point") {
        return Type::Point;
    }
    if (name == "vertex") {
        return Type::Vertex;
    }
    if (name == "edge") {
        return Type::Edge;
    }
    if (name == "ellipse-curve") {
        return Type::Ellipse;
    }
    return name == "sphere-surface" ? Type::Sphere : Type::Other;
}

struct Record {
    Type type = Type::Other;
    std::uint8_t count = 0;  ///< kept values
    std::uint32_t first = 0; ///< index of the first kept value in Body::values
};

/// Records in stream order (index = SAB record number) and the values they keep. Memory is
/// bounded by SabLimits::maxRecords and SabLimits::maxValues.
struct Body {
    std::vector<Record> records;
    std::vector<Value> values;
};

class Cursor {
public:
    explicit Cursor(std::span<const std::byte> data) : data_(data) {}

    [[nodiscard]] bool atEnd() const noexcept { return pos_ == data_.size(); }

    std::span<const std::byte> take(std::size_t count) {
        if (count > data_.size() - pos_) {
            throw SabError("SAB data is truncated");
        }
        const auto bytes = data_.subspan(pos_, count);
        pos_ += count;
        return bytes;
    }

    template <class T>
    T little() {
        T value{};
        std::memcpy(&value, take(sizeof(T)).data(), sizeof(T)); // the format is little-endian
        return value;
    }

    double finite() {
        const auto value = little<double>();
        if (!std::isfinite(value)) {
            throw SabError("SAB data holds a non-finite number");
        }
        return value;
    }

    std::string_view text() {
        const auto length = std::to_integer<std::size_t>(take(1)[0]);
        const auto bytes = take(length);
        return {reinterpret_cast<const char*>(bytes.data()), bytes.size()}; // NOLINT
    }

private:
    std::span<const std::byte> data_;
    std::size_t pos_ = 0;
};

void appendName(std::string& name, std::string_view part, bool last) {
    if (name.size() + part.size() + 1 > MAX_NAME_BYTES) {
        throw SabError("SAB record name is too long");
    }
    name.append(part);
    if (!last) {
        name.push_back('-');
    }
}

// Reads one tagged value. Returns nullopt for name parts and the record end, and sets `ended`
// at the end of the record.
std::optional<Value> readValue(Cursor& in, std::string& name, bool& ended) {
    const auto tag = static_cast<Tag>(std::to_integer<std::uint8_t>(in.take(1)[0]));
    Value value;
    switch (tag) {
    case Tag::Integer:
    case Tag::Pointer:
        value.kind = tag == Tag::Integer ? Kind::Integer : Kind::Pointer;
        value.integer = in.little<std::int32_t>();
        return value;
    case Tag::Double:
        value.kind = Kind::Double;
        value.values[0] = in.finite();
        return value;
    case Tag::Position:
    case Tag::Vector:
        value.kind = tag == Tag::Position ? Kind::Position : Kind::Vector;
        value.values = {in.finite(), in.finite(), in.finite()};
        return value;
    case Tag::Text:
        static_cast<void>(in.text());
        value.kind = Kind::Text;
        return value;
    case Tag::TrueFlag:
    case Tag::FalseFlag:
        value.integer = tag == Tag::TrueFlag ? 1 : 0;
        return value;
    case Tag::Identifier:
    case Tag::SubIdentifier:
        appendName(name, in.text(), tag == Tag::Identifier);
        ended = name == END_RECORD; // the end marker has no record-end tag
        return std::nullopt;
    case Tag::RecordEnd:
        ended = true;
        return std::nullopt;
    default:
        throw SabError("SAB data holds an unknown tag");
    }
}

void checkHeader(Cursor& in) {
    const auto magic = in.take(MAGIC.size());
    if (!std::equal(magic.begin(), magic.end(), MAGIC.begin(),
                    [](std::byte b, char c) { return b == static_cast<std::byte>(c); })) {
        throw SabError("not SAB data");
    }
    static_cast<void>(in.take(HEADER_INTEGERS * sizeof(std::int32_t)));
}

// Reads one record, appending its first KEPT_VALUES values to body.values; they are dropped
// again unless the record type is used. Returns false after the end marker.
bool readRecord(Cursor& in, Body& body, const SabLimits& limits) {
    std::string name;
    Record record;
    record.first = static_cast<std::uint32_t>(body.values.size());
    bool ended = false;
    while (!ended) {
        const auto value = readValue(in, name, ended);
        if (value && record.count < KEPT_VALUES) {
            if (body.values.size() == limits.maxValues) {
                throw SabError("SAB data has too many values");
            }
            body.values.push_back(*value);
            ++record.count;
        }
    }
    record.type = typeOf(name);
    if (record.type == Type::Other) {
        body.values.resize(record.first);
        record.count = 0;
    }
    body.records.push_back(record);
    return name != END_RECORD;
}

Body readBody(std::span<const std::byte> data, const SabLimits& limits) {
    if (data.size() > limits.maxBytes) {
        throw SabError("SAB data is too large");
    }
    Cursor in(data);
    checkHeader(in);
    Body body;
    bool more = true;
    while (more && !in.atEnd()) {
        if (body.records.size() == limits.maxRecords) {
            throw SabError("SAB data has too many records");
        }
        more = readRecord(in, body, limits);
    }
    return body;
}

Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 operator*(const Vec3& a, double s) {
    return {a.x * s, a.y * s, a.z * s};
}
double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double length(const Vec3& a) {
    return std::hypot(a.x, a.y, a.z);
}
bool isFinite(const Vec3& a) {
    return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}
std::optional<Vec3> unit(const Vec3& a) {
    const double l = length(a);
    if (!(l > 0) || !std::isfinite(l)) {
        return std::nullopt;
    }
    return a * (1 / l);
}

// Typed access to a kept value of a record; nullopt when the layout differs.
class RecordView {
public:
    RecordView(const Body& body, std::size_t index) : body_(&body) {
        if (index < body.records.size()) {
            record_ = &body.records[index];
        }
    }
    [[nodiscard]] bool is(Type type) const { return record_ != nullptr && record_->type == type; }
    [[nodiscard]] std::optional<Value> at(std::size_t i, Kind kind) const {
        if (record_ == nullptr || i >= record_->count) {
            return std::nullopt;
        }
        const Value& value = body_->values[record_->first + i];
        return value.kind == kind ? std::optional(value) : std::nullopt;
    }
    [[nodiscard]] RecordView follow(std::size_t i) const {
        const auto value = at(i, Kind::Pointer);
        const bool valid = value && value->integer >= 0;
        return {*body_, valid ? static_cast<std::size_t>(value->integer) : body_->records.size()};
    }

private:
    const Body* body_;
    const Record* record_ = nullptr;
};

// Positions of kept values, as observed in ASM 216-era SAB. see notes/acis.md#records
constexpr std::size_t FIRST_FIELD = 3; // after the attribute, integer and owner fields
constexpr std::size_t EDGE_START = 3;
constexpr std::size_t EDGE_START_PARAM = 4;
constexpr std::size_t EDGE_END = 5;
constexpr std::size_t EDGE_END_PARAM = 6;
constexpr std::size_t EDGE_CURVE = 8;
constexpr std::size_t VERTEX_POINT = 5;

std::optional<Vec3> vertexPosition(const RecordView& vertex) {
    if (!vertex.is(Type::Vertex)) {
        return std::nullopt;
    }
    const RecordView point = vertex.follow(VERTEX_POINT);
    const auto position =
        point.is(Type::Point) ? point.at(FIRST_FIELD, Kind::Position) : std::nullopt;
    return position ? std::optional(position->vector()) : std::nullopt;
}

struct Ellipse {
    Vec3 center;
    Vec3 major;
    Vec3 minor; ///< normal x major, scaled by the radius ratio
    [[nodiscard]] Vec3 at(double t) const {
        return center + major * std::cos(t) + minor * std::sin(t);
    }
};

std::optional<Ellipse> ellipseOf(const RecordView& curve) {
    const auto center = curve.at(FIRST_FIELD, Kind::Position);
    const auto normal = curve.at(FIRST_FIELD + 1, Kind::Vector);
    const auto major = curve.at(FIRST_FIELD + 2, Kind::Vector);
    const auto ratio = curve.at(FIRST_FIELD + 3, Kind::Double);
    if (!curve.is(Type::Ellipse) || !center || !normal || !major || !ratio) {
        return std::nullopt;
    }
    const auto axis = unit(normal->vector());
    if (!axis) {
        return std::nullopt;
    }
    return Ellipse{center->vector(), major->vector(),
                   cross(*axis, major->vector()) * ratio->values[0]};
}

class Builder {
public:
    explicit Builder(const SabLimits& limits) : limits_(limits) {}

    // Polylines with a non-finite point (huge finite inputs can overflow) are dropped.
    void add(Polyline3 polyline) {
        if (!std::ranges::all_of(polyline, isFinite)) {
            return;
        }
        if (polyline.size() > limits_.maxPoints - points_) {
            throw SabError("SAB wireframe has too many points");
        }
        points_ += polyline.size();
        wires_.push_back(std::move(polyline));
    }
    [[nodiscard]] std::vector<Polyline3> result() && { return std::move(wires_); }

private:
    SabLimits limits_;
    std::size_t points_ = 0;
    std::vector<Polyline3> wires_;
};

// Samples an elliptical edge. A reversed edge runs on the negated curve parameter; the edge
// direction is not stored where it was observed, so the variant whose ends meet the vertices
// is used, and the chord when neither does.
Polyline3 ellipseEdge(const Ellipse& e, const Vec3& start, const Vec3& end, double t0, double t1) {
    const double tolerance = MATCH_TOLERANCE * (1 + length(e.center) + length(e.major));
    const double forward = length(e.at(t0) - start) + length(e.at(t1) - end);
    const double reversed = length(e.at(-t0) - start) + length(e.at(-t1) - end);
    const double sweep = t1 - t0;
    // Written so that NaN distances or sweeps also take the chord.
    if (!(std::min(forward, reversed) <= tolerance) || !(std::abs(sweep) <= MAX_SWEEP)) {
        return {start, end};
    }
    const double sign = forward <= reversed ? 1.0 : -1.0;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(std::abs(sweep) / TAU * TURN_SEGMENTS)), 1, 2 * TURN_SEGMENTS);
    Polyline3 points;
    points.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        points.push_back(e.at(sign * (t0 + sweep * i / segments)));
    }
    points.front() = start;
    points.back() = end;
    return points;
}

std::optional<Polyline3> edgePolyline(const RecordView& edge) {
    const auto start = vertexPosition(edge.follow(EDGE_START));
    const auto end = vertexPosition(edge.follow(EDGE_END));
    const auto t0 = edge.at(EDGE_START_PARAM, Kind::Double);
    const auto t1 = edge.at(EDGE_END_PARAM, Kind::Double);
    if (!start || !end || !t0 || !t1) {
        return std::nullopt;
    }
    if (const auto ellipse = ellipseOf(edge.follow(EDGE_CURVE))) {
        return ellipseEdge(*ellipse, *start, *end, t0->values[0], t1->values[0]);
    }
    return Polyline3{*start, *end}; // straight edges, and the chord of other curves
}

Polyline3 circle(const Vec3& center, const Vec3& u, const Vec3& v, double radius) {
    Polyline3 points;
    points.reserve(TURN_SEGMENTS + 1);
    for (int i = 0; i <= TURN_SEGMENTS; ++i) {
        const double t = TAU * i / TURN_SEGMENTS;
        points.push_back(center + u * (radius * std::cos(t)) + v * (radius * std::sin(t)));
    }
    return points;
}

// Three great circles: the equator and two meridians, in an orthonormal frame built from the
// surface's pole and reference axes (world axes when they are degenerate or parallel).
void addSphere(Builder& out, const RecordView& sphere) {
    const auto center = sphere.at(FIRST_FIELD, Kind::Position);
    const auto radius = sphere.at(FIRST_FIELD + 1, Kind::Double);
    const auto reference = sphere.at(FIRST_FIELD + 2, Kind::Vector);
    const auto pole = sphere.at(FIRST_FIELD + 3, Kind::Vector);
    if (!center || !radius || !reference || !pole) {
        return;
    }
    Vec3 a{1, 0, 0};
    Vec3 b{0, 0, 1};
    if (const auto up = unit(pole->vector())) {
        const Vec3 r = reference->vector();
        if (const auto across = unit(r - *up * dot(r, *up))) {
            a = *across;
            b = *up;
        }
    }
    const Vec3 c = cross(b, a);
    const double r = std::abs(radius->values[0]);
    out.add(circle(center->vector(), a, c, r));
    out.add(circle(center->vector(), a, b, r));
    out.add(circle(center->vector(), c, b, r));
}

} // namespace

std::vector<Polyline3> sabWireframe(std::span<const std::byte> data, const SabLimits& limits) {
    const Body body = readBody(data, limits);
    Builder out(limits);
    for (std::size_t i = 0; i < body.records.size(); ++i) {
        if (body.records[i].type == Type::Edge) {
            if (auto polyline = edgePolyline(RecordView(body, i))) {
                out.add(std::move(*polyline));
            }
        }
    }
    auto wires = std::move(out).result();
    if (!wires.empty()) {
        return wires;
    }
    Builder spheres(limits);
    for (std::size_t i = 0; i < body.records.size(); ++i) {
        if (body.records[i].type == Type::Sphere) {
            addSphere(spheres, RecordView(body, i));
        }
    }
    return std::move(spheres).result();
}

} // namespace cadpreview::dwghelper
