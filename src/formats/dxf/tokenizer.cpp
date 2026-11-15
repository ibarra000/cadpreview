#include "formats/dxf/tokenizer.hpp"

#include "common/errors.hpp"

#include <algorithm>
#include <charconv>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace cadpreview::dxf {

namespace {

constexpr std::size_t MAX_LINE = 64 * 1024;
constexpr int COMMENT_CODE = 999;
constexpr std::string_view BOM = "\xEF\xBB\xBF";
// see notes/dxf.md#binary-sentinel
constexpr std::string_view BINARY_SENTINEL{"AutoCAD Binary DXF\r\n\x1A\0", 22};

std::string_view asText(std::span<const std::byte> data) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view as characters
    return {reinterpret_cast<const char*>(data.data()), data.size()};
}

std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::string_view trimEnd(std::string_view text) {
    const auto last = text.find_last_not_of(" \t\r");
    return last == std::string_view::npos ? std::string_view{} : text.substr(0, last + 1);
}

} // namespace

Tokenizer::Tokenizer(std::span<const std::byte> data) : text_(asText(data)) {
    if (text_.starts_with(BOM)) {
        position_ = BOM.size();
    }
}

std::optional<std::string_view> Tokenizer::line() {
    if (position_ >= text_.size()) {
        return std::nullopt;
    }
    const std::size_t end = std::min(text_.find('\n', position_), text_.size());
    if (end - position_ > MAX_LINE) {
        throw FormatError("DXF line too long");
    }
    const std::string_view result = text_.substr(position_, end - position_);
    position_ = end + 1;
    return result;
}

std::optional<Group> Tokenizer::next() {
    while (true) {
        auto codeLine = line();
        while (codeLine && trim(*codeLine).empty()) {
            codeLine = line(); // tolerate blank lines between groups at the end of a file
        }
        if (!codeLine) {
            return std::nullopt;
        }
        const auto code = toInteger(trim(*codeLine));
        if (!code || *code < -5 || *code > 1071) {
            throw FormatError("malformed DXF group code");
        }
        const auto value = line();
        if (!value) {
            throw FormatError("DXF ends inside a group");
        }
        if (*code != COMMENT_CODE) {
            return Group{static_cast<int>(*code), trimEnd(*value)};
        }
    }
}

bool looksLikeAsciiDxf(std::span<const std::byte> prefix) {
    try {
        Tokenizer tokenizer(prefix);
        const auto first = tokenizer.next();
        if (!first || first->code != 0 || trim(first->value) != "SECTION") {
            return false;
        }
        const auto second = tokenizer.next();
        return second && second->code == 2;
    } catch (const FormatError&) {
        return false;
    }
}

std::string headerVersion(std::span<const std::byte> prefix) {
    constexpr std::size_t MAX_HEADER_GROUPS = 100'000;
    try {
        Tokenizer tokenizer(prefix);
        bool nextIsVersion = false;
        for (std::size_t i = 0; i < MAX_HEADER_GROUPS; ++i) {
            const auto group = tokenizer.next();
            if (!group || (group->code == 0 && trim(group->value) == "ENDSEC")) {
                break;
            }
            if (nextIsVersion && group->code == 1) {
                return std::string(trim(group->value));
            }
            nextIsVersion = group->code == 9 && trim(group->value) == "$ACADVER";
        }
    } catch (const FormatError&) { // NOLINT(bugprone-empty-catch): a cut-off prefix has no version
    }
    return {};
}

bool isBinaryDxf(std::span<const std::byte> prefix) {
    return asText(prefix).starts_with(BINARY_SENTINEL);
}

std::optional<double> toDouble(std::string_view value) {
    const std::string_view text = trim(value);
    double result = 0;
    const auto [end, error] = std::from_chars(text.data(), std::to_address(text.end()), result);
    if (text.empty() || error != std::errc{} || end != std::to_address(text.end())) {
        return std::nullopt;
    }
    return result;
}

std::optional<long long> toInteger(std::string_view value) {
    std::string_view text = trim(value);
    if (text.starts_with('+')) {
        text.remove_prefix(1);
    }
    long long result = 0;
    const auto [end, error] = std::from_chars(text.data(), std::to_address(text.end()), result);
    if (text.empty() || error != std::errc{} || end != std::to_address(text.end())) {
        return std::nullopt;
    }
    return result;
}

} // namespace cadpreview::dxf
