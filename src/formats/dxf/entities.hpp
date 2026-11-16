#pragma once

#include "formats/dxf/document.hpp"
#include "render/vector/drawing.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace cadpreview::dxf {

/// Bounds on the work a hostile drawing can cause. One work unit is charged for every record
/// visited (so each block copy pays for its contents even when nothing is drawn), every array
/// copy, every output point, and every TEXT_BYTES_PER_WORK bytes of text.
struct ConvertLimits {
    std::size_t maxWork = 4'000'000;
    int maxBlockDepth = 16;
};

inline constexpr std::size_t TEXT_BYTES_PER_WORK = 16;

/// Converts the visible model-space entities (paper space when model space draws nothing) into
/// a Drawing, expanding block references. Throws FormatError when a limit is exceeded.
[[nodiscard]] Drawing toDrawing(const Document& document, const ConvertLimits& limits = {});

/// Plain text from TEXT values (%%c, %%d, %%p codes) and MTEXT values (formatting codes
/// removed, \P as newline). see notes/dxf.md#text-codes
[[nodiscard]] std::string plainText(std::string_view value, bool mtext);

} // namespace cadpreview::dxf
