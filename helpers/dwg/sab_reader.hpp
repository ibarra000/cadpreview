#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

// Reads the edges of an ACIS body stored as SAB (binary ACIS, "ACIS BinaryFile"), as found in
// the 3DSOLID, REGION, BODY and surface entities of DWG files from AutoCAD 2007 onwards. Only
// the tags and records observed in sample files are understood. see notes/acis.md
//
// This file has no LibreDWG dependency; it is compiled into the DWG helper and into the unit
// tests and fuzz harness of the default build.
namespace cadpreview::dwghelper {

struct Vec3 {
    double x = 0;
    double y = 0;
    double z = 0;
};

/// A polyline through 3D points; a closed curve repeats its first point at the end.
using Polyline3 = std::vector<Vec3>;

class SabError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct SabLimits {
    std::size_t maxBytes = std::size_t{64} * 1024 * 1024;
    std::size_t maxRecords = 1'000'000;
    std::size_t maxPoints = 2'000'000; ///< output points over all polylines
    /// Values kept from the records the wireframe uses (32 bytes each); at most 2^32 - 1.
    std::size_t maxValues = 2'000'000;
};

/// The body's wireframe: one polyline per edge (straight edges as segments, elliptical edges
/// sampled, other curves as the chord between their vertices) and, for a body with no edges,
/// three great circles of each sphere. Edges whose references do not resolve, and polylines
/// that would have a non-finite point, are skipped.
/// Throws SabError when the data is not SAB, is truncated or malformed, holds a non-finite
/// number, or exceeds a limit.
[[nodiscard]] std::vector<Polyline3> sabWireframe(std::span<const std::byte> data,
                                                  const SabLimits& limits = {});

} // namespace cadpreview::dwghelper
