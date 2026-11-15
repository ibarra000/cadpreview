#include "common/errors.hpp"
#include "render/compose.hpp"
#include "render/decode/dib.hpp"
#include "render/decode/png_decode.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

// The thumbnail decoders and the canvas composer. Byte 0 picks the decoder.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size < 1) {
        return 0;
    }
    const std::span<const std::uint8_t> input(data, size);
    const auto bytes = std::as_bytes(input.subspan(1));
    try {
        const auto image =
            (input[0] & 1U) != 0 ? cadpreview::decodePngImage(bytes) : cadpreview::decodeDib(bytes);
        static_cast<void>(cadpreview::composeOnCanvas(image, 64));
    } catch (const cadpreview::FormatError&) { // NOLINT(bugprone-empty-catch): expected
    }
    return 0;
}
