#include "render/vector/drawing.hpp"
#include "render/vector/projection.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

namespace cadpreview {
namespace {

constexpr double ROOT2 = std::numbers::sqrt2;
constexpr double ROOT6 = std::numbers::sqrt2 * std::numbers::sqrt3;

// The twelve edges of an axis-aligned box from (0,0,0) to (size,size,size), as wires of a
// solid (body) or as plain 3D lines.
std::vector<Wire> boxEdges(double size, bool body = true) {
    const double s = size;
    return {
        {{{0, 0, 0}, {s, 0, 0}, {s, s, 0}, {0, s, 0}}, true, body},
        {{{0, 0, s}, {s, 0, s}, {s, s, s}, {0, s, s}}, true, body},
        {{{0, 0, 0}, {0, 0, s}}, false, body},
        {{{s, 0, 0}, {s, 0, s}}, false, body},
        {{{s, s, 0}, {s, s, s}}, false, body},
        {{{0, s, 0}, {0, s, s}}, false, body},
    };
}

TEST(Projection, DrawingWithoutWiresIsUnchanged) {
    Drawing drawing;
    drawing.polylines.push_back({{{0, 0}, {10, 5}}, true});
    drawing.arcs.push_back({{3, 4}, 2, 0.5, 1.5});
    drawing.texts.push_back({"A", {1, 2}, 1.0, 0.25, 1.0, HAlign::Left, VAlign::Baseline});
    EXPECT_EQ(chooseView(drawing), View::Top);
    const Drawing flat = flatten(drawing);
    ASSERT_EQ(flat.polylines.size(), 1U);
    EXPECT_DOUBLE_EQ(flat.polylines[0].points[1].x, 10);
    EXPECT_TRUE(flat.polylines[0].closed);
    ASSERT_EQ(flat.arcs.size(), 1U);
    EXPECT_DOUBLE_EQ(flat.arcs[0].startAngle, 0.5);
    ASSERT_EQ(flat.texts.size(), 1U);
    EXPECT_DOUBLE_EQ(flat.texts[0].position.y, 2);
}

TEST(Projection, WiresOnOnePlaneUseTheTopView) {
    Drawing drawing;
    drawing.wires.push_back({{{0, 0, 5}, {10, 0, 5}, {10, 10, 5}}, true, true});
    EXPECT_EQ(chooseView(drawing), View::Top);
    const Drawing flat = flatten(drawing);
    EXPECT_TRUE(flat.wires.empty());
    ASSERT_EQ(flat.polylines.size(), 1U);
    EXPECT_TRUE(flat.polylines[0].closed);
    EXPECT_DOUBLE_EQ(flat.polylines[0].points[2].x, 10);
    EXPECT_DOUBLE_EQ(flat.polylines[0].points[2].y, 10);
}

TEST(Projection, ASolidUsesTheIsometricView) {
    Drawing drawing;
    drawing.wires = boxEdges(10);
    EXPECT_EQ(chooseView(drawing), View::Isometric);
    const Drawing flat = flatten(drawing);
    EXPECT_TRUE(flat.wires.empty());
    EXPECT_EQ(flat.polylines.size(), 6U);
}

TEST(Projection, BodyDepthBelowOnePercentKeepsTheTopView) {
    Drawing drawing;
    drawing.polylines.push_back({{{0, 0}, {100, 100}}, false});
    drawing.wires.push_back({{{0, 0, 0}, {50, 50, 0.5}}, false, true});
    EXPECT_EQ(chooseView(drawing), View::Top);
    drawing.wires.push_back({{{0, 0, 0}, {0, 0, 2}}, false, true});
    EXPECT_EQ(chooseView(drawing), View::Isometric);
}

TEST(Projection, LinesWithDepthAloneKeepTheTopView) {
    // 2D drawings often carry lines and faces with stray z values; only edges of solids,
    // surfaces and meshes switch the view.
    Drawing drawing;
    drawing.wires = boxEdges(10, false);
    EXPECT_EQ(chooseView(drawing), View::Top);
    drawing.wires.push_back({{{20, 20, 0}, {30, 30, 0}}, false, true});
    EXPECT_EQ(chooseView(drawing), View::Top); // a flat body edge adds no depth
    drawing.wires.push_back({{{20, 20, 0}, {20, 20, 0.5}}, false, true});
    EXPECT_EQ(chooseView(drawing), View::Isometric);
}

TEST(Projection, VeryManyArcsKeepTheTopView) {
    Drawing drawing;
    drawing.wires = boxEdges(10);
    drawing.arcs.assign(50'001, Arc{{0, 0}, 1, 0, 1});
    EXPECT_EQ(chooseView(drawing), View::Top);
    drawing.arcs.resize(50'000);
    EXPECT_EQ(chooseView(drawing), View::Isometric);
}

TEST(Projection, AVerticalBodyEdgeAloneIsIsometric) {
    Drawing drawing;
    drawing.wires.push_back({{{1, 1, 0}, {1, 1, 3}}, false, true});
    EXPECT_EQ(chooseView(drawing), View::Isometric);
}

TEST(Projection, IsometricAxesFollowTheSouthEastView) {
    const Point x = project({1, 0, 0}, View::Isometric);
    const Point y = project({0, 1, 0}, View::Isometric);
    const Point z = project({0, 0, 1}, View::Isometric);
    EXPECT_NEAR(x.x, 1 / ROOT2, 1e-12);
    EXPECT_NEAR(x.y, -1 / ROOT6, 1e-12);
    EXPECT_NEAR(y.x, 1 / ROOT2, 1e-12);
    EXPECT_NEAR(y.y, 1 / ROOT6, 1e-12);
    EXPECT_NEAR(z.x, 0, 1e-12);
    EXPECT_NEAR(z.y, 2 / ROOT6, 1e-12);
    const Point top = project({3, 4, 5}, View::Top);
    EXPECT_DOUBLE_EQ(top.x, 3);
    EXPECT_DOUBLE_EQ(top.y, 4);
}

TEST(Projection, IsometricTurnsArcsIntoPolylinesAndMovesText) {
    Drawing drawing;
    drawing.wires = boxEdges(10);
    drawing.arcs.push_back({{0, 0}, 5, 0, std::numbers::pi});
    drawing.texts.push_back({"A", {1, 0}, 1.0, 0.0, 1.0, HAlign::Left, VAlign::Baseline});
    const Drawing flat = flatten(drawing);
    EXPECT_TRUE(flat.arcs.empty());
    EXPECT_EQ(flat.polylines.size(), 7U);
    const Point moved = project({1, 0, 0}, View::Isometric);
    ASSERT_EQ(flat.texts.size(), 1U);
    EXPECT_DOUBLE_EQ(flat.texts[0].position.x, moved.x);
    EXPECT_DOUBLE_EQ(flat.texts[0].position.y, moved.y);
}

TEST(Projection, NonFiniteCoordinatesDoNotDecideTheView) {
    constexpr double NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
    constexpr double INF = std::numeric_limits<double>::infinity();
    Drawing drawing;
    drawing.wires.push_back({{{0, 0, 0}, {10, 10, NAN_VALUE}, {5, 5, INF}}, false, true});
    EXPECT_EQ(chooseView(drawing), View::Top);
    EXPECT_NO_THROW(static_cast<void>(flatten(drawing)));
}

TEST(Projection, TheTopViewKeepsTheSourceDrawingOrder) {
    Drawing drawing;
    drawing.polylines.push_back({{{0, 0}, {1, 0}}, false});
    drawing.polylines.push_back({{{2, 0}, {3, 0}}, false});
    drawing.wires.push_back({{{9, 9, 1}, {9, 9, 2}}, false, false, 1}); // drawn between them
    drawing.wires.push_back({{{7, 7, 1}, {7, 7, 2}}, false, false, 99}); // order past the end
    const Drawing flat = flatten(drawing);
    ASSERT_EQ(flat.polylines.size(), 4U);
    EXPECT_DOUBLE_EQ(flat.polylines[0].points[0].x, 0);
    EXPECT_DOUBLE_EQ(flat.polylines[1].points[0].x, 9);
    EXPECT_DOUBLE_EQ(flat.polylines[2].points[0].x, 2);
    EXPECT_DOUBLE_EQ(flat.polylines[3].points[0].x, 7);
}

TEST(Projection, WiresCountAsPrimitives) {
    Drawing drawing;
    EXPECT_EQ(drawing.primitiveCount(), 0U);
    drawing.wires.push_back({{{0, 0, 0}, {1, 1, 1}}, false});
    EXPECT_EQ(drawing.primitiveCount(), 1U);
}

} // namespace
} // namespace cadpreview
