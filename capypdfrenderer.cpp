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

#include <capypdfrenderer.hpp>
#include <utils.hpp>

#include <cassert>

#include <hb.h>
#include <glib.h>

#include <hbmeasurer.hpp>
#include <cstring>
#include <algorithm>

#include <sstream>

namespace {

size_t
get_endpoint(hb_glyph_info_t *glyph_info, size_t glyph_count, size_t i, const char *sampletext) {
    if(i + 1 < glyph_count) {
        return glyph_info[i + 1].cluster;
    }
    return strlen(sampletext);
}

void hb_buffer_to_textsequence(hb_buffer_t *buf,
                               capypdf::TextSequence &ts,
                               FontInfo &fontinfo,
                               double hbscale,
                               const char *unshaped_text) {

    unsigned int glyph_count;
    hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
    for(unsigned int i = 0; i < glyph_count; i++) {
        const hb_glyph_info_t *current = glyph_info + i;
        const hb_glyph_position_t *curpos = glyph_pos + i;
        hb_codepoint_t glyphid = current->codepoint;
        hb_position_t x_offset = curpos->x_offset;
        // hb_position_t y_offset = curpos->y_offset;
        hb_position_t x_advance = curpos->x_advance;
        // hb_position_t y_advance = curpos->y_advance;
        const auto original_text_start = unshaped_text + glyph_info[i].cluster;
        const auto original_text_end =
            unshaped_text + get_endpoint(glyph_info, glyph_count, i, unshaped_text);
        const std::string_view original_text(original_text_start, original_text_end);
        const auto hb_glyph_advance_in_font_units =
            hb_font_get_glyph_h_advance(fontinfo.f, current->codepoint) / hbscale *
            fontinfo.units_per_em;
        const auto hb_advance_in_font_units = x_advance / hbscale * fontinfo.units_per_em;
        const int32_t kerning_delta =
            int32_t(hb_glyph_advance_in_font_units - hb_advance_in_font_units);
        const auto *one_char_forward = g_utf8_next_char(original_text_start);
        if(one_char_forward == original_text_end) {
            const uint32_t original_codepoint = g_utf8_get_char(original_text_start);
            ts.append_raw_glyph(glyphid, original_codepoint);
        } else {
            ts.append_ligature_glyph(glyphid, original_text);
        }
        if(kerning_delta != 0) {
            ts.append_kerning(kerning_delta);
        }
    }
}

} // namespace

CapyPdfRenderer::CapyPdfRenderer(const char *ofname,
                                 Length pagew_,
                                 Length pageh_,
                                 Length bleed_,
                                 const capypdf::DocumentProperties &docprop,
                                 HBFontCache &fc_)
    : capygen{ofname, docprop}, ctx{capygen.new_page_context()}, bleed{bleed_.pt()},
      pagew{pagew_.pt()}, pageh{pageh_.pt()}, mediaw{pagew_.pt() + 2 * bleed},
      mediah{pageh_.pt() + 2 * bleed}, fc{fc_}, meas(fc, "fi") {

    init_page();

    ctx.cmd_w(0.1);
    outname = ofname;
}

CapyPdfRenderer::~CapyPdfRenderer() { capygen.write(); }

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

void CapyPdfRenderer::draw_box(Length x, Length y, Length w, Length h, double g, Length thickness) {
    ctx.cmd_q();
    ctx.cmd_w(thickness.pt());
    ctx.cmd_G(g);
    ctx.cmd_re(x.pt(), y.pt(), w.pt(), h.pt());
    ctx.cmd_S();
    ctx.cmd_Q();
}

void CapyPdfRenderer::fill_box(Length x, Length y, Length w, Length h, double color) {
    ctx.cmd_q();
    ctx.cmd_g(color);
    ctx.cmd_re(x.pt(), y.pt(), (x + w).pt(), (y + h).pt());
    ctx.cmd_f();
    ctx.cmd_Q();
}

