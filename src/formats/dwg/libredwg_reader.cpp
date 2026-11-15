#include "formats/dwg/libredwg_reader.hpp"

#include "common/errors.hpp"
#include "containers/source.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <libredwg/dwg.h>
#include <libredwg/dwg_api.h>
#include <mutex>
#include <numbers>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace cadpreview::dwg {

namespace {

// LibreDWG's C API exposes entity data as unions selected by `fixedtype` and as counted C
// arrays. Every union member is read only after its fixedtype is checked and the pointer is
// tested for null, and every array is indexed below its stored count, so these two checks are
// suppressed for this adapter only.
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-pro-bounds-pointer-arithmetic)

constexpr double TO_DEGREES = 180.0 / std::numbers::pi; // DWG stores radians, DXF degrees
constexpr unsigned LWPOLYLINE_CLOSED = 512;             // see the flag comment in LibreDWG's dwg.h

using Records = std::vector<dxf::Record>;

// LibreDWG has process-wide state (log level, internal buffers), so decoding is serialized.
std::mutex& libreDwgMutex() {
    static std::mutex mutex;
    return mutex;
}

struct DwgData {
    Dwg_Data data{};
    DwgData() = default;
    ~DwgData() { dwg_free(&data); }
    DwgData(const DwgData&) = delete;
    DwgData& operator=(const DwgData&) = delete;
    DwgData(DwgData&&) = delete;
    DwgData& operator=(DwgData&&) = delete;
};

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

// Builds one dxf::Record; values are stored in the document's string deque, which never moves
// its elements, so the record's views stay valid.
class Builder {
public:
    Builder(TranslatedDocument& out, std::string_view type) : out_(&out) { record_.type = type; }

    Builder& text(int code, std::string value) {
        out_->strings.push_back(std::move(value));
        record_.groups.push_back({code, out_->strings.back()});
        return *this;
    }
    Builder& number(int code, double value) {
        std::array<char, 32> buffer{};
        const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        return text(code, std::string(buffer.data(), result.ptr));
    }
    Builder& point(int xCode, double x, double y) { return number(xCode, x).number(xCode + 10, y); }
    dxf::Record take() { return std::move(record_); }

private:
    TranslatedDocument* out_; // never null
    dxf::Record record_;
};

// Where translated records go: a block's entity list or the document's, in model or paper space.
struct Target {
    Records* records; // never null
    bool paper;
};

class Translator {
public:
    Translator(Dwg_Data& dwg, TranslatedDocument& out) : dwg_(&dwg), out_(&out) {}

    void run() {
        const Dwg_Object* model = dwg_model_space_object(dwg_);
        const Dwg_Object* paper = dwg_paper_space_object(dwg_);
        for (BITCODE_BL i = 0; i < dwg_->num_objects; ++i) {
            const Dwg_Object& object = dwg_->object[i];
            if (object.fixedtype == DWG_TYPE_LAYER) {
                addLayer(object);
            } else if (object.fixedtype == DWG_TYPE_BLOCK_HEADER && &object != model &&
                       &object != paper) {
                addBlock(object);
            }
        }
        appendOwned(model, {&out_->document.entities, false});
        appendOwned(paper, {&out_->document.entities, true});
    }

private:
    void addLayer(const Dwg_Object& object) {
        const Dwg_Object_LAYER* layer =
            object.tio.object != nullptr ? object.tio.object->tio.LAYER : nullptr;
        if (layer != nullptr && (layer->frozen != 0 || layer->off != 0)) {
            out_->document.hiddenLayers.insert(key('L', &object));
        }
    }

    void addBlock(const Dwg_Object& header) {
        const Dwg_Object_BLOCK_HEADER* block =
            header.tio.object != nullptr ? header.tio.object->tio.BLOCK_HEADER : nullptr;
        if (block == nullptr) {
            return;
        }
        dxf::Block converted{{block->base_pt.x, block->base_pt.y}, {}};
        appendOwned(&header, {&converted.entities, false});
        out_->document.blocks.insert_or_assign(key('B', &header), std::move(converted));
    }

    void appendOwned(const Dwg_Object* owner, Target target) {
        if (owner == nullptr) {
            return;
        }
        for (const Dwg_Object* entity = get_first_owned_entity(owner); entity != nullptr;
             entity = get_next_owned_entity(owner, entity)) {
            if (entity->supertype == DWG_SUPERTYPE_ENTITY && entity->tio.entity != nullptr) {
                translate(*entity, target);
            }
        }
    }

