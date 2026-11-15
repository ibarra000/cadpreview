#include <cadpreview/cadpreview.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

// Exercises the C boundary: the first STRUCT_BYTES bytes become a cp_options with a fuzzed
// struct_size (kept inside the buffer, as the contract requires), the rest is the file content.
// The resulting preview is then queried through an info struct with another fuzzed size.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    constexpr std::size_t STRUCT_BYTES = 64;
    if (size < STRUCT_BYTES) {
        return 0;
    }
    const std::span<const std::uint8_t> input(data, size);

    alignas(cp_options) std::array<std::uint8_t, STRUCT_BYTES> optionBytes{};
    std::ranges::copy(input.first(STRUCT_BYTES), optionBytes.begin());
    cp_options options{};
    std::memcpy(&options, optionBytes.data(), sizeof(options));
    options.struct_size %= static_cast<std::uint32_t>(STRUCT_BYTES + 1);
    options.log = nullptr; // a fuzzed function pointer would only test the caller's crash
    std::memcpy(optionBytes.data(), &options, sizeof(options));

    const auto content = input.subspan(STRUCT_BYTES);
    cp_preview* preview = nullptr;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): fuzzed struct bytes
    const auto* fuzzedOptions = reinterpret_cast<const cp_options*>(optionBytes.data());
    if (cp_render_memory(content.data(), content.size(), fuzzedOptions, &preview) != CP_OK) {
        return 0;
    }

    alignas(std::uint64_t) std::array<std::uint8_t, 256> infoBytes{};
    const auto infoSize = static_cast<std::uint32_t>(input[1] % (infoBytes.size() + 1));
    std::memcpy(infoBytes.data(), &infoSize, sizeof(infoSize));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): fuzzed struct bytes
    auto* info = reinterpret_cast<struct cp_preview_info*>(infoBytes.data());
    static_cast<void>(cp_preview_info(preview, info));
    const void* png = nullptr;
    std::size_t length = 0;
    static_cast<void>(cp_preview_png(preview, &png, &length));
    cp_preview_free(preview);
    return 0;
}
