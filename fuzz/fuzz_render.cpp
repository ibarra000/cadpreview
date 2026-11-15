#include "common/log.hpp"
#include "tiers/pipeline.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

// Byte 0 picks the output size, byte 1 the tier; the rest is the file content.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    constexpr std::size_t HEADER = 2;
    constexpr std::uint32_t SIZE_STEPS = 8;
    constexpr std::uint32_t SIZE_STEP = 16;
    constexpr std::uint32_t TIER_COUNT = 6;
    if (size < HEADER) {
        return 0;
    }
    const std::span<const std::uint8_t> input(data, size);
    const cadpreview::RenderOptions options{cadpreview::MIN_SIZE +
                                                (input[0] % SIZE_STEPS) * SIZE_STEP,
                                            static_cast<cadpreview::Tier>(input[1] % TIER_COUNT)};
    const auto content = std::as_bytes(input.subspan(HEADER));
    static_cast<void>(cadpreview::renderMemory(content, options, cadpreview::defaultRegistry(),
                                               cadpreview::Logger{}));
    return 0;
}
