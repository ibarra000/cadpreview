#include "formats/detect.hpp"

#include "formats/dxf/tokenizer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace cadpreview {

namespace {

// see notes/containers.md#compound-file-signature
constexpr std::array<std::byte, 8> CFB_SIGNATURE{std::byte{0xD0}, std::byte{0xCF}, std::byte{0x11},
                                                 std::byte{0xE0}, std::byte{0xA1}, std::byte{0xB1},
                                                 std::byte{0x1A}, std::byte{0xE1}};

// see notes/containers.md#zip-local-file-header
constexpr std::array<std::byte, 4> ZIP_LOCAL_HEADER{std::byte{0x50}, std::byte{0x4B},
                                                    std::byte{0x03}, std::byte{0x04}};

// Enough for the start of a DXF HEADER section, where $ACADVER is the first variable.
constexpr std::size_t SNIFF_BYTES = 64 * 1024;
constexpr std::size_t DWG_VERSION_LENGTH = 6;

bool startsWith(std::span<const std::byte> data, std::span<const std::byte> prefix) noexcept {
    return data.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin());
}

Container sniffContainer(std::span<const std::byte> prefix) noexcept {
    if (startsWith(prefix, CFB_SIGNATURE)) {
        return Container::CompoundFile;
    }
    if (startsWith(prefix, ZIP_LOCAL_HEADER)) {
        return Container::Zip;
    }
    return Container::None;
}

// "AC10" followed by two digits; see notes/dwg.md#version-string
std::string dwgVersion(std::span<const std::byte> prefix) {
    if (prefix.size() < DWG_VERSION_LENGTH) {
        return {};
    }
    std::string version;
    for (const std::byte b : prefix.first(DWG_VERSION_LENGTH)) {
        version.push_back(static_cast<char>(b));
    }
    const bool digits =
        version[4] >= '0' && version[4] <= '9' && version[5] >= '0' && version[5] <= '9';
    return version.starts_with("AC10") && digits ? version : std::string{};
}

std::string_view unknownDetail(Container container) {
    switch (container) {
    case Container::CompoundFile:
        return "unrecognized OLE compound file";
    case Container::Zip:
        return "unrecognized ZIP archive";
    case Container::None:
        break;
    }
    return "unrecognized file format";
}

} // namespace

Detection detect(Source& source) {
    Detection detection;
    const auto prefix = readPrefix(source, SNIFF_BYTES);
    if (!prefix) {
        detection.readFailed = true;
        detection.detail = "read error";
        return detection;
    }
    if (auto version = dwgVersion(*prefix); !version.empty()) {
        detection.format = Format::Dwg;
        detection.version = std::move(version);
        return detection;
    }
    if (dxf::isBinaryDxf(*prefix)) {
        detection.format = Format::Dxf;
        detection.detail = "binary DXF";
        return detection;
    }
    if (dxf::looksLikeAsciiDxf(*prefix)) {
        detection.format = Format::Dxf;
        detection.version = dxf::headerVersion(*prefix);
        return detection;
    }
    detection.container = sniffContainer(*prefix);
    detection.detail = std::string(unknownDetail(detection.container));
    return detection;
}

Detection detectFile(const std::filesystem::path& path) {
    const FileOpenResult opened = openFile(path);
    if (!opened.source) {
        Detection detection;
        detection.readFailed = true;
        detection.detail = std::string(opened.error);
        return detection;
    }
    return detect(*opened.source);
}

} // namespace cadpreview
