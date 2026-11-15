#include "common/log.hpp"
#include "common/utf8.hpp"
#include "formats/detect.hpp"
#include "tiers/ladder.hpp"
#include "tiers/pipeline.hpp"

#include <cadpreview/cadpreview.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>

struct cp_preview {
    cadpreview::Preview impl;
};

namespace {

using cadpreview::Format;
using cadpreview::Reason;
using cadpreview::Tier;

static_assert(static_cast<int>(Tier::Placeholder) == CP_TIER_PLACEHOLDER);
static_assert(static_cast<int>(Tier::Vector) == CP_TIER_VECTOR);
static_assert(static_cast<int>(Format::Unknown) == CP_FORMAT_UNKNOWN);
static_assert(static_cast<int>(Format::Dwg) == CP_FORMAT_DWG);
static_assert(static_cast<int>(Format::Dxf) == CP_FORMAT_DXF);
static_assert(static_cast<int>(Reason::PlaceholderRequested) == CP_REASON_PLACEHOLDER_REQUESTED);
static_assert(static_cast<int>(Reason::TiersFailed) == CP_REASON_TIERS_FAILED);
static_assert(static_cast<int>(cadpreview::LogLevel::Error) == CP_LOG_ERROR);
static_assert(std::is_same_v<cadpreview::LogCallback, cp_log_fn>);

// Version-1 sizes are the smallest struct_size ever accepted. When a field is appended, the v1
// size becomes the offset of the first appended field, so that old callers keep working.
constexpr std::size_t OPTIONS_V1_SIZE = offsetof(cp_options, dwg_helper_path);
constexpr std::size_t PREVIEW_INFO_V1_SIZE = sizeof(struct cp_preview_info);
constexpr std::size_t FORMAT_INFO_V1_SIZE = sizeof(cp_format_info);
constexpr std::uint32_t MAX_STRUCT_SIZE = 4096;

bool isZero(std::byte b) noexcept {
    return b == std::byte{0};
}

// Copies an extensible input struct. Bytes past what this version knows must be zero, so a
// newer caller cannot set a field that would silently be ignored.
template <class T>
bool copyIn(const T& in, std::size_t minSize, T& out) noexcept {
    const std::uint32_t declared = in.struct_size;
    if (declared < minSize || declared > MAX_STRUCT_SIZE) {
        return false;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view of a C struct
    const std::span<const std::byte> bytes(reinterpret_cast<const std::byte*>(&in), declared);
    if (declared > sizeof(T) && !std::ranges::all_of(bytes.subspan(sizeof(T)), isZero)) {
        return false;
    }
    out = T{};
    std::memcpy(&out, &in, std::min<std::size_t>(declared, sizeof(T)));
    return true;
}

// The caller's declared size of an output struct, read once, or nullopt when it is unusable.
template <class T>
std::optional<std::uint32_t> outputSize(const T* out, std::size_t minSize) noexcept {
    if (out == nullptr) {
        return std::nullopt;
    }
    const std::uint32_t declared = out->struct_size;
    if (declared < minSize || declared > MAX_STRUCT_SIZE) {
        return std::nullopt;
    }
    return declared;
}

// Writes an extensible output struct: zero-fills the caller's declared size, copies what this
// version knows, and reports the filled size back in struct_size.
template <class T>
void copyOut(T& out, std::uint32_t declared, const T& value) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): byte view of a C struct
    const std::span<std::byte> bytes(reinterpret_cast<std::byte*>(&out), declared);
    std::ranges::fill(bytes, std::byte{0});
    const std::size_t filled = std::min<std::size_t>(declared, sizeof(T));
    std::memcpy(&out, &value, filled);
    out.struct_size = static_cast<std::uint32_t>(filled);
}

struct ParsedOptions {
    cadpreview::RenderOptions render;
    cadpreview::Logger log;
};

std::optional<ParsedOptions> parseOptions(const cp_options* opts) noexcept {
    ParsedOptions parsed;
    if (opts == nullptr) {
        return parsed;
    }
    cp_options copy{};
    if (!copyIn(*opts, OPTIONS_V1_SIZE, copy) || copy.reserved != 0 ||
        copy.tier > CP_TIER_PLACEHOLDER) {
        return std::nullopt;
    }
    const std::uint32_t size = copy.size == 0 ? cadpreview::DEFAULT_SIZE : copy.size;
    if (size < cadpreview::MIN_SIZE || size > cadpreview::MAX_SIZE) {
        return std::nullopt;
    }
    parsed.render.size = size;
    parsed.render.tier = static_cast<Tier>(copy.tier);
    if (copy.dwg_helper_path != nullptr) {
        if (!cadpreview::isValidUtf8(copy.dwg_helper_path)) {
            return std::nullopt;
        }
        parsed.render.dwgHelper = cadpreview::pathFromUtf8(copy.dwg_helper_path);
    }
    parsed.log = cadpreview::Logger(copy.log, copy.log_user);
    return parsed;
}

// Hands a finished preview to the caller. Ownership crosses the C boundary here and comes back
// in cp_preview_free.
cp_status release(cadpreview::Preview&& preview, cp_preview** out) {
    auto holder = std::make_unique<cp_preview>(cp_preview{std::move(preview)});
    *out = holder.release(); // NOLINT(cppcoreguidelines-owning-memory): owned by the caller
    return CP_OK;
}

