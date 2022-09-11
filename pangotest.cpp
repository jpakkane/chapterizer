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

#include <locale.h>
#include <assert.h>

double mm2pt(const double x) { return x * 2.8346456693; }
double pt2mm(const double x) { return x / 2.8346456693; }

int main() {
    setlocale(LC_ALL, "");
    //    cairo_status_t status;
    cairo_surface_t *surface = cairo_pdf_surface_create("pangocairotest.pdf", 595, 842);
    cairo_t *cr = cairo_create(surface);
    // cairo_set_source_rgb(cr, 1.0, 0.2, 0.1);
    printf("PANGO_SCALE = %d\n", PANGO_SCALE);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc;
    desc = pango_font_description_from_string("Gentium");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    cairo_move_to(cr, 72, 72);
    pango_layout_set_markup(layout, "This is a line of text.", -1);
    pango_cairo_update_layout(cr, layout);
    const auto plain_baseline = pango_layout_get_baseline(layout);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_attributes(layout, NULL);

    cairo_move_to(cr, 72, 72 + 1 * 12);
    pango_layout_set_markup(layout, "This is a line of text.", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_attributes(layout, NULL);

    pango_layout_set_markup(layout, "This is a <tt>line</tt> of text.", -1);
    pango_cairo_update_layout(cr, layout);
    auto current_baseline = pango_layout_get_baseline(layout);
    cairo_move_to(cr, 72, 72 + 2 * 12 + (plain_baseline - current_baseline) / PANGO_SCALE);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_attributes(layout, NULL);

    cairo_move_to(cr, 72, 72 + 3 * 12);
    pango_layout_set_markup(layout, "This is a line of text.", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_attributes(layout, NULL);

    cairo_move_to(cr, 72, 72 + 4 * 12);
    pango_layout_set_markup(
        layout, "This is a <span font=\"Noto Sans Mono\" size=\"6pt\">line</span> of text.", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_attributes(layout, NULL);

    cairo_move_to(cr, 72, 72 + 5 * 12);
    pango_layout_set_markup(layout, "This is a line of text.", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_attributes(layout, NULL);

    cairo_move_to(cr, 72, 72 + 6 * 12);
    pango_layout_set_markup(
        layout, "This is a <span font=\"Liberation Mono\">line</span> of text.", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_attributes(layout, NULL);

    cairo_move_to(cr, 72, 72 + 7 * 12);
    pango_layout_set_markup(layout, "This is a line of text.", -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
    pango_layout_set_attributes(layout, NULL);
    cairo_surface_destroy(surface);

    cairo_destroy(cr);
    return 0;
}
