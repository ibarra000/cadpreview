#include "dxf_writer.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <memory>
#include <string>

namespace cadpreview::dwghelper {

namespace {

constexpr unsigned char CONTINUATION_MASK = 0xC0U;
constexpr unsigned char CONTINUATION_BITS = 0x80U;

bool isContinuation(char c) noexcept {
    return (static_cast<unsigned char>(c) & CONTINUATION_MASK) == CONTINUATION_BITS;
}

std::string withoutLineBreaks(std::string_view value) {
    std::string clean(value);
    for (char& c : clean) {
        if (c == '\r' || c == '\n') {
            c = ' ';
        }
    }
    return clean;
}

} // namespace

std::size_t utf8Cut(std::string_view text, std::size_t limit) noexcept {
    if (text.size() <= limit) {
        return text.size();
    }
    std::size_t cut = limit;
    // Step back over continuation bytes so the cut lands before a character's lead byte.
    while (cut > 0 && isContinuation(text[cut])) {
        --cut;
    }
    return cut;
}

void DxfWriter::group(int code, std::string_view value) {
    const std::string clean = withoutLineBreaks(value.substr(0, utf8Cut(value, MAX_VALUE_BYTES)));
    *out_ << code << '\n' << clean << '\n';
}

void DxfWriter::number(int code, double value) {
    if (!std::isfinite(value)) {
        group(code, "0");
        return;
    }
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), std::to_address(buffer.end()), value);
    group(code, std::string_view(buffer.data(), result.ptr));
}

void DxfWriter::point(int xCode, double x, double y) {
    number(xCode, x);
    number(xCode + 10, y);
}

void DxfWriter::point(int xCode, double x, double y, double z) {
    point(xCode, x, y);
    number(xCode + 20, z);
}

void DxfWriter::longText(std::string_view text) {
    std::string paragraphs;
    for (const char c : text) {
        if (c == '\n') {
            paragraphs += "\\P";
        } else if (c != '\r') {
            paragraphs += c;
        }
    }
    std::string_view rest = paragraphs;
    while (rest.size() > CHUNK_BYTES) {
        std::size_t cut = utf8Cut(rest, CHUNK_BYTES);
        if (cut == 0) {
            cut = CHUNK_BYTES; // not UTF-8: any split will do
        }
        group(3, rest.substr(0, cut));
        rest.remove_prefix(cut);
    }
    group(1, rest);
}

void DxfWriter::beginSection(std::string_view name) {
    group(0, "SECTION");
    group(2, name);
}

void DxfWriter::endSection() {
    group(0, "ENDSEC");
}

void DxfWriter::finish() {
    group(0, "EOF");
    out_->flush();
}

} // namespace cadpreview::dwghelper
