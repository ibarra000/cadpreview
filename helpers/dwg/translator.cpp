#include "translator.hpp"

#include <cstdlib>
#include <libredwg/dwg_api.h>
#include <numbers>
#include <string>
#include <string_view>

namespace cadpreview::dwghelper {

namespace {

// LibreDWG's C API exposes entity data as unions selected by `fixedtype` and as counted C
// arrays. Every union member is read only after its fixedtype is checked and the pointer is
// tested for null, and every array is indexed below its stored count, so these two checks are
// suppressed for this file only.
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-pro-bounds-pointer-arithmetic)

constexpr double TO_DEGREES = 180.0 / std::numbers::pi; // DWG stores radians, DXF degrees
constexpr unsigned LWPOLYLINE_CLOSED = 512;             // see the flag comment in LibreDWG's dwg.h

// Every DIMENSION_* struct starts with the common dimension fields, including `block`.
template <typename Dimension>
const Dwg_Object* blockOf(const Dimension* dimension) {
    return dimension != nullptr && dimension->block != nullptr ? dimension->block->obj : nullptr;
}

// The anonymous block that holds a dimension's drawn geometry, or null.
const Dwg_Object* dimensionBlock(const Dwg_Object& object) {
    const auto& tio = object.tio.entity->tio;
    switch (object.fixedtype) {
    case DWG_TYPE_DIMENSION_ORDINATE:
        return blockOf(tio.DIMENSION_ORDINATE);
    case DWG_TYPE_DIMENSION_LINEAR:
        return blockOf(tio.DIMENSION_LINEAR);
    case DWG_TYPE_DIMENSION_ALIGNED:
        return blockOf(tio.DIMENSION_ALIGNED);
    case DWG_TYPE_DIMENSION_ANG3PT:
        return blockOf(tio.DIMENSION_ANG3PT);
    case DWG_TYPE_DIMENSION_ANG2LN:
        return blockOf(tio.DIMENSION_ANG2LN);
    case DWG_TYPE_DIMENSION_RADIUS:
        return blockOf(tio.DIMENSION_RADIUS);
    case DWG_TYPE_DIMENSION_DIAMETER:
        return blockOf(tio.DIMENSION_DIAMETER);
    default:
        return nullptr;
    }
}

// Keys stand in for layer and block names; "-" marks a missing reference so it cannot collide
// with the object at index 0.
std::string key(char prefix, const Dwg_Object* object) {
    return std::string(1, prefix) + (object != nullptr ? std::to_string(object->index) : "-");
}

std::string utf8(void* entity, const char* type, const char* field) {
    char* text = nullptr;
    int isNew = 0;
    if (entity == nullptr ||
        !dwg_dynapi_entity_utf8text(entity, type, field, &text, &isNew, nullptr) ||
        text == nullptr) {
        return {};
    }
    std::string result(text);
    if (isNew != 0) {
        // LibreDWG allocated the string with malloc and hands ownership to the caller.
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
        std::free(text);
    }
    return result;
}

class Translator {
public:
    Translator(Dwg_Data& dwg, DxfWriter& writer) : dwg_(&dwg), w_(&writer) {}

    void run() {
        const Dwg_Object* model = dwg_model_space_object(dwg_);
        const Dwg_Object* paper = dwg_paper_space_object(dwg_);
        writeHiddenLayers();
        w_->beginSection("BLOCKS");
        for (BITCODE_BL i = 0; i < dwg_->num_objects; ++i) {
            const Dwg_Object& object = dwg_->object[i];
            if (object.fixedtype == DWG_TYPE_BLOCK_HEADER && &object != model && &object != paper) {
                writeBlock(object);
            }
        }
        w_->endSection();
        w_->beginSection("ENTITIES");
        writeOwned(model);
        paper_ = true;
        writeOwned(paper);
        w_->endSection();
        w_->finish();
    }

private:
    void writeHiddenLayers() {
        w_->beginSection("TABLES");
        w_->group(0, "TABLE");
        w_->group(2, "LAYER");
        for (BITCODE_BL i = 0; i < dwg_->num_objects; ++i) {
            const Dwg_Object& object = dwg_->object[i];
            const Dwg_Object_LAYER* layer =
                object.fixedtype == DWG_TYPE_LAYER && object.tio.object != nullptr
                    ? object.tio.object->tio.LAYER
                    : nullptr;
            if (layer != nullptr && (layer->frozen != 0 || layer->off != 0)) {
                w_->group(0, "LAYER");
                w_->group(2, key('L', &object));
                w_->group(70, "1"); // frozen: the library skips it
            }
        }
        w_->group(0, "ENDTAB");
        w_->endSection();
    }

