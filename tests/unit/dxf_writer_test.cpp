// The DWG helper's DXF output must read back through the library's DXF reader unchanged.

#include "common/utf8.hpp"
#include "dxf_writer.hpp"
#include "formats/dxf/document.hpp"
#include "formats/dxf/entities.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <span>
#include <sstream>
#include <string>

namespace cadpreview::dwghelper {
namespace {

// The document's views point into `text`, which must outlive it.
dxf::Document readBack(const std::string& text) {
    const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
    return dxf::readDocument(bytes, dxf::Parts::Everything);
}

TEST(DxfWriter, WritesSectionsThatReadBack) {
    std::ostringstream out;
    DxfWriter writer(out);
    writer.beginSection("TABLES");
    writer.group(0, "LAYER");
    writer.group(2, "L7");
    writer.group(70, "1");
    writer.endSection();
    writer.beginSection("BLOCKS");
    writer.group(0, "BLOCK");
    writer.group(2, "B3");
    writer.point(10, 1.5, -2);
    writer.group(0, "LINE");
    writer.point(10, 0, 0);
    writer.point(11, 1, 0);
    writer.group(0, "ENDBLK");
    writer.endSection();
    writer.beginSection("ENTITIES");
    writer.group(0, "INSERT");
    writer.group(2, "B3");
    writer.point(10, 10, 20);
    writer.endSection();
    writer.finish();

    const std::string text = out.str();
    const dxf::Document document = readBack(text);
    EXPECT_TRUE(document.hiddenLayers.contains("L7"));
    ASSERT_TRUE(document.blocks.contains("B3"));
    EXPECT_DOUBLE_EQ(document.blocks.at("B3").base.x, 1.5);
    const Drawing drawing = dxf::toDrawing(document);
    ASSERT_EQ(drawing.polylines.size(), 1U);
    EXPECT_DOUBLE_EQ(drawing.polylines[0].points[1].x, 9.5); // 1 - 1.5 base + 10 insertion
}

TEST(DxfWriter, ThreeDPointsReadBackAsWires) {
    std::ostringstream out;
    DxfWriter writer(out);
    writer.beginSection("ENTITIES");
    writer.group(0, "LINE");
    writer.point(10, 1, 2, 3);
    writer.point(11, 4, 5, -6);
    writer.endSection();
    writer.finish();
    const Drawing drawing = dxf::toDrawing(readBack(out.str()));
    ASSERT_EQ(drawing.wires.size(), 1U);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[0].z, 3);
    EXPECT_DOUBLE_EQ(drawing.wires[0].points[1].z, -6);
}

TEST(DxfWriter, LineBreaksInValuesCannotStartNewGroups) {
    std::ostringstream out;
    DxfWriter writer(out);
    writer.beginSection("ENTITIES");
    writer.group(0, "TEXT");
    writer.point(10, 0, 0);
    writer.number(40, 1);
    writer.group(1, "first\r\n0\nLINE");
    writer.endSection();
    writer.finish();

    const std::string text = out.str();
    const dxf::Document document = readBack(text);
    ASSERT_EQ(document.entities.size(), 1U);
    EXPECT_EQ(document.entities[0].text(1), "first  0 LINE");
}

TEST(DxfWriter, LongTextIsSplitIntoChunksAndKeepsParagraphs) {
    std::string text(600, 'a');
    text += "\nnext";
    std::ostringstream out;
    DxfWriter writer(out);
    writer.beginSection("ENTITIES");
    writer.group(0, "MTEXT");
    writer.point(10, 0, 0);
    writer.number(40, 1);
    writer.longText(text);
    writer.endSection();
    writer.finish();

    const std::string written = out.str();
    const Drawing drawing = dxf::toDrawing(readBack(written));
    ASSERT_EQ(drawing.texts.size(), 2U);
    EXPECT_EQ(drawing.texts[0].text, std::string(600, 'a'));
    EXPECT_EQ(drawing.texts[1].text, "next");
}

TEST(DxfWriter, ChunksNeverSplitAUtf8Sequence) {
    std::string text = "a"; // shifts every character to an odd offset
    for (int i = 0; i < 300; ++i) {
        text += "\xC3\x98"; // two-byte character
    }
    std::ostringstream out;
    DxfWriter writer(out);
    writer.longText(text);
    const std::string written = out.str();
    std::size_t start = 0;
    while (start < written.size()) {
        const std::size_t codeEnd = written.find('\n', start);
        const std::size_t valueEnd = written.find('\n', codeEnd + 1);
        const std::string value = written.substr(codeEnd + 1, valueEnd - codeEnd - 1);
        EXPECT_LE(value.size(), DxfWriter::CHUNK_BYTES);
        EXPECT_TRUE(isValidUtf8(value)) << "chunk ends inside a character";
        start = valueEnd + 1;
    }
}

TEST(DxfWriter, NonFiniteNumbersAreWrittenAsZero) {
    std::ostringstream out;
    DxfWriter writer(out);
    writer.number(40, std::numeric_limits<double>::infinity());
    writer.number(41, std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(out.str(), "40\n0\n41\n0\n");
}

TEST(DxfWriter, OverlongValuesAreTruncated) {
    std::ostringstream out;
    DxfWriter writer(out);
    writer.group(1, std::string(DxfWriter::MAX_VALUE_BYTES + 100, 'x'));
    EXPECT_EQ(out.str().size(), std::string("1\n\n").size() + DxfWriter::MAX_VALUE_BYTES);
}

} // namespace
} // namespace cadpreview::dwghelper
