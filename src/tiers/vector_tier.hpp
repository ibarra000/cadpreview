#pragma once

#include "render/vector/drawing.hpp"
#include "tiers/ladder.hpp"

#include <cstdint>

namespace cadpreview {

/// Finishes tier 4 for any format: projects 3D wires (see render/vector/projection.hpp) and
/// renders the drawing. An empty drawing fails with a reason that says when only 3D bodies
/// were present; bodies left out of a drawn preview are reported as a warning.
[[nodiscard]] TierOutcome renderVectorTier(Drawing drawing, std::uint32_t size);

} // namespace cadpreview
