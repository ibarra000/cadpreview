#include "common/utf8.hpp"

#include <cstdint>
#include <string>

namespace cadpreview {

namespace {

bool isContinuation(unsigned char byte) noexcept {
    return (byte & 0xC0U) == 0x80U;
}

struct LeadByte {
    std::size_t length = 0; ///< Sequence length, or 0 for an invalid lead byte.
    unsigned char low = 0x80;
    unsigned char high = 0xBF; ///< Allowed range of the second byte.
};

// Ranges follow RFC 3629 section 4 (the UTF8-octets grammar).
LeadByte classifyLead(unsigned char lead) noexcept {
    if (lead < 0x80) {
        return {1, 0x80, 0xBF};
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        return {2, 0x80, 0xBF};
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        return {3, static_cast<unsigned char>(lead == 0xE0 ? 0xA0 : 0x80),
                static_cast<unsigned char>(lead == 0xED ? 0x9F : 0xBF)};
    }
    if (lead >= 0xF0 && lead <= 0xF4) {
        return {4, static_cast<unsigned char>(lead == 0xF0 ? 0x90 : 0x80),
                static_cast<unsigned char>(lead == 0xF4 ? 0x8F : 0xBF)};
    }
    return {};
}

// Returns the sequence length that starts at text[index], or 0 when it is malformed.
std::size_t sequenceLength(std::string_view text, std::size_t index) noexcept {
    const auto [length, low, high] = classifyLead(static_cast<unsigned char>(text[index]));
    if (length <= 1) {
        return length;
    }
    if (text.size() - index < length) {
        return 0;
    }
    const auto second = static_cast<unsigned char>(text[index + 1]);
    if (second < low || second > high) {
        return 0;
    }
    for (std::size_t i = 2; i < length; ++i) {
        if (!isContinuation(static_cast<unsigned char>(text[index + i]))) {
            return 0;
        }
    }
    return length;
}

} // namespace

bool isValidUtf8(std::string_view text) noexcept {
    std::size_t index = 0;
    while (index < text.size()) {
        const std::size_t length = sequenceLength(text, index);
        if (length == 0) {
            return false;
        }
        index += length;
    }
    return true;
}

std::size_t utf8PrefixLength(std::string_view text, std::size_t maxBytes) noexcept {
    if (text.size() <= maxBytes) {
        return text.size();
    }
    std::size_t length = maxBytes;
    while (length > 0 && isContinuation(static_cast<unsigned char>(text[length]))) {
        --length;
    }
    return length;
}

std::filesystem::path pathFromUtf8(std::string_view text) {
    std::u8string converted;
    converted.reserve(text.size());
    for (const char c : text) {
        converted.push_back(static_cast<char8_t>(c));
    }
    return {converted};
}

} // namespace cadpreview
