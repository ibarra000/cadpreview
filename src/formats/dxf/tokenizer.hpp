#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace cadpreview::dxf {

/// One DXF group: an integer code and its value as text. see notes/dxf.md#ascii-groups
struct Group {
    int code = 0;
    std::string_view value; ///< Points into the tokenizer's input; trailing whitespace removed.
};

/// Reads ASCII DXF group pairs (a code line, then a value line) from untrusted bytes. Accepts
/// LF or CRLF line ends and a UTF-8 byte-order mark. Throws FormatError on a malformed code
/// line, a missing value line, or an over-long line.
class Tokenizer {
public:
    explicit Tokenizer(std::span<const std::byte> data);

    /// The next group, or nullopt at the end of the data. Comment groups (999) are skipped.
    [[nodiscard]] std::optional<Group> next();

    /// Bytes consumed so far, for error messages.
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

private:
    std::optional<std::string_view> line();

    std::string_view text_;
    std::size_t position_ = 0;
};

/// True when data starts like an ASCII DXF file: optional comments, then "0" and "SECTION".
[[nodiscard]] bool looksLikeAsciiDxf(std::span<const std::byte> prefix);

/// True when data starts with the binary DXF sentinel. see notes/dxf.md#binary-sentinel
[[nodiscard]] bool isBinaryDxf(std::span<const std::byte> prefix);

/// The $ACADVER header value if it appears within prefix (which may be truncated), else "".
[[nodiscard]] std::string headerVersion(std::span<const std::byte> prefix);

/// Parses a group value as a number; nullopt when it is not one.
[[nodiscard]] std::optional<double> toDouble(std::string_view value);
[[nodiscard]] std::optional<long long> toInteger(std::string_view value);

} // namespace cadpreview::dxf
