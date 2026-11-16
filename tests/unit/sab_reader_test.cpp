// SAB input is built here from the tag layout recorded in notes/acis.md; no vendor data is
// used.
#include "sab_reader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

namespace cadpreview::dwghelper {
namespace {

constexpr double PI = std::numbers::pi;

class Sab {
public:
    Sab() {
        raw("ACIS BinaryFile");
        for (const std::int32_t value : {21500, 0, 1, 0}) {
            little(value);
        }
        text(0x07, "test");
        text(0x07, "ASM test");
        text(0x07, "date");
        number(25.4);
        number(1e-6);
        number(1e-10);
        name("asmheader");
        pointer(-1);
        integer(-1);
        text(0x07, "216");
        end();
    }

    // Records are numbered from 0 (the header record) in order.
    int point(Vec3 p) {
        header("point");
        position(0x13, p);
        return end();
    }
    int vertex(int point) {
        header("vertex");
        pointer(-1);
        integer(0);
        pointer(point);
        return end();
    }
    int straight(Vec3 root, Vec3 direction) {
        header("straight-curve");
        position(0x13, root);
        position(0x14, direction);
        flag(false);
        flag(false);
        return end();
    }
    int ellipse(Vec3 center, Vec3 normal, Vec3 major, double ratio) {
        header("ellipse-curve");
        position(0x13, center);
        position(0x14, normal);
        position(0x14, major);
        number(ratio);
        flag(false);
        flag(false);
        return end();
    }
    int sphere(Vec3 center, double radius, Vec3 reference = {1, 0, 0}, Vec3 pole = {0, 0, 1}) {
        header("sphere-surface");
        position(0x13, center);
        number(radius);
        position(0x14, reference);
        position(0x14, pole);
        flag(false);
        return end();
    }
    int edge(int start, double t0, int finish, double t1, int curve) {
        header("edge");
        pointer(start);
        number(t0);
        pointer(finish);
        number(t1);
        pointer(-1); // coedge
        pointer(curve);
        flag(false);
        text(0x07, "unknown");
        return end();
    }
    int segment(Vec3 a, Vec3 b) {
        const int curve = straight(a, {b.x - a.x, b.y - a.y, b.z - a.z});
        return edge(vertex(point(a)), 0, vertex(point(b)), 1, curve);
    }
    void finish() {
        for (const std::string_view part : {"End", "of", "ACIS"}) {
            text(0x0E, part);
        }
        text(0x0D, "data"); // observed without a record-end tag
    }
    void tag(std::uint8_t value) { bytes_.push_back(std::byte{value}); }

    [[nodiscard]] const std::vector<std::byte>& bytes() const { return bytes_; }
    std::vector<std::byte>& mutableBytes() { return bytes_; }

    void number(double value) {
        tag(0x06);
        little(value);
    }

private:
    // Entity records start with an attribute pointer, an integer and an owner pointer.
    void header(std::string_view recordName) {
        name(recordName);
        pointer(-1);
        integer(-1);
        pointer(-1);
    }
    void name(std::string_view full) {
        std::size_t start = 0;
        for (std::size_t dash = full.find('-'); dash != std::string_view::npos;
             dash = full.find('-', start)) {
            text(0x0E, full.substr(start, dash - start));
            start = dash + 1;
        }
        text(0x0D, full.substr(start));
    }
    void raw(std::string_view s) {
        for (const char c : s) {
            bytes_.push_back(static_cast<std::byte>(c));
        }
    }
    void text(std::uint8_t kind, std::string_view s) {
        tag(kind);
        bytes_.push_back(static_cast<std::byte>(s.size()));
        raw(s);
    }
    template <class T>
    void little(T value) {
        std::array<std::byte, sizeof(T)> b{};
        std::memcpy(b.data(), &value, sizeof(T));
        bytes_.insert(bytes_.end(), b.begin(), b.end());
    }
    void pointer(std::int32_t index) {
        tag(0x0C);
        little(index);
    }
    void integer(std::int32_t value) {
        tag(0x04);
        little(value);
    }
    void position(std::uint8_t kind, Vec3 v) {
        tag(kind);
        little(v.x);
        little(v.y);
        little(v.z);
    }
    void flag(bool value) { tag(value ? 0x0A : 0x0B); }
    int end() {
        tag(0x11);
        return records_++;
    }

