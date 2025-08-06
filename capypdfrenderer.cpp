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

#include "capypdfrenderer.hpp"
#include <utils.hpp>

#include <cairo-pdf.h>
#include <cassert>

#include <textstats.hpp>
#include <algorithm>

static TextStats hack{};

#include <sstream>

namespace {

const char *workname = "turbotempfile.pdf";

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

uint32_t get_last_char(const std::string &markup) {
    const gchar *txt = markup.c_str();
    int angle_depth = 0;
    uint32_t last_char = 0;
    while(*txt) {
        const uint32_t cur_char = g_utf8_get_char(txt);
        if(cur_char == '<') {
            ++angle_depth;
        } else if(cur_char == '>') {
            --angle_depth;
        } else if(angle_depth == 0) {
            last_char = cur_char; // FIXME, won't work with quoted angle brackets.
        }
        txt = g_utf8_next_char(txt);
    }
    return last_char;
}

} // namespace

CapyPdfRenderer::CapyPdfRenderer(const char *ofname,
                                 Length pagew,
                                 Length pageh,
                                 Length bleed_,
                                 const capypdf::DocumentProperties &docprop)
    : capygen{ofname, docprop}, capyctx{capygen.new_page_context()}, bleed{bleed_.pt()},
      mediaw{pagew.pt() + 2 * bleed}, mediah{pageh.pt() + 2 * bleed} {

    init_page();

    capyctx.cmd_w(0.1);
    outname = ofname;
}

CapyPdfRenderer::~CapyPdfRenderer() {
    capygen.write();
    /*
    // Cairo only supports RGB output, so convert to Gray.
    assert(outname.find('"') == std::string::npos);
    std::string graycmd{"gs \"-sOutputFile="};
    graycmd += outname;
    graycmd += "\" -sDEVICE=pdfwrite -sColorConversionStrategy=Gray "
               "-dProcessColorModel=/DeviceGray -dCompatibilityLevel=1.6 -dNOPAUSE -dBATCH \"";
    graycmd += workname;
    graycmd += "\"";
    // printf("%s\n", graycmd.c_str());
    auto rc = system(graycmd.c_str());
    if(rc != 0) {
        std::abort();
    }
    unlink(workname);
*/
}

void CapyPdfRenderer::draw_grid() {
    std::abort();
    /*
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
*/
}

void CapyPdfRenderer::draw_box(Length x, Length y, Length w, Length h, Length thickness) {
    capyctx.cmd_q();
    capyctx.cmd_w(thickness.pt());
    capyctx.cmd_G(0.0);
    capyctx.cmd_re(x.pt(), y.pt(), (x + w).pt(), (y + h).pt());
    capyctx.cmd_S();
    capyctx.cmd_Q();
}

void CapyPdfRenderer::fill_box(Length x, Length y, Length w, Length h, double color) {
    capyctx.cmd_q();
    capyctx.cmd_g(color);
    capyctx.cmd_re(x.pt(), y.pt(), (x + w).pt(), (y + h).pt());
    capyctx.cmd_f();
    capyctx.cmd_Q();
}

void CapyPdfRenderer::fill_rounded_corner_box(
    Length x, Length y, Length w, Length h, double color) {
    std::abort();

    /*
    const double round_fraction = 0.5;
    const auto round_distance = round_fraction * w;

    capyctx.cmd_q();
    cairo_set_source_rgb(cr, color, color, color);

    cairo_move_to(cr, (x + round_fraction * w).pt(), y.pt());
    cairo_line_to(cr, (x + w - round_distance).pt(), y.pt());
    cairo_curve_to(
        cr, (x + w).pt(), y.pt(), (x + w).pt(), y.pt(), (x + w).pt(), (y + round_distance).pt());

    cairo_line_to(cr, (x + w).pt(), (y + h - round_distance).pt());
    cairo_curve_to(cr,
                   (x + w).pt(),
                   (y + h).pt(),
                   (x + w).pt(),
                   (y + h).pt(),
                   (x + w - round_distance).pt(),
                   (y + h).pt());

    cairo_line_to(cr, (x + round_distance).pt(), (y + h).pt());
    cairo_curve_to(cr,
                   (x).pt(),
                   (y + h).pt(),
                   (x).pt(),
                   (y + h).pt(),
                   (x).pt(),
                   (y + h - round_distance).pt());

    cairo_line_to(cr, x.pt(), (y + round_distance).pt());
    cairo_curve_to(cr, x.pt(), y.pt(), x.pt(), y.pt(), (x + round_distance).pt(), y.pt());
    cairo_close_path(cr);
    cairo_fill(cr);
    capyctx.cmd_Q();
*/
}

