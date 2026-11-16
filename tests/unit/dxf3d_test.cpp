// 3D content in DXF: lines and faces with depth, 3D polylines, polygon and polyface meshes, and
// solids that tier 4 cannot draw. Group codes follow the published DXF Reference.
#include "common/errors.hpp"
#include "common/log.hpp"
#include "dxf_text.hpp"
#include "formats/dxf/document.hpp"
#include "formats/dxf/entities.hpp"
#include "tiers/pipeline.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <vector>

namespace cadpreview::dxf {
namespace {

using test::bytesOf;
using test::concat;
using test::file;
using test::num;
using test::Pairs;
using test::section;

Drawing drawingOf(const std::string& text, const ConvertLimits& limits = {}) {
    const auto data = bytesOf(text);
    return toDrawing(readDocument(data, Parts::Everything), limits);
}

Pairs line3(Point3 a, Point3 b) {
    return {{0, "LINE"},    {8, "0"},       {10, num(a.x)}, {20, num(a.y)},
            {30, num(a.z)}, {11, num(b.x)}, {21, num(b.y)}, {31, num(b.z)}};
}

Pairs vertex(Point3 p, int flags) {
    return {
        {0, "VERTEX"}, {10, num(p.x)}, {20, num(p.y)}, {30, num(p.z)}, {70, std::to_string(flags)}};
}

Pairs faceRecord(std::array<int, 4> indices) {
    return {{0, "VERTEX"},
            {10, "0"},
            {20, "0"},
            {30, "0"},
            {70, "128"},
            {71, std::to_string(indices[0])},
            {72, std::to_string(indices[1])},
            {73, std::to_string(indices[2])},
            {74, std::to_string(indices[3])}};
}

Pairs polylineHead(int flags, int m = 0, int n = 0) {
    return {{0, "POLYLINE"},
            {8, "0"},
            {66, "1"},
            {70, std::to_string(flags)},
            {71, std::to_string(m)},
            {72, std::to_string(n)}};
}

Pairs seqend() {
    return {{0, "SEQEND"}};
}

// A unit square at z = 0 and z = 1 alternately, as polyface coordinate vertices.
Pairs squareVertices() {
    return concat({vertex({0, 0, 0}, 192), vertex({1, 0, 1}, 192), vertex({1, 1, 0}, 192),
                   vertex({0, 1, 1}, 192)});
}

TEST(Entities3d, LinesWithDepthBecomeWires) {
    const Drawing drawing = drawingOf(file(
        {section("ENTITIES", concat({line3({0, 0, 0}, {0, 0, 5}), line3({0, 0, 0}, {4, 0, 0})}))}));
    ASSERT_EQ(drawing.wires.size(), 1U);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[1].z, 5);
    EXPECT_FALSE(drawing.wires[0].body);
    EXPECT_EQ(drawing.polylines.size(), 1U); // a flat line stays 2D, as before
}

TEST(Entities3d, ThreeDPolylineKeepsDepthAndClosure) {
    const Drawing drawing = drawingOf(file(
        {section("ENTITIES", concat({polylineHead(9 /* 3D, closed */), vertex({0, 0, 0}, 32),
                                     vertex({1, 0, 2}, 32), vertex({1, 1, 4}, 32), seqend()}))}));
    ASSERT_EQ(drawing.wires.size(), 1U);
    EXPECT_TRUE(drawing.wires[0].closed);
    ASSERT_EQ(drawing.wires[0].points.size(), 3U);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[2].z, 4);
    EXPECT_TRUE(drawing.polylines.empty());
}

TEST(Entities3d, PolygonMeshDrawsRowsAndColumns) {
    Pairs mesh = polylineHead(48, 2, 3); // polygon mesh (16), closed in the N direction (32)
    for (int m = 0; m < 2; ++m) {
        for (int n = 0; n < 3; ++n) {
            mesh = concat({mesh, vertex({static_cast<double>(n), static_cast<double>(m),
                                         static_cast<double>(m + n)},
                                        64)});
        }
    }
    const Drawing drawing = drawingOf(file({section("ENTITIES", concat({mesh, seqend()}))}));
    ASSERT_EQ(drawing.wires.size(), 5U); // 2 rows of 3 points, 3 columns of 2 points
    EXPECT_TRUE(drawing.wires[0].body);
    EXPECT_EQ(drawing.wires[0].points.size(), 3U);
    EXPECT_TRUE(drawing.wires[0].closed);
    EXPECT_EQ(drawing.wires[2].points.size(), 2U);
    EXPECT_FALSE(drawing.wires[2].closed);
}

