#include "args.hpp"
#include "json.hpp"

#include <cadpreview/cadpreview.h>

#include <array>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace {

using cadpreview::cli::Args;
using cadpreview::cli::jsonString;

constexpr int EXIT_OK = 0;
constexpr int EXIT_NO_MEMORY = 1;
constexpr int EXIT_USAGE = 2;

struct PreviewDeleter {
    void operator()(cp_preview* p) const noexcept { cp_preview_free(p); }
};

void write(std::FILE* stream, std::string_view text) noexcept {
    std::fwrite(text.data(), 1, text.size(), stream);
}

void error(std::string_view message) noexcept {
    write(stderr, "cadpreview: ");
    write(stderr, message);
    write(stderr, "\n");
}

void logToStderr(void* /*user*/, cp_log_level level, const char* message) {
    constexpr std::array<std::string_view, 4> NAMES{"debug", "info", "warning", "error"};
    const auto name = (level >= CP_LOG_DEBUG && level <= CP_LOG_ERROR)
                          ? NAMES.at(static_cast<std::size_t>(level))
                          : std::string_view("log");
    write(stderr, "[");
    write(stderr, name);
    write(stderr, "] ");
    write(stderr, message);
    write(stderr, "\n");
}

std::filesystem::path toPath(std::string_view utf8) {
    std::u8string text;
    text.reserve(utf8.size());
    for (const char c : utf8) {
        text.push_back(static_cast<char8_t>(c));
    }
    return {text};
}

bool sameFile(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code error;
    return std::filesystem::equivalent(a, b, error) && !error;
}

// Writes next to the target and renames, so a failed write never leaves a truncated PNG and a
// symlink at the target is replaced rather than followed.
bool writeFile(const std::filesystem::path& target, const void* data, std::size_t length) {
    std::filesystem::path partial = target;
    partial += ".cadpreview-partial";
    std::error_code error;
    {
        std::ofstream out(partial, std::ios::binary);
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(length));
        out.close();
        if (!out) {
            std::filesystem::remove(partial, error);
            return false;
        }
    }
    std::filesystem::rename(partial, target, error);
    if (error) {
        std::filesystem::remove(partial, error);
        return false;
    }
    return true;
}

std::string warningsJson(std::string_view warnings) {
    std::string json = "[";
    while (!warnings.empty()) {
        const auto end = warnings.find('\n');
        json += (json.size() > 1 ? "," : "") + jsonString(warnings.substr(0, end));
        warnings = end == std::string_view::npos ? std::string_view{} : warnings.substr(end + 1);
    }
    return json + "]";
}

std::string resultLine(const Args& args, const struct cp_preview_info& info) {
    const std::string_view version = info.version;
    return std::format(
        R"({{"input":{},"output":{},"format":{},"version":{},"tier":{},"ok":{},)"
        R"("render_ms":{:.3f},"reason":{},"reason_detail":{},"warnings":{}}})",
        jsonString(args.input), jsonString(args.output), jsonString(info.format_name),
        version.empty() ? std::string("null") : jsonString(version), info.tier,
        info.ok != 0 ? "true" : "false", static_cast<double>(info.render_time_us) / 1000.0,
        jsonString(cp_reason_name(info.reason_code)), jsonString(info.reason),
        warningsJson(info.warnings));
}

int render(const Args& args) {
    const std::filesystem::path output = toPath(args.output);
    if (sameFile(toPath(args.input), output)) {
        error("the output path is the input file; refusing to overwrite it");
        return EXIT_USAGE;
    }
    cp_options options{};
    options.struct_size = sizeof(options);
    options.size = args.size;
    options.tier = args.tier;
    options.log = args.verbose ? &logToStderr : nullptr;

    cp_preview* raw = nullptr;
    const cp_status status = cp_render_file(args.input.c_str(), &options, &raw);
    if (status == CP_ERROR_INVALID_ARGUMENT) {
        error("invalid arguments (--size must be 16-4096, --tier 0-5, paths valid UTF-8)");
        return EXIT_USAGE;
    }
    if (status != CP_OK) {
        error("out of memory");
        return EXIT_NO_MEMORY;
    }
    const std::unique_ptr<cp_preview, PreviewDeleter> preview(raw);

    const void* png = nullptr;
    std::size_t length = 0;
    struct cp_preview_info info{};
    info.struct_size = sizeof(info);
    if (cp_preview_png(preview.get(), &png, &length) != CP_OK ||
        cp_preview_info(preview.get(), &info) != CP_OK) {
        error("could not read the preview");
        return EXIT_NO_MEMORY;
    }
    if (!writeFile(output, png, length)) {
        error("cannot write " + args.output);
        return EXIT_USAGE;
    }
    write(stdout, resultLine(args, info) + "\n");
    return EXIT_OK;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const auto parsed = cadpreview::cli::parseArgs({argv, static_cast<std::size_t>(argc)});
        if (!parsed.args) {
            error(parsed.error);
            write(stderr, "\n");
            write(stderr, cadpreview::cli::USAGE);
            return EXIT_USAGE;
        }
        switch (parsed.args->mode) {
        case cadpreview::cli::Mode::Help:
            write(stdout, cadpreview::cli::USAGE);
            return EXIT_OK;
        case cadpreview::cli::Mode::Version:
            write(stdout, std::string("cadpreview ") + cp_version() + "\n");
            return EXIT_OK;
        case cadpreview::cli::Mode::Render:
            break;
        }
        return render(*parsed.args);
    } catch (const std::exception& exception) {
        error(exception.what());
        return EXIT_NO_MEMORY;
    }
}
