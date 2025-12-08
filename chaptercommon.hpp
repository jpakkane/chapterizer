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

#include <units.hpp>

#include <functional>
#include <string>
#include <cmath>
#include <cstdint>

enum class TextAlignment : int {
    Left,
    Centered,
    Right,
    // Justified is stored in a separate struct.
};

enum class ExtraPenaltyTypes : int {
    ConsecutiveDashes,
    // River,
    SingleWordLastLine,
    SplitWordLastLine,
};

struct ExtraPenaltyStatistics {
    ExtraPenaltyTypes type;
    int line;
    double penalty;
};

struct ExtraPenaltyAmounts {
    double multiple_dashes = 10; // Total is num_consecutive_dashes * multiple_dashes.
    // double river;
    double single_word_line = 10;
    double single_split_word_line = 500;
};

// Some fonts have "medium" instead of "regular" as their weight.
// For example Nimbus Roman. This is currently ignored, might need
// to be fixed.
enum class FontStyle : int {
    Regular,
    Italic,
    Bold,
    BoldItalic,
};

struct FontParameters {
    std::string name;                    // Fontconfig name as a string.
    Length size = Length::from_pt(1000); // Be careful with comparisons.
    FontStyle type = FontStyle::Regular;

    bool operator==(const FontParameters &o) const noexcept {
        return name == o.name && (fabs((size - o.size).pt()) < 0.05) && type == o.type;
    }
};

template<> struct std::hash<FontParameters> {
    std::size_t operator()(FontParameters const &s) const noexcept {
        auto h1 = std::hash<int>{}(int(10 * s.size.pt()));
        auto h2 = std::hash<int>{}(int(s.type));
        auto h3 = std::hash<std::string>{}(s.name);
        return ((h1 * 13) + h2) * 13 + h3;
    }
};

struct ChapterParameters {
    Length line_height;
    Length indent; // Of first line.
    FontParameters font;
    bool indent_last_line = false;
};

struct FontStyles {
    FontParameters basic;
    FontParameters heading;
    FontParameters code;
    FontParameters footnote;
};

struct ChapterStyles {
    ChapterParameters normal;
    ChapterParameters normal_noindent;
    ChapterParameters code;
    ChapterParameters section;
    ChapterParameters letter;
    ChapterParameters footnote;
    ChapterParameters lists;
    ChapterParameters title;
    ChapterParameters author;
    ChapterParameters colophon;
    ChapterParameters dedication;
};

// Replace types above with these once HB
// is the only backend.

enum class TextCategory : uint8_t {
    Serif,
    SansSerif,
    Monospace,
};

enum class TextStyle : uint8_t {
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

struct HBTextParameters {
    Length size = Length::from_pt(1000); // Be careful with comparisons.
    HBFontProperties par;

    bool operator==(const HBTextParameters &o) const noexcept {
        return (fabs((size - o.size).pt()) < 0.05) && par == o.par;
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

struct HBChapterParameters {
    Length line_height;
    Length indent; // Of first line.
    HBTextParameters font;
    bool indent_last_line = false;
};

struct HBFontStyles {
    HBTextParameters basic;
    HBTextParameters heading;
    HBTextParameters code;
    HBTextParameters footnote;
};

struct HBChapterStyles {
    HBChapterParameters normal;
    HBChapterParameters normal_noindent;
    HBChapterParameters code;
    HBChapterParameters section;
    HBChapterParameters letter;
    HBChapterParameters footnote;
    HBChapterParameters sign;
    HBChapterParameters lists;
    HBChapterParameters title;
    HBChapterParameters author;
    HBChapterParameters colophon;
    HBChapterParameters dedication;
};

struct HBRun {
    HBTextParameters par;
    std::string text;
};

struct HBWord {
    std::vector<HBRun> runs;
};

struct HBLine {
    std::vector<HBWord> words;
};

struct HBStyledPlainText {
    std::string text;
    HBTextParameters font;

    bool operator==(const HBStyledPlainText &o) const noexcept {
        return text == o.text && font == o.font;
    }
};

template<> struct std::hash<HBStyledPlainText> {
    std::size_t operator()(HBStyledPlainText const &s) const noexcept {
        auto h1 = std::hash<std::string>{}(s.text);
        auto h2 = std::hash<HBTextParameters>{}(s.font);
        return (h1 * 13) + h2;
    }
};
