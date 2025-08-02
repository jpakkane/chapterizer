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

#pragma once

#include <hbfontcache.hpp>

#include <chaptercommon.hpp>
#include <utils.hpp>

#include <string>
#include <unordered_map>

struct HBTextParameters {
    Length size = Length::from_pt(1000); // Be careful with comparisons.
    HBFontProperties par;

    bool operator==(const HBTextParameters &o) const noexcept {
        return (fabs((size - o.size).pt()) < 0.05) && par == o.par;
    }
};

struct HBStyledPlainText {
    std::string text;
    HBTextParameters font;

    bool operator==(const HBStyledPlainText &o) const noexcept {
        return text == o.text && font == o.font;
    }
};

template<> struct std::hash<HBTextParameters> {
    std::size_t operator()(HBTextParameters const &fp) const noexcept {
        const size_t shuffle = 13;
        auto h1 = std::hash<size_t>{}((size_t)(fp.size.pt() + 0.001));
        auto h2 = std::hash<HBFontProperties>{}(fp.par);
        size_t hashvalue = (h1 * shuffle) + h2;
        return hashvalue;
    }
};

template<> struct std::hash<HBStyledPlainText> {
    std::size_t operator()(HBStyledPlainText const &s) const noexcept {
        auto h1 = std::hash<std::string>{}(s.text);
        auto h2 = std::hash<HBTextParameters>{}(s.font);
        return (h1 * 13) + h2;
    }
};

// Measures a single "run", that is, a single text sequence with the same font properties.

class HBMeasurer {
public:
    explicit HBMeasurer(const HBFontCache &cache, const char *language);
    ~HBMeasurer();

    Length text_width(const char *utf8_text, const HBTextParameters &font) const;

    Length text_width(const std::string &s, const HBTextParameters &font) const {
        return text_width(s.c_str(), font);
    };

    Length codepoint_right_overhang(const uint32_t uchar, const HBTextParameters &font) const;

private:
    Length compute_width(const char *utf8_text, const HBTextParameters &text_par) const;

    const HBFontCache &fc;

    hb_buffer_t *buf;
    mutable std::unordered_map<HBStyledPlainText, Length> plaintext_widths;
};
