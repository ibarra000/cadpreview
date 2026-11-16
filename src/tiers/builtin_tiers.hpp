#pragma once

#include "tiers/ladder.hpp"

#include <vector>

namespace cadpreview {

/// Tier 1 (embedded thumbnail) and tier 4 (vector drawing) for DXF files.
[[nodiscard]] std::vector<TierEntry> dxfTiers();

/// Tier 1 (embedded preview) for DWG files, and tier 4 through the separate cadpreview-dwg
/// helper when it is installed.
[[nodiscard]] std::vector<TierEntry> dwgTiers();

} // namespace cadpreview
