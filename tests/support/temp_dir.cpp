#include "temp_dir.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>

namespace cadpreview::test {

namespace {

std::filesystem::path uniqueName() {
    static std::atomic<unsigned> counter{0};
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    return "cadpreview-test-" + std::to_string(ticks) + "-" + std::to_string(counter++);
}

} // namespace

TempDir::TempDir() : path_(std::filesystem::temp_directory_path() / uniqueName()) {
    std::filesystem::create_directories(path_);
}

TempDir::~TempDir() {
    try {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    } catch (...) { // NOLINT(bugprone-empty-catch): best-effort cleanup of a temp directory
    }
}

std::filesystem::path TempDir::write(const std::filesystem::path& name,
                                     std::span<const std::uint8_t> bytes) const {
    const auto file = path_ / name;
    std::ofstream out(file, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), // NOLINT: byte view for ostream
              static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw std::runtime_error("cannot write test file");
    }
    return file;
}

} // namespace cadpreview::test
