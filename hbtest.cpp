// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <hb.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cassert>

#include <filesystem>
#include <memory>
#include <optional>

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

struct FontOwner {
    std::unique_ptr<hb_font_t, HBFontCloser> handle;
    uint32_t units_per_em;
};

struct FontPtrs {
    FontOwner regular;
    FontOwner italic;
    FontOwner bold;
    FontOwner bolditalic;
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

struct FontInfo {
    FontInfo() = default;
    FontInfo(const FontOwner &o) {
        f = o.handle.get();
        units_per_em = o.units_per_em;
    }
    hb_font_t *f;
    uint32_t units_per_em;
};

class FontCache {
public:
    FontCache();

    std::optional<FontInfo> get_font(TextCategory cat, TextStyle style);

private:
    static constexpr double NUM_STEPS = 64;

    void
    open_files(FontPtrs &ptrs, const std::filesystem::path &font_root, const FontFiles &fnames);

    FontOwner open_file(const std::filesystem::path &font_file);

    uint32_t get_em_units(const std::filesystem::path &fontfile);

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
        ptrs.regular = open_file(font_root / fnames.regular);
    }
    if(!fnames.italic.empty()) {
        ptrs.italic = open_file(font_root / fnames.italic);
    }
    if(!fnames.bold.empty()) {
        ptrs.bold = open_file(font_root / fnames.bold);
    }
    if(!fnames.bolditalic.empty()) {
        ptrs.bolditalic = open_file(font_root / fnames.bolditalic);
    }
}

FontOwner FontCache::open_file(const std::filesystem::path &fontfile) {
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
    std::unique_ptr<hb_font_t, HBFontCloser> h{font};
    FontOwner result{std::move(h), get_em_units(fontfile)};

    return result;
}

uint32_t FontCache::get_em_units(const std::filesystem::path &fontfile) {
    // HB does not seem to expose this value to end users, but it
    // is required to make PDF kerning work.
    FT_Library ft;
    FT_Face ftface;
    FT_Error fte;
    fte = FT_Init_FreeType(&ft);
    if(fte != 0) {
        std::abort();
    }
    fte = FT_New_Face(ft, fontfile.string().c_str(), 0, &ftface);
    if(fte != 0) {
        std::abort();
    }
    uint32_t units = ftface->units_per_EM;
    FT_Done_Face(ftface);
    FT_Done_FreeType(ft);
    return units;
}

std::optional<FontInfo> FontCache::get_font(TextCategory cat, TextStyle style) {
    FontPtrs *p;
    FontInfo result;
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
        result = FontInfo(p->regular);
        break;
    case TextStyle::Italic:
        result = FontInfo(p->italic);
        break;
    case TextStyle::Bold:
        result = FontInfo(p->bold);
        break;
    case TextStyle::BoldItalic:
        result = FontInfo(p->bolditalic);
        break;
    }
    if(!result.f) {
        return {};
    }
    return result;
}

double width_test(FontCache &fc,
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