TEST(Entities3d, PolygonMeshWithWrongVertexCountIsSkipped) {
    const Drawing drawing = drawingOf(
        file({section("ENTITIES", concat({polylineHead(16, 30000, 30000), vertex({0, 0, 0}, 64),
                                          vertex({1, 0, 1}, 64), seqend()}))}));
    EXPECT_TRUE(drawing.wires.empty());
    EXPECT_TRUE(drawing.polylines.empty());
}

TEST(Entities3d, PolyfaceMeshDrawsVisibleFaceEdges) {
    const Drawing drawing =
        drawingOf(file({section("ENTITIES", concat({polylineHead(64, 4, 1), squareVertices(),
                                                    faceRecord({1, 2, -3, 4}), seqend()}))}));
    // Edges 1-2-3 (joined) and 4-1; the edge starting at vertex 3 is hidden (negative index).
    ASSERT_EQ(drawing.wires.size(), 2U);
    EXPECT_TRUE(drawing.wires[0].body);
    ASSERT_EQ(drawing.wires[0].points.size(), 3U);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[1].x, 1);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[2].y, 1);
    ASSERT_EQ(drawing.wires[1].points.size(), 2U);
    EXPECT_DOUBLE_EQ(drawing.wires[1].points[0].y, 1); // vertex 4
    EXPECT_DOUBLE_EQ(drawing.wires[1].points[1].y, 0); // vertex 1
}

TEST(Entities3d, PolyfaceFacesWithBadIndicesAreSkipped) {
    const Drawing drawing = drawingOf(file({section(
        "ENTITIES", concat({polylineHead(64, 4, 3), squareVertices(), faceRecord({1, 9, 3, 0}),
                            faceRecord({-40000, 2, 3, 0}), faceRecord({1, 2, 3, 0}), seqend()}))}));
    ASSERT_EQ(drawing.wires.size(), 1U); // only the last triangle, as one closed chain
    EXPECT_EQ(drawing.wires[0].points.size(), 4U);
}

TEST(Entities3d, ConsecutiveTwoVertexFacesJoinIntoOneWire) {
    // The DWG helper writes each solid edge this way: a polyface with one face per segment.
    const Drawing drawing = drawingOf(file({section(
        "ENTITIES", concat({polylineHead(64, 4, 3), squareVertices(), faceRecord({1, 2, 0, 0}),
                            faceRecord({2, 3, 0, 0}), faceRecord({3, 4, 0, 0}), seqend()}))}));
    ASSERT_EQ(drawing.wires.size(), 1U);
    EXPECT_EQ(drawing.wires[0].points.size(), 4U);
}

TEST(Entities3d, FacesWithDepthBecomeWires) {
    const Pairs flat{{0, "3DFACE"}, {10, "0"}, {20, "0"}, {11, "1"}, {21, "0"},
                     {12, "1"},     {22, "1"}, {13, "0"}, {23, "1"}};
    const Pairs tilted{{0, "3DFACE"}, {10, "0"}, {20, "0"}, {30, "0"}, {11, "1"}, {21, "0"},
                       {31, "1"},     {12, "1"}, {22, "1"}, {32, "1"}, {13, "0"}, {23, "1"}};
    const Drawing drawing = drawingOf(file({section("ENTITIES", concat({flat, tilted}))}));
    EXPECT_EQ(drawing.polylines.size(), 1U);
    ASSERT_EQ(drawing.wires.size(), 1U);
    EXPECT_TRUE(drawing.wires[0].closed);
}

