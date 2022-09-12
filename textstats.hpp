/*
 * Copyright 2022 Jussi Pakkanen
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

#include <chaptercommon.hpp>

#include <pango/pangocairo.h>
#include <string>
#include <unordered_map>

struct StyledText {
    std::string text;
    FontParameters font;

    bool operator==(const StyledText &o) const noexcept { return text == o.text && font == o.font; }
};

template<> struct std::hash<StyledText> {
    std::size_t operator()(StyledText const &s) const noexcept {
        auto h1 = std::hash<std::string>{}(s.text);
        auto h2 = std::hash<FontParameters>{}(s.font);
        return ((h1 * 13) + h2);
    }
};

class TextStats {
public:
    TextStats();
    ~TextStats();

    double text_width(const char *utf8_text, const FontParameters &font) const;

    double text_width(const std::string &s, const FontParameters &font) const {
        return text_width(s.c_str(), font);
    };

    double markup_width(const char *utf8_text, const FontParameters &font) const;

private:
    void set_pango_state(const char *utf8_text,
                         const FontParameters &font,
                         bool is_markup = false) const;

    cairo_t *cr;
    cairo_surface_t *surface;
    PangoLayout *layout;
    mutable std::unordered_map<StyledText, double> widths;
};
