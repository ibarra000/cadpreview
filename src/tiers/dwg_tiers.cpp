#include "formats/dwg/preview.hpp"
#include "render/compose.hpp"
#include "render/decode/dib.hpp"
#include "render/decode/png_decode.hpp"
#include "render/vector/cairo_renderer.hpp"
#include "tiers/builtin_tiers.hpp"

#include <vector>

#ifdef CADPREVIEW_WITH_DWG
#include "formats/dwg/libredwg_reader.hpp"
#include "formats/dxf/entities.hpp"
#endif

namespace cadpreview {

namespace {

TierOutcome thumbnail(const TierContext& context) {
    const auto data = readAll(context.source);
    const dwg::PreviewImage preview = dwg::findPreview(data);
    const Image image = preview.kind == dwg::ImageKind::Png ? decodePngImage(preview.data)
                                                            : decodeDib(preview.data);
    return {composeOnCanvas(image, context.size), {}, {}};
}

#ifndef CADPREVIEW_WITH_DWG
TierOutcome missing(const TierContext& /*context*/) {
    return {std::nullopt, "not built", {}};
}
#endif

} // namespace

#ifdef CADPREVIEW_WITH_DWG
namespace {

TierOutcome vector(const TierContext& context) {
    const std::filesystem::path* path = context.source.filePath();
    if (path == nullptr) {
        return {std::nullopt, "DWG vector rendering needs a file path, not in-memory data", {}};
    }
    const auto translated = dwg::readWithLibreDwg(*path);
    const Drawing drawing = dxf::toDrawing(translated->document);
    if (drawing.primitiveCount() == 0) {
        return {std::nullopt, "no visible entities", {}};
    }
    return {renderDrawing(drawing, context.size), {}, {}};
}

} // namespace

TierEntry dwgVectorTier() {
    return {Format::Dwg, Tier::Vector, vector};
}
#endif

std::vector<TierEntry> dwgTiers() {
    std::vector<TierEntry> tiers{{Format::Dwg, Tier::Thumbnail, thumbnail}};
#ifdef CADPREVIEW_WITH_DWG
    tiers.push_back(dwgVectorTier());
#else
    tiers.emplace_back(Format::Dwg, Tier::Vector, missing,
                       "DWG vector rendering needs a build with CADPREVIEW_WITH_DWG (GPL)");
#endif
    return tiers;
}

} // namespace cadpreview