template <class Render>
cp_status guardedRender(const cp_options* opts, cp_preview** out, const Render& render) noexcept {
    if (out == nullptr) {
        return CP_ERROR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    const auto parsed = parseOptions(opts);
    if (!parsed) {
        return CP_ERROR_INVALID_ARGUMENT;
    }
    // The pipeline turns every input problem into a placeholder, so reaching a catch means the
    // placeholder itself could not be built; the API defines no other status for that.
    try {
        return release(render(*parsed), out);
    } catch (const std::bad_alloc&) {
        parsed->log.error("out of memory while building the preview");
    } catch (const std::exception& error) {
        parsed->log.error(std::string("internal error while building the preview: ") +
                          error.what());
    } catch (...) {
        parsed->log.error("internal error while building the preview");
    }
    return CP_ERROR_OUT_OF_MEMORY;
}

void copyText(std::span<char> destination, std::string_view text) noexcept {
    const std::size_t length = cadpreview::utf8PrefixLength(text, destination.size() - 1);
    std::copy_n(text.begin(), length, destination.begin());
    destination[length] = '\0';
}

} // namespace

extern "C" {

cp_status cp_render_file(const char* utf8_path, const cp_options* opts, cp_preview** out) noexcept {
    if (out != nullptr) {
        *out = nullptr;
    }
    if (utf8_path == nullptr || !cadpreview::isValidUtf8(utf8_path)) {
        return CP_ERROR_INVALID_ARGUMENT;
    }
    return guardedRender(opts, out, [utf8_path](const ParsedOptions& parsed) {
        return cadpreview::renderFile(cadpreview::pathFromUtf8(utf8_path), parsed.render,
                                      cadpreview::defaultRegistry(), parsed.log);
    });
}

cp_status cp_render_memory(const void* data, size_t len, const cp_options* opts,
                           cp_preview** out) noexcept {
    if (out != nullptr) {
        *out = nullptr;
    }
    if (data == nullptr && len != 0) {
        return CP_ERROR_INVALID_ARGUMENT;
    }
    const std::span<const std::byte> bytes(static_cast<const std::byte*>(data), len);
    return guardedRender(opts, out, [bytes](const ParsedOptions& parsed) {
        return cadpreview::renderMemory(bytes, parsed.render, cadpreview::defaultRegistry(),
                                        parsed.log);
    });
}

cp_status cp_detect_format(const char* utf8_path, cp_format_info* out) noexcept {
    const auto declared = outputSize(out, FORMAT_INFO_V1_SIZE);
    if (utf8_path == nullptr || !cadpreview::isValidUtf8(utf8_path) || !declared) {
        return CP_ERROR_INVALID_ARGUMENT;
    }
    try {
        const cadpreview::Detection detection =
            cadpreview::detectFile(cadpreview::pathFromUtf8(utf8_path));
        cp_format_info info{};
        info.struct_size = sizeof(info);
        info.format = static_cast<cp_format>(detection.format);
        copyText(info.name, cadpreview::formatId(detection.format));
        copyText(info.version, detection.version);
        copyOut(*out, *declared, info);
        return CP_OK;
    } catch (...) {
        return CP_ERROR_OUT_OF_MEMORY;
    }
}

cp_status cp_preview_png(const cp_preview* p, const void** data, size_t* len) noexcept {
    if (p == nullptr || data == nullptr || len == nullptr) {
        return CP_ERROR_INVALID_ARGUMENT;
    }
    *data = p->impl.png.data();
    *len = p->impl.png.size();
    return CP_OK;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow" // see the declaration in cadpreview.h
#endif
cp_status cp_preview_info(const cp_preview* p, struct cp_preview_info* out) noexcept {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    const auto declared = outputSize(out, PREVIEW_INFO_V1_SIZE);
    if (p == nullptr || !declared) {
        return CP_ERROR_INVALID_ARGUMENT;
    }
    const cadpreview::Preview& preview = p->impl;
    struct cp_preview_info info{};
    info.struct_size = sizeof(info);
    info.ok = preview.ok ? 1 : 0;
    info.tier = static_cast<std::uint32_t>(preview.tier);
    info.width = preview.width;
    info.height = preview.height;
    info.format = static_cast<cp_format>(preview.format);
    info.reason_code = static_cast<cp_reason>(preview.reason);
    info.render_time_us = preview.renderTimeUs;
    info.format_name = preview.formatName.c_str();
    info.version = preview.version.c_str();
    info.reason = preview.reasonText.c_str();
    info.warnings = preview.warnings.c_str();
    copyOut(*out, *declared, info);
    return CP_OK;
}

void cp_preview_free(cp_preview* p) noexcept {
    const std::unique_ptr<cp_preview> owned(p); // NOLINT(cppcoreguidelines-owning-memory)
}

const char* cp_version(void) noexcept {
    return CP_VERSION_STRING;
}

const char* cp_reason_name(cp_reason reason) noexcept {
    // reasonId returns views of string literals, so data() is NUL-terminated.
    return cadpreview::reasonId(static_cast<Reason>(reason)).data();
}

} // extern "C"
