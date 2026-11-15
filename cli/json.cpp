#include "json.hpp"

#include <array>

namespace cadpreview::cli {

std::string jsonString(std::string_view text) {
    constexpr std::array<char, 16> HEX{'0', '1', '2', '3', '4', '5', '6', '7',
                                       '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string quoted = "\"";
    quoted.reserve(text.size() + 2);
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') {
            quoted.push_back('\\');
            quoted.push_back(c);
        } else if (c == '\n') {
            quoted.append("\\n");
        } else if (byte < 0x20) {
            quoted.append("\\u00");
            quoted.push_back(HEX.at(byte >> 4U));
            quoted.push_back(HEX.at(byte & 0x0FU));
        } else {
            quoted.push_back(c);
        }
    }
    quoted.push_back('"');
    return quoted;
}

} // namespace cadpreview::cli
