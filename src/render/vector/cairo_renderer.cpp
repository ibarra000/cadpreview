#include "render/vector/cairo_renderer.hpp"

#include "common/errors.hpp"
#include "common/utf8.hpp"
#include "render/compose.hpp"
#include "render/text.hpp"

#include <cairo-ft.h>
#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>

namespace cadpreview {

std::span<const std::byte> embeddedFont() noexcept; // generated from resources/fonts

namespace {

constexpr double INK_GRAY = 0x20 / 255.0; // #202124, close to black but softer on screens
constexpr double CAP_HEIGHT = 0.714;      // Noto Sans cap height as a fraction of the em
constexpr double MIN_TEXT_PIXELS = 1.0;   // smaller text is invisible and skipped
constexpr double REFERENCE_SIZE = 512.0;  // output size at which strokes are one pixel wide

struct SurfaceDeleter {
    void operator()(cairo_surface_t* s) const noexcept { cairo_surface_destroy(s); }
};
struct ContextDeleter {
    void operator()(cairo_t* c) const noexcept { cairo_destroy(c); }
};
using Surface = std::unique_ptr<cairo_surface_t, SurfaceDeleter>;
using Context = std::unique_ptr<cairo_t, ContextDeleter>;

// The FreeType library and face live for the whole process: cairo caches font faces beyond a
// single render, so they cannot be freed per call. They are created once and never modified.
cairo_font_face_t* sharedFontFace() {
    static cairo_font_face_t* const FACE = [] {
        FT_Library library = nullptr;
        FT_Face face = nullptr;
        const auto font = embeddedFont();
        const auto* bytes = static_cast<const FT_Byte*>(static_cast<const void*>(font.data()));
        if (FT_Init_FreeType(&library) != 0 ||
            FT_New_Memory_Face(library, bytes, static_cast<FT_Long>(font.size()), 0, &face) != 0) {
            throw std::runtime_error("cannot load the embedded font");
        }
        return cairo_ft_font_face_create_for_ft_face(face, 0);
    }();
    return FACE;
}

struct Fit {
    double scale = 1;
    Point center;
    double half = 0;

