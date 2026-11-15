#pragma once

#include <string>
#include <string_view>

namespace cadpreview::cli {

/// Returns text as a quoted JSON string literal.
[[nodiscard]] std::string jsonString(std::string_view text);

} // namespace cadpreview::cli
