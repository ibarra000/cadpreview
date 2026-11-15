#pragma once

#include "tiers/ladder.hpp"

#include <vector>

namespace cadpreview {

/// Tier 1 (embedded thumbnail) and tier 4 (vector drawing) for DXF files.
[[nodiscard]] std::vector<TierEntry> dxfTiers();

/// Tier 1 for DWG files, plus tier 4 (available only in CADPREVIEW_WITH_DWG builds).
[[nodiscard]] std::vector<TierEntry> dwgTiers();

#ifdef CADPREVIEW_WITH_DWG
/// DWG tier 4 through LibreDWG; GPL-only build.
[[nodiscard]] TierEntry dwgVectorTier();
#endif

} // namespace cadpreview
