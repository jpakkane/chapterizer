// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include "hbfontcache.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

HBFontCache::HBFontCache() {
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
    open_files(sansserif, font_root, nsans);
    open_files(monospace, font_root, mspace);
}

void HBFontCache::open_files(FontPtrs &ptrs,
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

FontOwner HBFontCache::open_file(const std::filesystem::path &fontfile) {
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
    FontOwner result{std::move(h), fontfile, get_em_units(fontfile)};

    return result;
}

uint32_t HBFontCache::get_em_units(const std::filesystem::path &fontfile) {
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

std::optional<FontInfo> HBFontCache::get_font(TextCategory cat, TextStyle style) const {
    const FontPtrs *p;
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