void CapyPdfRenderer::draw_dash_line(const std::vector<Coord> &points, double line_width) {
    /*
    if(points.size() < 2) {
        return;
    }
    capyctx.cmd_q();
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, line_width);
    const double dashes[2] = {4.0, 2.0};
    cairo_set_dash(cr, dashes, 2, 0.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, points[0].x.pt(), points[0].y.pt());
    for(size_t i = 1; i < points.size(); ++i) {
        cairo_line_to(cr, points[i].x.pt(), points[i].y.pt());
    }
    cairo_stroke(cr);
    capyctx.cmd_Q();
*/
    std::abort();
}

void CapyPdfRenderer::draw_poly_line(const std::vector<Coord> &points, Length thickness) {
    if(points.size() < 2) {
        return;
    }
    capyctx.cmd_q();
    capyctx.cmd_G(0);
    capyctx.cmd_w(thickness.pt());
    capyctx.cmd_m(points[0].x.pt(), points[0].y.pt());
    for(size_t i = 1; i < points.size(); ++i) {
        capyctx.cmd_l(points[i].x.pt(), points[i].y.pt());
    }
    capyctx.cmd_S();
    capyctx.cmd_Q();
}

void CapyPdfRenderer::draw_arc(
    Length x, Length y, Length r, double angle1, double angle2, Length thickness) {
    /*
    capyctx.cmd_q();
    cairo_new_sub_path(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, thickness.pt());
    cairo_arc_negative(cr, x.pt(), y.pt(), r.pt(), angle1, angle2);
    cairo_stroke(cr);
    capyctx.cmd_Q();
*/
    std::abort();
}

void CapyPdfRenderer::render_line_justified(const std::string &line_text,
                                            const FontParameters &par,
                                            Length line_width,
                                            Length x,
                                            Length y) {
    /*
    assert(line_text.find('\n') == std::string::npos);
    setup_pango(par);
    const auto words = hack_split(line_text);
    Length text_width = hack.text_width(line_text.c_str(), par);
    const double num_spaces = std::count(line_text.begin(), line_text.end(), ' ');
    const Length space_extra_width{num_spaces > 0 ? ((line_width - text_width) / num_spaces)
                                                  : Length::zero()};

    std::string tmp;
    for(size_t i = 0; i < words.size(); ++i) {
        cairo_move_to(cr, x.pt(), y.pt());
        PangoRectangle r;

        tmp = words[i];
        tmp += ' ';
        pango_layout_set_attributes(layout, nullptr);
        pango_layout_set_text(layout, tmp.c_str(), -1);
        pango_layout_get_extents(layout, nullptr, &r);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        x += Length::from_pt(double(r.width) / PANGO_SCALE);
        x += space_extra_width;
    }
    */
    std::abort();
}