    Builder common(const Dwg_Object& object, std::string_view type, Target target) {
        Builder builder(*out_, type);
        const Dwg_Object_Entity* entity = object.tio.entity;
        const Dwg_Object* layer = entity->layer != nullptr ? entity->layer->obj : nullptr;
        builder.text(8, key('L', layer)).number(60, entity->invisible != 0 ? 1 : 0);
        if (target.paper) {
            builder.number(67, 1);
        }
        return builder;
    }

    void translate(const Dwg_Object& object, Target target);
    bool translateCurve(const Dwg_Object& object, Target target);
    bool translateArea(const Dwg_Object& object, Target target);
    bool translateAnnotation(const Dwg_Object& object, Target target);
    void translateLwPolyline(const Dwg_Object& object, Target target);
    void translateText(const Dwg_Object& object, Target target);
    void translateMText(const Dwg_Object& object, Target target);
    void translateInsert(const Dwg_Object& object, Target target);
    void translateAttributes(const Dwg_Object& object, Target target);
    void translatePolyline(const Dwg_Object& object, Target target);
    void translateSpline(const Dwg_Object& object, Target target);

    Dwg_Data* dwg_;           // never null
    TranslatedDocument* out_; // never null
};

void Translator::translate(const Dwg_Object& object, Target target) {
    if (!translateCurve(object, target) && !translateArea(object, target)) {
        static_cast<void>(translateAnnotation(object, target));
    }
}

// Lines, circles, arcs, ellipses and polylines. Returns false for other types.
bool Translator::translateCurve(const Dwg_Object& object, Target target) {
    const auto& tio = object.tio.entity->tio;
    switch (object.fixedtype) {
    case DWG_TYPE_LINE:
        if (const auto* line = tio.LINE; line != nullptr) {
            target.records->push_back(common(object, "LINE", target)
                                          .point(10, line->start.x, line->start.y)
                                          .point(11, line->end.x, line->end.y)
                                          .take());
        }
        return true;
    case DWG_TYPE_CIRCLE:
        if (const auto* circle = tio.CIRCLE; circle != nullptr) {
            target.records->push_back(common(object, "CIRCLE", target)
                                          .point(10, circle->center.x, circle->center.y)
                                          .number(40, circle->radius)
                                          .take());
        }
        return true;
    case DWG_TYPE_ARC:
        if (const auto* arc = tio.ARC; arc != nullptr) {
            target.records->push_back(common(object, "ARC", target)
                                          .point(10, arc->center.x, arc->center.y)
                                          .number(40, arc->radius)
                                          .number(50, arc->start_angle * TO_DEGREES)
                                          .number(51, arc->end_angle * TO_DEGREES)
                                          .take());
        }
        return true;
    case DWG_TYPE_ELLIPSE:
        if (const auto* ellipse = tio.ELLIPSE; ellipse != nullptr) {
            target.records->push_back(common(object, "ELLIPSE", target)
                                          .point(10, ellipse->center.x, ellipse->center.y)
                                          .point(11, ellipse->sm_axis.x, ellipse->sm_axis.y)
                                          .number(40, ellipse->axis_ratio)
                                          .number(41, ellipse->start_angle)
                                          .number(42, ellipse->end_angle)
                                          .take());
        }
        return true;
    case DWG_TYPE_LWPOLYLINE:
        translateLwPolyline(object, target);
        return true;
    case DWG_TYPE_POLYLINE_2D:
        translatePolyline(object, target);
        return true;
    case DWG_TYPE_SPLINE:
        translateSpline(object, target);
        return true;
    default:
        return false;
    }
}

// Filled outlines: SOLID, TRACE and 3DFACE. Returns false for other types.
bool Translator::translateArea(const Dwg_Object& object, Target target) {
    const auto& tio = object.tio.entity->tio;
    switch (object.fixedtype) {
    case DWG_TYPE_SOLID:
    case DWG_TYPE_TRACE:
        if (const auto* s = tio.SOLID; s != nullptr) {
            target.records->push_back(common(object, "SOLID", target)
                                          .point(10, s->corner1.x, s->corner1.y)
                                          .point(11, s->corner2.x, s->corner2.y)
                                          .point(12, s->corner3.x, s->corner3.y)
                                          .point(13, s->corner4.x, s->corner4.y)
                                          .take());
        }
        return true;
    case DWG_TYPE__3DFACE:
        if (const auto* f = tio._3DFACE; f != nullptr) {
            target.records->push_back(common(object, "3DFACE", target)
                                          .point(10, f->corner1.x, f->corner1.y)
                                          .point(11, f->corner2.x, f->corner2.y)
                                          .point(12, f->corner3.x, f->corner3.y)
                                          .point(13, f->corner4.x, f->corner4.y)
                                          .take());
        }
        return true;
    default:
        return false;
    }
}

