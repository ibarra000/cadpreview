#include "common/errors.hpp"
#include "formats/dxf/document.hpp"
#include "formats/dxf/entities.hpp"
#include "render/vector/cairo_renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

// The DXF reader, entity converter and cairo renderer, with small limits and a small canvas
// so the fuzzer explores structure rather than size.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto bytes = std::as_bytes(std::span(data, size));
    try {
        cadpreview::dxf::ReadLimits limits;
        limits.maxEntities = 10'000;
        limits.maxGroups = 200'000;
        const auto document =
            cadpreview::dxf::readDocument(bytes, cadpreview::dxf::Parts::Everything, limits);
        cadpreview::dxf::ConvertLimits convert;
        convert.maxWork = 200'000;
        const auto drawing = cadpreview::dxf::toDrawing(document, convert);
        constexpr std::uint32_t CANVAS = 32;
        static_cast<void>(cadpreview::renderDrawing(drawing, CANVAS));
    } catch (const cadpreview::FormatError&) { // NOLINT(bugprone-empty-catch): expected
    }
    return 0;
}