void CapyPdfRenderer::fill_rounded_corner_box(
    Length x, Length y, Length w, Length h, double color) {

    const double round_fraction = 0.5;
    const auto round_distance = round_fraction * w;

    ctx.cmd_q();
    ctx.cmd_g(color);

    ctx.cmd_m((x + round_fraction * w).pt(), y.pt());
    ctx.cmd_l((x + w - round_distance).pt(), y.pt());
    ctx.cmd_c((x + w).pt(), y.pt(), (x + w).pt(), y.pt(), (x + w).pt(), (y + round_distance).pt());

    ctx.cmd_l((x + w).pt(), (y + h - round_distance).pt());
    ctx.cmd_c((x + w).pt(),
              (y + h).pt(),
              (x + w).pt(),
              (y + h).pt(),
              (x + w - round_distance).pt(),
              (y + h).pt());

    ctx.cmd_l((x + round_distance).pt(), (y + h).pt());
    ctx.cmd_c(
        (x).pt(), (y + h).pt(), (x).pt(), (y + h).pt(), (x).pt(), (y + h - round_distance).pt());

    ctx.cmd_l(x.pt(), (y + round_distance).pt());
    ctx.cmd_c(x.pt(), y.pt(), x.pt(), y.pt(), (x + round_distance).pt(), y.pt());
    ctx.cmd_h();
    ctx.cmd_f();
    ctx.cmd_Q();
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
    ctx.cmd_q();
    ctx.cmd_G(0);
    ctx.cmd_w(thickness.pt());
    ctx.cmd_m(points[0].x.pt(), points[0].y.pt());
    for(size_t i = 1; i < points.size(); ++i) {
        ctx.cmd_l(points[i].x.pt(), points[i].y.pt());
    }
    ctx.cmd_S();
    ctx.cmd_Q();
}

void CapyPdfRenderer::render_line_justified(
    const HBLine line, const HBTextParameters &par, Length line_width, Length x, Length y) {
    const Length text_width = meas.text_width(line);
    const double num_spaces = line.words.size() - 1;
    // assert(num_spaces > 1);
    const Length space_extra_width{num_spaces > 0 ? ((line_width - text_width) / num_spaces)
                                                  : Length::zero()};
    const Length space_extra_width_fontunits = 1000 * space_extra_width;

    hb_buffer_t *buf = hb_buffer_create();
    std::unique_ptr<hb_buffer_t, HBBufferCloser> bc(buf);

    const double num_steps = HBFontCache::NUM_STEPS;

    assert(!line.words.empty());

    capypdf::Text text = ctx.text_new();
    text.cmd_Td(x.pt(), y.pt());
    std::vector<hb_feature_t> features;

    for(size_t i = 0; i < line.words.size(); ++i) {
        const bool is_first = i == 0;
        const bool is_last = i == line.words.size() - 1;
        const auto &word = line.words[i];
        capypdf::TextSequence ts = capypdf::TextSequence();
        for(const auto &run : word.runs) {
            // FIXME, only set font if the style changes.
            auto fontinfo = std::move(fc.get_font(run.par.par).value());
            auto capyfont_id = hbfont2capyfont(run.par, fontinfo);
            const double run_font_size = run.par.size.pt();

            const double hbscale = run_font_size * num_steps;

            hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
            hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
            hb_buffer_set_language(buf, hb_language_from_string("fi", -1));
            hb_buffer_add_utf8(buf, run.text.data(), run.text.size(), 0, -1);

            hb_font_set_scale(fontinfo.f, hbscale, hbscale);

            features.clear();
            append_shaping_options(run.par, features);
            hb_shape(fontinfo.f, buf, features.data(), features.size());

            text.cmd_Tf(capyfont_id, run.par.size.pt());
            hb_buffer_to_textsequence(buf, ts, fontinfo, hbscale, run.text.c_str());
            if(!is_last) {
                ts.append_kerning(-space_extra_width_fontunits.pt() / run.par.size.pt());
            }
            hb_buffer_reset(buf);
        }
        text.cmd_TJ(ts);
    }
    ctx.render_text_obj(text);
}

void CapyPdfRenderer::render_text_as_is(const char *line,
                                        const HBTextParameters &par,
                                        Length x,
                                        Length y) {
    auto line_size = strlen(line);
    if(line_size == 0) {
        return;
    }
    HBRun single_run;
    single_run.text = line;
    single_run.par = par;
    render_run(single_run, x, y);
}

