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

#include "pdfrenderer.hpp"
#include <utils.hpp>

#include <cairo-pdf.h>
#include <cassert>

#include <textstats.hpp>
#include <algorithm>

static TextStats hack{};

#include <sstream>

namespace {

std::vector<std::string> hack_split(const std::string &in_text) {
    std::string text;
    text.reserve(in_text.size());
    for(size_t i = 0; i < in_text.size(); ++i) {
        if(in_text[i] == '\n') {
            text.push_back(' ');
        } else {
            text.push_back(in_text[i]);
        }
    }
    std::string val;
    const char separator = ' ';
    std::vector<std::string> words;
    std::stringstream sstream(text);
    while(std::getline(sstream, val, separator)) {
        words.push_back(val);
    }
    return words;
}
} // namespace

PdfRenderer::PdfRenderer(const char *ofname, int pagew, int pageh) {
    surf = cairo_pdf_surface_create(ofname, pagew, pageh);
    cr = cairo_create(surf);
    layout = pango_cairo_create_layout(cr);
    PangoContext *context = pango_layout_get_context(layout);
    pango_context_set_round_glyph_positions(context, FALSE);

    // Can't use scale to draw in millimeters because it also scales text size.
    // cairo_scale(cr, 595.0 / 21.0, 595.0 / 21.0);

    //    cairo_select_font_face(cr, "Gentium", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    //    cairo_set_font_size(cr, 10.0);
    cairo_set_line_width(cr, 0.1);
}

PdfRenderer::~PdfRenderer() {
    g_object_unref(G_OBJECT(layout));
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

void PdfRenderer::draw_grid() {
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 0.1);
    for(double x = 0.0; x < mm2pt(210); x += mm2pt(10)) {
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, mm2pt(300));
    }
    for(double y = 0.0; y < mm2pt(300); y += mm2pt(10)) {
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, mm2pt(210), y);
    }
    cairo_stroke(cr);
}

void PdfRenderer::draw_box(Point x, Point y, Point w, Point h) {
    cairo_move_to(cr, x.v, y.v);
    cairo_line_to(cr, (x + w).v, y.v);
    cairo_line_to(cr, (x + w).v, (y + h).v);
    cairo_line_to(cr, x.v, (y + h).v);
    cairo_close_path(cr);
    cairo_stroke(cr);
}

void PdfRenderer::render_line_justified(const std::string &line_text,
                                        const FontParameters &par,
                                        Millimeter line_width,
                                        Point x,
                                        Point y) {
    assert(line_text.find('\n') == std::string::npos);
    setup_pango(par);
    const auto words = hack_split(line_text);
    Millimeter text_width = Millimeter::from_value(hack.text_width(line_text.c_str(), par));
    const double num_spaces = std::count(line_text.begin(), line_text.end(), ' ');
    const Point space_extra_width{num_spaces > 0 ? ((line_width - text_width) / num_spaces).topt()
                                                 : Point{}};

    std::string tmp;
    for(size_t i = 0; i < words.size(); ++i) {
        cairo_move_to(cr, x.v, y.v);
        PangoRectangle r;

        tmp = words[i];
        tmp += ' ';
        pango_layout_set_attributes(layout, nullptr);
        pango_layout_set_text(layout, tmp.c_str(), -1);
        pango_layout_get_extents(layout, nullptr, &r);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        x += Point::from_value(double(r.width) / PANGO_SCALE);
        x += space_extra_width;

        /*
        cairo_show_text(cr, words[i].c_str());
        cairo_show_text(cr, " ");
        cairo_get_current_point(cr, &x, &y);
        x += space_extra_width;
        cairo_move_to(cr, x, y);
        */
    }
}

void PdfRenderer::render_line_justified(const std::vector<std::string> &markup_words,
                                        const FontParameters &par,
                                        double line_width_mm,
                                        Point x,
                                        Point y) {
    setup_pango(par);
    std::string full_line;
    for(const auto &w : markup_words) {
        full_line += w; // Markup words end in spaces (except the last one).
    }

    pango_layout_set_attributes(layout, nullptr);
    // Pango aligns by top, we want alignment by baseline.
    // https://gitlab.gnome.org/GNOME/pango/-/issues/698
    pango_layout_set_text(layout, "A", 1);
    const auto desired_baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;

    const Point target_width = Point::from_value(mm2pt(line_width_mm));
    pango_layout_set_markup(layout, full_line.c_str(), full_line.length());
    const Point text_width =
        Millimeter::from_value(hack.markup_width(full_line.c_str(), par)).topt();
    const double num_spaces = double(markup_words.size() - 1);
    const Point space_extra_width =
        num_spaces > 0 ? (target_width - text_width) / num_spaces : Point();

    std::string tmp;
    for(const auto &markup_word : markup_words) {
        cairo_move_to(cr, x.v, y.v);
        PangoRectangle r;

        pango_layout_set_attributes(layout, nullptr);
        pango_layout_set_markup(layout, markup_word.c_str(), -1);
        const auto current_baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;

        cairo_rel_move_to(cr, 0, desired_baseline - current_baseline);
        pango_layout_get_extents(layout, nullptr, &r);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        x += Point::from_value(double(r.width) / PANGO_SCALE);
        x += space_extra_width;
    }
}

void PdfRenderer::setup_pango(const FontParameters &par) {
    PangoFontDescription *desc = pango_font_description_from_string(par.name.c_str());
    if(par.type == FontStyle::Bold || par.type == FontStyle::BoldItalic) {
        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    } else {
        pango_font_description_set_weight(desc, PANGO_WEIGHT_NORMAL);
    }
    if(par.type == FontStyle::Italic || par.type == FontStyle::BoldItalic) {
        pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
    } else {
        pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);
    }
    pango_font_description_set_absolute_size(desc, par.point_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
}

void PdfRenderer::render_text_as_is(const char *line, const FontParameters &par, Point x, Point y) {
    setup_pango(par);
    cairo_move_to(cr, x.v, y.v);
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_text(layout, line, -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
}

void PdfRenderer::render_markup_as_is(const char *line,
                                      const FontParameters &par,
                                      Point x,
                                      Point y) {
    setup_pango(par);
    cairo_move_to(cr, x.v, y.v);
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_markup(layout, line, -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
}

void PdfRenderer::render_markup_as_is(const std::vector<std::string> markup_words,
                                      const FontParameters &par,
                                      double x,
                                      double y) {
    std::string full_line;
    for(const auto &w : markup_words) {
        full_line += w;
    }
    setup_pango(par);
    cairo_move_to(cr, x, y);
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_markup(layout, full_line.c_str(), -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
}

void PdfRenderer::render_line_centered(const char *line,
                                       const FontParameters &par,
                                       Point x,
                                       Point y) {
    PangoRectangle r;

    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_text(layout, line, -1);
    pango_layout_get_extents(layout, nullptr, &r);
    render_markup_as_is(line, par, x - Point::from_value(r.width / (2 * PANGO_SCALE)), y);
}

void PdfRenderer::new_page() { cairo_surface_show_page(surf); }
