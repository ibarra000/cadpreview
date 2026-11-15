#pragma once

#include "common/errors.hpp"
#include "containers/source.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace cadpreview {

/// Bounds-checked little-endian reader over untrusted bytes. Every read past the end throws
/// FormatError and leaves the position unchanged.
class ByteReader {
public:
    explicit ByteReader(std::span<const std::byte> data) noexcept : data_(data) {}

    [[nodiscard]] std::size_t position() const noexcept { return position_; }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - position_; }

    void seek(std::size_t position) {
        if (position > data_.size()) {
            throw FormatError("offset outside the data");
        }
        position_ = position;
    }

    void skip(std::size_t count) { static_cast<void>(bytes(count)); }

    /// The next count bytes, advancing past them.
    [[nodiscard]] std::span<const std::byte> bytes(std::size_t count) {
        if (!inRange(position_, count, data_.size())) {
            throw FormatError("data ends early");
        }
        const auto view = data_.subspan(position_, count);
        position_ += count;
        return view;
    }

    /// A reader over the next count bytes; this reader moves past them.
    [[nodiscard]] ByteReader sub(std::size_t count) { return ByteReader(bytes(count)); }

    [[nodiscard]] std::uint8_t u8() { return static_cast<std::uint8_t>(little(1)); }
    [[nodiscard]] std::uint16_t u16() { return static_cast<std::uint16_t>(little(2)); }
    [[nodiscard]] std::uint32_t u32() { return static_cast<std::uint32_t>(little(4)); }
    [[nodiscard]] std::uint64_t u64() { return little(8); }
    [[nodiscard]] std::int16_t i16() { return static_cast<std::int16_t>(u16()); }
    [[nodiscard]] std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

private:
    std::uint64_t little(std::size_t width) {
        const auto view = bytes(width);
        std::uint64_t value = 0;
        for (std::size_t i = width; i > 0; --i) {
            value = (value << 8U) | static_cast<std::uint64_t>(view[i - 1]);
        }
        return value;
    }

    std::span<const std::byte> data_;
    std::size_t position_ = 0;
};

} // namespace cadpreview
