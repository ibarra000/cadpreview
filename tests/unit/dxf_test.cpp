#include "common/errors.hpp"
#include "common/log.hpp"
#include "formats/dxf/document.hpp"
#include "formats/dxf/entities.hpp"
#include "formats/dxf/tokenizer.hpp"
#include "tiers/pipeline.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace cadpreview::dxf {
namespace {

using Pairs = std::vector<std::pair<int, std::string>>;

std::vector<std::byte> bytesOf(const std::string& text) {
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

// Builds ASCII DXF text from group pairs, as laid out in the published DXF Reference.
std::string dxfText(const Pairs& pairs) {
    std::string text;
    for (const auto& [code, value] : pairs) {
        text += std::to_string(code) + "\n" + value + "\n";
    }
    return text;
}

Pairs section(const std::string& name, const Pairs& body) {
    Pairs pairs{{0, "SECTION"}, {2, name}};
    pairs.insert(pairs.end(), body.begin(), body.end());
    pairs.emplace_back(0, "ENDSEC");
    return pairs;
}

std::string file(std::initializer_list<Pairs> sections) {
    Pairs all;
    for (const Pairs& s : sections) {
        all.insert(all.end(), s.begin(), s.end());
    }
    all.emplace_back(0, "EOF");
    return dxfText(all);
}

Pairs line(double x1, double y1, double x2, double y2, const std::string& layer = "0") {
    return {{0, "LINE"},
            {8, layer},
            {10, std::to_string(x1)},
            {20, std::to_string(y1)},
            {11, std::to_string(x2)},
            {21, std::to_string(y2)}};
}

Pairs concat(std::initializer_list<Pairs> parts) {
    Pairs all;
    for (const Pairs& p : parts) {
        all.insert(all.end(), p.begin(), p.end());
    }
    return all;
}

Drawing drawingOf(const std::string& text, const ConvertLimits& limits = {}) {
    const auto data = bytesOf(text);
    return toDrawing(readDocument(data, Parts::Everything), limits);
}

TEST(Tokenizer, ReadsPairsWithCrlfBomAndComments) {
    const auto data =
        bytesOf("\xEF\xBB\xBF  0\r\nSECTION\r\n999\r\na comment\r\n  2\r\nHEADER  \r\n");
    Tokenizer tokenizer(data);
    const auto first = tokenizer.next();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->code, 0);
    EXPECT_EQ(first->value, "SECTION");
    const auto second = tokenizer.next();
    ASSERT_TRUE(second);
    EXPECT_EQ(second->code, 2);
    EXPECT_EQ(second->value, "HEADER");
    EXPECT_FALSE(tokenizer.next());
}

TEST(Tokenizer, RejectsMalformedGroups) {
    const auto badCode = bytesOf("abc\nvalue\n");
    EXPECT_THROW(static_cast<void>(Tokenizer(badCode).next()), FormatError);
    const auto missingValue = bytesOf("0\n");
    EXPECT_THROW(static_cast<void>(Tokenizer(missingValue).next()), FormatError);
    const auto hugeLine = bytesOf("0\n" + std::string(70000, 'x') + "\n");
    EXPECT_THROW(static_cast<void>(Tokenizer(hugeLine).next()), FormatError);
}

TEST(Tokenizer, RecognisesFilesAndVersion) {
    const std::string text = file({section("HEADER", {{9, "$ACADVER"}, {1, "AC1027"}})});
    EXPECT_TRUE(looksLikeAsciiDxf(bytesOf(text)));
    EXPECT_EQ(headerVersion(bytesOf(text)), "AC1027");
    EXPECT_EQ(headerVersion(bytesOf(text.substr(0, 20))), "");
    EXPECT_FALSE(looksLikeAsciiDxf(bytesOf("hello\nworld\n")));
    EXPECT_TRUE(isBinaryDxf(bytesOf(std::string("AutoCAD Binary DXF\r\n\x1A\0", 22))));
    EXPECT_EQ(toDouble(" 1.5 "), 1.5);
    EXPECT_FALSE(toDouble("1.5x"));
    EXPECT_EQ(toInteger("+12"), 12);
}

TEST(Tokenizer, NumbersMustBeFinite) {
    EXPECT_FALSE(toDouble("nan"));
    EXPECT_FALSE(toDouble("inf"));
    EXPECT_FALSE(toDouble("-Infinity"));
    EXPECT_FALSE(toDouble("1e999"));
    EXPECT_EQ(toDouble("+1.5"), 1.5);
}

TEST(Document, ReadsLayersBlocksEntitiesAndThumbnail) {
    const std::string text = file({
        section("HEADER", {{9, "$ACADVER"}, {1, "AC1032"}}),
        section("TABLES", {{0, "TABLE"},
                           {2, "LAYER"},
                           {0, "LAYER"},
                           {2, "Frozen"},
                           {70, "1"},
                           {62, "7"},
                           {0, "LAYER"},
                           {2, "off"},
                           {70, "0"},
                           {62, "-3"},
                           {0, "LAYER"},
                           {2, "Shown"},
                           {70, "0"},
                           {62, "1"},
                           {0, "ENDTAB"}}),
        section("BLOCKS", concat({{{0, "BLOCK"}, {2, "Box"}, {10, "1"}, {20, "2"}},
                                  line(0, 0, 1, 0),
                                  {{0, "ENDBLK"}}})),
        section("ENTITIES", line(0, 0, 5, 5)),
        section("THUMBNAILIMAGE", {{90, "3"}, {310, "0aFF"}, {310, "10"}}),
    });
    const Document document = readDocument(bytesOf(text), Parts::Everything);
    EXPECT_EQ(document.version, "AC1032");
    EXPECT_TRUE(document.hiddenLayers.contains("FROZEN"));
    EXPECT_TRUE(document.hiddenLayers.contains("OFF"));
    EXPECT_FALSE(document.hiddenLayers.contains("SHOWN"));
    ASSERT_TRUE(document.blocks.contains("BOX"));
    EXPECT_DOUBLE_EQ(document.blocks.at("BOX").base.y, 2);
    EXPECT_EQ(document.entities.size(), 1U);
    EXPECT_EQ(document.thumbnail,
              (std::vector<std::byte>{std::byte{0x0A}, std::byte{0xFF}, std::byte{0x10}}));
}

TEST(Document, RejectsBadThumbnailHexAndLimits) {
    const std::string badHex = file({section("THUMBNAILIMAGE", {{90, "1"}, {310, "zz"}})});
    EXPECT_THROW(static_cast<void>(readDocument(bytesOf(badHex), Parts::HeaderAndThumbnail)),
                 FormatError);
    const std::string many =
        file({section("ENTITIES", concat({line(0, 0, 1, 1), line(1, 1, 2, 2)}))});
    ReadLimits limits;
    limits.maxEntities = 1;
    EXPECT_THROW(static_cast<void>(readDocument(bytesOf(many), Parts::Everything, limits)),
                 FormatError);
}

TEST(Entities, ConvertsBasicShapes) {
    const std::string text = file({section(
        "ENTITIES",
        concat({line(0, 0, 10, 0),
                {{0, "CIRCLE"}, {10, "5"}, {20, "5"}, {40, "2"}},
                {{0, "ARC"}, {10, "0"}, {20, "0"}, {40, "1"}, {50, "0"}, {51, "90"}},
                {{0, "LWPOLYLINE"},
                 {90, "2"},
                 {70, "0"},
                 {10, "0"},
                 {20, "0"},
                 {42, "1"},
                 {10, "2"},
                 {20, "0"}},
                {{0, "ELLIPSE"}, {10, "0"}, {20, "0"}, {11, "4"}, {21, "0"}, {40, "0.5"}}}))});
    const Drawing drawing = drawingOf(text);
    EXPECT_EQ(drawing.arcs.size(), 2U);
    ASSERT_EQ(drawing.polylines.size(), 3U);
    EXPECT_NEAR(drawing.arcs[1].endAngle, std::numbers::pi / 2, 1e-9);
    EXPECT_GT(drawing.polylines[1].points.size(), 3U); // bulge expanded into an arc
}

TEST(Entities, HeavyPolylineWithVertices) {
    const std::string text = file({section("ENTITIES", {{0, "POLYLINE"},
                                                        {66, "1"},
                                                        {70, "1"},
                                                        {0, "VERTEX"},
                                                        {10, "0"},
                                                        {20, "0"},
                                                        {0, "VERTEX"},
                                                        {10, "1"},
                                                        {20, "0"},
                                                        {0, "VERTEX"},
                                                        {10, "1"},
                                                        {20, "1"},
                                                        {0, "SEQEND"}})});
    const Drawing drawing = drawingOf(text);
    ASSERT_EQ(drawing.polylines.size(), 1U);
    EXPECT_TRUE(drawing.polylines[0].closed);
    EXPECT_EQ(drawing.polylines[0].points.size(), 3U);
}

TEST(Entities, ExpandsNestedBlocksWithTransforms) {
    const std::string text = file({
        section("BLOCKS", concat({{{0, "BLOCK"}, {2, "Inner"}, {10, "0"}, {20, "0"}},
                                  line(0, 0, 1, 0),
                                  {{0, "ENDBLK"}},
                                  {{0, "BLOCK"},
                                   {2, "Outer"},
                                   {10, "0"},
                                   {20, "0"},
                                   {0, "INSERT"},
                                   {2, "INNER"},
                                   {10, "10"},
                                   {20, "0"},
                                   {0, "ENDBLK"}}})),
        section("ENTITIES", {{0, "INSERT"},
                             {2, "outer"},
                             {10, "0"},
                             {20, "100"},
                             {41, "2"},
                             {42, "2"},
                             {50, "90"}}),
    });
    const Drawing drawing = drawingOf(text);
    ASSERT_EQ(drawing.polylines.size(), 1U);
    const Point end = drawing.polylines[0].points[1];
    // Inner line (0,0)-(1,0) shifted to x=10, then scaled 2 and rotated 90 degrees at (0,100).
    EXPECT_NEAR(end.x, 0, 1e-9);
    EXPECT_NEAR(end.y, 122, 1e-9);
}

TEST(Entities, ArraysCountTowardTheLimit) {
    const std::string text = file({
        section("BLOCKS", concat({{{0, "BLOCK"}, {2, "B"}}, line(0, 0, 1, 0), {{0, "ENDBLK"}}})),
        section("ENTITIES",
                {{0, "INSERT"}, {2, "B"}, {70, "30"}, {71, "40"}, {44, "2"}, {45, "2"}}),
    });
    EXPECT_EQ(drawingOf(text).polylines.size(), 1200U);
    ConvertLimits limits;
    limits.maxWork = 100;
    EXPECT_THROW(static_cast<void>(drawingOf(text, limits)), FormatError);
}

TEST(Entities, HugeArrayCountsDoNotWrapAroundTheLimit) {
    // 2^32 x 2^32 copies wraps to 0 in 64-bit arithmetic.
    const std::string text = file({
        section("BLOCKS", concat({{{0, "BLOCK"}, {2, "B"}}, line(0, 0, 1, 0), {{0, "ENDBLK"}}})),
        section("ENTITIES", {{0, "INSERT"}, {2, "B"}, {70, "4294967296"}, {71, "4294967296"}}),
    });
    EXPECT_THROW(static_cast<void>(drawingOf(text)), FormatError);
}

TEST(Entities, BlockFanOutIsChargedEvenWhenNothingIsDrawn) {
    // Each level inserts the one below 100 times; the innermost block draws nothing. Without a
    // charge per visited record this is 10^12 visits.
    Pairs blocks{{0, "BLOCK"}, {2, "B0"}, {0, "UNKNOWN_THING"}, {0, "ENDBLK"}};
    for (int level = 1; level <= 6; ++level) {
        const Pairs next{{0, "BLOCK"},  {2, "B" + std::to_string(level)},
                         {0, "INSERT"}, {2, "B" + std::to_string(level - 1)},
                         {70, "10"},    {71, "10"},
                         {0, "ENDBLK"}};
        blocks.insert(blocks.end(), next.begin(), next.end());
    }
    const std::string text =
        file({section("BLOCKS", blocks), section("ENTITIES", {{0, "INSERT"}, {2, "B6"}})});
    EXPECT_THROW(static_cast<void>(drawingOf(text)), FormatError);
}

TEST(Entities, PointsAndTextBytesCountTowardTheLimit) {
    Pairs polyline{{0, "LWPOLYLINE"}};
    for (int i = 0; i < 50; ++i) {
        polyline.emplace_back(10, std::to_string(i));
        polyline.emplace_back(20, "0");
    }
    ConvertLimits limits;
    limits.maxWork = 20;
    EXPECT_THROW(static_cast<void>(drawingOf(file({section("ENTITIES", polyline)}), limits)),
                 FormatError);
    const std::string longText = file({section(
        "ENTITIES", {{0, "TEXT"}, {10, "0"}, {20, "0"}, {40, "1"}, {1, std::string(1000, 'x')}})});
    EXPECT_THROW(static_cast<void>(drawingOf(longText, limits)), FormatError);
}

TEST(Entities, SelfReferencingBlocksStopAtTheDepthLimit) {
    const std::string text = file({
        section("BLOCKS", {{0, "BLOCK"}, {2, "Loop"}, {0, "INSERT"}, {2, "Loop"}, {0, "ENDBLK"}}),
        section("ENTITIES", {{0, "INSERT"}, {2, "Loop"}}),
    });
    EXPECT_THROW(static_cast<void>(drawingOf(text)), FormatError);
}

TEST(Entities, SkipsHiddenLayersAndFallsBackToPaperSpace) {
    const std::string hidden = file({
        section("TABLES", {{0, "TABLE"}, {0, "LAYER"}, {2, "H"}, {70, "1"}, {0, "ENDTAB"}}),
        section("ENTITIES", concat({line(0, 0, 1, 1, "h"), line(0, 0, 2, 2)})),
    });
    EXPECT_EQ(drawingOf(hidden).polylines.size(), 1U);
    const std::string paper = file({section("ENTITIES", concat({line(0, 0, 1, 1), {{67, "1"}}}))});
    EXPECT_EQ(drawingOf(paper).polylines.size(), 1U);
}

TEST(Entities, ConvertsTextAndMtext) {
    const std::string text =
        file({section("ENTITIES", {{0, "TEXT"},
                                   {10, "0"},
                                   {20, "0"},
                                   {11, "5"},
                                   {21, "5"},
                                   {40, "2"},
                                   {1, "%%c10 %%d"},
                                   {72, "1"},
                                   {73, "2"},
                                   {0, "MTEXT"},
                                   {10, "0"},
                                   {20, "0"},
                                   {40, "1"},
                                   {71, "7"},
                                   {1, "{\\fArial|b1;Line one}\\PLine two"}})});
    const Drawing drawing = drawingOf(text);
    ASSERT_EQ(drawing.texts.size(), 3U);
    EXPECT_EQ(drawing.texts[0].text, "\xC3\x98"
                                     "10 \xC2\xB0");
    EXPECT_EQ(drawing.texts[0].hAlign, HAlign::Center);
    EXPECT_EQ(drawing.texts[0].vAlign, VAlign::Middle);
    EXPECT_DOUBLE_EQ(drawing.texts[0].position.x, 5);
    EXPECT_EQ(drawing.texts[1].text, "Line one");
    EXPECT_EQ(drawing.texts[2].text, "Line two");
    EXPECT_GT(drawing.texts[1].position.y, drawing.texts[2].position.y);
}

TEST(PlainText, HandlesCodes) {
    EXPECT_EQ(plainText("50%%%", false), "50%");
    EXPECT_EQ(plainText("\\U+00e9t\\U+00E9", false), "\xC3\xA9t\xC3\xA9");
    EXPECT_EQ(plainText("\\S1^2;\\~in", true), "1/2 in");
    EXPECT_EQ(plainText("a\\\\b\\{c\\}", true), "a\\b{c}");
    EXPECT_EQ(plainText("\\H2.5x;big", true), "big");
}

TEST(DxfTiers, ThumbnailThenVectorThroughThePipeline) {
    // 1x1 white DIB: BITMAPINFOHEADER (40 bytes) + one 24-bit pixel padded to 4 bytes.
    const std::string dib =
        "28000000010000000100000001001800000000000000000000000000000000000000000000000000"
        "FFFFFF00";
    const std::string withThumbnail = file({section("ENTITIES", line(0, 0, 1, 1)),
                                            section("THUMBNAILIMAGE", {{90, "44"}, {310, dib}})});
    const auto data = bytesOf(withThumbnail);
    const Preview auto1 = renderMemory(data, {64, Tier::Auto}, defaultRegistry(), Logger{});
    EXPECT_TRUE(auto1.ok);
    EXPECT_EQ(auto1.tier, Tier::Thumbnail);
    EXPECT_EQ(auto1.format, Format::Dxf);
    const Preview vector = renderMemory(data, {64, Tier::Vector}, defaultRegistry(), Logger{});
    EXPECT_TRUE(vector.ok);
    EXPECT_EQ(vector.tier, Tier::Vector);

    const auto plain = bytesOf(file({section("ENTITIES", line(0, 0, 1, 1))}));
    const Preview fallback = renderMemory(plain, {64, Tier::Auto}, defaultRegistry(), Logger{});
    EXPECT_TRUE(fallback.ok);
    EXPECT_EQ(fallback.tier, Tier::Vector);
}

TEST(DxfTiers, EmptyDrawingFailsBothTiers) {
    const auto empty = bytesOf(file({section("ENTITIES", {})}));
    const Preview preview = renderMemory(empty, {64, Tier::Auto}, defaultRegistry(), Logger{});
    EXPECT_FALSE(preview.ok);
    EXPECT_EQ(preview.reason, Reason::TiersFailed);
    EXPECT_NE(preview.reasonText.find("no embedded preview"), std::string::npos);
    EXPECT_NE(preview.reasonText.find("no visible entities"), std::string::npos);
}

} // namespace
} // namespace cadpreview::dxf