// Text, block references, leaders and dimensions. Returns false for other types.
bool Translator::translateAnnotation(const Dwg_Object& object, Target target) {
    switch (object.fixedtype) {
    case DWG_TYPE_TEXT:
        translateText(object, target);
        return true;
    case DWG_TYPE_MTEXT:
        translateMText(object, target);
        return true;
    case DWG_TYPE_INSERT:
    case DWG_TYPE_MINSERT:
        translateInsert(object, target);
        return true;
    case DWG_TYPE_LEADER:
        if (const auto* leader = object.tio.entity->tio.LEADER;
            leader != nullptr && leader->points != nullptr) {
            Builder builder = common(object, "LEADER", target);
            for (BITCODE_BL i = 0; i < leader->num_points; ++i) {
                builder.point(10, leader->points[i].x, leader->points[i].y);
            }
            target.records->push_back(builder.take());
        }
        return true;
    default:
        if (const Dwg_Object* block = dimensionBlock(object); block != nullptr) {
            target.records->push_back(
                common(object, "DIMENSION", target).text(2, key('B', block)).take());
            return true;
        }
        return false;
    }
}

void Translator::translateLwPolyline(const Dwg_Object& object, Target target) {
    const Dwg_Entity_LWPOLYLINE* lw = object.tio.entity->tio.LWPOLYLINE;
    if (lw == nullptr || lw->points == nullptr) {
        return;
    }
    Builder builder = common(object, "LWPOLYLINE", target);
    builder.number(70, (lw->flag & LWPOLYLINE_CLOSED) != 0 ? 1 : 0);
    for (BITCODE_BL i = 0; i < lw->num_points; ++i) {
        builder.point(10, lw->points[i].x, lw->points[i].y);
        if (lw->bulges != nullptr && i < lw->num_bulges) {
            builder.number(42, lw->bulges[i]);
        }
    }
    target.records->push_back(builder.take());
}

void Translator::translateText(const Dwg_Object& object, Target target) {
    const Dwg_Entity_TEXT* t = object.tio.entity->tio.TEXT;
    if (t == nullptr) {
        return;
    }
    target.records->push_back(common(object, "TEXT", target)
                                  .point(10, t->ins_pt.x, t->ins_pt.y)
                                  .point(11, t->alignment_pt.x, t->alignment_pt.y)
                                  .number(40, t->height)
                                  .number(50, t->rotation * TO_DEGREES)
                                  .number(41, t->width_factor != 0 ? t->width_factor : 1.0)
                                  .number(72, t->horiz_alignment)
                                  .number(73, t->vert_alignment)
                                  .text(1, utf8(object.tio.entity->tio.TEXT, "TEXT", "text_value"))
                                  .take());
}

void Translator::translateMText(const Dwg_Object& object, Target target) {
    const Dwg_Entity_MTEXT* m = object.tio.entity->tio.MTEXT;
    if (m == nullptr) {
        return;
    }
    target.records->push_back(common(object, "MTEXT", target)
                                  .point(10, m->ins_pt.x, m->ins_pt.y)
                                  .point(11, m->x_axis_dir.x, m->x_axis_dir.y)
                                  .number(40, m->text_height)
                                  .number(71, m->attachment)
                                  .text(1, utf8(object.tio.entity->tio.MTEXT, "MTEXT", "text"))
                                  .take());
}

void Translator::translateInsert(const Dwg_Object& object, Target target) {
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
    Builder builder = common(object, "INSERT", target);
    builder.text(2, key('B', header->obj))
        .point(10, ins.x, ins.y)
        .number(41, scale.x)
        .number(42, scale.y)
        .number(50, (array ? minsert->rotation : insert->rotation) * TO_DEGREES);
    if (array) {
        builder.number(70, minsert->num_cols).number(71, minsert->num_rows);
        builder.number(44, minsert->col_spacing).number(45, minsert->row_spacing);
    }
    target.records->push_back(builder.take());
    translateAttributes(object, target);
}

