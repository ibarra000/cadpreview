#pragma once

#include <cstddef>
#include <ostream>
#include <string_view>

namespace cadpreview::dwghelper {

/// Writes ASCII DXF group pairs (code line, value line), the format the DWG helper uses to hand
/// a drawing to the library. see notes/dwg.md#helper-protocol
class DxfWriter {
public:
    /// MTEXT values are split into chunks of at most this many bytes (groups 3, then 1).
    static constexpr std::size_t CHUNK_BYTES = 250;
    /// Longer single values are cut, so no line can reach the reader's 64 KiB line limit.
    static constexpr std::size_t MAX_VALUE_BYTES = 4096;

    explicit DxfWriter(std::ostream& out) : out_(&out) {}

    /// Writes one group. Line breaks in the value become spaces, so a value can never start a
    /// new group; the value is cut at MAX_VALUE_BYTES on a UTF-8 character boundary.
    void group(int code, std::string_view value);
    /// Writes a number in the shortest round-trip form; non-finite values are written as 0.
    void number(int code, double value);
    /// Writes x at xCode and y at xCode + 10.
    void point(int xCode, double x, double y);
    /// Writes x at xCode, y at xCode + 10 and z at xCode + 20.
    void point(int xCode, double x, double y, double z);
    /// Writes MTEXT content: line breaks become \P paragraph codes, and the text is split into
    /// group 3 chunks followed by a final group 1.
    void longText(std::string_view text);

    void beginSection(std::string_view name);
    void endSection();
    void finish();

private:
    std::ostream* out_; // never null
};

/// Length of the longest prefix of `text` that is at most `limit` bytes and does not end
/// inside a UTF-8 multi-byte sequence.
[[nodiscard]] std::size_t utf8Cut(std::string_view text, std::size_t limit) noexcept;

} // namespace cadpreview::dwghelper