void CapyPdfRenderer::render_line_justified(const std::vector<std::string> &markup_words,
                                            const FontParameters &par,
                                            Length line_width,
                                            Length x,
                                            Length y) {
    /*
    if(markup_words.empty()) {
        return;
    }
    const uint32_t last_char = get_last_char(markup_words.back());
    const Length overhang_right = hack.codepoint_right_overhang(last_char, par);
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

    const Length target_width = line_width;
    pango_layout_set_markup(layout, full_line.c_str(), full_line.length());
    const Length text_width = hack.markup_width(full_line.c_str(), par);
    const double num_spaces = double(markup_words.size() - 1);
    const Length space_extra_width =
        num_spaces > 0 ? (target_width - text_width + overhang_right) / num_spaces : Length::zero();

    for(const auto &markup_word : markup_words) {
        cairo_move_to(cr, x.pt(), y.pt());
        PangoRectangle r;

        pango_layout_set_attributes(layout, nullptr);
        pango_layout_set_markup(layout, markup_word.c_str(), -1);
        const auto current_baseline = pango_layout_get_baseline(layout) / PANGO_SCALE;

        cairo_rel_move_to(cr, 0, desired_baseline - current_baseline);
        pango_layout_get_extents(layout, nullptr, &r);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        x += Length::from_pt(double(r.width) / PANGO_SCALE);
        x += space_extra_width;
    }
*/
    std::abort();
}

void CapyPdfRenderer::render_text_as_is(const char *line,
                                        const FontParameters &par,
                                        Length x,
                                        Length y) {
    /*
    cairo_move_to(cr, x.pt(), y.pt());
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_text(layout, line, -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
*/
    std::abort();
}

void CapyPdfRenderer::render_markup_as_is(
    const char *line, const FontParameters &par, Length x, Length y, TextAlignment alignment) {
    /*

    switch(alignment) {
    case TextAlignment::Left:
        cairo_move_to(cr, x.pt(), y.pt());
        break;
    case TextAlignment::Centered:
        PangoRectangle r;
        pango_layout_get_extents(layout, nullptr, &r);
        cairo_move_to(cr, (x - Length::from_pt(r.width / (2 * PANGO_SCALE))).pt(), y.pt());
        break;
    case TextAlignment::Right:
        pango_layout_get_extents(layout, nullptr, &r);
        cairo_move_to(cr, (x - Length::from_pt(r.width / PANGO_SCALE)).pt(), y.pt());
    }

    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
*/
    std::abort();
}

void CapyPdfRenderer::render_markup_as_is(const std::vector<std::string> markup_words,
                                          const FontParameters &par,
                                          Length x,
                                          Length y,
                                          TextAlignment alignment) {
    std::string full_line;
    for(const auto &w : markup_words) {
        full_line += w;
    }
    render_markup_as_is(full_line.c_str(), par, x, y, alignment);
}

void CapyPdfRenderer::render_line_centered(const char *line,
                                           const FontParameters &par,
                                           Length x,
                                           Length y) {
    /*
    PangoRectangle r;

    setup_pango(par);
    pango_layout_set_attributes(layout, nullptr);
    pango_layout_set_text(layout, line, -1);
    pango_layout_get_extents(layout, nullptr, &r);
    render_text_as_is(line, par, x - Length::from_pt(r.width / (2 * PANGO_SCALE)), y);
*/
    std::abort();
}

void CapyPdfRenderer::render_wonky_text(const char *text,
                                        const FontParameters &par,
                                        Length raise,
                                        Length shift,
                                        double tilt,
                                        double color,
                                        Length x,
                                        Length y) {
    /*
    capyctx.cmd_q();
    cairo_set_source_rgb(cr, color, color, color);
    cairo_translate(cr, (x + shift).pt(), (y + raise).pt());
    cairo_rotate(cr, tilt);
    render_text_as_is(text, par, Length::zero(), Length::zero());
    capyctx.cmd_Q();
    */
    std::abort();
}

void CapyPdfRenderer::new_page() {
    finalize_page();
    capygen.add_page(capyctx);
    init_page();
    ++pages;
}

void CapyPdfRenderer::init_page() {
    if(bleed > 0) {
        capyctx.cmd_q();
        capyctx.cmd_cm(1.0, 0, 0, 1.0, bleed, bleed);
    }
}

void CapyPdfRenderer::finalize_page() {
    if(bleed > 0) {
        capyctx.cmd_Q();
        draw_cropmarks();
    }
}

