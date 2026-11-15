// Stands in for the cadpreview-dwg helper in tests, so process handling and the DWG vector tier
// can be checked without LibreDWG.
//
//   fake_helper --echo ARG...   prints each argument on its own line
//   fake_helper --flood         writes output until killed
//   fake_helper --sleep         sleeps for a minute
//   fake_helper --fail          prints to stderr and exits with status 3
//   fake_helper --crash         terminates abnormally
//   fake_helper --spawn PROGRAM starts PROGRAM --sleep sharing its output, prints "blocked" if
//                               that is refused, then exits
//   fake_helper [OPTIONS] FILE  helper mode: behaves as the MODE=<mode> marker in FILE says
//                               (flood, sleep, fail, crash); without a marker it prints a DXF
//                               drawing of one line from (0,0) to (10,10)

#include <array>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <thread>

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
#include <spawn.h>
#include <unistd.h>

extern char** environ; // NOLINT(readability-redundant-declaration): POSIX declares it nowhere
#endif

namespace {

constexpr int FAILED = 3;

int run(std::string_view mode) {
    if (mode == "flood") {
        const std::string block(std::size_t{64} * 1024, 'x');
        for (;;) {
            std::cout << block;
        }
    }
    if (mode == "sleep") {
        std::this_thread::sleep_for(std::chrono::minutes(1));
        return 0;
    }
    if (mode == "fail") {
        std::cerr << "fake helper: cannot decode\n";
        return FAILED;
    }
    if (mode == "crash") {
#ifdef _WIN32
        std::_Exit(static_cast<int>(0xC0000005U)); // the status of an access violation
#else
        std::raise(SIGKILL);
#endif
    }
    std::cout << "0\nSECTION\n2\nENTITIES\n0\nLINE\n10\n0\n20\n0\n11\n10\n21\n10\n"
                 "0\nENDSEC\n0\nEOF\n";
    return 0;
}

// Starts a grandchild that inherits standard output and sleeps, then returns at once: a runner
// that waits for the output pipe to close would hang unless it kills the grandchild too.
int spawnGrandchild(const char* program) {
#ifdef _WIN32
    std::wstring line = L"\"";
    for (const char* c = program; *c != '\0'; ++c) { // NOLINT: test-only ASCII widening
        line.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
    }
    line += L"\" --sleep";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    if (CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup,
                       &info) == 0) {
        std::cout << "blocked\n";
        return 0;
    }
    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);
#else
    std::string path = program;
    std::string sleep = "--sleep";
    std::array<char*, 3> argv{path.data(), sleep.data(), nullptr};
    pid_t pid = 0;
    if (posix_spawn(&pid, path.c_str(), nullptr, nullptr, argv.data(), environ) != 0) {
        std::cout << "blocked\n";
    }
#endif
    std::cout << "spawned\n" << std::flush;
    return 0;
}

std::string modeIn(const char* path) {
    std::ifstream file(path, std::ios::binary);
    const std::string content{std::istreambuf_iterator<char>(file), {}};
    const auto at = content.find("MODE=");
    if (at == std::string::npos) {
        return "dxf";
    }
    const auto end = content.find_first_of(" \r\n", at);
    return content.substr(at + 5, end == std::string::npos ? std::string::npos : end - at - 5);
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    static_cast<void>(_setmode(_fileno(stdout), _O_BINARY));
#endif
    const std::span<char*> args(argv, static_cast<std::size_t>(argc));
    if (args.size() < 2) {
        return 2;
    }
    const std::string_view first = args[1];
    if (first == "--echo") {
        for (const char* arg : args.subspan(2)) {
            std::cout << arg << '\n';
        }
        return 0;
    }
    if (first == "--spawn" && args.size() == 3) {
        return spawnGrandchild(args[2]);
    }
    if (first.starts_with("--") && first != "--max-memory-mb") {
        return run(first.substr(2));
    }
    return run(modeIn(args.back()));
}
