/*
 * Copyright 2022 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <pango/pangocairo.h>
#include <cairo-pdf.h>

#include <clocale>
#include <cassert>

double mm2pt(const double x) { return x * 2.8346456693; }
double pt2mm(const double x) { return x / 2.8346456693; }

int main() {
    setlocale(LC_ALL, "");
    //    cairo_status_t status;
    cairo_surface_t *surface = cairo_pdf_surface_create("pangocairotest.pdf", 595, 842);
    cairo_t *cr = cairo_create(surface);
    cairo_save(cr);
    cairo_set_source_rgb(cr, 1.0, 0.2, 0.1);
    cairo_move_to(cr, 72, 72);
    printf("PANGO_SCALE = %d\n", PANGO_SCALE);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc;
    desc = pango_font_description_from_string("Gentium");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
    pango_layout_set_text(layout, "Persian saddle-bags on which he", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_extents(layout, &ink_rect, &logical_rect);
    printf("Text width is %.2f mm\n", pt2mm(double(logical_rect.width) / PANGO_SCALE));
    int w, h;
    pango_layout_get_size(layout, &w, &h);
    printf("Size: (%d, %d)\n", w / PANGO_SCALE, h / PANGO_SCALE);
    g_object_unref(G_OBJECT(layout));
    //    g_object_unref(G_OBJECT(context));
    cairo_restore(cr);

    // Image
    cairo_surface_t *image;
    image = cairo_image_surface_create_from_png("../chapterizer/1bit.png");
    //    cairo_rectangle(cr, 100, 100, 100, 100);
    //    cairo_clip(cr);
    cairo_translate(cr, 100, 100);
    cairo_scale(cr, 0.1, 0.1);
    cairo_set_source_surface(cr, image, 0, 0);
    cairo_paint(cr);
    cairo_surface_destroy(image);
    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    return 0;
}
