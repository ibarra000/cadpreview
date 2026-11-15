/*
 * cadpreview: read-only 2D PNG previews for CAD files.
 *
 * Every function is safe to call from several threads at once. The library keeps no global
 * state and never writes to stdout or stderr; messages go only to the optional log callback.
 * Unreadable or unsupported input is not an error: it yields a placeholder preview whose
 * info has ok == 0 and a reason. Error statuses mean an invalid argument or out of memory.
 *
 * Extensible structs start with struct_size. Callers set it to sizeof(struct) and zero any
 * field they do not use. Output structs are zero-filled up to struct_size, and struct_size is
 * then set to the number of bytes this library version filled.
 */
#ifndef CADPREVIEW_CADPREVIEW_H
#define CADPREVIEW_CADPREVIEW_H

#include <cadpreview/version.h>

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#if defined(CADPREVIEW_BUILD)
#define CP_API __declspec(dllexport)
#else
#define CP_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define CP_API __attribute__((visibility("default")))
#else
#define CP_API
#endif

#ifdef __cplusplus
#define CP_NOEXCEPT noexcept
extern "C" {
#else
#define CP_NOEXCEPT
#endif

/// Result of an API call.
typedef int32_t cp_status;
enum {
    CP_OK = 0,                     ///< The call succeeded.
    CP_ERROR_INVALID_ARGUMENT = 1, ///< An argument broke the documented contract.
    CP_ERROR_OUT_OF_MEMORY = 2     ///< Not even a placeholder could be built (memory exhausted).
};

/// Strategy tiers, tried in this order when the tier option is CP_TIER_AUTO.
enum {
    CP_TIER_AUTO = 0,       ///< Options only: try every tier in order.
    CP_TIER_THUMBNAIL = 1,  ///< Preview image embedded in the file.
    CP_TIER_MESH = 2,       ///< Cached display mesh.
    CP_TIER_BREP = 3,       ///< B-rep geometry from the modeling kernel stream.
    CP_TIER_VECTOR = 4,     ///< 2D vector entities (DWG/DXF).
    CP_TIER_PLACEHOLDER = 5 ///< Neutral placeholder image.
};

/// Detected file format. Values are only ever appended, never renumbered.
typedef int32_t cp_format;
enum {
    CP_FORMAT_UNKNOWN = 0, ///< Not a supported format.
    CP_FORMAT_DWG = 1,     ///< AutoCAD DWG drawing.
    CP_FORMAT_DXF = 2      ///< AutoCAD DXF drawing (ASCII or binary).
};

/// Why a preview is a placeholder. Values are only ever appended, never renumbered.
typedef int32_t cp_reason;
enum {
    CP_REASON_NONE = 0,                 ///< Not a placeholder.
    CP_REASON_CANNOT_OPEN = 1,          ///< The input could not be opened or read.
    CP_REASON_EMPTY_INPUT = 2,          ///< The input has no bytes.
    CP_REASON_UNKNOWN_FORMAT = 3,       ///< The content matches no supported format.
    CP_REASON_TIER_UNAVAILABLE = 4,     ///< The forced tier does not exist for this format.
    CP_REASON_TIERS_FAILED = 5,         ///< Every applicable tier failed; see reason text.
    CP_REASON_PLACEHOLDER_REQUESTED = 6 ///< The caller forced CP_TIER_PLACEHOLDER.
};

/// Severity of a log message.
typedef int32_t cp_log_level;
enum {
    CP_LOG_DEBUG = 0,   ///< Diagnostic detail.
    CP_LOG_INFO = 1,    ///< Normal progress.
    CP_LOG_WARNING = 2, ///< Something was skipped or degraded.
    CP_LOG_ERROR = 3    ///< An operation failed.
};

/// Receives one log message. It is called only on the thread that made the API call, before
/// that call returns. A callback shared by concurrent calls must be thread-safe. It must not
/// throw or longjmp.
typedef void (*cp_log_fn)(void* user, cp_log_level level, const char* utf8_message);

/// Options for cp_render_file and cp_render_memory. Pass NULL for defaults.
typedef struct cp_options {
    uint32_t struct_size; ///< sizeof(cp_options) as compiled by the caller.
    uint32_t size;        ///< Output edge length in pixels, 16 to 4096; 0 selects 512.
    uint32_t tier;        ///< CP_TIER_AUTO, or the tier (1-5) to force.
    uint32_t reserved;    ///< Must be 0.
    cp_log_fn log;        ///< Optional log callback.
    void* log_user;       ///< Passed unchanged to log.
} cp_options;

/// Opaque rendered preview. Release it with cp_preview_free.
typedef struct cp_preview cp_preview;

/// Facts about a rendered preview. The strings stay valid until the preview is freed.
/// There is no typedef: the name belongs to the function cp_preview_info, so callers write
/// `struct cp_preview_info`, as with POSIX `struct stat` and stat().
struct cp_preview_info {
    uint32_t struct_size;    ///< In: the caller's sizeof. Out: bytes this library filled.
    int32_t ok;              ///< 1 for a real preview, 0 for a placeholder.
    uint32_t tier;           ///< Tier that produced the image, 1 to 5.
    uint32_t width;          ///< Image width in pixels.
    uint32_t height;         ///< Image height in pixels.
    cp_format format;        ///< Detected format.
    cp_reason reason_code;   ///< CP_REASON_NONE when ok is 1.
    uint32_t reserved;       ///< Always 0.
    uint64_t render_time_us; ///< Time spent in the render call, in microseconds.
    const char* format_name; ///< Stable short format id, e.g. "unknown".
    const char* version;     ///< Detected format version, or "".
    const char* reason;      ///< Human-readable reason when ok is 0, otherwise "".
    const char* warnings;    ///< Newline-separated warnings, or "".
};

/// Result of format detection.
typedef struct cp_format_info {
    uint32_t struct_size; ///< In: the caller's sizeof. Out: bytes this library filled.
    cp_format format;     ///< Detected format.
    char name[32];        ///< Stable short format id, NUL-terminated.
    char version[64];     ///< Detected format version, NUL-terminated; empty if unknown.
} cp_format_info;

/// Renders the file at utf8_path, which must be valid UTF-8 (else CP_ERROR_INVALID_ARGUMENT).
/// *out receives the preview, or NULL on error.
CP_API cp_status cp_render_file(const char* utf8_path, const cp_options* opts,
                                cp_preview** out) CP_NOEXCEPT;

/// Renders len bytes at data, which may be NULL only when len is 0. *out as for cp_render_file.
CP_API cp_status cp_render_memory(const void* data, size_t len, const cp_options* opts,
                                  cp_preview** out) CP_NOEXCEPT;

/// Detects the format of the file at utf8_path; unreadable files report CP_FORMAT_UNKNOWN.
CP_API cp_status cp_detect_format(const char* utf8_path, cp_format_info* out) CP_NOEXCEPT;

/// Returns the PNG bytes of a preview; they stay valid until the preview is freed.
CP_API cp_status cp_preview_png(const cp_preview* p, const void** data, size_t* len) CP_NOEXCEPT;

/// Fills *out with facts about a preview.
#if defined(__cplusplus) && defined(__GNUC__) && !defined(__clang__)
// GCC's -Wshadow reports that this function hides the struct's implicit constructor; the
// shared name is intended (see struct cp_preview_info).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif
CP_API cp_status cp_preview_info(const cp_preview* p, struct cp_preview_info* out) CP_NOEXCEPT;
#if defined(__cplusplus) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

/// Releases a preview. Passing NULL does nothing.
CP_API void cp_preview_free(cp_preview* p) CP_NOEXCEPT;

/// Returns the library version, e.g. "0.1.0".
CP_API const char* cp_version(void) CP_NOEXCEPT;

/// Returns the stable snake_case id of a reason code, or "unrecognized"; never NULL.
CP_API const char* cp_reason_name(cp_reason reason) CP_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif
