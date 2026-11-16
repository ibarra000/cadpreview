#pragma once

#include <cstddef>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

// Builders for ASCII DXF test input, laid out as in the published DXF Reference.
namespace cadpreview::test {

using Pairs = std::vector<std::pair<int, std::string>>;

[[nodiscard]] std::vector<std::byte> bytesOf(const std::string& text);
/// Group pairs as DXF text: code line, value line.
[[nodiscard]] std::string dxfText(const Pairs& pairs);
[[nodiscard]] Pairs section(const std::string& name, const Pairs& body);
/// The sections followed by EOF.
[[nodiscard]] std::string file(std::initializer_list<Pairs> sections);
[[nodiscard]] Pairs concat(std::initializer_list<Pairs> parts);
/// A number as DXF text, in the shortest form that reads back exactly.
[[nodiscard]] std::string num(double value);

} // namespace cadpreview::test
