#include "formats/dxf/entities.hpp"
#include "formats/dxf/tokenizer.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace cadpreview::dxf {

namespace {

// ASCII lower case; leaves bytes outside A-Z unchanged apart from setting bit 5.
char asciiFold(char c) {
    return static_cast<char>(static_cast<unsigned char>(c) | 0x20U);
}

void appendUtf8(std::string& out, std::uint32_t code) {
    if (code == 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
        out.push_back('?');
    } else if (code < 0x80) {
        out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0U | (code >> 6U)));
        out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    } else if (code < 0x10000) {
        out.push_back(static_cast<char>(0xE0U | (code >> 12U)));
        out.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (code >> 18U)));
        out.push_back(static_cast<char>(0x80U | ((code >> 12U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    }
}

// Handles "\U+XXXX"; returns the characters consumed after the backslash, or 0.
std::size_t unicodeEscape(std::string_view rest, std::string& out) {
    constexpr std::size_t LENGTH = 6; // U+XXXX
    if (rest.size() < LENGTH || !rest.starts_with("U+")) {
        return 0;
    }
    std::uint32_t code = 0;
    for (const char c : rest.substr(2, 4)) {
        const char lower = asciiFold(c);
        const bool digit = c >= '0' && c <= '9';
        const bool letter = lower >= 'a' && lower <= 'f';
        if (!digit && !letter) {
            return 0;
        }
        code = code * 16 + static_cast<std::uint32_t>(digit ? c - '0' : lower - 'a' + 10);
    }
    appendUtf8(out, code);
    return LENGTH;
}

// "%%" control codes shared by TEXT and MTEXT; returns characters consumed after "%%".
std::size_t percentCode(std::string_view rest, std::string& out) {
    if (rest.empty()) {
        return 0;
    }
    switch (asciiFold(rest.front())) {
    case 'c':
        appendUtf8(
            out, 0x00D8); // diameter: the embedded font has no U+2300, and Ø is the usual stand-in
        return 1;
    case 'd':
        appendUtf8(out, 0x00B0);
        return 1;
    case 'p':
        appendUtf8(out, 0x00B1);
        return 1;
    case 'u': // underline and overline toggles have no visible character
    case 'o':
    case 'k':
        return 1;
    default:
        break;
    }
    if (rest.front() == '%') {
        out.push_back('%');
        return 1;
    }
    return 0;
}

// MTEXT formatting code after a backslash; returns characters consumed after the backslash.
std::size_t mtextCode(std::string_view rest, std::string& out) {
    if (rest.empty()) {
        return 0;
    }
    switch (rest.front()) {
    case 'P':
        out.push_back('\n');
        return 1;
    case '~':
        out.push_back(' ');
        return 1;
    case '\\':
    case '{':
    case '}':
        out.push_back(rest.front());
        return 1;
    case 'L':
    case 'l':
    case 'O':
    case 'o':
    case 'K':
    case 'k':
        return 1;
    case 'S': { // stacked fraction "\Sa^b;" shown as "a/b"
        const auto end = rest.find(';');
        const std::string_view body =
            rest.substr(1, end == std::string_view::npos ? std::string_view::npos : end - 1);
        for (const char c : body) {
            out.push_back(c == '^' || c == '#' ? '/' : c);
        }
        return end == std::string_view::npos ? rest.size() : end + 1;
    }
    default: { // \f..; \H..; \C..; and similar take a parameter ending in ';'
        const auto end = rest.find(';');
        return end == std::string_view::npos ? rest.size() : end + 1;
    }
    }
}

} // namespace

std::string plainText(std::string_view value, bool mtext) {
    std::string out;
    out.reserve(value.size());
    std::size_t i = 0;
    while (i < value.size()) {
        const char c = value[i];
        const std::string_view rest = value.substr(i + 1);
        std::size_t used = 0;
        if (c == '\\') {
            used = unicodeEscape(rest, out);
            if (used == 0 && mtext) {
                used = mtextCode(rest, out);
            }
        } else if (c == '%' && rest.starts_with('%')) {
            used = percentCode(rest.substr(1), out);
            used = used == 0 ? 0 : used + 1;
        } else if (mtext && (c == '{' || c == '}')) {
            ++i;
            continue;
        }
        if (used == 0) {
            out.push_back(c);
        }
        i += used + 1;
    }
    return out;
}

} // namespace cadpreview::dxf
