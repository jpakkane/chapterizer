#pragma once

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

enum class FontStyle : int {
    Regular,
    Italic,
    Bold,
    BoldItalic,
};

struct FontParameters {
    std::string name;       // Fontconfig name as a string.
    double point_size = 10; // Be careful with comparisons.
    FontStyle type = FontStyle::Regular;

    bool operator==(const FontParameters &o) const noexcept {
        return name == o.name && (fabs(point_size - o.point_size) < 0.05) && type == o.type;
    }
};

template<> struct std::hash<FontParameters> {
    std::size_t operator()(FontParameters const &s) const noexcept {
        auto h1 = std::hash<int>{}(int(10 * s.point_size));
        auto h2 = std::hash<int>{}(int(s.type));
        auto h3 = std::hash<std::string>{}(s.name);
        return ((h1 * 13) + h2) * 13 + h3;
    }
};

struct ChapterParameters {
    double paragraph_width_mm;
    double line_height_pt;
    double indent; // Of first line.
    FontParameters font;
};
