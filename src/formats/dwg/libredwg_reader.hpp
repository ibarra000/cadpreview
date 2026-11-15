#pragma once

#include "formats/dxf/document.hpp"

#include <deque>
#include <filesystem>
#include <memory>
#include <string>

namespace cadpreview::dwg {

/// A DWG drawing expressed as the DXF document model, so the DXF entity converter can draw it.
/// The records' string views point into `strings`.
struct TranslatedDocument {
    std::deque<std::string> strings;
    dxf::Document document;
};

/// Decodes a DWG file with LibreDWG (GPL builds only). LibreDWG keeps process-wide state, so
/// calls are serialized. Throws FormatError when the file cannot be decoded.
[[nodiscard]] std::unique_ptr<TranslatedDocument>
readWithLibreDwg(const std::filesystem::path& path);

} // namespace cadpreview::dwg
