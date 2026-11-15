#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cadpreview {

/// True when [offset, offset + length) lies inside [0, size), without overflowing.
[[nodiscard]] constexpr bool inRange(std::uint64_t offset, std::uint64_t length,
                                     std::uint64_t size) noexcept {
    return offset <= size && length <= size - offset;
}

/// Random-access, bounds-checked view of untrusted input bytes. One instance per render call.
class Source {
public:
    Source() = default;
    virtual ~Source() = default;
    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;
    Source(Source&&) = delete;
    Source& operator=(Source&&) = delete;

    [[nodiscard]] virtual std::uint64_t size() const noexcept = 0;

    /// Fills out with the bytes at offset; false (and out unspecified) when out of range or on
    /// an I/O error.
    [[nodiscard]] virtual bool readAt(std::uint64_t offset, std::span<std::byte> out) = 0;
};

/// Source over caller-owned memory, which must outlive it.
class MemorySource final : public Source {
public:
    explicit MemorySource(std::span<const std::byte> data) noexcept : data_(data) {}

    [[nodiscard]] std::uint64_t size() const noexcept override { return data_.size(); }
    [[nodiscard]] bool readAt(std::uint64_t offset, std::span<std::byte> out) override;

private:
    std::span<const std::byte> data_;
};

struct FileOpenResult {
    std::unique_ptr<Source> source;
    std::string_view error; ///< Display-safe reason when source is null; never contains the path.
};

/// Opens a regular file for reading.
[[nodiscard]] FileOpenResult openFile(const std::filesystem::path& path);

/// Reads the first min(maxBytes, size) bytes; nullopt on an I/O error.
[[nodiscard]] std::optional<std::vector<std::byte>> readPrefix(Source& source,
                                                               std::size_t maxBytes);

} // namespace cadpreview
