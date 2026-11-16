#include "common/errors.hpp"
#include "formats/dwg/preview.hpp"
#include "render/decode/dib.hpp"
#include "render/decode/png_decode.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

// Locates the DWG preview block and decodes whatever image it points to.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto bytes = std::as_bytes(std::span(data, size));
    try {
        const auto preview = cadpreview::dwg::findPreview(bytes);
        static_cast<void>(preview.kind == cadpreview::dwg::ImageKind::Png
                              ? cadpreview::decodePngImage(preview.data)
                              : cadpreview::decodeDib(preview.data));
    } catch (const cadpreview::FormatError&) { // NOLINT(bugprone-empty-catch): expected
    }
    return 0;
}
