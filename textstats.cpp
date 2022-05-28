#include "textstats.hpp"
#include <utils.hpp>

#include <cairo-pdf.h>
#include <cassert>

TextStats::TextStats() {
    surface = cairo_pdf_surface_create("/tmp/dummy.pdf", 595, 842);
    cr = cairo_create(surface);
    assert(cr);
    cairo_move_to(cr, 72, 72);
    layout = pango_cairo_create_layout(cr);
    desc = pango_font_description_from_string("Gentium");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_layout_set_text(layout, "I am Bender, please insert girder.", -1);
}

TextStats::~TextStats() {
    pango_font_description_free(desc);
    g_object_unref(G_OBJECT(layout));
    cairo_surface_destroy(surface);
    cairo_destroy(cr);
}

double TextStats::text_width(const char *utf8_text) const {
    auto f = widths.find(utf8_text);
    if(f != widths.end()) {
        return f->second;
    }
    pango_layout_set_text(layout, utf8_text, -1);
    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_extents(layout, &ink_rect, &logical_rect);
    // printf("Text width is %.2f mm\n", double(logical_rect.width) / PANGO_SCALE / 595 * 220);
    const double w_mm = pt2mm(double(logical_rect.width) / PANGO_SCALE);
    widths[utf8_text] = w_mm;
    return w_mm;
}
