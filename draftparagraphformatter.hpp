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
#include <chaptercommon.hpp>
#include <wordhyphenator.hpp>
#include <formatting.hpp>
#include <utils.hpp>

class HBFontCache;

class DraftParagraphFormatter {
public:
    DraftParagraphFormatter(const std::vector<EnrichedWord> &words,
                       const Length target_width,
                       const ChapterParameters &in_params,
                       HBFontCache &hbfc);

    std::vector<std::vector<std::string>> split_formatted_lines();

private:
    TextLocation point_to_location(const SplitPoint &p) const;
    LineStats
    get_closest_line_end(size_t start_split, const TextStats &shaper, size_t line_num) const;
    LineStats
    compute_closest_line_end(size_t start_split, const TextStats &shaper, size_t line_num) const;

    std::vector<LineStats> simple_split(TextStats &shaper);
    std::vector<std::vector<std::string>>
    stats_to_markup_lines(const std::vector<LineStats> &linestats) const;
    Length current_line_width(size_t line_num) const;

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

    // Cached results of best states we have achieved thus far.
    SplitStates state_cache;
    ChapterParameters params;
    ExtraPenaltyAmounts extras;
    HBFontCache &fc;
};
