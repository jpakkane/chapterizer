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
    double single_split_word_line = 50;
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
    std::string name;                     // Fontconfig name as a string.
    Point size = Point::from_value(1000); // Be careful with comparisons.
    FontStyle type = FontStyle::Regular;

    bool operator==(const FontParameters &o) const noexcept {
        return name == o.name && (fabs((size - o.size).v) < 0.05) && type == o.type;
    }
};

template<> struct std::hash<FontParameters> {
    std::size_t operator()(FontParameters const &s) const noexcept {
        auto h1 = std::hash<int>{}(int(10 * s.size.v));
        auto h2 = std::hash<int>{}(int(s.type));
        auto h3 = std::hash<std::string>{}(s.name);
        return ((h1 * 13) + h2) * 13 + h3;
    }
};

struct ChapterParameters {
    Millimeter paragraph_width;
    Point line_height;
    Millimeter indent; // Of first line.
    FontParameters font;
};

struct FontStyles {
    FontParameters basic;
    FontParameters heading;
    FontParameters code;
    FontParameters footnote;
};
