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

#include "textstats.hpp"
#include <utils.hpp>

#include <cairo-pdf.h>
#include <cassert>

TextStats::TextStats(const std::string &font, int fontsize) {
    surface = cairo_pdf_surface_create(nullptr, 595, 842);
    cr = cairo_create(surface);
    assert(cr);
    cairo_move_to(cr, 72, 72);
    layout = pango_cairo_create_layout(cr);
    desc = pango_font_description_from_string(font.c_str());
    assert(desc);
    pango_font_description_set_absolute_size(desc, fontsize * PANGO_SCALE);
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