TEST(Entities3d, WiresInBlocksMoveWithTheInsert) {
    const std::string text = file({
        section("BLOCKS", concat({{{0, "BLOCK"}, {2, "Post"}, {10, "0"}, {20, "0"}},
                                  line3({0, 0, 0}, {0, 0, 3}),
                                  {{0, "ENDBLK"}}})),
        section("ENTITIES", {{0, "INSERT"}, {2, "Post"}, {10, "10"}, {20, "5"}}),
    });
    const Drawing drawing = drawingOf(text);
    ASSERT_EQ(drawing.wires.size(), 1U);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[1].x, 10);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[1].y, 5);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[1].z, 3);
}

TEST(Entities3d, SolidsAndSurfacesAreCountedAsUndrawn) {
    Pairs bodies;
    for (const char* type :
         {"3DSOLID", "REGION", "BODY", "PLANESURFACE", "EXTRUDEDSURFACE", "MESH"}) {
        bodies = concat({bodies, {{0, type}, {8, "0"}}});
    }
    bodies = concat({bodies, {{0, "3DSOLID"}, {8, "0"}, {60, "1"}}}); // invisible: not counted
    const Drawing drawing = drawingOf(file({section("ENTITIES", bodies)}));
    EXPECT_EQ(drawing.undrawnBodies, 6U);
    EXPECT_EQ(drawing.primitiveCount(), 0U);
}

TEST(Entities3d, MeshPointsCountTowardTheWorkLimit) {
    Pairs mesh = polylineHead(16, 10, 10);
    for (int i = 0; i < 100; ++i) {
        mesh = concat({mesh, vertex({static_cast<double>(i), 0, static_cast<double>(i)}, 64)});
    }
    const std::string text = file({section("ENTITIES", concat({mesh, seqend()}))});
    EXPECT_THROW(static_cast<void>(drawingOf(text, ConvertLimits{150, 16})), FormatError);
    EXPECT_NO_THROW(static_cast<void>(drawingOf(text)));
}

TEST(Entities3d, VerticesAreChargedEvenWhenNothingIsDrawn) {
    // A block whose polyline draws nothing (vertices with no usable flags), inserted as a
    // large array: every placement re-reads the vertices, so each must cost work.
    Pairs vertices = polylineHead(64, 0, 0);
    for (int i = 0; i < 1000; ++i) {
        vertices = concat({vertices, vertex({0, 0, 0}, 0)});
    }
    const std::string text = file({
        section("BLOCKS", concat({{{0, "BLOCK"}, {2, "Empty"}, {10, "0"}, {20, "0"}},
                                  vertices,
                                  seqend(),
                                  {{0, "ENDBLK"}}})),
        section("ENTITIES", {{0, "INSERT"}, {2, "Empty"}, {70, "1000"}, {71, "1000"}}),
    });
    EXPECT_THROW(static_cast<void>(drawingOf(text)), FormatError);
}

TEST(Dxf3dTier, OnlySolidsGiveAClearReason) {
    const auto data = bytesOf(file({section("ENTITIES", {{0, "3DSOLID"}, {8, "0"}})}));
    const Preview preview = renderMemory(data, {64, Tier::Vector}, defaultRegistry(), Logger{});
    EXPECT_FALSE(preview.ok);
    EXPECT_NE(preview.reasonText.find("3D solids or surfaces"), std::string::npos)
        << preview.reasonText;
}

TEST(Dxf3dTier, AWireframeOfLinesRenders) {
    Pairs box;
    for (const double z : {0.0, 1.0}) {
        box = concat({box, line3({0, 0, z}, {1, 0, z}), line3({1, 0, z}, {1, 1, z}),
                      line3({1, 1, z}, {0, 1, z}), line3({0, 1, z}, {0, 0, z})});
    }
    for (const auto& [x, y] :
         std::array<std::array<double, 2>, 4>{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}}) {
        box = concat({box, line3({x, y, 0}, {x, y, 1})});
    }
    const auto data = bytesOf(file({section("ENTITIES", box)}));
    const Preview preview = renderMemory(data, {64, Tier::Vector}, defaultRegistry(), Logger{});
    EXPECT_TRUE(preview.ok) << preview.reasonText;
    EXPECT_EQ(preview.tier, Tier::Vector);
}

} // namespace
} // namespace cadpreview::dxf
