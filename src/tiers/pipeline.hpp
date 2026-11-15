#pragma once

#include "common/log.hpp"
#include "tiers/ladder.hpp"

#include <cstddef>
#include <filesystem>
#include <span>

namespace cadpreview {

/// Detects the format of source and runs the ladder. Never throws for bad input; only
/// allocation failure while building the placeholder escapes.
[[nodiscard]] Preview renderSource(Source& source, const RenderOptions& options,
                                   const TierRegistry& registry, const Logger& log);

/// renderSource over a file; files that cannot be opened yield a CannotOpen placeholder.
[[nodiscard]] Preview renderFile(const std::filesystem::path& path, const RenderOptions& options,
                                 const TierRegistry& registry, const Logger& log);

/// renderSource over caller-owned memory.
[[nodiscard]] Preview renderMemory(std::span<const std::byte> data, const RenderOptions& options,
                                   const TierRegistry& registry, const Logger& log);

} // namespace cadpreview
