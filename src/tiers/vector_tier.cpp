#include "tiers/vector_tier.hpp"

#include "render/vector/cairo_renderer.hpp"
#include "render/vector/projection.hpp"

#include <format>
#include <utility>

namespace cadpreview {

TierOutcome renderVectorTier(Drawing drawing, std::uint32_t size) {
    const std::size_t undrawn = drawing.undrawnBodies;
    if (drawing.primitiveCount() == 0) {
        if (undrawn > 0) {
            return {std::nullopt,
                    std::format("the drawing holds only 3D solids or surfaces ({}), which tier "
                                "4 cannot draw",
                                undrawn),
                    {}};
        }
        return {std::nullopt, "no visible entities", {}};
    }
    TierOutcome outcome{renderDrawing(flatten(std::move(drawing)), size), {}, {}};
    if (undrawn > 0) {
        outcome.warnings.push_back(
            std::format("{} 3D solids or surfaces were not drawn", undrawn));
    }
    return outcome;
}

} // namespace cadpreview
