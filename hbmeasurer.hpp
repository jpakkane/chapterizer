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

// Measures a single "run", that is, a single text sequence with the same font properties.

void append_shaping_options(const HBTextParameters &pars, std::vector<hb_feature_t> &out);

class HBMeasurer {
public:
    explicit HBMeasurer(const HBFontCache &cache, const char *language);
    ~HBMeasurer();

    Length text_width(const char *utf8_text, const HBTextParameters &font) const;

    Length text_width(const std::string &s, const HBTextParameters &font) const {
        return text_width(s.c_str(), font);
    };

    Length text_width(const HBLine &line) const;

    Length text_width(const std::vector<HBRun> &runs) const;

    Length text_width(const HBWord &word) const;

    Length codepoint_right_overhang(const uint32_t uchar, const HBTextParameters &font) const;

    Length markup_width(const char *utf8_text, const HBTextParameters &font) const;

private:
    Length compute_width(const char *utf8_text, const HBTextParameters &text_par) const;

    const HBFontCache &fc;

    hb_buffer_t *buf;
    mutable std::unordered_map<HBStyledPlainText, Length> plaintext_widths;
};
