#pragma once

#include "formats/dxf/tokenizer.hpp"
#include "render/vector/drawing.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cadpreview::dxf {

/// One DXF object: the value of its leading 0 group and every group up to the next 0 group.
struct Record {
    std::string_view type;
    std::vector<Group> groups;

    /// First value with this code, if any.
    [[nodiscard]] std::optional<std::string_view> text(int code) const;
    [[nodiscard]] double number(int code, double fallback) const;
    [[nodiscard]] long long integer(int code, long long fallback) const;
};

struct Block {
    Point base;
    std::vector<Record> entities;
};

/// What to keep while reading; skipping entities makes thumbnail extraction cheap.
enum class Parts : std::uint8_t { Header, HeaderAndThumbnail, Everything };

/// The parts of a DXF file cadpreview uses. Views point into the input bytes, which must
/// outlive the document. Layer and block names are stored upper-case (DXF compares them
/// case-insensitively).
// NOLINTNEXTLINE(bugprone-exception-escape): MSVC's unordered_map move is not noexcept
struct Document {
    std::string version; ///< $ACADVER, e.g. "AC1032"; empty if absent
    std::vector<std::byte> thumbnail;
    bool hasThumbnailSection = false;
    std::unordered_set<std::string> hiddenLayers;
    std::unordered_map<std::string, Block> blocks;
    std::vector<Record> entities;
};

/// Limits that bound memory and time on hostile input.
struct ReadLimits {
    std::size_t maxGroups = 16'000'000; ///< about 400 MB of group views at most
    std::size_t maxEntities = 1'000'000;
    std::size_t maxThumbnailBytes = std::size_t{16} * 1024 * 1024;
};

/// Reads an ASCII DXF file. Throws FormatError when it is malformed or exceeds the limits.
[[nodiscard]] Document readDocument(std::span<const std::byte> data, Parts parts,
                                    const ReadLimits& limits = {});

[[nodiscard]] std::string upperCase(std::string_view text);

} // namespace cadpreview::dxf
