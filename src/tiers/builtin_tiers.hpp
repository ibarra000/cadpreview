#pragma once

#include "tiers/ladder.hpp"

#include <vector>

namespace cadpreview {

/// Tier 1 (embedded thumbnail) and tier 4 (vector drawing) for DXF files.
[[nodiscard]] std::vector<TierEntry> dxfTiers();

} // namespace cadpreview
