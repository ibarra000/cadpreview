#include "common/errors.hpp"
#include "formats/dxf/document.hpp"
#include "formats/dxf/entities.hpp"
#include "formats/dxf/tokenizer.hpp"
#include "render/compose.hpp"
#include "render/decode/dib.hpp"
#include "render/decode/png_decode.hpp"
#include "tiers/builtin_tiers.hpp"
#include "tiers/vector_tier.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace cadpreview {

namespace {

constexpr std::array<std::byte, 8> PNG_SIGNATURE{std::byte{0x89}, std::byte{'P'},  std::byte{'N'},
                                                 std::byte{'G'},  std::byte{'\r'}, std::byte{'\n'},
                                                 std::byte{0x1A}, std::byte{'\n'}};

std::vector<std::byte> asciiDxf(Source& source) {
    auto data = readAll(source);
    if (dxf::isBinaryDxf(data)) {
        throw FormatError("binary DXF is not supported yet");
    }
    return data;
}

// Thumbnails are DIBs; PNG data is accepted too in case newer writers use it.
// see notes/dxf.md#thumbnail
Image decodeThumbnail(std::span<const std::byte> bytes) {
    const bool png = bytes.size() >= PNG_SIGNATURE.size() &&
                     std::equal(PNG_SIGNATURE.begin(), PNG_SIGNATURE.end(), bytes.begin());
    return png ? decodePngImage(bytes) : decodeDib(bytes);
}

TierOutcome thumbnail(const TierContext& context) {
    const auto data = asciiDxf(context.source);
    const dxf::Document document = dxf::readDocument(data, dxf::Parts::HeaderAndThumbnail);
    if (document.thumbnail.empty()) {
        return {std::nullopt, "no embedded preview", {}};
    }
    return {composeOnCanvas(decodeThumbnail(document.thumbnail), context.size), {}, {}};
}

TierOutcome vector(const TierContext& context) {
    const auto data = asciiDxf(context.source);
    const dxf::Document document = dxf::readDocument(data, dxf::Parts::Everything);
    return renderVectorTier(dxf::toDrawing(document), context.size);
}

} // namespace

std::vector<TierEntry> dxfTiers() {
    return {
        {Format::Dxf, Tier::Thumbnail, thumbnail},
        {Format::Dxf, Tier::Vector, vector},
    };
}

} // namespace cadpreview
