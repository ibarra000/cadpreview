// cadpreview-dwg: decodes one DWG file with LibreDWG and writes its 2D content to standard
// output as ASCII DXF. This program is GPL-3.0-or-later because it links LibreDWG; the
// cadpreview library starts it as a separate process. see notes/dwg.md#helper-protocol
//
//   cadpreview-dwg [--max-memory-mb N] FILE.dwg
//
// Exit status: 0 drawing written; 2 usage error; 3 not a DWG file or not decodable;
// 4 internal error; 5 out of memory.

#include "dxf_writer.hpp"
#include "translator.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libredwg/dwg.h>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/resource.h>
#endif

namespace {

constexpr int EXIT_USAGE = 2;
constexpr int EXIT_NOT_DWG = 3;
constexpr int EXIT_INTERNAL = 4;
constexpr int EXIT_NO_MEMORY = 5;
constexpr std::uintmax_t MAX_INPUT_BYTES = std::uintmax_t{256} * 1024 * 1024; // as the library
constexpr std::uint64_t MEBIBYTE = std::uint64_t{1024} * 1024;

struct Arguments {
    std::uint64_t maxMemoryMb = 0; // 0: no limit
    std::string file;              // UTF-8 (the Windows build runs with the UTF-8 code page)
};

std::optional<Arguments> parse(std::span<char*> args) {
    Arguments parsed;
    std::size_t i = 1;
    if (args.size() == 4 && std::string_view(args[1]) == "--max-memory-mb") {
        const std::string_view value = args[2];
        const char* end = std::to_address(value.end());
        const auto result =
            std::from_chars(std::to_address(value.begin()), end, parsed.maxMemoryMb);
        if (result.ec != std::errc{} || result.ptr != end) {
            return std::nullopt;
        }
        i = 3;
    }
    if (args.size() != i + 1) {
        return std::nullopt;
    }
    parsed.file = args[i];
    return parsed;
}

// The helper limits its own memory (the library's job object on Windows limits only the
// process count). Returns false if the limit could not be applied; the helper then refuses to
// run rather than parse untrusted input without it.
bool limitMemory(std::uint64_t megabytes) {
    if (megabytes == 0) {
        return true;
    }
    const std::uint64_t bytes = megabytes * MEBIBYTE;
#ifdef _WIN32
    HANDLE job = CreateJobObjectW(nullptr, nullptr); // released when the process exits
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
    limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limit.ProcessMemoryLimit = static_cast<SIZE_T>(bytes);
    const bool applied = job != nullptr &&
                         SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limit,
                                                 sizeof(limit)) != 0 &&
                         AssignProcessToJobObject(job, GetCurrentProcess()) != 0;
#else
    const rlimit limit{bytes, bytes};
    const bool applied = setrlimit(RLIMIT_AS, &limit) == 0;
#endif
    return applied;
}

std::filesystem::path pathOf(const std::string& utf8) {
    return {std::u8string(utf8.begin(), utf8.end())};
}

// Cheap checks before LibreDWG sees the file: the size cap and the "AC10" version prefix.
bool looksLikeDwg(const std::filesystem::path& path) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size > MAX_INPUT_BYTES) {
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    std::array<char, 4> magic{};
    in.read(magic.data(), magic.size());
    return in && std::string_view(magic.data(), magic.size()) == "AC10";
}

struct DwgData {
    Dwg_Data data{};
    DwgData() = default;
    ~DwgData() { dwg_free(&data); }
    DwgData(const DwgData&) = delete;
    DwgData& operator=(const DwgData&) = delete;
    DwgData(DwgData&&) = delete;
    DwgData& operator=(DwgData&&) = delete;
};

int convert(const Arguments& arguments) {
    if (!looksLikeDwg(pathOf(arguments.file))) {
        std::cerr << "cadpreview-dwg: not a DWG file\n";
        return EXIT_NOT_DWG;
    }
    DwgData dwg;
    dwg.data.opts = 0; // LibreDWG log level: quiet
    if (dwg_read_file(arguments.file.c_str(), &dwg.data) >= DWG_ERR_CRITICAL) {
        std::cerr << "cadpreview-dwg: LibreDWG could not decode the drawing\n";
        return EXIT_NOT_DWG;
    }
    cadpreview::dwghelper::DxfWriter writer(std::cout);
    cadpreview::dwghelper::writeDrawing(dwg.data, writer);
    if (!std::cout) {
        std::cerr << "cadpreview-dwg: cannot write the output\n";
        return EXIT_INTERNAL;
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        static_cast<void>(_setmode(_fileno(stdout), _O_BINARY)); // no CRLF translation
#endif
        std::ios::sync_with_stdio(false);
        const auto arguments = parse(std::span(argv, static_cast<std::size_t>(argc)));
        if (!arguments) {
            std::cerr << "usage: cadpreview-dwg [--max-memory-mb N] FILE.dwg\n";
            return EXIT_USAGE;
        }
        if (!limitMemory(arguments->maxMemoryMb)) {
            std::cerr << "cadpreview-dwg: cannot apply the memory limit\n";
            return EXIT_INTERNAL;
        }
        return convert(*arguments);
    } catch (const std::bad_alloc&) {
        return EXIT_NO_MEMORY;
    } catch (const std::exception& error) {
        // C stdio cannot throw, unlike std::cerr inside a handler.
        std::fputs("cadpreview-dwg: ", stderr);
        std::fputs(error.what(), stderr);
        std::fputs("\n", stderr);
        return EXIT_INTERNAL;
    } catch (...) {
        return EXIT_INTERNAL;
    }
}
