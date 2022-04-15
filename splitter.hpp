#pragma once

#include <wordhyphenator.hpp>
#include <variant>

class TextShaper;

// clang-format off

/*
 * Numbering of splitting points is tricky.
 *
 * You want to be able to do build_line(from_ind, to_ind)
 *
 * 0  1   2  3   4
 * |  |   |  |   |
 * V  V   V  V   V
 * foo-bar fu-baz
 *
 * Thus it needs a split point at both ends.
 *
 * Word index _can_ point one-past-the-end.
 *
 * Hyphen index can _not_ point one-past-the-end.
 */

// clang-format on

struct BetweenWordSplit {
    size_t word_index;
};

struct WithinWordSplit {
    size_t word_index;
    size_t hyphen_index;
};

struct TextLocation {
    size_t word_index;
    size_t offset; // in characters
};

typedef std::variant<BetweenWordSplit, WithinWordSplit> SplitPoint;

class Splitter {
public:
    Splitter(const std::vector<HyphenatedWord> &words, double paragraph_width_mm);

    std::vector<std::string> split_lines();

private:
    void precompute();
    TextLocation point_to_location(const SplitPoint &p) const;
    size_t get_line_end(size_t start_split, TextShaper &shaper) const;

    std::string build_line(size_t from_split, size_t to_split) const;
    std::vector<HyphenatedWord> words;
    double target_width; // in mm
    std::vector<SplitPoint> split_points;
    std::vector<TextLocation> split_locations;
};
