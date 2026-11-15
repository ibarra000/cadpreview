#include "containers/source.hpp"

#include "common/errors.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <system_error>

namespace cadpreview {

namespace {

class FileSource final : public Source {
public:
    FileSource(std::filesystem::path path, std::ifstream stream, std::uint64_t size)
        : path_(std::move(path)), stream_(std::move(stream)), size_(size) {}

    [[nodiscard]] const std::filesystem::path* filePath() const noexcept override { return &path_; }

    [[nodiscard]] std::uint64_t size() const noexcept override { return size_; }

    [[nodiscard]] bool readAt(std::uint64_t offset, std::span<std::byte> out) override {
        if (!inRange(offset, out.size(), size_)) {
            return false;
        }
        if (out.empty()) {
            return true;
        }
        stream_.clear();
        stream_.seekg(static_cast<std::streamoff>(offset));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): istream reads char
        stream_.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        return stream_.gcount() == static_cast<std::streamsize>(out.size());
    }

private:
    std::filesystem::path path_;
    std::ifstream stream_;
    std::uint64_t size_;
};

} // namespace

bool MemorySource::readAt(std::uint64_t offset, std::span<std::byte> out) {
    if (!inRange(offset, out.size(), data_.size())) {
        return false;
    }
    const auto chunk = data_.subspan(static_cast<std::size_t>(offset), out.size());
    std::ranges::copy(chunk, out.begin());
    return true;
}

FileOpenResult openFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (error || !std::filesystem::exists(status)) {
        return {nullptr, "file not found"};
    }
    if (!std::filesystem::is_regular_file(status)) {
        return {nullptr, "not a regular file"};
    }
    const std::uint64_t size = std::filesystem::file_size(path, error);
    if (error || size > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
        return {nullptr, "cannot determine file size"};
    }
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {nullptr, "cannot open file"};
    }
    return {std::make_unique<FileSource>(path, std::move(stream), size), {}};
}

std::vector<std::byte> readAll(Source& source, std::uint64_t maxBytes) {
    if (source.size() > maxBytes) {
        throw FormatError("file too large to read into memory");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(source.size()));
    if (!source.readAt(0, bytes)) {
        throw FormatError("read error");
    }
    return bytes;
}

std::optional<std::vector<std::byte>> readPrefix(Source& source, std::size_t maxBytes) {
    const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(source.size(), maxBytes));
    std::vector<std::byte> bytes(length);
    if (!source.readAt(0, bytes)) {
        return std::nullopt;
    }
    return bytes;
}

} // namespace cadpreview
