#include <pango/pangocairo.h>
#include <cairo-pdf.h>

#include <clocale>
#include <cassert>

int main() {
    cairo_status_t status;
    cairo_surface_t *surface = cairo_pdf_surface_create("pangocairotest.pdf", 595, 842);
    cairo_t *cr = cairo_create(surface);
    cairo_move_to(cr, 72, 72);
    setlocale(LC_ALL, "");
    printf("PANGO_SCALE = %d\n", PANGO_SCALE);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc;
    desc = pango_font_description_from_string("Gentium");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 12 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_layout_set_text(layout,
                          "kenoiva ja jonka vetolujuus oli terasvaijerin tasoa. Kestavyys tuli "
                          "tarpeeseen, silla kyseinen",
                          -1);
    auto liter = pango_layout_get_iter(layout);
    PangoRectangle ink_rect, logical_rect;
    pango_layout_iter_get_line_extents(liter, &ink_rect, &logical_rect);
    pango_layout_iter_free(liter);
    printf("Text width is %.2f mm\n", double(logical_rect.width) / PANGO_SCALE / 595 * 220);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    int w, h;
    pango_layout_get_size(layout, &w, &h);
    printf("Size: (%d, %d)\n", w / PANGO_SCALE, h / PANGO_SCALE);
    g_object_unref(G_OBJECT(layout));
    //    g_object_unref(G_OBJECT(context));
    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    return 0;
}
