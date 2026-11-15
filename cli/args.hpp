#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace cadpreview::cli {

enum class Mode : std::uint8_t { Render, Help, Version };

struct Args {
    Mode mode = Mode::Render;
    std::string input;
    std::string output;
    std::uint32_t size = 0; ///< 0 lets the library choose its default.
    std::uint32_t tier = 0;
    bool verbose = false;
};

struct ParseResult {
    std::optional<Args> args;
    std::string error; ///< Set when args is empty.
};

/// Parses argv (including argv[0]). Range checks on --size and --tier are left to the library.
[[nodiscard]] ParseResult parseArgs(std::span<char* const> argv);

extern const char* const USAGE;

} // namespace cadpreview::cli
