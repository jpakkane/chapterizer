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

#include "paragraphformatter.hpp"
#include <textstats.hpp>
#include <algorithm>
#include <numeric>
#include <optional>
#include <cassert>
#include <cmath>

namespace {

int next_char_utf8_length(const char *src) {
    gunichar c = g_utf8_get_char(src);
    return g_unichar_to_utf8(c, nullptr);
}

double difference_penalty(double actual_width, double target_width) {
    // assert(actual_width >= 0);
    assert(target_width > 0);
    const auto delta = abs(actual_width - target_width);
    return delta * delta;
}

double line_penalty(const LineStats &line, double target_width) {
    return difference_penalty(line.text_width, target_width);
}

std::vector<LinePenaltyStatistics> compute_line_penalties(const std::vector<std::string> &lines,
                                                          const ChapterParameters &par) {
    TextStats shaper;
    std::vector<LinePenaltyStatistics> penalties;
    penalties.reserve(lines.size());
    double indent = par.indent;
    for(const auto &line : lines) {
        const double w = shaper.text_width(line, par.font);
        const double delta = w - (par.paragraph_width_mm - indent);
        indent = 0;
        penalties.emplace_back(LinePenaltyStatistics{delta, pow(delta, 2)});
    }
    // The last line does not get a length penalty unless it is the only line.
    if(penalties.size() > 1) {
        penalties.back() = LinePenaltyStatistics{0, 0};
    }
    return penalties;
}

bool line_ends_in_dash(const std::string &line) {
    // FIXME: Unicode.
    return !line.empty() && line.back() == '-';
}

bool line_ends_in_dash(const LineStats &line) { return line.ends_in_dash; }

template<typename T>
std::vector<ExtraPenaltyStatistics>
compute_multihyphen_penalties(const std::vector<T> &lines, const ExtraPenaltyAmounts &amounts) {
    std::vector<ExtraPenaltyStatistics> penalties;
    int dashes = 0;
    for(size_t i = 0; i < lines.size(); ++i) {
        const auto &line = lines[i];
        if(line_ends_in_dash(line)) {
            ++dashes;
        } else {
            if(dashes >= 3) {
                penalties.emplace_back(ExtraPenaltyStatistics{ExtraPenaltyTypes::ConsecutiveDashes,
                                                              int(i) - dashes,
                                                              dashes * amounts.multiple_dashes});
            }
            dashes = 0;
        }
    }
    if(dashes >= 3) {
        penalties.emplace_back(ExtraPenaltyStatistics{ExtraPenaltyTypes::ConsecutiveDashes,
                                                      int(lines.size()) - 1 - dashes,
                                                      dashes * amounts.multiple_dashes});
    }
    return penalties;
}

double total_penalty(const std::vector<LineStats> &lines,
                     const ChapterParameters &params,
                     const ExtraPenaltyAmounts &amounts) {
    double total = 0;
    double last_line_penalty = 0;
    double indent = params.indent;
    for(const auto &l : lines) {
        last_line_penalty = line_penalty(l, params.paragraph_width_mm - indent);
        indent = 0;
        total += last_line_penalty;
    }
    const auto line_penalty = total - last_line_penalty;
    const auto extras = compute_multihyphen_penalties(lines, amounts);
    const auto extra_penalty = std::accumulate(
        extras.begin(), extras.end(), 0.0, [](double i, const ExtraPenaltyStatistics &stats) {
            return i + stats.penalty;
        });
    // FIXME, add chapter end penalty.
    return line_penalty + extra_penalty;
}

std::optional<ExtraPenaltyStatistics>
compute_chapter_end_penalty(const std::vector<std::string> &lines,
                            const ExtraPenaltyAmounts &amounts) {
    std::vector<ExtraPenaltyStatistics> penalties;
    if(lines.size() < 2) {
        return {};
    }
    if(lines.back().find(' ') == std::string::npos) {
        const auto &penultimate_line = lines[lines.size() - 2];
        assert(!penultimate_line.empty());
        if(penultimate_line.back() == '-') {
            return ExtraPenaltyStatistics{ExtraPenaltyTypes::SplitWordLastLine,
                                          int(lines.size()) - 2,
                                          amounts.single_split_word_line};
        }
        return ExtraPenaltyStatistics{
            ExtraPenaltyTypes::SingleWordLastLine, int(lines.size() - 1), amounts.single_word_line};
    }
    return {};
}

std::vector<ExtraPenaltyStatistics> compute_extra_penalties(const std::vector<std::string> &lines,
                                                            const ExtraPenaltyAmounts &amounts) {
    std::vector<ExtraPenaltyStatistics> penalties = compute_multihyphen_penalties(lines, amounts);
    //{ExtraPenaltyStatistics{ExtraPenaltyTypes::ConsecutiveDashes, 0, 20},
    //                    ExtraPenaltyStatistics{ExtraPenaltyTypes::SplitWordLastLine, 1, 100}}   ;
    auto end_penalty = compute_chapter_end_penalty(lines, amounts);
    if(end_penalty) {
        penalties.push_back(*end_penalty);
    }
    return penalties;
}

} // namespace

