#pragma once

#include "common/log.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace cadpreview::dwg {

/// File name of the GPL helper program that decodes DWG drawings with LibreDWG.
#ifdef _WIN32
inline constexpr const char* HELPER_NAME = "cadpreview-dwg.exe";
#else
inline constexpr const char* HELPER_NAME = "cadpreview-dwg";
#endif

/// The helper to use: `configured` if it names an existing file, nothing if it names none, and
/// when it is empty, HELPER_NAME next to this library or in ../bin relative to it.
[[nodiscard]] std::optional<std::filesystem::path>
locateHelper(const std::filesystem::path& configured);

/// Runs the helper on a DWG file and returns the drawing as ASCII DXF text. The helper runs
/// with time, memory and output limits; a failure of any kind throws FormatError with a
/// display-safe message, and the helper's own error output goes only to the debug log.
/// see notes/dwg.md#helper-protocol
[[nodiscard]] std::string runHelper(const std::filesystem::path& helper,
                                    const std::filesystem::path& drawing, const Logger& log);

} // namespace cadpreview::dwg
