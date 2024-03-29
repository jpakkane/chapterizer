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
#include <wordhyphenator.hpp>
#include <formatting.hpp>
#include <utils.hpp>
#include <variant>
#include <optional>

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
    Length text_width;
    // int num_spaces;
    bool ends_in_dash;
};

struct UpTo {
    double penalty;
    std::vector<LineStats> splits;

    bool operator<(const UpTo &o) const { return penalty < o.penalty; }
};

struct WordStart {
    size_t word;
    size_t from_bytes;
};

struct WordEnd {
    size_t word;
    size_t to_bytes;
    bool add_dash;
};

struct WordsOnLine {
    std::optional<WordStart> first;
    size_t full_word_begin;
    size_t full_word_end;
    std::optional<WordEnd> last;
};

struct SplitStates {
    size_t cache_size = 12;
    std::vector<std::vector<UpTo>> best_to;

    void clear() { best_to.clear(); }

    bool abandon_search(const std::vector<LineStats> &new_splits, const double new_penalty);
};

struct LinePenaltyStatistics {
    Length delta;
    double penalty;
};

typedef std::variant<BetweenWordSplit, WithinWordSplit> SplitPoint;

struct PenaltyStatistics {
    std::vector<LinePenaltyStatistics> lines;
    std::vector<ExtraPenaltyStatistics> extras;
};

PenaltyStatistics compute_stats(const std::vector<std::string> &lines,
                                const Length paragraph_width,
                                const ChapterParameters &par,
                                const ExtraPenaltyAmounts &amounts);

class ParagraphFormatter {
public:
    ParagraphFormatter(const std::vector<EnrichedWord> &words,
                       const Length target_width,
                       const ChapterParameters &in_params,
                       const ExtraPenaltyAmounts &ea);

    std::vector<std::string> split_lines();
    std::vector<std::vector<std::string>> split_formatted_lines();

    double paragraph_end_penalty(const std::vector<LineStats> &lines) const;

private:
    void precompute();
    TextLocation point_to_location(const SplitPoint &p) const;
    LineStats
    get_closest_line_end(size_t start_split, const TextStats &shaper, size_t line_num) const;
    LineStats
    compute_closest_line_end(size_t start_split, const TextStats &shaper, size_t line_num) const;

    std::vector<LineStats>
    get_line_end_choices(size_t start_split, const TextStats &shaper, size_t line_num) const;

    std::vector<LineStats> simple_split(TextStats &shaper);
    std::vector<std::vector<std::string>> global_split_markup(const TextStats &shaper);
    void global_split_recursive(const TextStats &shaper,
                                std::vector<LineStats> &line_stats,
                                size_t split_pos);
    std::vector<std::vector<std::string>>
    stats_to_markup_lines(const std::vector<LineStats> &linestats) const;
    Length current_line_width(size_t line_num) const;
    double total_penalty(const std::vector<LineStats> &lines, bool is_complete = false) const;

    WordsOnLine words_for_splits(size_t from_split_ind, size_t to_split_ind) const;
    std::string build_line_markup(size_t from_split_ind, size_t to_split_ind) const;
    std::string build_line_text_debug(size_t from_split_ind, size_t to_split_ind) const;
    std::vector<std::string> build_line_words_markup(size_t from_split_ind,
                                                     size_t to_split_ind) const;

    Length paragraph_width;
    std::vector<EnrichedWord> words;
    std::vector<SplitPoint> split_points;
    std::vector<TextLocation> split_locations;

    StyleStack determine_style(TextLocation t) const;

    double best_penalty = 1e100;
    std::vector<LineStats> best_split;

    // Cached results of best states we have achieved thus far.
    SplitStates state_cache;
    ChapterParameters params;
    ExtraPenaltyAmounts extras;

    mutable std::unordered_map<size_t, LineStats> closest_line_ends;
};