PenaltyStatistics compute_stats(const std::vector<std::string> &lines,
                                const ChapterParameters &par,
                                const ExtraPenaltyAmounts &amounts) {

    return PenaltyStatistics{compute_line_penalties(lines, par),
                             compute_extra_penalties(lines, amounts)};
}

ParagraphFormatter::ParagraphFormatter(const std::vector<HyphenatedWord> &words_,
                                       const ChapterParameters &in_params,
                                       const ExtraPenaltyAmounts &ea)
    : words{words_}, params{in_params}, extras(ea) {}

std::vector<std::string> ParagraphFormatter::split_lines() {
    precompute();
    TextStats shaper;
    best_penalty = 1e100;
    best_split.clear();
    if(false) {
        best_split = simple_split(shaper);
        best_penalty = total_penalty(best_split, params, extras);
        return stats_to_lines(best_split);
    } else {
        return global_split(shaper);
    }
}

std::vector<LineStats> ParagraphFormatter::simple_split(TextStats &shaper) {
    std::vector<LineStats> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    while(current_split < split_points.size() - 1) {
        auto line_end = get_closest_line_end(current_split, shaper, lines.size());
        lines.emplace_back(line_end);
        current_split = line_end.end_split;
    }
    const auto total = total_penalty(lines, params, extras);
    printf("Total penalty: %.1f\n", total);
    return lines;
}

std::vector<std::string>
ParagraphFormatter::stats_to_lines(const std::vector<LineStats> &linestats) const {
    std::vector<std::string> lines;
    lines.reserve(linestats.size());
    lines.emplace_back(build_line(0, linestats[0].end_split));
    for(size_t i = 1; i < linestats.size(); ++i) {
        lines.emplace_back(build_line(linestats[i - 1].end_split, linestats[i].end_split));
    }
    return lines;
}

double ParagraphFormatter::current_line_width(size_t line_num) const {
    if(line_num == 0) {
        return params.paragraph_width_mm - params.indent;
    }
    return params.paragraph_width_mm;
}

std::vector<std::string> ParagraphFormatter::global_split(const TextStats &shaper) {
    std::vector<std::string> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    std::vector<LineStats> line_stats;

    global_split_recursive(shaper, line_stats, current_split);
    printf("Total penalty: %.2f\n", best_penalty);
    return stats_to_lines(best_split);
}

void ParagraphFormatter::global_split_recursive(const TextStats &shaper,
                                                std::vector<LineStats> &line_stats,
                                                size_t current_split) {
    if(state_cache.abandon_search(line_stats, params, extras)) {
        return;
    }
    auto line_end_choices = get_line_end_choices(current_split, shaper, line_stats.size());
    if(line_end_choices.front().end_split == split_points.size() - 1) {
        // Text exhausted.
        line_stats.emplace_back(line_end_choices.front());
        const auto current_penalty = total_penalty(line_stats, params, extras);
        // printf("Total penalty: %.1f\n", current_penalty);
        // FIXME: change to include extra penalties here.
        if(current_penalty < best_penalty) {
            best_penalty = current_penalty;
            best_split = line_stats;
        }
        line_stats.pop_back();
    } else {
        for(const auto &line_choice : line_end_choices) {
            line_stats.emplace_back(line_choice);
            current_split = line_choice.end_split;
            const auto sanity_check = line_stats.size();
            global_split_recursive(shaper, line_stats, current_split);
            assert(sanity_check == line_stats.size());
            line_stats.pop_back();
        }
    }
}

