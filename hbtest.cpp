// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <hb.h>

#include <filesystem>
#include <memory>

struct HBFontCloser {
    void operator()(hb_font_t *f) const noexcept { hb_font_destroy(f); }
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

int width_test(
    FontCache &fc, TextStyle style, TextExtra extra, double pointsize, const char *u8text) {
    return pointsize;
}

int main(int, char **) {
    FontCache fc;
    const int text_width = width_test(fc, TextStyle::Regular, TextExtra::None, 12, "abc");
    printf("Width of text is: %d\n", text_width);
    return 0;
}
