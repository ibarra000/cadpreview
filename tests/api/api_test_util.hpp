#pragma once

#include <cadpreview/cadpreview.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace cadpreview::test {

struct PreviewDeleter {
    void operator()(cp_preview* p) const noexcept { cp_preview_free(p); }
};
using PreviewPtr = std::unique_ptr<cp_preview, PreviewDeleter>;

inline cp_options makeOptions(std::uint32_t size = 0, std::uint32_t tier = CP_TIER_AUTO) {
    cp_options options{};
    options.struct_size = sizeof(cp_options);
    options.size = size;
    options.tier = tier;
    return options;
}

inline struct cp_preview_info infoOf(const cp_preview* p) {
    struct cp_preview_info info{};
    info.struct_size = sizeof(struct cp_preview_info);
    if (cp_preview_info(p, &info) != CP_OK) {
        info = {};
    }
    return info;
}

inline std::span<const std::uint8_t> pngOf(const cp_preview* p) {
    const void* data = nullptr;
    std::size_t len = 0;
    if (cp_preview_png(p, &data, &len) != CP_OK) {
        return {};
    }
    return {static_cast<const std::uint8_t*>(data), len};
}

inline PreviewPtr renderBytes(std::span<const std::uint8_t> bytes, const cp_options* options) {
    cp_preview* raw = nullptr;
    if (cp_render_memory(bytes.data(), bytes.size(), options, &raw) != CP_OK) {
        return nullptr;
    }
    return PreviewPtr(raw);
}

inline PreviewPtr renderPath(const std::string& utf8Path, const cp_options* options) {
    cp_preview* raw = nullptr;
    if (cp_render_file(utf8Path.c_str(), options, &raw) != CP_OK) {
        return nullptr;
    }
    return PreviewPtr(raw);
}

/// Converts a filesystem path to the UTF-8 string the C API expects.
inline std::string utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    std::string result;
    result.reserve(text.size());
    for (const char8_t c : text) {
        result.push_back(static_cast<char>(c));
    }
    return result;
}

inline std::vector<std::uint8_t> cfbSignatureFile() {
    std::vector<std::uint8_t> bytes(512, 0);
    const std::array<std::uint8_t, 8> signature{0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    std::ranges::copy(signature, bytes.begin());
    return bytes;
}

} // namespace cadpreview::test