void ParagraphFormatter::precompute() {
    split_points.clear();
    split_points.reserve(words.size() * 3);
    for(size_t word_index = 0; word_index < words.size(); ++word_index) {
        split_points.emplace_back(BetweenWordSplit{word_index});
        for(size_t hyphen_index = 0; hyphen_index < words[word_index].hyphen_points.size();
            ++hyphen_index) {
            split_points.emplace_back(WithinWordSplit{word_index, hyphen_index});
        }
    }
    split_points.emplace_back(BetweenWordSplit{words.size()}); // The end sentinel
    printf("The text has a total of %d words and %d split points.\n\n",
           (int)words.size(),
           (int)split_points.size());
    split_locations.clear();
    split_locations.reserve(split_points.size());
    for(const auto &i : split_points) {
        split_locations.emplace_back(point_to_location(i));
    }
    assert(split_points.size() == split_locations.size());
    state_cache.clear();
    for(size_t i = 0; i < split_points.size(); ++i) {
        state_cache.best_to.emplace_back(std::vector<UpTo>{});
    }
}

std::string ParagraphFormatter::build_line(size_t from_split_ind, size_t to_split_ind) const {
    assert(to_split_ind >= from_split_ind);
    std::string line;
    if(to_split_ind == from_split_ind) {
        return line;
    }
    const auto &from_split = split_points[from_split_ind];
    const auto &to_split = split_points[to_split_ind];
    const auto &from_loc = split_locations[from_split_ind];
    const auto &to_loc = split_locations[to_split_ind];

    // A single word spans the entire line.
    const bool pathological_single_word = from_loc.word_index == to_loc.word_index;
    if(pathological_single_word) {
        const auto &bad_word = words[from_loc.word_index].word;
        auto substr_start = from_loc.offset + 1;
        auto substr_length = to_loc.offset - from_loc.offset;
        if(from_loc.offset == 0) {
            // Indices in the middle point to the last character of the leftside word,
            // because that is how the hyphenator code sets them.
            // However we also need to point to the "very first" character, which is at
            // index 0. In that case the "pointed to" character needs to be part of the substring.
            substr_start -= 1;
            substr_length += 1;
        }
        line = bad_word.substr(substr_start, substr_length);
        if(std::holds_alternative<WithinWordSplit>(to_split)) {
            const auto &source_loc = std::get<WithinWordSplit>(to_split);
            if(words[source_loc.word_index].hyphen_points[source_loc.hyphen_index].type ==
               SplitType::Regular) {
                line += '-';
            }
        }
        assert(g_utf8_validate(line.c_str(), line.size(), nullptr));
        return line;
    }
    assert(g_utf8_validate(line.c_str(), line.size(), nullptr));

    // First word.
    if(std::holds_alternative<BetweenWordSplit>(from_split)) {
        line = words[from_loc.word_index].word;
    } else {
        const auto &word_to_split = words[from_loc.word_index];
        const auto sv = std::string_view(word_to_split.word);
        line = sv.substr(from_loc.offset + 1);
    }
    assert(g_utf8_validate(line.c_str(), line.size(), nullptr));

    if(from_loc.word_index == to_loc.word_index) {
        // Pathological case, one word spans the entire line.
    } else {
        assert(to_loc.word_index > from_loc.word_index);
        size_t current_word_index = from_loc.word_index + 1;
        // Intermediate words.
        while(current_word_index < to_loc.word_index && current_word_index < words.size()) {
            line += ' ';
            line += words[current_word_index].word;
            ++current_word_index;
        }
    }
    assert(g_utf8_validate(line.c_str(), line.size(), nullptr));

    // Final word.
    if(std::holds_alternative<BetweenWordSplit>(to_split)) {
        // Do nothing, I guess?
    } else {
        const auto &source_loc = std::get<WithinWordSplit>(to_split);
        line += ' ';
        const auto &word_to_split = words[to_loc.word_index];
        const auto sv = std::string_view(word_to_split.word);
        line += sv.substr(0, to_loc.offset + 1);
        assert(g_utf8_validate(line.c_str(), line.size(), nullptr));
        if(words[source_loc.word_index].hyphen_points[source_loc.hyphen_index].type ==
           SplitType::Regular) {
            line += '-';
        }
    }

    return line;
}

