#include <pango/pangocairo.h>

#include <clocale>
#include <cassert>

int main() {
    cairo_status_t status;
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1024, 1024);
    cairo_t *cr = cairo_create(surface);
    setlocale(LC_ALL, "");
    printf("PANGO_SCALE = %d\n", PANGO_SCALE);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc;
    desc = pango_font_description_from_string("Liberation Serif,Serif");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 12 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_layout_set_text(layout, "Maapallon keskushallinto Omniviraatin avaruusvoimien.", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    int w, h;
    pango_layout_get_size(layout, &w, &h);
    printf("Size: (%d, %d)\n", w / PANGO_SCALE, h / PANGO_SCALE);
    g_object_unref(G_OBJECT(layout));
    //    g_object_unref(G_OBJECT(context));
    status = cairo_surface_write_to_png(surface, "cairotest.png");
    assert(status == 0);
    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    return 0;
}
