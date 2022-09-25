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

TextStats::TextStats() {
    surface = cairo_pdf_surface_create(nullptr, 595, 842);
    cr = cairo_create(surface);
    assert(cr);
    cairo_move_to(cr, 72, 72);
    layout = pango_cairo_create_layout(cr);
    PangoContext *context = pango_layout_get_context(layout);
    pango_context_set_round_glyph_positions(context, FALSE);
}

TextStats::~TextStats() {
    g_object_unref(G_OBJECT(layout));
    cairo_surface_destroy(surface);
    cairo_destroy(cr);
}

void TextStats::set_pango_state(const char *utf8_text,
                                const FontParameters &font,
                                bool is_markup) const {
    auto *desc = pango_font_description_from_string(font.name.c_str());
    assert(desc);
    pango_font_description_set_absolute_size(desc, font.point_size * PANGO_SCALE);
    if(font.type == FontStyle::Bold || font.type == FontStyle::BoldItalic) {
        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    } else {
        pango_font_description_set_weight(desc, PANGO_WEIGHT_NORMAL);
    }
    if(font.type == FontStyle::Italic || font.type == FontStyle::BoldItalic) {
        pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
    } else {
        pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);
    }
    pango_layout_set_font_description(layout, desc);
    assert(g_utf8_validate(utf8_text, -1, nullptr));
    pango_layout_set_attributes(layout, nullptr);
    if(is_markup) {
        pango_layout_set_markup(layout, utf8_text, -1);
    } else {
        pango_layout_set_text(layout, utf8_text, -1);
    }
    pango_font_description_free(desc);
}

Millimeter TextStats::text_width(const char *utf8_text, const FontParameters &font) const {
    StyledPlainText k;
    k.text = utf8_text;
    k.font = font;
    auto f = plaintext_widths.find(k);
    if(f != plaintext_widths.end()) {
        return f->second;
    }
    set_pango_state(utf8_text, font);
    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_extents(layout, &ink_rect, &logical_rect);
    // printf("Text width is %.2f mm\n", double(logical_rect.width) / PANGO_SCALE / 595 * 220);
    Millimeter w = Point::from_value(double(logical_rect.width) / PANGO_SCALE).tomm();
    plaintext_widths[k] = w;
    return w;
}

Millimeter TextStats::markup_width(const char *utf8_text, const FontParameters &font) const {
    StyledMarkupText k;
    k.text = utf8_text;
    k.font = font;
    auto f = markup_widths.find(k);
    if(f != markup_widths.end()) {
        return f->second;
    }
    set_pango_state(utf8_text, font, true);
    PangoRectangle ink_rect, logical_rect;
    pango_layout_get_extents(layout, &ink_rect, &logical_rect);

    Millimeter w = Point::from_value(double(logical_rect.width) / PANGO_SCALE).tomm();
    markup_widths[k] = w;
    return w;
}
