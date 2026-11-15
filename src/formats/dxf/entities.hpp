#pragma once

#include "formats/dxf/document.hpp"
#include "render/vector/drawing.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace cadpreview::dxf {

struct ConvertLimits {
    std::size_t maxPrimitives = 2'000'000; ///< counts block and array expansion
    int maxBlockDepth = 16;
};

/// Converts the visible model-space entities (paper space when model space draws nothing) into
/// a Drawing, expanding block references. Throws FormatError when a limit is exceeded.
[[nodiscard]] Drawing toDrawing(const Document& document, const ConvertLimits& limits = {});

/// Plain text from TEXT values (%%c, %%d, %%p codes) and MTEXT values (formatting codes
/// removed, \P as newline). see notes/dxf.md#text-codes
[[nodiscard]] std::string plainText(std::string_view value, bool mtext);

} // namespace cadpreview::dxf
