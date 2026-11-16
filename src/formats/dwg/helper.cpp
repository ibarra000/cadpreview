#include "formats/dwg/helper.hpp"

#include "common/errors.hpp"
#include "platform/module_path.hpp"
#include "platform/process.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cadpreview::dwg {

namespace {

// see notes/dwg.md#helper-protocol
constexpr std::chrono::seconds HELPER_TIMEOUT{30};
constexpr int HELPER_MEMORY_MB = 2048;
// The helper's DXF is about three times the size of the DWG on the sample drawings.
constexpr std::size_t HELPER_MAX_OUTPUT = std::size_t{128} * 1024 * 1024;
constexpr int EXIT_NOT_DWG = 3;

bool isFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

std::string utf8Of(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    std::string result;
    result.reserve(text.size());
    for (const char8_t c : text) {
        result.push_back(static_cast<char>(c));
    }
    return result;
}

// The helper's error output is untrusted (it has just parsed a hostile file): control
// characters are replaced so it cannot forge or garble lines in the caller's log.
std::string printable(std::string_view text) {
    std::string clean(text);
    for (char& c : clean) {
        const auto byte = static_cast<unsigned char>(c);
        if ((byte < 0x20U && c != '\n') || byte == 0x7FU) {
            c = '?';
        }
    }
    return clean;
}

std::string failureText(const ProcessResult& result) {
    switch (result.end) {
    case ProcessEnd::TimedOut:
        return "DWG helper timed out";
    case ProcessEnd::OutputTooLarge:
        return "DWG helper output too large";
    case ProcessEnd::Crashed:
        return "DWG helper crashed";
    case ProcessEnd::Exited:
        break;
    }
    return result.exitCode == EXIT_NOT_DWG
               ? "DWG helper could not decode the drawing"
               : std::format("DWG helper failed (status {})", result.exitCode);
}

} // namespace

std::optional<std::filesystem::path> locateHelper(const std::filesystem::path& configured) {
    if (!configured.empty()) {
        return isFile(configured) ? std::optional(configured) : std::nullopt;
    }
    const std::filesystem::path module = currentModulePath();
    if (module.empty()) {
        return std::nullopt;
    }
    const std::filesystem::path directory = module.parent_path();
    for (const auto& candidate :
         std::array{directory / HELPER_NAME, directory.parent_path() / "bin" / HELPER_NAME}) {
        if (isFile(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}

std::string runHelper(const std::filesystem::path& helper, const std::filesystem::path& drawing,
                      const Logger& log) {
    ProcessLimits limits;
    limits.timeout = HELPER_TIMEOUT;
    limits.maxOutput = HELPER_MAX_OUTPUT;
    ProcessResult result;
    try {
        result = runProcess(
            helper, {"--max-memory-mb", std::to_string(HELPER_MEMORY_MB), utf8Of(drawing)}, limits);
    } catch (const std::system_error& error) {
        if (log.enabled()) {
            log.debug(std::format("cannot start the DWG helper: {}", error.what()));
        }
        throw FormatError("DWG helper could not be started");
    }
    if (result.end == ProcessEnd::Exited && result.exitCode == 0) {
        return std::move(result.output);
    }
    if (log.enabled() && !result.errorOutput.empty()) {
        log.debug(std::format("DWG helper: {}", printable(result.errorOutput)));
    }
    throw FormatError(failureText(result));
}

} // namespace cadpreview::dwg
