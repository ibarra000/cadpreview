#include "sab_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

// The DWG helper's SAB reader (ACIS solids), with small limits so the fuzzer explores structure
// rather than size. It has no LibreDWG dependency, so it is fuzzed in the default build.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto bytes = std::as_bytes(std::span(data, size));
    try {
        cadpreview::dwghelper::SabLimits limits;
        limits.maxRecords = 20'000;
        limits.maxPoints = 200'000;
        static_cast<void>(cadpreview::dwghelper::sabWireframe(bytes, limits));
    } catch (const cadpreview::dwghelper::SabError&) { // NOLINT(bugprone-empty-catch): expected
    }
    return 0;
}
