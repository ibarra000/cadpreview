# Script mode: writes OUTPUT (C++) holding the bytes of INPUT as cadpreview::SYMBOL.
file(READ ${INPUT} hex HEX)
string(LENGTH "${hex}" hex_length)
math(EXPR size "${hex_length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "std::byte{0x\\1}," bytes "${hex}")
string(REGEX REPLACE "(([^,]*,){12})" "\\1\n    " bytes "${bytes}")
file(WRITE ${OUTPUT}
"// Generated from ${INPUT}; do not edit.\n"
"#include <array>\n#include <cstddef>\n#include <span>\n\n"
"namespace cadpreview {\n\n"
"namespace {\nconstexpr std::array<std::byte, ${size}> DATA{\n    ${bytes}\n};\n} // namespace\n\n"
"std::span<const std::byte> ${SYMBOL}() noexcept { return DATA; }\n\n"
"} // namespace cadpreview\n")
