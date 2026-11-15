#include "args.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>

namespace cadpreview::cli {

const char* const USAGE =
    "usage: cadpreview <input> -o <output.png> [--size N] [--tier N] [--verbose]\n"
    "       cadpreview --help | --version\n"
    "\n"
    "  -o, --output PATH  PNG file to write\n"
    "  --size N           edge length in pixels, 16-4096 (default 512)\n"
    "  --tier N           0 = automatic (default); 1-5 forces a tier:\n"
    "                     1 thumbnail, 2 mesh, 3 b-rep, 4 vector, 5 placeholder\n"
    "  -v, --verbose      print library log messages to stderr\n"
    "\n"
    "Prints one JSON line describing the result. Exit status: 0 when a PNG was written\n"
    "(including placeholders for unreadable files), 2 on usage errors, 1 when out of memory.\n";

namespace {

std::optional<std::uint32_t> parseNumber(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    const std::string_view::const_pointer first = text.data();
    const std::string_view::const_pointer last = first + text.size(); // NOLINT: pointer range
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::nullopt;
    }
    return value;
}

// Applies an option that takes a value; returns an error message or an empty string.
std::string applyValue(Args& args, std::string_view option, std::string_view value) {
    if (option == "-o" || option == "--output") {
        args.output = value;
        return {};
    }
    const auto number = parseNumber(value);
    if (!number) {
        return std::string(option) + " needs a non-negative integer";
    }
    (option == "--size" ? args.size : args.tier) = *number;
    return {};
}

// Applies a flag or positional argument; returns an error message or an empty string.
std::string applyArgument(Args& args, std::string_view arg) {
    if (arg == "-h" || arg == "--help") {
        args.mode = Mode::Help;
    } else if (arg == "--version") {
        args.mode = Mode::Version;
    } else if (arg == "-v" || arg == "--verbose") {
        args.verbose = true;
    } else if (arg.starts_with('-') && arg != "-") {
        return "unknown option " + std::string(arg);
    } else if (args.input.empty()) {
        args.input = arg;
    } else {
        return "only one input file is accepted";
    }
    return {};
}

bool takesValue(std::string_view arg) {
    return arg == "-o" || arg == "--output" || arg == "--size" || arg == "--tier";
}

} // namespace

ParseResult parseArgs(std::span<char* const> argv) {
    Args args;
    for (std::size_t i = 1; i < argv.size(); ++i) {
        const std::string_view arg = argv[i];
        std::string problem;
        if (takesValue(arg)) {
            if (i + 1 >= argv.size()) {
                return {std::nullopt, std::string(arg) + " needs a value"};
            }
            problem = applyValue(args, arg, argv[++i]);
        } else {
            problem = applyArgument(args, arg);
        }
        if (!problem.empty()) {
            return {std::nullopt, std::move(problem)};
        }
    }
    if (args.mode == Mode::Render && (args.input.empty() || args.output.empty())) {
        return {std::nullopt, "an input file and -o <output.png> are required"};
    }
    return {args, {}};
}

} // namespace cadpreview::cli