void Translator::translateAttributes(const Dwg_Object& object, Target target) {
    for (const Dwg_Object* sub = get_first_owned_subentity(&object); sub != nullptr;
         sub = get_next_owned_subentity(&object, sub)) {
        if (sub->fixedtype != DWG_TYPE_ATTRIB || sub->tio.entity == nullptr) {
            continue;
        }
        const Dwg_Entity_ATTRIB* a = sub->tio.entity->tio.ATTRIB;
        if (a == nullptr) {
            continue;
        }
        target.records->push_back(
            common(*sub, "ATTRIB", target)
                .point(10, a->ins_pt.x, a->ins_pt.y)
                .point(11, a->alignment_pt.x, a->alignment_pt.y)
                .number(40, a->height)
                .number(50, a->rotation * TO_DEGREES)
                .number(41, a->width_factor != 0 ? a->width_factor : 1.0)
                .number(72, a->horiz_alignment)
                .number(73, a->vert_alignment)
                .text(1, utf8(sub->tio.entity->tio.ATTRIB, "ATTRIB", "text_value"))
                .take());
    }
}

void Translator::translatePolyline(const Dwg_Object& object, Target target) {
    const Dwg_Entity_POLYLINE_2D* polyline = object.tio.entity->tio.POLYLINE_2D;
    if (polyline == nullptr) {
        return;
    }
    target.records->push_back(common(object, "POLYLINE", target).number(70, polyline->flag).take());
    for (const Dwg_Object* sub = get_first_owned_subentity(&object); sub != nullptr;
         sub = get_next_owned_subentity(&object, sub)) {
        if (sub->fixedtype != DWG_TYPE_VERTEX_2D || sub->tio.entity == nullptr) {
            continue;
        }
        const Dwg_Entity_VERTEX_2D* v = sub->tio.entity->tio.VERTEX_2D;
        if (v != nullptr) {
            target.records->push_back(Builder(*out_, "VERTEX")
                                          .point(10, v->point.x, v->point.y)
                                          .number(42, v->bulge)
                                          .number(70, v->flag)
                                          .take());
        }
    }
    target.records->push_back(Builder(*out_, "SEQEND").take());
}

void Translator::translateSpline(const Dwg_Object& object, Target target) {
    const Dwg_Entity_SPLINE* s = object.tio.entity->tio.SPLINE;
    if (s == nullptr) {
        return;
    }
    Builder builder = common(object, "SPLINE", target);
    builder.number(71, s->degree).number(70, s->closed_b != 0 ? 1 : 0);
    for (BITCODE_BL i = 0; s->knots != nullptr && i < s->num_knots; ++i) {
        builder.number(40, s->knots[i]);
    }
    for (BITCODE_BL i = 0; s->ctrl_pts != nullptr && i < s->num_ctrl_pts; ++i) {
        builder.point(10, s->ctrl_pts[i].x, s->ctrl_pts[i].y);
    }
    for (BITCODE_BL i = 0; s->weighted != 0 && s->ctrl_pts != nullptr && i < s->num_ctrl_pts; ++i) {
        builder.number(41, s->ctrl_pts[i].w);
    }
    for (BITCODE_BS i = 0; s->fit_pts != nullptr && i < s->num_fit_pts; ++i) {
        builder.point(11, s->fit_pts[i].x, s->fit_pts[i].y);
    }
    target.records->push_back(builder.take());
}

// NOLINTEND(cppcoreguidelines-pro-type-union-access,cppcoreguidelines-pro-bounds-pointer-arithmetic)

} // namespace

std::unique_ptr<TranslatedDocument> readWithLibreDwg(const std::filesystem::path& path) {
    // LibreDWG reads the file itself, so the in-memory input cap is applied here too.
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > MAX_IN_MEMORY_INPUT) {
        throw FormatError("DWG file is missing or too large for LibreDWG");
    }
    const std::scoped_lock lock(libreDwgMutex());
    DwgData dwg;
    const std::string narrow = path.string();
    const int status = dwg_read_file(narrow.c_str(), &dwg.data);
    if (status >= DWG_ERR_CRITICAL) {
        throw FormatError("LibreDWG could not decode the drawing");
    }
    auto out = std::make_unique<TranslatedDocument>();
    Translator(dwg.data, *out).run();
    return out;
}

} // namespace cadpreview::dwg