    void writeBlock(const Dwg_Object& header) {
        const Dwg_Object_BLOCK_HEADER* block =
            header.tio.object != nullptr ? header.tio.object->tio.BLOCK_HEADER : nullptr;
        if (block == nullptr) {
            return;
        }
        w_->group(0, "BLOCK");
        w_->group(2, key('B', &header));
        w_->point(10, block->base_pt.x, block->base_pt.y);
        writeOwned(&header);
        w_->group(0, "ENDBLK");
    }

    void writeOwned(const Dwg_Object* owner) {
        if (owner == nullptr) {
            return;
        }
        for (const Dwg_Object* entity = get_first_owned_entity(owner); entity != nullptr;
             entity = get_next_owned_entity(owner, entity)) {
            if (entity->supertype == DWG_SUPERTYPE_ENTITY && entity->tio.entity != nullptr) {
                translate(*entity);
            }
        }
    }

    // Starts an entity record: type, layer, visibility, and the paper-space mark.
    void common(const Dwg_Object& object, std::string_view type) {
        const Dwg_Object_Entity* entity = object.tio.entity;
        const Dwg_Object* layer = entity->layer != nullptr ? entity->layer->obj : nullptr;
        w_->group(0, type);
        w_->group(8, key('L', layer));
        w_->group(60, entity->invisible != 0 ? "1" : "0");
        if (paper_) {
            w_->group(67, "1");
        }
    }

    void translate(const Dwg_Object& object) {
        if (!translateCurve(object) && !translateArea(object)) {
            static_cast<void>(translateAnnotation(object));
        }
    }

    bool translateCurve(const Dwg_Object& object);
    bool translateArea(const Dwg_Object& object);
    bool translateAnnotation(const Dwg_Object& object);
    void translateLwPolyline(const Dwg_Object& object);
    void translateText(const Dwg_Object& object);
    void translateMText(const Dwg_Object& object);
    void translateInsert(const Dwg_Object& object);
    void translateAttributes(const Dwg_Object& object);
    void translatePolyline(const Dwg_Object& object);
    void translateSpline(const Dwg_Object& object);

