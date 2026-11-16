#include "dxf_text.hpp"

#include <format>

namespace cadpreview::test {

std::vector<std::byte> bytesOf(const std::string& text) {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

std::string dxfText(const Pairs& pairs) {
    std::string text;
    for (const auto& [code, value] : pairs) {
        text += std::to_string(code) + "\n" + value + "\n";
    }
    return text;
}

Pairs section(const std::string& name, const Pairs& body) {
    Pairs pairs{{0, "SECTION"}, {2, name}};
    pairs.insert(pairs.end(), body.begin(), body.end());
    pairs.emplace_back(0, "ENDSEC");
    return pairs;
}

std::string file(std::initializer_list<Pairs> sections) {
    Pairs all;
    for (const Pairs& s : sections) {
        all.insert(all.end(), s.begin(), s.end());
    }
    all.emplace_back(0, "EOF");
    return dxfText(all);
}

Pairs concat(std::initializer_list<Pairs> parts) {
    Pairs all;
    for (const Pairs& p : parts) {
        all.insert(all.end(), p.begin(), p.end());
    }
    return all;
}

std::string num(double value) {
    return std::format("{}", value);
}

} // namespace cadpreview::test
