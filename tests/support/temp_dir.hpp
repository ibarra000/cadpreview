#pragma once

#include <cstdint>
#include <filesystem>
#include <span>

namespace cadpreview::test {

/// Creates a unique directory under the system temp directory and removes it on destruction.
class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }
    [[nodiscard]] std::filesystem::path write(const std::filesystem::path& name,
                                              std::span<const std::uint8_t> bytes) const;

private:
    std::filesystem::path path_;
};

} // namespace cadpreview::test
