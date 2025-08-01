// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <hb.h>
#include <cassert>

#include <filesystem>
#include <memory>

struct HBFontCloser {
    void operator()(hb_font_t *f) const noexcept { hb_font_destroy(f); }
};

struct HBBufferCloser {
    void operator()(hb_buffer_t *b) const noexcept { hb_buffer_destroy(b); }
};

struct FontFiles {
    std::string regular;
    std::string italic;
    std::string bold;
    std::string bolditalic;
};

struct FontPtrs {
    std::unique_ptr<hb_font_t, HBFontCloser> regular;
    std::unique_ptr<hb_font_t, HBFontCloser> italic;
    std::unique_ptr<hb_font_t, HBFontCloser> bold;
    std::unique_ptr<hb_font_t, HBFontCloser> bolditalic;
};

enum class TextCategory : uint8_t {
    Serif,
    SansSerif,
    Monospace,
};

enum class TextStyle {
    Regular,
    Italic,
    Bold,
    BoldItalic,
};

enum class TextExtra : uint8_t {
    None,
    SmallCaps,
};

class FontCache {
public:
    FontCache();

    hb_font_t *get_font(TextCategory cat, TextStyle style);

private:
    static constexpr double NUM_STEPS = 64;

    void
    open_files(FontPtrs &ptrs, const std::filesystem::path &font_root, const FontFiles &fnames);

    hb_font_t *open_file(const std::filesystem::path &font_file);

    FontPtrs serif;
    FontPtrs sansserif;
    FontPtrs monospace;
};

FontCache::FontCache() {
    std::filesystem::path font_root("/usr/share/fonts/truetype/noto");
    FontFiles nserif{"NotoSerif-Regular.ttf",
                     "NotoSerif-Italic.ttf",
                     "NotoSerif-Bold.ttf",
                     "NotoSerif-BoldItalic.ttf"};
    FontFiles nsans{"NotoSans-Regular.ttf",
                    "NotoSans-Italic.ttf",
                    "NotoSans-Bold.ttf",
                    "NotoSans-BoldItalic.ttf"};
    FontFiles mspace{"NotoMono-Regular.ttf", "", "", ""};
    open_files(serif, font_root, nserif);
    open_files(sansserif, font_root, nserif);
    open_files(monospace, font_root, mspace);
}

void FontCache::open_files(FontPtrs &ptrs,
                           const std::filesystem::path &font_root,
                           const FontFiles &fnames) {
    if(!fnames.regular.empty()) {
        ptrs.regular.reset(open_file(font_root / fnames.regular));
    }
    if(!fnames.italic.empty()) {
        ptrs.italic.reset(open_file(font_root / fnames.italic));
    }
    if(!fnames.bold.empty()) {
        ptrs.bold.reset(open_file(font_root / fnames.bold));
    }
    if(!fnames.bolditalic.empty()) {
        ptrs.bolditalic.reset(open_file(font_root / fnames.bolditalic));
    }
}

hb_font_t *FontCache::open_file(const std::filesystem::path &fontfile) {
    hb_blob_t *blob = hb_blob_create_from_file(fontfile.string().c_str());
    if(!blob) {
        throw std::runtime_error("HB file open failed.");
    }
    hb_face_t *face = hb_face_create(blob, 0);
    hb_blob_destroy(blob);
    if(!face) {
        throw std::runtime_error("HB face creation failed.\n");
    }
    hb_font_t *font = hb_font_create(face);
    hb_face_destroy(face);
    if(!font) {
        throw std::runtime_error("HB font creation failed.\n");
    }
    // hb_font_set_scale(font, hbscale, hbscale);
    return font;
}

hb_font_t *FontCache::get_font(TextCategory cat, TextStyle style) {
    FontPtrs *p;
    switch(cat) {
    case TextCategory::Serif:
        p = &serif;
        break;
    case TextCategory::SansSerif:
        p = &sansserif;
        break;
    case TextCategory::Monospace:
        p = &monospace;
        break;
    }
    switch(style) {
    case TextStyle::Regular:
        return p->regular.get();
    case TextStyle::Italic:
        return p->italic.get();
    case TextStyle::Bold:
        return p->bold.get();
    case TextStyle::BoldItalic:
        return p->bolditalic.get();
    }
    std::abort();
}

double width_test(FontCache &fc,
                  TextCategory cat,
                  TextStyle style,
                  TextExtra extra,
                  double pointsize,
                  const char *u8text) {
    double total_width = 0;
    hb_buffer_t *buf;
    const double num_steps = 64;
    const double hbscale = pointsize * num_steps;
    buf = hb_buffer_create();
    assert(buf);
    std::unique_ptr<hb_buffer_t, HBBufferCloser> bc(buf);
    hb_buffer_add_utf8(buf, u8text, -1, 0, -1);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_buffer_guess_segment_properties(buf);

    hb_font_t *font = fc.get_font(cat, style);
    if(!font) {
        printf("Requested font does not exist.\n");
        std::abort();
    }
    hb_font_set_scale(font, hbscale, hbscale);

    if(extra == TextExtra::None) {
        hb_shape(font, buf, nullptr, 0);
    } else if(extra == TextExtra::SmallCaps) {
        hb_feature_t userfeatures[1];
        userfeatures[0].tag = HB_TAG('s', 'm', 'c', 'p');
        userfeatures[0].value = 1;
        userfeatures[0].start = HB_FEATURE_GLOBAL_START;
        userfeatures[0].end = HB_FEATURE_GLOBAL_END;
        hb_shape(font, buf, userfeatures, 1);
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
    FontCache fc;
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
