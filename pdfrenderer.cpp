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
static const FontParameters fp{"Gentium", 10, FontStyle::Regular};

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
    PangoFontDescription *desc;
    desc = pango_font_description_from_string("Gentium");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    // Can't use scale to draw in millimeters because it also scales text size.
    // cairo_scale(cr, 595.0 / 21.0, 595.0 / 21.0);

    //    cairo_select_font_face(cr, "Gentium", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    //    cairo_set_font_size(cr, 10.0);
}

PdfRenderer::~PdfRenderer() {
    g_object_unref(G_OBJECT(layout));
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

void PdfRenderer::render(const std::vector<std::string> &lines, const double target_width_mm) {
    draw_grid();
    const double line_height = 12.0;
    const double target_width_pt = mm2pt(target_width_mm);
    cairo_set_source_rgb(cr, 0, 0, 0);
    const double left_box_origin_x = mm2pt(20);
    const double left_box_origin_y = mm2pt(20);
    const double right_box_origin_x = mm2pt(20 + 10 + target_width_mm);
    const double right_box_origin_y = mm2pt(20);
    FontParameters par;
    par.name = "Gentium";
    par.point_size = 10;
    par.type = FontStyle::Regular;

    for(size_t i = 0; i < lines.size(); ++i) {
        if(i < lines.size() - 1) {
            render_line_justified(lines[i],
                                  par,
                                  left_box_origin_x,
                                  target_width_mm,
                                  left_box_origin_y + i * line_height);
        } else {
            render_line_as_is(
                lines[i].c_str(), par, left_box_origin_x, left_box_origin_y + i * line_height);
        }
        render_line_as_is(
            lines[i].c_str(), par, right_box_origin_x, right_box_origin_y + i * line_height);
    }
    cairo_set_source_rgb(cr, 0, 0, 1.0);
    cairo_set_line_width(cr, 0.5);
    const double box_height = line_height * lines.size();
    draw_box(left_box_origin_x, left_box_origin_y, target_width_pt, box_height);
    draw_box(right_box_origin_x, right_box_origin_y, target_width_pt, box_height);
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

void PdfRenderer::draw_box(double x, double y, double w, double h) {
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + w, y);
    cairo_line_to(cr, x + w, y + h);
    cairo_line_to(cr, x, y + h);
    cairo_close_path(cr);
    cairo_stroke(cr);
}

void PdfRenderer::render_line_justified(const std::string &line_text,
                                        const FontParameters &par,
                                        double line_width_mm,
                                        double x,
                                        double y) {
    assert(line_text.find('\n') == std::string::npos);
    setup_pango(par);
    const auto words = hack_split(line_text);
    const double target_width_pt = mm2pt(line_width_mm);
    double text_width_mm = hack.text_width(line_text.c_str(), fp);
    const double text_width_pt = mm2pt(text_width_mm);
    const double num_spaces = std::count(line_text.begin(), line_text.end(), ' ');
    const double space_extra_width =
        num_spaces > 0 ? (target_width_pt - text_width_pt) / num_spaces : 0.0;
#if 0
    std::string tmp;
    for(size_t i = 0; i < words.size(); ++i) {
        cairo_move_to(cr, x, y);
        PangoRectangle r;

        tmp = words[i];
        tmp += ' ';
        pango_layout_set_attributes(layout, nullptr);
        pango_layout_set_text(layout, tmp.c_str(), -1);
        pango_layout_get_extents(layout, nullptr, &r);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        x += double(r.width) / PANGO_SCALE;
        x += space_extra_width;

        /*
        cairo_show_text(cr, words[i].c_str());
        cairo_show_text(cr, " ");
        cairo_get_current_point(cr, &x, &y);
        x += space_extra_width;
        cairo_move_to(cr, x, y);
        */
    }
#else
    cairo_move_to(cr, x, y);
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_markup(layout, line_text.c_str(), line_text.length());
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
#endif
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

void PdfRenderer::render_line_as_is(const char *line,
                                    const FontParameters &par,
                                    double x,
                                    double y) {
    setup_pango(par);
    cairo_move_to(cr, x, y);
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_markup(layout, line, -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
}

void PdfRenderer::render_line_centered(const char *line,
                                       const FontParameters &par,
                                       double x,
                                       double y) {
    PangoRectangle r;

    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_text(layout, line, -1);
    pango_layout_get_extents(layout, nullptr, &r);
    render_line_as_is(line, par, x - (r.width / (2 * PANGO_SCALE)), y);
}

void PdfRenderer::new_page() { cairo_surface_show_page(surf); }