TextLocation ParagraphFormatter::point_to_location(const SplitPoint &p) const {
    if(std::holds_alternative<BetweenWordSplit>(p)) {
        const auto &r = std::get<BetweenWordSplit>(p);
        return TextLocation{r.word_index, 0};
    } else if(std::holds_alternative<WithinWordSplit>(p)) {
        const auto &r = std::get<WithinWordSplit>(p);
        return TextLocation{r.word_index, words[r.word_index].hyphen_points[r.hyphen_index].loc};
    } else {
        assert(false);
    }
}

LineStats ParagraphFormatter::get_closest_line_end(size_t start_split,
                                                   const TextStats &shaper,
                                                   size_t line_num) const {
    auto f = closest_line_ends.find(start_split);
    if(f != closest_line_ends.end()) {
        return f->second;
    }
    auto val = compute_closest_line_end(start_split, shaper, line_num);
    closest_line_ends[start_split] = val;
    return val;
}

LineStats ParagraphFormatter::compute_closest_line_end(size_t start_split,
                                                       const TextStats &shaper,
                                                       size_t line_num) const {
    assert(start_split < split_points.size() - 1);
    double target_line_width_mm = current_line_width(line_num);
    size_t chosen_point = -1;
    auto ppoint = std::partition_point(
        split_points.begin() + start_split + 2,
        split_points.end(),
        [this, &shaper, start_split, target_line_width_mm](const SplitPoint &p) {
            const auto loc = &p - split_points.data();
            const auto trial_line = build_line(start_split, loc);
            const auto trial_width = shaper.text_width(trial_line, params.font);
            return trial_width <= target_line_width_mm;
        });
    if(ppoint == split_points.end()) {
        chosen_point = split_points.size() - 1;
    } else {
        --ppoint; // We want the last point that satisfies the constraint rather than the first
                  // which does not.
        chosen_point = size_t(&(*ppoint) - split_points.data());
    }

    const auto final_line = build_line(start_split, chosen_point);
    const auto final_width = shaper.text_width(final_line, params.font);
    // FIXME, check whether the word ends in a dash.
    return LineStats{chosen_point,
                     final_width,
                     std::holds_alternative<WithinWordSplit>(split_points[chosen_point])};
}

// Sorted by decreasing fitness.
std::vector<LineStats> ParagraphFormatter::get_line_end_choices(size_t start_split,
                                                                const TextStats &shaper,
                                                                size_t line_num) const {
    std::vector<LineStats> potentials;
    potentials.reserve(5);
    auto tightest_split = get_closest_line_end(start_split, shaper, line_num);
    potentials.push_back(tightest_split);

    // Lambdas, yo!
    auto add_point = [&](size_t split_point) {
        const auto trial_split = split_point;
        const auto trial_line = build_line(start_split, trial_split);
        const auto trial_width = shaper.text_width(trial_line, params.font);
        potentials.emplace_back(LineStats{
            trial_split,
            trial_width,
            std::holds_alternative<WithinWordSplit>(
                split_points[trial_split])}); // FIXME, check if word ends with a dash character.
    };

    if(tightest_split.end_split > start_split + 2) {
        add_point(tightest_split.end_split - 1);
    }
    if(tightest_split.end_split + 2 < split_points.size()) {
        add_point(tightest_split.end_split + 1);
    }

    if(tightest_split.end_split > start_split + 3) {
        add_point(tightest_split.end_split - 2);
    }

    return potentials;
}

bool SplitStates::abandon_search(const std::vector<LineStats> &new_splits,
                                 const ChapterParameters &params,
                                 const ExtraPenaltyAmounts &extras) {
    const double new_penalty = total_penalty(new_splits, params, extras);
    const auto current_index = new_splits.size();
    auto &current_slot = best_to[current_index];
    if(current_slot.size() >= cache_size && current_slot.back().penalty < new_penalty) {
        return true;
    }
    UpTo new_value{new_penalty, new_splits};
    auto insertion_point = std::lower_bound(current_slot.begin(), current_slot.end(), new_value);
    current_slot.insert(insertion_point, std::move(new_value));
    while(current_slot.size() > cache_size) {
        current_slot.pop_back();
    }
    return false;
}
