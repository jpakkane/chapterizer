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

double difference_penalty(Millimeter actual_width, Millimeter target_width) {
    // assert(actual_width >= 0);
    assert(target_width > Millimeter::from_value(0));
    const auto delta = abs((actual_width - target_width).v);
    return delta * delta;
}

double line_penalty(const LineStats &line, Millimeter target_width) {
    return difference_penalty(line.text_width, target_width);
}

std::vector<LinePenaltyStatistics> compute_line_penalties(const std::vector<std::string> &lines,
                                                          const ChapterParameters &par) {
    TextStats shaper;
    std::vector<LinePenaltyStatistics> penalties;
    penalties.reserve(lines.size());
    Millimeter indent = par.indent;
    for(const auto &line : lines) {
        const Millimeter w = shaper.text_width(line, par.font);
        const Millimeter delta = w - (par.paragraph_width - indent);
        indent = Millimeter::from_value(0);
        penalties.emplace_back(LinePenaltyStatistics{delta, pow(delta.v, 2)});
    }
    // The last line does not get a length penalty unless it is the only line.
    if(penalties.size() > 1) {
        penalties.back() = LinePenaltyStatistics{Millimeter(), 0};
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
    Millimeter indent = params.indent;
    for(const auto &l : lines) {
        last_line_penalty = line_penalty(l, params.paragraph_width - indent);
        indent = Millimeter::from_value(0);
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

void toggle_format(StyleStack &current_style, std::string &line, const char format_to_toggle) {
    if(current_style.contains(format_to_toggle)) {
        current_style.pop(format_to_toggle);
        switch(format_to_toggle) {
        case ITALIC_S:
            line.append("</i>");
            break;
        case BOLD_S:
            line.append("</b>");
            break;
        case TT_S:
            line.append("</tt>");
            break;
        case SMALLCAPS_S:
            line.append("</span>");
            break;
        case SUPERSCRIPT_S:
            line.append("</sup>");
            break;
        default:
            printf("Bad style end bit.\n");
            std::abort();
        }
    } else {
        current_style.push(format_to_toggle);
        switch(format_to_toggle) {
        case ITALIC_S:
            line.append("<i>");
            break;
        case BOLD_S:
            line.append("<b>");
            break;
        case TT_S:
            line.append("<tt>");
            break;
        case SMALLCAPS_S:
            line.append("<span variant=\"small-caps\" letter_spacing=\"100\">");
            break;
        case SUPERSCRIPT_S:
            line.append("<sup>");
            break;
        default:
            printf("Bad style start bit.\n");
            std::abort();
        }
    }
}

} // namespace

PenaltyStatistics compute_stats(const std::vector<std::string> &lines,
                                const ChapterParameters &par,
                                const ExtraPenaltyAmounts &amounts) {

    return PenaltyStatistics{compute_line_penalties(lines, par),
                             compute_extra_penalties(lines, amounts)};
}

ParagraphFormatter::ParagraphFormatter(const std::vector<EnrichedWord> &words_,
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
        std::abort();
    } else {
        std::abort();
    }
}

std::vector<std::vector<std::string>> ParagraphFormatter::split_formatted_lines() {
    precompute();
    TextStats shaper;
    best_penalty = 1e100;
    best_split.clear();
    return global_split_markup(shaper);
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

std::vector<std::vector<std::string>>
ParagraphFormatter::stats_to_markup_lines(const std::vector<LineStats> &linestats) const {
    std::vector<std::vector<std::string>> lines;
    lines.reserve(linestats.size());
    lines.emplace_back(build_line_words_markup(0, linestats[0].end_split));
    for(size_t i = 1; i < linestats.size(); ++i) {
        lines.emplace_back(
            build_line_words_markup(linestats[i - 1].end_split, linestats[i].end_split));
    }
    return lines;
}

Millimeter ParagraphFormatter::current_line_width(size_t line_num) const {
    if(line_num == 0) {
        return params.paragraph_width - params.indent;
    }
    return params.paragraph_width;
}

std::vector<std::vector<std::string>>
ParagraphFormatter::global_split_markup(const TextStats &shaper) {
    std::vector<std::string> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    std::vector<LineStats> line_stats;

    global_split_recursive(shaper, line_stats, current_split);
    // printf("Total penalty: %.2f\n", best_penalty);
    return stats_to_markup_lines(best_split);
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
    // printf("The text has a total of %d words and %d split points.\n\n",
    //        (int)words.size(),
    //        (int)split_points.size());
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

std::string ParagraphFormatter::build_line_markup(size_t from_split_ind,
                                                  size_t to_split_ind) const {
    std::string line;
    const auto markup_words = build_line_words_markup(from_split_ind, to_split_ind);
    for(const auto &w : markup_words) {
        line += w;
    }
    return line;
}

std::vector<std::string> ParagraphFormatter::build_line_words_markup(size_t from_split_ind,
                                                                     size_t to_split_ind) const {
    assert(to_split_ind >= from_split_ind);
    std::vector<std::string> line;
    if(to_split_ind == from_split_ind) {
        return line;
    }
    // const auto &from_split = split_points[from_split_ind];
    const auto &to_split = split_points[to_split_ind];
    const auto &from_loc = split_locations[from_split_ind];
    const auto &to_loc = split_locations[to_split_ind];

    StyleStack current_style = determine_style(from_loc);

    bool first_word = true;
    std::string markup;
    for(size_t word_index = from_loc.word_index; word_index <= to_loc.word_index; ++word_index) {
        markup.clear();
        if(word_index >= words.size()) {
            continue;
        }
        if(word_index == to_loc.word_index && to_loc.offset == 0) {
            continue;
        }
        current_style.write_buildup_markup(markup);
        const auto &current_word = words[word_index];
        size_t word_start = -1;
        size_t word_end = -1;
        size_t style_point = 0;
        const bool last_word = (word_index == to_loc.word_index) ||
                               (word_index + 1 == to_loc.word_index && to_loc.offset == 0);
        if(first_word) {
            first_word = false;
            word_start = from_loc.offset;
            if(word_start != 0) {
                ++word_start;
            }
        } else {
            word_start = 0;
        }
        if(last_word && std::holds_alternative<WithinWordSplit>(to_split)) {
            word_end = to_loc.offset + 1;
        } else {
            word_end = current_word.text.length();
        }
        assert(word_end >= word_start);
        while(style_point < current_word.f.size() &&
              current_word.f[style_point].offset < word_start) {
            ++style_point;
        }
        std::string_view view =
            std::string_view(current_word.text).substr(word_start, word_end - word_start);
        std::string debug_aid{view};
        assert(g_utf8_validate(view.data(), view.length(), nullptr));
        for(size_t i = 0; i < view.size(); ++i) {
            while(style_point < current_word.f.size() &&
                  current_word.f[style_point].offset == word_start + i) {
                toggle_format(current_style, markup, current_word.f[style_point].format);
                ++style_point;
            }
            if(view[i] == '<') {
                markup += "&lt;";
            } else if(view[i] == '>') {
                markup += "&gt;";
            } else {
                markup += view[i];
            }
        }

        if(last_word) {
            if(std::holds_alternative<WithinWordSplit>(to_split)) {
                const auto &source_loc = std::get<WithinWordSplit>(to_split);
                if(words[source_loc.word_index].hyphen_points[source_loc.hyphen_index].type ==
                   SplitType::Regular) {
                    markup += '-';
                }
            }
        }
        while(style_point < current_word.f.size()) {
            toggle_format(current_style, markup, current_word.f[style_point].format);
            ++style_point;
        }
        if(!last_word) {
            markup += ' '; // The space must be inside the marku to get the correct width.
        }
        current_style.write_teardown_markup(markup);
        assert(g_utf8_validate(markup.c_str(), -1, nullptr));
        line.emplace_back(std::move(markup));
    }

    return line;
}

StyleStack ParagraphFormatter::determine_style(TextLocation t) const {
    const auto word = words[t.word_index].text;
    StyleStack style = words[t.word_index].start_style;
    // FIXME, advance to the actual location.
    return style;
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
    const Millimeter target_line_width_mm = current_line_width(line_num);
    size_t chosen_point = -1;
    auto ppoint = std::partition_point(
        split_points.begin() + start_split + 2,
        split_points.end(),
        [this, &shaper, start_split, target_line_width_mm](const SplitPoint &p) {
            const auto loc = &p - split_points.data();
            const auto trial_line = build_line_markup(start_split, loc);
            const auto trial_width = shaper.markup_width(trial_line.c_str(), params.font);
            return trial_width <= target_line_width_mm;
        });
    if(ppoint == split_points.end()) {
        chosen_point = split_points.size() - 1;
    } else {
        --ppoint; // We want the last point that satisfies the constraint rather than the first
                  // which does not.
        chosen_point = size_t(&(*ppoint) - split_points.data());
    }

    const auto final_line = build_line_markup(start_split, chosen_point);
    const auto final_width = shaper.markup_width(final_line.c_str(), params.font);
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
        const auto trial_line = build_line_markup(start_split, trial_split);
        const auto trial_width = shaper.markup_width(trial_line.c_str(), params.font);
        potentials.emplace_back(
            LineStats{trial_split,
                      trial_width,
                      std::holds_alternative<WithinWordSplit>(
                          split_points[trial_split])}); // FIXME, check if word ends with a dash
                                                        // character.
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
