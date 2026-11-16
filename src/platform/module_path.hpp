#pragma once

#include <filesystem>

namespace cadpreview {

/// Full path of the binary that contains this library's code: cadpreview.dll or
/// libcadpreview.so, or the executable when the library is linked statically. Empty if the
/// system cannot say.
[[nodiscard]] std::filesystem::path currentModulePath();

} // namespace cadpreview
