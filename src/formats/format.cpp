#include "formats/format.hpp"

#include <array>

namespace cadpreview {

namespace {

struct FormatNames {
    Format format;
    std::string_view id;
    std::string_view displayName;
};

// One row per format; the first row is the fallback for values this version does not know.
constexpr std::array<FormatNames, 1> FORMATS{{
    {Format::Unknown, "unknown", "Unknown format"},
}};

const FormatNames& namesOf(Format format) noexcept {
    for (const FormatNames& names : FORMATS) {
        if (names.format == format) {
            return names;
        }
    }
    return FORMATS.front();
}

} // namespace

std::string_view formatId(Format format) noexcept {
    return namesOf(format).id;
}

std::string_view formatDisplayName(Format format) noexcept {
    return namesOf(format).displayName;
}

} // namespace cadpreview
