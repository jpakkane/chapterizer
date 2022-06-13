#pragma once

#include <wordhyphenator.hpp>
#include <variant>

class TextStats;

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

struct LineStats {
    size_t end_split;
    double text_width;
    // int num_spaces;
};

struct UpTo {
    double penalty;
    std::vector<LineStats> splits;

    bool operator<(const UpTo &o) const { return penalty < o.penalty; }
};

struct SplitStates {
    size_t cache_size = 12;
    std::vector<std::vector<UpTo>> best_to;

    void clear() { best_to.clear(); }

    bool abandon_search(const std::vector<LineStats> &new_splits, const double target_width);
};

struct ChapterParameters {
    double paragraph_width_mm;
    std::string font;
    int fontsize;
};

struct PenaltyStatistics {
    double delta; // mm
    double penalty;
};

typedef std::variant<BetweenWordSplit, WithinWordSplit> SplitPoint;

std::vector<PenaltyStatistics> compute_stats(const std::vector<std::string> &lines,
                                             const ChapterParameters &par);

class ChapterBuilder {
public:
    ChapterBuilder(const std::vector<HyphenatedWord> &words, const ChapterParameters &in_params);

    std::vector<std::string> split_lines();

private:
    void precompute();
    TextLocation point_to_location(const SplitPoint &p) const;
    LineStats get_line_end(size_t start_split, const TextStats &shaper) const;
    std::vector<LineStats> get_line_end_choices(size_t start_split, const TextStats &shaper) const;

    std::vector<LineStats> simple_split(TextStats &shaper);
    std::vector<std::string> global_split(const TextStats &shaper);
    void global_split_recursive(const TextStats &shaper,
                                std::vector<LineStats> &line_stats,
                                size_t split_pos);
    std::vector<std::string> stats_to_lines(const std::vector<LineStats> &linestats) const;

    std::string build_line(size_t from_split, size_t to_split) const;
    std::vector<HyphenatedWord> words;
    std::vector<SplitPoint> split_points;
    std::vector<TextLocation> split_locations;

    double best_penalty = 1e100;
    std::vector<LineStats> best_split;

    // Cached results of best states we have achieved thus far.
    SplitStates state_cache;
    ChapterParameters params;
};