    std::vector<std::byte> bytes_;
    int records_ = 0;
};

double distance(Vec3 a, Vec3 b) {
    return std::hypot(a.x - b.x, a.y - b.y, a.z - b.z);
}

TEST(SabReader, BoxEdgesBecomeSegments) {
    Sab sab;
    const std::array<Vec3, 8> c{{{0, 0, 0},
                                 {1, 0, 0},
                                 {1, 1, 0},
                                 {0, 1, 0},
                                 {0, 0, 1},
                                 {1, 0, 1},
                                 {1, 1, 1},
                                 {0, 1, 1}}};
    for (std::size_t i = 0; i < 4; ++i) {
        sab.segment(c.at(i), c.at((i + 1) % 4));
        sab.segment(c.at(i + 4), c.at(((i + 1) % 4) + 4));
        sab.segment(c.at(i), c.at(i + 4));
    }
    sab.finish();
    const auto wires = sabWireframe(sab.bytes());
    ASSERT_EQ(wires.size(), 12U);
    for (const Polyline3& wire : wires) {
        ASSERT_EQ(wire.size(), 2U);
        EXPECT_NEAR(distance(wire[0], wire[1]), 1.0, 1e-12);
    }
}

TEST(SabReader, EllipticalEdgeFollowsTheArc) {
    Sab sab;
    const int curve = sab.ellipse({0, 0, 0}, {0, 0, 1}, {2, 0, 0}, 1);
    sab.edge(sab.vertex(sab.point({2, 0, 0})), 0, sab.vertex(sab.point({0, 2, 0})), PI / 2,
             curve);
    sab.finish();
    const auto wires = sabWireframe(sab.bytes());
    ASSERT_EQ(wires.size(), 1U);
    const Polyline3& arc = wires[0];
    ASSERT_GT(arc.size(), 8U);
    EXPECT_NEAR(distance(arc.front(), {2, 0, 0}), 0, 1e-9);
    EXPECT_NEAR(distance(arc.back(), {0, 2, 0}), 0, 1e-9);
    for (const Vec3& p : arc) {
        EXPECT_NEAR(std::hypot(p.x, p.y), 2, 1e-9);
        EXPECT_GE(p.x, -1e-9);
        EXPECT_GE(p.y, -1e-9);
    }
}

TEST(SabReader, ReversedEdgeParametersAreNegatedOnTheCurve) {
    Sab sab;
    const int curve = sab.ellipse({0, 0, 0}, {0, 0, 1}, {2, 0, 0}, 0.5);
    // Start (0, 1, 0) is the curve at +pi/2, so the edge range -pi/2..0 runs on -t.
    sab.edge(sab.vertex(sab.point({0, 1, 0})), -PI / 2, sab.vertex(sab.point({2, 0, 0})), 0,
             curve);
    sab.finish();
    const auto wires = sabWireframe(sab.bytes());
    ASSERT_EQ(wires.size(), 1U);
    EXPECT_NEAR(distance(wires[0].front(), {0, 1, 0}), 0, 1e-9);
    EXPECT_NEAR(distance(wires[0].back(), {2, 0, 0}), 0, 1e-9);
    for (const Vec3& p : wires[0]) {
        EXPECT_GE(p.y, -1e-9); // the upper half, not the long way round
    }
}

TEST(SabReader, ClosedEdgeIsAFullEllipse) {
    Sab sab;
    const int curve = sab.ellipse({5, 5, 1}, {0, 0, 1}, {1, 0, 0}, 1);
    const int v = sab.vertex(sab.point({6, 5, 1}));
    sab.edge(v, 0, v, 2 * PI, curve);
    sab.finish();
    const auto wires = sabWireframe(sab.bytes());
    ASSERT_EQ(wires.size(), 1U);
    EXPECT_GE(wires[0].size(), 32U);
    EXPECT_NEAR(distance(wires[0].front(), wires[0].back()), 0, 1e-9);
    EXPECT_NEAR(distance(wires[0][wires[0].size() / 2], {4, 5, 1}), 0, 1e-2);
}

TEST(SabReader, UnknownCurvesUseTheChord) {
    Sab sab;
    const int curve = sab.point({9, 9, 9}); // not a curve record
    sab.edge(sab.vertex(sab.point({0, 0, 0})), 0, sab.vertex(sab.point({3, 4, 0})), 1, curve);
    sab.finish();
    const auto wires = sabWireframe(sab.bytes());
    ASSERT_EQ(wires.size(), 1U);
    EXPECT_EQ(wires[0].size(), 2U);
    EXPECT_NEAR(distance(wires[0][0], wires[0][1]), 5, 1e-12);
}

TEST(SabReader, SphereWithoutEdgesGivesThreeGreatCircles) {
    Sab sab;
    sab.sphere({1, 2, 3}, 4);
    sab.finish();
    const auto wires = sabWireframe(sab.bytes());
    ASSERT_EQ(wires.size(), 3U);
    for (const Polyline3& circle : wires) {
        for (const Vec3& p : circle) {
            EXPECT_NEAR(distance(p, {1, 2, 3}), 4, 1e-9);
        }
    }
}

TEST(SabReader, ParallelSphereAxesFallBackToWorldAxes) {
    Sab sab;
    sab.sphere({0, 0, 0}, 1, {0, 0, 1}, {0, 0, 2});
    sab.finish();
    const auto wires = sabWireframe(sab.bytes());
    ASSERT_EQ(wires.size(), 3U);
    for (const Polyline3& circle : wires) {
        for (const Vec3& p : circle) {
            EXPECT_NEAR(distance(p, {0, 0, 0}), 1, 1e-9);
        }
    }
}

TEST(SabReader, OverflowingCurvesAreDropped) {
    Sab sab;
    const int curve = sab.ellipse({0, 0, 0}, {0, 0, 1}, {1e308, 0, 0}, 1e308);
    sab.edge(sab.vertex(sab.point({1e308, 0, 0})), 0, sab.vertex(sab.point({0, 1e308, 0})),
             PI / 2, curve);
    sab.sphere({0, 0, 0}, 1e308);
    sab.finish();
    for (const Polyline3& wire : sabWireframe(sab.bytes())) {
        for (const Vec3& p : wire) {
            EXPECT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
        }
    }
}

TEST(SabReader, EdgesWithBadReferencesAreSkipped) {
    Sab sab;
    const int curve = sab.straight({0, 0, 0}, {1, 0, 0});
    const int good = sab.vertex(sab.point({0, 0, 0}));
    sab.edge(good, 0, 999, 1, curve);                 // out of range
    sab.edge(good, 0, -5, 1, curve);                  // negative
    sab.edge(good, 0, curve, 1, curve);               // not a vertex
    sab.edge(good, 0, sab.vertex(curve), 1, curve);   // vertex without a point
    sab.segment({0, 0, 0}, {0, 0, 1});
    sab.finish();
    EXPECT_EQ(sabWireframe(sab.bytes()).size(), 1U);
}

TEST(SabReader, StopsAtTheEndMarker) {
    Sab sab;
    sab.segment({0, 0, 0}, {1, 0, 0});
    sab.finish();
    sab.tag(0x42); // anything after the marker is not read
    EXPECT_EQ(sabWireframe(sab.bytes()).size(), 1U);
}

TEST(SabReader, ReadsTheSyntheticFixture) {
    // Written by tools/make_sab_fixtures.py: twelve box edges and one circular edge.
    std::ifstream in(CADPREVIEW_CORPUS_DIR "/synthetic/sab/box-with-hole.sab", std::ios::binary);
    ASSERT_TRUE(in);
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    const auto wires = sabWireframe(std::as_bytes(std::span(chars)));
    ASSERT_EQ(wires.size(), 13U);
    EXPECT_GT(wires.back().size(), 30U);
    EXPECT_NEAR(distance(wires.back()[wires.back().size() / 2], {3, 5, 4}), 0, 1e-2);
}

TEST(SabReader, RejectsDataThatIsNotSab) {
    const std::string text = "ACIS TextFile 21500 0 1 0";
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    EXPECT_THROW(static_cast<void>(sabWireframe(bytes)), SabError);
    EXPECT_THROW(static_cast<void>(sabWireframe({})), SabError);
}

TEST(SabReader, RejectsTruncatedData) {
    Sab sab;
    sab.segment({0, 0, 0}, {1, 1, 1});
    const auto& full = sab.bytes();
    for (std::size_t cut = 1; cut < 24; ++cut) {
        const std::span<const std::byte> shorter(full.data(), full.size() - cut);
        EXPECT_THROW(static_cast<void>(sabWireframe(shorter)), SabError) << cut;
    }
}

TEST(SabReader, RejectsUnknownTagsAndNonFiniteNumbers) {
    Sab unknown;
    unknown.tag(0x42);
    EXPECT_THROW(static_cast<void>(sabWireframe(unknown.bytes())), SabError);

    Sab infinite;
    infinite.number(std::numeric_limits<double>::infinity());
    EXPECT_THROW(static_cast<void>(sabWireframe(infinite.bytes())), SabError);
}

TEST(SabReader, EnforcesLimits) {
    Sab sab;
    for (int i = 0; i < 10; ++i) {
        sab.segment({0, 0, static_cast<double>(i)}, {1, 0, static_cast<double>(i)});
    }
    sab.finish();
    EXPECT_THROW(static_cast<void>(sabWireframe(sab.bytes(), {1024, 1'000'000, 2'000'000})),
                 SabError);
    EXPECT_THROW(static_cast<void>(sabWireframe(sab.bytes(), {1U << 20U, 20, 2'000'000})),
                 SabError);
    EXPECT_THROW(static_cast<void>(sabWireframe(sab.bytes(), {1U << 20U, 1'000'000, 10})),
                 SabError);
    EXPECT_THROW(
        static_cast<void>(sabWireframe(sab.bytes(), {1U << 20U, 1'000'000, 2'000'000, 30})),
        SabError);
    EXPECT_EQ(sabWireframe(sab.bytes()).size(), 10U);
}

} // namespace
} // namespace cadpreview::dwghelper