void CapyPdfRenderer::render_run(const HBRun &run, Length x, Length y) {
    auto fontinfo = std::move(fc.get_font(run.par.par).value());
    auto capyfont_id = hbfont2capyfont(run.par, fontinfo);
    hb_buffer_t *buf = hb_buffer_create();
    std::unique_ptr<hb_buffer_t, HBBufferCloser> bc(buf);

    const double num_steps = HBFontCache::NUM_STEPS;
    const double hbscale = run.par.size.pt() * num_steps;

    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("fi", -1));

    hb_buffer_guess_segment_properties(buf);
    hb_font_set_scale(fontinfo.f, hbscale, hbscale);

    capypdf::Text text = ctx.text_new();
    text.cmd_Tf(capyfont_id, run.par.size.pt());
    text.cmd_Td(x.pt(), y.pt());
    serialize_single_run(run, text, buf);

    ctx.render_text_obj(text);
}

void CapyPdfRenderer::render_text_as_is(
    const char *line, const HBTextParameters &par, Length x, Length y, TextAlignment align) {
    if(align == TextAlignment::Left) {
        render_text_as_is(line, par, x, y);
    } else {
        const auto line_width = meas.text_width(line, par);
        if(align == TextAlignment::Right) {
            render_text_as_is(line, par, x - line_width, y);
        } else {
            render_text_as_is(line, par, x - line_width / 2, y);
        }
    }
}

void CapyPdfRenderer::serialize_single_run(const HBRun &run,
                                           capypdf::Text &tobj,
                                           hb_buffer_t *buf) {
    auto fontinfo = std::move(fc.get_font(run.par.par).value());
    auto *hbfont = fontinfo.f;

    if(run.text.empty()) {
        return;
    }

    const double num_steps = HBFontCache::NUM_STEPS;
    const double hbscale = run.par.size.pt() * num_steps;

    hb_buffer_clear_contents(buf);
    hb_buffer_add_utf8(buf, run.text.data(), run.text.size(), 0, -1);

    hb_buffer_guess_segment_properties(buf);
    hb_font_set_scale(hbfont, hbscale, hbscale);

    // tobj.cmd_Tf(capyfont_id, run.par.size.pt());
    capypdf::TextSequence ts;

    std::vector<hb_feature_t> features;
    append_shaping_options(run.par, features);
    hb_shape(hbfont, buf, features.data(), features.size());

    hb_buffer_to_textsequence(buf, ts, fontinfo, hbscale, run.text.c_str());

    tobj.cmd_TJ(ts);
}

void CapyPdfRenderer::render_text(
    const char *line, const HBTextParameters &par, Length x, Length y, TextAlignment alignment) {
    if(alignment == TextAlignment::Left) {
        render_text_as_is(line, par, x, y);
        return;
    }

    Length text_width = meas.text_width(line, par);
    switch(alignment) {
    case TextAlignment::Left:
        std::abort();
        // Unreachable
        break;
    case TextAlignment::Centered:
        render_text_as_is(line, par, x - text_width / 2, y);
        break;
    case TextAlignment::Right:
        render_text_as_is(line, par, x - text_width, y);
        break;
    }
}

void CapyPdfRenderer::render_runs(const std::vector<HBRun> &runs,
                                  Length x,
                                  Length y,
                                  TextAlignment alignment) {
    // FIXME, use alignment.

    hb_buffer_t *buf = hb_buffer_create();
    std::unique_ptr<hb_buffer_t, HBBufferCloser> bc(buf);

    const double num_steps = HBFontCache::NUM_STEPS;

    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("fi", -1));

    hb_buffer_guess_segment_properties(buf);

    capypdf::Text text = ctx.text_new();
    text.cmd_Td(x.pt(), y.pt());
    for(size_t i = 0; i < runs.size(); ++i) {
        const auto &run = runs[i];
        if(i == 0 || runs[i].par != runs[i - 1].par) {
            auto fontinfo = std::move(fc.get_font(run.par.par).value());
            auto capyfont_id = hbfont2capyfont(run.par, fontinfo);
            const double hbscale = run.par.size.pt() * num_steps;
            hb_font_set_scale(fontinfo.f, hbscale, hbscale);
            text.cmd_Tf(capyfont_id, run.par.size.pt());
        }
        serialize_single_run(run, text, buf);
    }

    ctx.render_text_obj(text);
}

void CapyPdfRenderer::render_wonky_text(const char *text,
                                        const HBTextParameters &par,
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
    capygen.add_page(ctx);
    init_page();
    ++pages;
}

