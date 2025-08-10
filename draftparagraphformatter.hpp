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

#include <paragraphformatter.hpp>
#include <hbmeasurer.hpp>
#include <chaptercommon.hpp>
#include <wordhyphenator.hpp>
#include <formatting.hpp>
#include <utils.hpp>

class HBFontCache;

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
    HBChapterParameters lists;
    HBChapterParameters title;
    HBChapterParameters author;
    HBChapterParameters colophon;
    HBChapterParameters dedication;
};

class DraftParagraphFormatter {
public:
    DraftParagraphFormatter(const std::vector<EnrichedWord> &words,
                            const Length target_width,
                            const HBChapterParameters &in_params,
                            HBFontCache &hbfc);

    std::vector<std::vector<std::string>> split_formatted_lines();

    std::vector<std::vector<HBRun>> split_formatted_lines_to_runs();

private:
    void precompute();
    TextLocation point_to_location(const SplitPoint &p) const;
    LineStats
    get_closest_line_end(size_t start_split, const HBMeasurer &shaper, size_t line_num) const;
    LineStats
    compute_closest_line_end(size_t start_split, const HBMeasurer &shaper, size_t line_num) const;

    std::vector<LineStats> simple_split(HBMeasurer &shaper);
    std::vector<std::vector<std::string>>
    stats_to_markup_lines(const std::vector<LineStats> &linestats) const;
    Length current_line_width(size_t line_num) const;

    std::vector<std::vector<HBRun>>
    stats_to_line_runs(const std::vector<LineStats> &linestats) const;

    WordsOnLine words_for_splits(size_t from_split_ind, size_t to_split_ind) const;
    std::string build_line_markup(size_t from_split_ind, size_t to_split_ind) const;
    std::string build_line_text_debug(size_t from_split_ind, size_t to_split_ind) const;
    std::vector<std::string> build_line_words_markup(size_t from_split_ind,
                                                     size_t to_split_ind) const;
    std::vector<HBRun> build_line_words_runs(size_t from_split_ind, size_t to_split_ind) const;

    Length paragraph_width;
    std::vector<EnrichedWord> words;
    std::vector<SplitPoint> split_points;
    std::vector<TextLocation> split_locations;

    StyleStack determine_style(TextLocation t) const;

    // Cached results of best states we have achieved thus far.
    SplitStates state_cache;
    HBChapterParameters params;
    ExtraPenaltyAmounts extras;
    HBFontCache &fc;
};
