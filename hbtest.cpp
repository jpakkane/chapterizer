// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <hbfontcache.hpp>

#include <cassert>

#include <memory>
#include <optional>

double width_test(HBFontCache &fc,
                  TextCategory cat,
                  TextStyle style,
                  TextExtra extra,
                  double pointsize,
                  const char *u8text) {
    const char *language = "en";
    double total_width = 0;
    hb_buffer_t *buf;
    const double num_steps = 64;
    const double hbscale = pointsize * num_steps;
    buf = hb_buffer_create();
    assert(buf);
    std::unique_ptr<hb_buffer_t, HBBufferCloser> bc(buf);
    // If we start reusing buffers in the future.
    // hb_buffer_clear_contents(buf):
    hb_buffer_add_utf8(buf, u8text, -1, 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string(language, -1));

    hb_buffer_guess_segment_properties(buf);

    auto font_o = fc.get_font(cat, style);
    if(!font_o) {
        printf("Requested font does not exist.\n");
        std::abort();
    }
    auto &font = font_o.value();
    hb_font_set_scale(font.f, hbscale, hbscale);

    if(extra == TextExtra::None) {
        hb_shape(font.f, buf, nullptr, 0);
    } else if(extra == TextExtra::SmallCaps) {
        hb_feature_t userfeatures[1];
        userfeatures[0].tag = HB_TAG('s', 'm', 'c', 'p');
        userfeatures[0].value = 1;
        userfeatures[0].start = HB_FEATURE_GLOBAL_START;
        userfeatures[0].end = HB_FEATURE_GLOBAL_END;
        hb_shape(font.f, buf, userfeatures, 1);
    } else {
        std::abort();
    }

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
        total_width += advance;
    }

    return total_width * pointsize;
}

int main(int, char **) {
    HBFontCache fc;
    const char *text = "Hello world!";
    const double ptsize = 10;
    double text_width =
        width_test(fc, TextCategory::Serif, TextStyle::Regular, TextExtra::None, ptsize, text);
    printf("Width of text is: %.2f\n", text_width);
    text_width =
        width_test(fc, TextCategory::Serif, TextStyle::Italic, TextExtra::None, ptsize, text);
    printf("Width of italic text is: %.2f\n", text_width);
    text_width =
        width_test(fc, TextCategory::Serif, TextStyle::Regular, TextExtra::SmallCaps, ptsize, text);
    printf("Width of smallcaps text is: %.2f\n", text_width);
    text_width =
        width_test(fc, TextCategory::Serif, TextStyle::Regular, TextExtra::None, ptsize, text);
    printf("Width of text is: %.2f\n", text_width);
    return 0;
}