void CapyPdfRenderer::init_page() {
    if(bleed > 0) {
        ctx.cmd_q();
        ctx.cmd_cm(1.0, 0, 0, 1.0, bleed, bleed);
    }
}

void CapyPdfRenderer::finalize_page() {
    if(bleed > 0) {
        ctx.cmd_Q();
        draw_cropmarks();
    }
}

void CapyPdfRenderer::draw_cropmarks() {
    const auto b = bleed;

    ctx.cmd_q();
    ctx.cmd_m(b, 0);
    ctx.cmd_l(b, b / 2);
    ctx.cmd_m(0, b);
    ctx.cmd_l(b / 2, b);

    ctx.cmd_m(b, 0);
    ctx.cmd_l(b, b / 2);
    ctx.cmd_m(0, b);
    ctx.cmd_l(b / 2, b);

    ctx.cmd_m(mediaw - b, 0);
    ctx.cmd_l(mediaw - b, b / 2);
    ctx.cmd_m(mediaw - b / 2, b);
    ctx.cmd_l(mediaw, b);

    ctx.cmd_m(b, mediah);
    ctx.cmd_l(b, mediah - b / 2);
    ctx.cmd_m(0, mediah - b);
    ctx.cmd_l(b / 2, mediah - b);

    ctx.cmd_m(mediaw - b, mediah);
    ctx.cmd_l(mediaw - b, mediah - b / 2);
    ctx.cmd_m(mediaw, mediah - b);
    ctx.cmd_l(mediaw - b / 2, mediah - b);

    /*
    capyctx.cmd_w(5);
    capyctx.cmd_G(1);
    capyctx.cmd_S();
    cairo_stroke_preserve(cr);
*/
    ctx.cmd_w(1);
    ctx.cmd_G(0);
    ctx.cmd_S();
    ctx.cmd_Q();
}

void CapyPdfRenderer::draw_line(Length x0, Length y0, Length x1, Length y1, Length thickness) {
    ctx.cmd_w(thickness.pt());
    ctx.cmd_m(x0.pt(), y0.pt());
    ctx.cmd_l(x1.pt(), y1.pt());
    ctx.cmd_S();
}

void CapyPdfRenderer::draw_line(
    Length x0, Length y0, Length x1, Length y1, Length thickness, double g, CapyPDF_Line_Cap cap) {
    ctx.cmd_q();
    ctx.cmd_G(g);
    ctx.cmd_J(cap);
    draw_line(x0, y0, x1, y1, thickness);
    ctx.cmd_Q();
}

CapyImageInfo CapyPdfRenderer::get_image(const std::filesystem::path &path) {
    auto it = loaded_images.find(path);
    if(it != loaded_images.end()) {
        return it->second;
    }

    CapyImageInfo info;
    capypdf::RasterImage rimage = capygen.load_image(path.string().c_str());
    auto size = rimage.get_size();
    info.w = size.w;
    info.h = size.h;
    capypdf::ImagePdfProperties iprops;
    info.id = capygen.add_image(rimage, iprops);
    loaded_images[path] = info;

    return info;
}

void CapyPdfRenderer::draw_image(
    const CapyImageInfo &image, Length x, Length y, Length w, Length h) {
    ctx.cmd_q();
    ctx.cmd_cm(1, 0, 0, 1, x.pt(), y.pt());
    ctx.cmd_cm(w.pt(), 0, 0, h.pt(), 0, 0);
    ctx.cmd_Do(image.id);
    ctx.cmd_Q();
}

void CapyPdfRenderer::add_section_outline(int section_number, const std::string &text) {
    std::string outline = std::to_string(section_number);
    outline += ". ";
    outline += text;
    capypdf::Destination dest;
    dest.set_page_xyz(page_num() - 1, {}, {}, {});
    capypdf::Outline ol;
    ol.set_title(outline);
    ol.set_destination(dest);
    capygen.add_outline(ol);
}

CapyPDF_FontId CapyPdfRenderer::hbfont2capyfont(const HBTextParameters &par,
                                                const FontInfo &fontinfo) {
    assert(fontinfo.f);
    auto it = loaded_fonts.find(fontinfo.f);
    if(it != loaded_fonts.end()) {
        return it->second;
    }
    capypdf::FontProperties fprop;
    auto font_id = capygen.load_font(fontinfo.fname->string().c_str(), fprop);
    loaded_fonts[fontinfo.f] = font_id;
    return font_id;
}
