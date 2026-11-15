#pragma once

#include "dxf_writer.hpp"

#include <libredwg/dwg.h>

namespace cadpreview::dwghelper {

/// Writes the 2D content of a decoded drawing as ASCII DXF: hidden layers (TABLES), block
/// definitions (BLOCKS), then model-space entities followed by paper-space entities marked with
/// group 67 (ENTITIES). Layers and blocks are named by object index ("L12", "B40"), which is
/// all the library needs. see notes/dwg.md#helper-protocol
void writeDrawing(Dwg_Data& dwg, DxfWriter& writer);

} // namespace cadpreview::dwghelper
