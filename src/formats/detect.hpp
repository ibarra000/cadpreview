#pragma once

#include "containers/source.hpp"
#include "formats/format.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace cadpreview {

/// Container recognised from its published signature; see notes/containers.md.
enum class Container : std::uint8_t { None, CompoundFile, Zip };

struct Detection {
    Format format = Format::Unknown;
    Container container = Container::None;
    std::string version;
    std::string detail;      ///< Display-safe explanation when the format is unknown.
    bool readFailed = false; ///< The input could not be read at all.
};

/// Identifies the format from the content alone (never the file name).
[[nodiscard]] Detection detect(Source& source);

/// Opens and identifies a file; unreadable files yield Format::Unknown with readFailed set.
[[nodiscard]] Detection detectFile(const std::filesystem::path& path);

} // namespace cadpreview
