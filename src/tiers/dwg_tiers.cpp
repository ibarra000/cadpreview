#include "common/errors.hpp"
#include "formats/dwg/helper.hpp"
#include "formats/dwg/preview.hpp"
#include "formats/dxf/document.hpp"
#include "formats/dxf/entities.hpp"
#include "render/compose.hpp"
#include "render/decode/dib.hpp"
#include "render/decode/png_decode.hpp"
#include "tiers/builtin_tiers.hpp"
#include "tiers/vector_tier.hpp"

#include <span>
#include <string>
#include <vector>

namespace cadpreview {

namespace {

TierOutcome thumbnail(const TierContext& context) {
    const auto data = readAll(context.source);
    const dwg::PreviewImage preview = dwg::findPreview(data);
    const Image image = preview.kind == dwg::ImageKind::Png ? decodePngImage(preview.data)
                                                            : decodeDib(preview.data);
    return {composeOnCanvas(image, context.size), {}, {}};
}

// LibreDWG (GPL) runs in the separate cadpreview-dwg program, never in this library; its DXF
// output is drawn by the same code as DXF files. see notes/dwg.md#helper-protocol
TierOutcome vector(const TierContext& context) {
    const std::filesystem::path* file = context.source.filePath();
    if (file == nullptr) {
        throw TierUnavailable("DWG vector rendering needs a file path, not in-memory data");
    }
    const auto helper = dwg::locateHelper(context.dwgHelper);
    if (!helper) {
        throw TierUnavailable(
            "DWG vector rendering needs the cadpreview-dwg helper (GPL), which was not found");
    }
    const std::string text = dwg::runHelper(*helper, *file, context.log);
    const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
    return renderVectorTier(dxf::toDrawing(dxf::readDocument(bytes, dxf::Parts::Everything)),
                            context.size);
}

} // namespace

std::vector<TierEntry> dwgTiers() {
    return {{Format::Dwg, Tier::Thumbnail, thumbnail}, {Format::Dwg, Tier::Vector, vector}};
}

} // namespace cadpreview
