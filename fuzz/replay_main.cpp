#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

constexpr std::uintmax_t MAX_INPUT_BYTES = std::uintmax_t{64} * 1024 * 1024;

bool replayFile(const std::filesystem::path& file) {
    if (std::filesystem::file_size(file) > MAX_INPUT_BYTES) {
        return false;
    }
    std::ifstream in(file, std::ios::binary);
    const std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    const auto view = std::as_bytes(std::span(bytes));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the harness takes uint8_t
    LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(view.data()), view.size());
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        std::size_t replayed = 0;
        for (const char* arg : std::span(argv, static_cast<std::size_t>(argc)).subspan(1)) {
            const std::filesystem::path root(arg);
            if (std::filesystem::is_regular_file(root)) {
                replayed += static_cast<std::size_t>(replayFile(root));
                continue;
            }
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
                if (entry.is_regular_file()) {
                    replayed += static_cast<std::size_t>(replayFile(entry.path()));
                }
            }
        }
        std::cout << "replayed " << replayed << " inputs\n";
        return replayed > 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::fputs("replay failed: ", stderr);
        std::fputs(error.what(), stderr);
        std::fputs("\n", stderr);
        return 1;
    }
}
