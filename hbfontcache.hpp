// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#pragma once

#include <hb.h>

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

class HBFontCache {
public:
    HBFontCache();

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
