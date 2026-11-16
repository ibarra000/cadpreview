#pragma once

#include "render/vector/drawing.hpp"

#include <cstdint>

namespace cadpreview {

/// How 3D content is shown. Top looks down the z axis (the 2D drawing as CAD shows it);
/// Isometric is the south-east isometric view, looking from (1, -1, 1) towards the origin.
enum class View : std::uint8_t { Top, Isometric };

/// Top unless the depth (z range) of the drawing's body edges (solids, surfaces and meshes) is
/// more than 1% of the drawing's plan size, so 2D drawings keep their exact top view even when
/// lines carry stray z values. Drawings with very many arcs (over 50,000) also stay in the
/// top view, since the isometric view turns arcs into polylines. Non-finite points are ignored.
[[nodiscard]] View chooseView(const Drawing& drawing);

/// Maps a world point to view coordinates; y points up in both views.
[[nodiscard]] Point project(Point3 point, View view) noexcept;

/// Returns the drawing with its wires turned into polylines in the chosen view. In the
/// isometric view, 2D content is placed at z = 0: polylines and text positions are projected,
/// and arcs become polylines (an arc seen at an angle is an ellipse).
[[nodiscard]] Drawing flatten(Drawing drawing);

} // namespace cadpreview
