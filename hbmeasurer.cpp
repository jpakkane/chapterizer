/*
 * Copyright 2025 Jussi Pakkanen
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

#include "hbmeasurer.hpp"
#include <utils.hpp>

#include <glib.h>
#include <cassert>

namespace {

const std::unordered_map<uint32_t, double> overhang_right{
    {'.', 0.8},
    {',', 0.8},
    {':', 0.8},
    {';', 0.8},
    {'!', 0.7},
    {'?', 0.4},
    //
    {'o', 0.2},
    {'p', 0.2},
    {'v', 0.2},
    {'b', 0.2},
    {'r', 0.2},
    //
    {'\'', 0.5},
    {'"', 0.5},
    {0xbb, 0.5},
    {0x201d, 0.5},
    {0x2019, 0.3},
    //
    {0x2013, 0.55},
    {0x2014, 0.50},
    {'-', 0.6},
};

}

void append_shaping_options(const HBTextParameters &par, std::vector<hb_feature_t> &out) {
    if(par.par.extra == TextExtra::SmallCaps) {
        hb_feature_t userfeature;
        userfeature.tag = HB_TAG('s', 'm', 'c', 'p');
        userfeature.value = 1;
        userfeature.start = HB_FEATURE_GLOBAL_START;
        userfeature.end = HB_FEATURE_GLOBAL_END;
        out.push_back(std::move(userfeature));
    }
    // FIXME, a hack, but works well enough.
    if(par.par.cat == TextCategory::Serif) {
        hb_feature_t userfeature;
        userfeature.tag = HB_TAG('o', 'n', 'u', 'm');
        userfeature.value = 1;
        userfeature.start = HB_FEATURE_GLOBAL_START;
        userfeature.end = HB_FEATURE_GLOBAL_END;
        out.push_back(std::move(userfeature));
    }
}

HBMeasurer::HBMeasurer(const HBFontCache &cache, const char *language) : fc{cache} {
    buf = hb_buffer_create();
    assert(buf);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string(language, -1));
}

HBMeasurer::~HBMeasurer() { hb_buffer_destroy(buf); }

Length HBMeasurer::text_width(const char *utf8_text, const HBTextParameters &text_par) const {
    HBStyledPlainText k{utf8_text, text_par};
    auto f = plaintext_widths.find(k);
    if(f != plaintext_widths.end()) {
        return f->second;
    }
    Length total_size = compute_width(utf8_text, text_par);
    plaintext_widths[k] = total_size;
    return total_size;
}

Length HBMeasurer::text_width(const HBLine &line) const {
    Length total_size;
    for(const auto &w : line.words) {
        total_size += text_width(w);
    }
    return total_size;
}

Length HBMeasurer::text_width(const HBWord &word) const {
    Length total_size;
    for(const auto &r : word.runs) {
        total_size += compute_width(r.text.c_str(), r.par);
    }
    return total_size;
}

Length HBMeasurer::text_width(const std::vector<HBRun> &runs) const {
    Length total_size;
    for(const auto &r : runs) {
        total_size += compute_width(r.text.c_str(), r.par);
    }
    return total_size;
}

Length HBMeasurer::compute_width(const char *utf8_text, const HBTextParameters &text_par) const {
    const double num_steps = 64;
    const double hbscale = text_par.size.pt() * num_steps;
    double total_width = 0;
    hb_buffer_clear_contents(buf);
    hb_buffer_add_utf8(buf, utf8_text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);

    auto font_o = fc.get_font(text_par.par.cat, text_par.par.style);
    if(!font_o) {
        printf("Requested font does not exist.\n");
        std::abort();
    }
    auto &font = font_o.value();
    hb_font_set_scale(font.f, hbscale, hbscale);

    std::vector<hb_feature_t> features;
    append_shaping_options(text_par, features);
    hb_shape(font.f, buf, features.data(), features.size());

    unsigned int glyph_count;
    hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    if(!glyph_info) {
        printf("Could not get glyph info.\n");
        std::abort();
    }
    hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);
    if(!glyph_pos) {
        printf("Could not get glyph pos.\n");
        std::abort();
    }

    for(unsigned int i = 0; i < glyph_count; i++) {
        // const hb_glyph_info_t *current = glyph_info + i;
        const hb_glyph_position_t *curpos = glyph_pos + i;
        const auto advance = curpos->x_advance / hbscale;
        // Glyph's plain advance can be obtained with
        // hb_font_get_glyph_h_advance(font, current->codepoint);
        total_width += advance;
    }
    Length total_size = total_width * text_par.size;
    return total_size;
}

Length HBMeasurer::codepoint_right_overhang(const uint32_t uchar,
                                            const HBTextParameters &font) const {
    auto it = overhang_right.find(uchar);
    if(it == overhang_right.end()) {
        return Length{};
    }
    const double hang_fraction = it->second;
    char tmp[10];
    memset(tmp, 0, 10);
    g_unichar_to_utf8(uchar, tmp);
    assert(g_utf8_validate(tmp, -1, nullptr));
    const auto letter_width = text_width(tmp, font);
    return hang_fraction * letter_width;
}