    [[nodiscard]] double x(double worldX) const { return half + (worldX - center.x) * scale; }
    [[nodiscard]] double y(double worldY) const { return half - (worldY - center.y) * scale; }
};

Fit fitTo(const Box& box, std::uint32_t size) {
    const double inner = size - 2.0 * canvasMargin(size);
    const double span = std::max(box.max.x - box.min.x, box.max.y - box.min.y);
    Fit fit;
    fit.scale = (span > 0 && std::isfinite(span)) ? inner / span : 1.0;
    fit.center = {(box.min.x + box.max.x) / 2, (box.min.y + box.max.y) / 2};
    fit.half = size / 2.0;
    return fit;
}

bool finite(Point p) {
    return std::isfinite(p.x) && std::isfinite(p.y);
}

void drawPolylines(cairo_t* cr, const Drawing& drawing, const Fit& fit) {
    for (const Polyline& polyline : drawing.polylines) {
        bool started = false;
        for (const Point& p : polyline.points) {
            if (!finite(p)) {
                started = false;
                continue;
            }
            (started ? cairo_line_to : cairo_move_to)(cr, fit.x(p.x), fit.y(p.y));
            started = true;
        }
        if (polyline.closed && started) {
            cairo_close_path(cr);
        }
        cairo_stroke(cr);
    }
}

void drawArcs(cairo_t* cr, const Drawing& drawing, const Fit& fit) {
    for (const Arc& arc : drawing.arcs) {
        if (!finite(arc.center) || !std::isfinite(arc.radius) || arc.radius <= 0 ||
            !std::isfinite(arc.startAngle) || !std::isfinite(arc.endAngle)) {
            continue;
        }
        double end = arc.endAngle;
        while (end <= arc.startAngle) {
            end += 2 * std::numbers::pi;
        }
        cairo_new_sub_path(cr);
        // The y axis is flipped on screen, so counter-clockwise becomes negative angles.
        cairo_arc_negative(cr, fit.x(arc.center.x), fit.y(arc.center.y), arc.radius * fit.scale,
                           -arc.startAngle, -end);
        cairo_stroke(cr);
    }
}

double verticalOffset(VAlign align, double capHeight, double descent) {
    switch (align) {
    case VAlign::Bottom:
        return -descent;
    case VAlign::Middle:
        return capHeight / 2;
    case VAlign::Top:
        return capHeight;
    case VAlign::Baseline:
        break;
    }
    return 0;
}

void drawText(cairo_t* cr, const TextItem& text, const Fit& fit) {
    const double capPixels = text.height * fit.scale;
    if (!finite(text.position) || !std::isfinite(capPixels) || capPixels < MIN_TEXT_PIXELS ||
        text.text.empty()) {
        return;
    }
    const std::string utf8 = isValidUtf8(text.text) ? text.text : toDisplayAscii(text.text);
    cairo_save(cr);
    cairo_set_font_size(cr, capPixels / CAP_HEIGHT);
    cairo_text_extents_t extents{};
    cairo_text_extents(cr, utf8.c_str(), &extents);
    cairo_font_extents_t font{};
    cairo_font_extents(cr, &font);
    const double hOffset = text.hAlign == HAlign::Left     ? 0
                           : text.hAlign == HAlign::Center ? -extents.x_advance / 2
                                                           : -extents.x_advance;
    cairo_translate(cr, fit.x(text.position.x), fit.y(text.position.y));
    cairo_rotate(cr, -text.rotation);
    cairo_scale(cr, std::isfinite(text.widthFactor) && text.widthFactor > 0 ? text.widthFactor : 1,
                1);
    cairo_move_to(cr, hOffset, verticalOffset(text.vAlign, capPixels, font.descent));
    cairo_show_text(cr, utf8.c_str());
    cairo_restore(cr);
}

Image toImage(cairo_surface_t* surface, std::uint32_t size) {
    cairo_surface_flush(surface);
    const unsigned char* data = cairo_image_surface_get_data(surface);
    const auto stride = static_cast<std::size_t>(cairo_image_surface_get_stride(surface));
    Image image(size, size, {});
    const std::span<const unsigned char> pixels(data, stride * size);
    for (std::uint32_t y = 0; y < size; ++y) {
        for (std::uint32_t x = 0; x < size; ++x) {
            // ARGB32 is a native-endian 32-bit word; the canvas is opaque, so no unpremultiply.
            const std::size_t offset = y * stride + x * 4U;
            std::uint32_t word = 0;
            for (std::size_t b = 0; b < 4; ++b) {
                word |= static_cast<std::uint32_t>(pixels[offset + b]) << (8U * b);
            }
            image.setPixel(x, y,
                           {static_cast<std::uint8_t>(word >> 16U),
                            static_cast<std::uint8_t>(word >> 8U), static_cast<std::uint8_t>(word),
                            255});
        }
    }
    return image;
}

} // namespace

Image renderDrawing(const Drawing& drawing, std::uint32_t size) {
    const auto box = extentsOf(drawing);
    if (!box) {
        throw FormatError("nothing to draw");
    }
    const auto edge = static_cast<int>(size);
    const Surface surface(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, edge, edge));
    const Context cr(cairo_create(surface.get()));
    if (cairo_status(cr.get()) != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error("cairo could not create a canvas");
    }
    cairo_set_source_rgb(cr.get(), 1, 1, 1);
    cairo_paint(cr.get());
    cairo_set_source_rgb(cr.get(), INK_GRAY, INK_GRAY, INK_GRAY);
    cairo_set_line_width(cr.get(), std::max(1.0, size / REFERENCE_SIZE));
    cairo_set_line_cap(cr.get(), CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr.get(), CAIRO_LINE_JOIN_ROUND);
    const Fit fit = fitTo(*box, size);
    drawPolylines(cr.get(), drawing, fit);
    drawArcs(cr.get(), drawing, fit);
    if (!drawing.texts.empty()) {
        cairo_set_font_face(cr.get(), sharedFontFace());
        for (const TextItem& text : drawing.texts) {
            drawText(cr.get(), text, fit);
        }
    }
    if (cairo_status(cr.get()) != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error(std::string("cairo: ") +
                                 cairo_status_to_string(cairo_status(cr.get())));
    }
    return toImage(surface.get(), size);
}

} // namespace cadpreview
