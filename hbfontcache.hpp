// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <units.hpp>
#include <hb.h>
#include <cmath>

#include <filesystem>
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
    std::filesystem::path file;
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

struct HBFontProperties {
    TextCategory cat = TextCategory::Serif;
    TextStyle style = TextStyle::Regular;
    TextExtra extra = TextExtra::None;

    bool operator==(const HBFontProperties &o) const noexcept {
        return cat == o.cat && style == o.style && extra == o.extra;
    }
};

template<> struct std::hash<HBFontProperties> {
    std::size_t operator()(HBFontProperties const &fp) const noexcept {
        const size_t shuffle = 13;
        auto h1 = std::hash<size_t>{}((size_t)fp.cat);
        auto h2 = std::hash<size_t>{}((size_t)fp.style);
        auto h3 = std::hash<size_t>{}((size_t)fp.extra);
        size_t hashvalue = (h1 * shuffle) + h2;
        hashvalue = hashvalue * shuffle + h3;
        return hashvalue;
    }
};

struct FontInfo {
    FontInfo() = default;
    FontInfo(const FontOwner &o) {
        f = o.handle.get();
        fname = &o.file;
        units_per_em = o.units_per_em;
    }
    hb_font_t *f;
    const std::filesystem::path *fname;
    uint32_t units_per_em;
};

class HBFontCache {
public:
    HBFontCache();

    std::optional<FontInfo> get_font(TextCategory cat, TextStyle style) const;
    std::optional<FontInfo> get_font(const HBFontProperties &par) const {
        return get_font(par.cat, par.style);
    }

    static constexpr double NUM_STEPS = 64;

private:
    void
    open_files(FontPtrs &ptrs, const std::filesystem::path &font_root, const FontFiles &fnames);

    FontOwner open_file(const std::filesystem::path &font_file);

    uint32_t get_em_units(const std::filesystem::path &fontfile);

    FontPtrs serif;
    FontPtrs sansserif;
    FontPtrs monospace;
};