    Dwg_Data* dwg_; // never null
    DxfWriter* w_;  // never null
    bool paper_ = false;
};

// Lines, circles, arcs, ellipses and polylines. Returns false for other types.
bool Translator::translateCurve(const Dwg_Object& object) {
    const auto& tio = object.tio.entity->tio;
    switch (object.fixedtype) {
    case DWG_TYPE_LINE:
        if (const auto* line = tio.LINE; line != nullptr) {
            common(object, "LINE");
            w_->point(10, line->start.x, line->start.y);
            w_->point(11, line->end.x, line->end.y);
        }
        return true;
    case DWG_TYPE_CIRCLE:
        if (const auto* circle = tio.CIRCLE; circle != nullptr) {
            common(object, "CIRCLE");
            w_->point(10, circle->center.x, circle->center.y);
            w_->number(40, circle->radius);
        }
        return true;
    case DWG_TYPE_ARC:
        if (const auto* arc = tio.ARC; arc != nullptr) {
            common(object, "ARC");
            w_->point(10, arc->center.x, arc->center.y);
            w_->number(40, arc->radius);
            w_->number(50, arc->start_angle * TO_DEGREES);
            w_->number(51, arc->end_angle * TO_DEGREES);
        }
        return true;
    case DWG_TYPE_ELLIPSE:
        if (const auto* ellipse = tio.ELLIPSE; ellipse != nullptr) {
            common(object, "ELLIPSE");
            w_->point(10, ellipse->center.x, ellipse->center.y);
            w_->point(11, ellipse->sm_axis.x, ellipse->sm_axis.y);
            w_->number(40, ellipse->axis_ratio);
            w_->number(41, ellipse->start_angle);
            w_->number(42, ellipse->end_angle);
        }
        return true;
    case DWG_TYPE_LWPOLYLINE:
        translateLwPolyline(object);
        return true;
    case DWG_TYPE_POLYLINE_2D:
        translatePolyline(object);
        return true;
    case DWG_TYPE_SPLINE:
        translateSpline(object);
        return true;
    default:
        return false;
    }
}

// Filled outlines: SOLID, TRACE and 3DFACE. Returns false for other types.
bool Translator::translateArea(const Dwg_Object& object) {
    const auto& tio = object.tio.entity->tio;
    switch (object.fixedtype) {
    case DWG_TYPE_SOLID:
    case DWG_TYPE_TRACE:
        if (const auto* s = tio.SOLID; s != nullptr) {
            common(object, "SOLID");
            w_->point(10, s->corner1.x, s->corner1.y);
            w_->point(11, s->corner2.x, s->corner2.y);
            w_->point(12, s->corner3.x, s->corner3.y);
            w_->point(13, s->corner4.x, s->corner4.y);
        }
        return true;
    case DWG_TYPE__3DFACE:
        if (const auto* f = tio._3DFACE; f != nullptr) {
            common(object, "3DFACE");
            w_->point(10, f->corner1.x, f->corner1.y);
            w_->point(11, f->corner2.x, f->corner2.y);
            w_->point(12, f->corner3.x, f->corner3.y);
            w_->point(13, f->corner4.x, f->corner4.y);
        }
        return true;
    default:
        return false;
    }
}

// Text, block references, leaders and dimensions. Returns false for other types.
bool Translator::translateAnnotation(const Dwg_Object& object) {
    switch (object.fixedtype) {
    case DWG_TYPE_TEXT:
        translateText(object);
        return true;
    case DWG_TYPE_MTEXT:
        translateMText(object);
        return true;
    case DWG_TYPE_INSERT:
    case DWG_TYPE_MINSERT:
        translateInsert(object);
        return true;
    case DWG_TYPE_LEADER:
        if (const auto* leader = object.tio.entity->tio.LEADER;
            leader != nullptr && leader->points != nullptr) {
            common(object, "LEADER");
            for (BITCODE_BL i = 0; i < leader->num_points; ++i) {
                w_->point(10, leader->points[i].x, leader->points[i].y);
            }
        }
        return true;
    default:
        if (const Dwg_Object* block = dimensionBlock(object); block != nullptr) {
            common(object, "DIMENSION");
            w_->group(2, key('B', block));
            return true;
        }
        return false;
    }
}

void Translator::translateLwPolyline(const Dwg_Object& object) {
    const Dwg_Entity_LWPOLYLINE* lw = object.tio.entity->tio.LWPOLYLINE;
    if (lw == nullptr || lw->points == nullptr) {
        return;
    }
    common(object, "LWPOLYLINE");
    w_->group(70, (lw->flag & LWPOLYLINE_CLOSED) != 0 ? "1" : "0");
    for (BITCODE_BL i = 0; i < lw->num_points; ++i) {
        w_->point(10, lw->points[i].x, lw->points[i].y);
        if (lw->bulges != nullptr && i < lw->num_bulges) {
            w_->number(42, lw->bulges[i]);
        }
    }
}

void Translator::translateText(const Dwg_Object& object) {
    const Dwg_Entity_TEXT* t = object.tio.entity->tio.TEXT;
    if (t == nullptr) {
        return;
    }
    common(object, "TEXT");
    w_->point(10, t->ins_pt.x, t->ins_pt.y);
    w_->point(11, t->alignment_pt.x, t->alignment_pt.y);
    w_->number(40, t->height);
    w_->number(50, t->rotation * TO_DEGREES);
    w_->number(41, t->width_factor != 0 ? t->width_factor : 1.0);
    w_->number(72, t->horiz_alignment);
    w_->number(73, t->vert_alignment);
    w_->group(1, utf8(object.tio.entity->tio.TEXT, "TEXT", "text_value"));
}

void Translator::translateMText(const Dwg_Object& object) {
    const Dwg_Entity_MTEXT* m = object.tio.entity->tio.MTEXT;
    if (m == nullptr) {
        return;
    }
    common(object, "MTEXT");
    w_->point(10, m->ins_pt.x, m->ins_pt.y);
    w_->point(11, m->x_axis_dir.x, m->x_axis_dir.y);
    w_->number(40, m->text_height);
    w_->number(71, m->attachment);
    w_->longText(utf8(object.tio.entity->tio.MTEXT, "MTEXT", "text"));
}

void Translator::translateInsert(const Dwg_Object& object) {
    const auto& tio = object.tio.entity->tio;
    const bool array = object.fixedtype == DWG_TYPE_MINSERT;
    const Dwg_Entity_INSERT* insert = array ? nullptr : tio.INSERT;
    const Dwg_Entity_MINSERT* minsert = array ? tio.MINSERT : nullptr;
    if (insert == nullptr && minsert == nullptr) {
        return;
    }
    const Dwg_Object_Ref* header = array ? minsert->block_header : insert->block_header;
    if (header == nullptr || header->obj == nullptr) {
        return;
    }
    const auto& ins = array ? minsert->ins_pt : insert->ins_pt;
    const auto& scale = array ? minsert->scale : insert->scale;
    common(object, "INSERT");
    w_->group(2, key('B', header->obj));
    w_->point(10, ins.x, ins.y);
    w_->number(41, scale.x);
    w_->number(42, scale.y);
    w_->number(50, (array ? minsert->rotation : insert->rotation) * TO_DEGREES);
    if (array) {
        w_->number(70, minsert->num_cols);
        w_->number(71, minsert->num_rows);
        w_->number(44, minsert->col_spacing);
        w_->number(45, minsert->row_spacing);
    }
    translateAttributes(object);
}

void Translator::translateAttributes(const Dwg_Object& object) {
    for (const Dwg_Object* sub = get_first_owned_subentity(&object); sub != nullptr;
         sub = get_next_owned_subentity(&object, sub)) {
        if (sub->fixedtype != DWG_TYPE_ATTRIB || sub->tio.entity == nullptr) {
            continue;
        }
        const Dwg_Entity_ATTRIB* a = sub->tio.entity->tio.ATTRIB;
        if (a == nullptr) {
            continue;
        }
        common(*sub, "ATTRIB");
        w_->point(10, a->ins_pt.x, a->ins_pt.y);
        w_->point(11, a->alignment_pt.x, a->alignment_pt.y);
        w_->number(40, a->height);
        w_->number(50, a->rotation * TO_DEGREES);
        w_->number(41, a->width_factor != 0 ? a->width_factor : 1.0);
        w_->number(72, a->horiz_alignment);
        w_->number(73, a->vert_alignment);
        w_->group(1, utf8(sub->tio.entity->tio.ATTRIB, "ATTRIB", "text_value"));
    }
}

void Translator::translatePolyline(const Dwg_Object& object) {
    const Dwg_Entity_POLYLINE_2D* polyline = object.tio.entity->tio.POLYLINE_2D;
    if (polyline == nullptr) {
        return;
    }
    common(object, "POLYLINE");
    w_->number(70, polyline->flag);
    for (const Dwg_Object* sub = get_first_owned_subentity(&object); sub != nullptr;
         sub = get_next_owned_subentity(&object, sub)) {
        if (sub->fixedtype != DWG_TYPE_VERTEX_2D || sub->tio.entity == nullptr) {
            continue;
        }
        if (const Dwg_Entity_VERTEX_2D* v = sub->tio.entity->tio.VERTEX_2D; v != nullptr) {
            w_->group(0, "VERTEX");
            w_->point(10, v->point.x, v->point.y);
            w_->number(42, v->bulge);
            w_->number(70, v->flag);
        }
    }
    w_->group(0, "SEQEND");
}

void Translator::translateSpline(const Dwg_Object& object) {
    const Dwg_Entity_SPLINE* s = object.tio.entity->tio.SPLINE;
    if (s == nullptr) {
        return;
    }
    common(object, "SPLINE");
    w_->number(71, s->degree);
    w_->group(70, s->closed_b != 0 ? "1" : "0");
    for (BITCODE_BL i = 0; s->knots != nullptr && i < s->num_knots; ++i) {
        w_->number(40, s->knots[i]);
    }
    for (BITCODE_BL i = 0; s->ctrl_pts != nullptr && i < s->num_ctrl_pts; ++i) {
        w_->point(10, s->ctrl_pts[i].x, s->ctrl_pts[i].y);
    }
    for (BITCODE_BL i = 0; s->weighted != 0 && s->ctrl_pts != nullptr && i < s->num_ctrl_pts; ++i) {
        w_->number(41, s->ctrl_pts[i].w);
    }
    for (BITCODE_BS i = 0; s->fit_pts != nullptr && i < s->num_fit_pts; ++i) {
        w_->point(11, s->fit_pts[i].x, s->fit_pts[i].y);
    }
}

// NOLINTEND(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-pro-bounds-pointer-arithmetic)

} // namespace

void writeDrawing(Dwg_Data& dwg, DxfWriter& writer) {
    Translator(dwg, writer).run();
}

} // namespace cadpreview::dwghelper