void CapyPdfRenderer::draw_cropmarks() {
    /*
    const auto b = bleed;

    capyctx.cmd_q();
    cairo_move_to(cr, b, 0);
    cairo_rel_line_to(cr, 0, b / 2);
    cairo_move_to(cr, 0, b);
    cairo_rel_line_to(cr, b / 2, 0);

    cairo_move_to(cr, b, 0);
    cairo_rel_line_to(cr, 0, b / 2);
    cairo_move_to(cr, 0, b);
    cairo_rel_line_to(cr, b / 2, 0);

    cairo_move_to(cr, mediaw - b, 0);
    cairo_rel_line_to(cr, 0, b / 2);
    cairo_move_to(cr, mediaw - b / 2, b);
    cairo_rel_line_to(cr, b / 2, 0);

    cairo_move_to(cr, b, mediah);
    cairo_rel_line_to(cr, 0, -b / 2);
    cairo_move_to(cr, 0, mediah - b);
    cairo_rel_line_to(cr, b / 2, 0);

    cairo_move_to(cr, mediaw - b, mediah);
    cairo_rel_line_to(cr, 0, -b / 2);
    cairo_move_to(cr, mediaw, mediah - b);
    cairo_rel_line_to(cr, -b / 2, 0);

    cairo_set_line_width(cr, 5);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_stroke_preserve(cr);
    cairo_set_line_width(cr, 1);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_stroke(cr);

    capyctx.cmd_Q();
    */
    std::abort();
}

void CapyPdfRenderer::draw_line(Length x0, Length y0, Length x1, Length y1, Length thickness) {
    capyctx.cmd_w(thickness.pt());
    capyctx.cmd_m(x0.pt(), y0.pt());
    capyctx.cmd_l(x1.pt(), y1.pt());
    capyctx.cmd_S();
}

void CapyPdfRenderer::draw_line(
    Length x0, Length y0, Length x1, Length y1, Length thickness, double g, CapyPDF_Line_Cap cap) {
    capyctx.cmd_q();
    capyctx.cmd_G(g);
    capyctx.cmd_J(cap);
    draw_line(x0, y0, x1, y1, thickness);
    capyctx.cmd_Q();
}

ImageInfo CapyPdfRenderer::get_image(const std::string &path) {
    /*
    ImageInfo result;
    auto it = loaded_images.find(path);
    if(it != loaded_images.end()) {
        result.surf = it->second;
    } else {
        result.surf = cairo_image_surface_create_from_png(path.c_str());
        if(cairo_surface_status(result.surf) != CAIRO_STATUS_SUCCESS) {
            printf("Failed to load image %s.\n", path.c_str());
            std::abort();
        }
        loaded_images[path] = result.surf;
    }
    result.w = cairo_image_surface_get_width(result.surf);
    result.h = cairo_image_surface_get_height(result.surf);
    return result;
    */
    std::abort();
}

void CapyPdfRenderer::draw_image(const ImageInfo &image, Length x, Length y, Length w, Length h) {
    capyctx.cmd_q();
    capyctx.cmd_re(x.pt(), y.pt(), w.pt(), h.pt());
    capyctx.cmd_cm(1, 0, 0, 1, x.pt(), y.pt());
    capyctx.cmd_cm(w.pt() / image.w, 0, 0, h.pt() / image.h, 0, 0);
    // capyctx.cmd_Do(image.id);
    std::abort();
    capyctx.cmd_Q();
}

void CapyPdfRenderer::add_section_outline(int section_number, const std::string &text) {
    /*
    std::string outline = std::to_string(section_number);
    outline += ". ";
    outline += text;
    std::string link = "page=";
    link += std::to_string(page_num());
    cairo_pdf_surface_add_outline(
        surf, CAIRO_PDF_OUTLINE_ROOT, outline.c_str(), link.c_str(), (cairo_pdf_outline_flags_t)0);
*/
    std::abort();
}
