#include "render/vector/geometry.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

namespace cadpreview {
namespace {

constexpr double PI = std::numbers::pi;

TEST(Transform, InsertMovesBaseToInsertionPoint) {
    const Transform t = Transform::insert({10, 20}, 2, 2, PI / 2, {1, 0});
    const Point base = t.apply({1, 0});
    EXPECT_NEAR(base.x, 10, 1e-9);
    EXPECT_NEAR(base.y, 20, 1e-9);
    const Point p = t.apply({2, 0}); // one unit right of base -> two units up after rotation
    EXPECT_NEAR(p.x, 10, 1e-9);
    EXPECT_NEAR(p.y, 22, 1e-9);
    EXPECT_TRUE(t.isSimilarity());
    EXPECT_NEAR(t.uniformScale(), 2, 1e-9);
    EXPECT_NEAR(t.rotation(), PI / 2, 1e-9);
}

TEST(Transform, NonUniformAndMirroredAreNotSimilarities) {
    EXPECT_FALSE(Transform::insert({}, 2, 1, 0, {}).isSimilarity());
    EXPECT_FALSE(Transform::insert({}, -1, 1, 0, {}).isSimilarity());
}

TEST(Transform, ComposesInOrder) {
    const Transform inner = Transform::insert({1, 0}, 1, 1, 0, {});
    const Transform outer = Transform::insert({0, 0}, 3, 3, 0, {});
    const Point p = outer.after(inner).apply({0, 0});
    EXPECT_NEAR(p.x, 3, 1e-9);
}

TEST(Bulge, SemicircleCounterClockwiseGoesBelowTheChord) {
    std::vector<Point> points;
    appendBulgeSegment(points, {0, 0}, {2, 0}, 1.0);
    ASSERT_GT(points.size(), 3U);
    const Point mid = points[points.size() / 2];
    EXPECT_NEAR(mid.x, 1, 0.1);
    EXPECT_NEAR(mid.y, -1, 0.05);
}

TEST(Bulge, ClockwiseGoesAbove) {
    std::vector<Point> points;
    appendBulgeSegment(points, {0, 0}, {2, 0}, -1.0);
    const Point mid = points[points.size() / 2];
    EXPECT_NEAR(mid.y, 1, 0.05);
}

TEST(Bulge, ZeroIsAStraightSegment) {
    std::vector<Point> points;
    appendBulgeSegment(points, {0, 0}, {2, 0}, 0.0);
    EXPECT_EQ(points.size(), 1U);
}

TEST(Ellipse, FullEllipseHitsAxisEnds) {
    const auto points = ellipsePoints({0, 0}, {4, 0}, 0.5, 0, 2 * PI);
    double maxX = 0;
    double maxY = 0;
    for (const Point& p : points) {
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
    EXPECT_NEAR(maxX, 4, 1e-9);
    EXPECT_NEAR(maxY, 2, 0.01);
}

TEST(Spline, ClampedQuadraticPassesThroughEnds) {
    const std::vector<double> knots{0, 0, 0, 1, 1, 1};
    const std::vector<Point> controls{{0, 0}, {1, 2}, {2, 0}};
    const auto points = splinePoints(2, knots, controls, {});
    ASSERT_GT(points.size(), 3U);
    EXPECT_NEAR(points.front().x, 0, 1e-9);
    EXPECT_NEAR(points.back().x, 2, 1e-9);
    EXPECT_NEAR(points[points.size() / 2].y, 1, 0.01); // peak of the quadratic is half the control
}

TEST(Spline, BadKnotsFallBackToTheControlPolygon) {
    const std::vector<Point> controls{{0, 0}, {1, 1}, {2, 0}};
    EXPECT_EQ(splinePoints(2, std::vector<double>{0, 1}, controls, {}).size(), 3U);
    EXPECT_EQ(splinePoints(2, std::vector<double>{1, 0, 0, 0, 0, 0}, controls, {}).size(), 3U);
}

} // namespace
} // namespace cadpreview
