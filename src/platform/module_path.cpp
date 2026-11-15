#include "platform/module_path.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <string>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace cadpreview {

#ifdef _WIN32

std::filesystem::path currentModulePath() {
    HMODULE module = nullptr;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): Win32 takes an address
    const auto* address = reinterpret_cast<LPCWSTR>(&currentModulePath);
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           address, &module) == 0) {
        return {};
    }
    constexpr DWORD MAX_PATH_CHARS = 32768; // the longest path Windows supports
    for (DWORD capacity = MAX_PATH; capacity <= MAX_PATH_CHARS; capacity *= 2) {
        std::wstring buffer(capacity, L'\0');
        const DWORD length = GetModuleFileNameW(module, buffer.data(), capacity);
        if (length == 0) {
            return {};
        }
        if (length < capacity) {
            buffer.resize(length);
            return {buffer};
        }
    }
    return {};
}

#else

std::filesystem::path currentModulePath() {
    Dl_info info{};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): dladdr takes an address
    if (dladdr(reinterpret_cast<void*>(&currentModulePath), &info) == 0 ||
        info.dli_fname == nullptr) {
        return {};
    }
    std::error_code error;
    auto path = std::filesystem::weakly_canonical(info.dli_fname, error);
    return error ? std::filesystem::path{} : path;
}

#endif

} // namespace cadpreview
