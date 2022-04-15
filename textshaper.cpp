#include "textshaper.hpp"

#include <cairo-pdf.h>
#include <cassert>

TextShaper::TextShaper() {
    surface = cairo_pdf_surface_create("/tmp/dummy.pdf", 595, 842);
    cairo_t *cr = cairo_create(surface);
    cairo_move_to(cr, 72, 72);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc;
    desc = pango_font_description_from_string("Gentium");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 12 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
}

TextShaper::~TextShaper() {
    g_object_unref(G_OBJECT(layout));
    cairo_surface_destroy(surface);
    cairo_destroy(cr);
}

double TextShaper::text_width(const char *utf8_text) const {
    pango_layout_set_text(layout, utf8_text, -1);
    auto liter = pango_layout_get_iter(layout);
    PangoRectangle ink_rect, logical_rect;
    pango_layout_iter_get_line_extents(liter, &ink_rect, &logical_rect);
    pango_layout_iter_free(liter);
    // printf("Text width is %.2f mm\n", double(logical_rect.width) / PANGO_SCALE / 595 * 220);
    return double(logical_rect.width);
}
