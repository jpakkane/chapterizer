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

#include <paragraphformatter.hpp>
#include <glib.h>
#include <algorithm>
#include <numeric>
#include <optional>
#include <cassert>
#include <cmath>

namespace {

double difference_penalty(Length actual_width, Length target_width) {
    // assert(actual_width >= 0);
    const double multiplier = actual_width > target_width ? 5.0 : 1.0;
    assert(target_width > Length::zero());
    const auto delta = abs((actual_width - target_width).mm());
    // A potential change would be to divide by the number of spaces on this line.
    return multiplier * delta * delta;
}

double line_penalty(const LineStats &line, Length target_width) {
    return difference_penalty(line.text_width, target_width);
}

std::vector<LinePenaltyStatistics> compute_line_penalties(const std::vector<std::string> &lines,
                                                          const HBChapterParameters &par,
                                                          const Length paragraph_width,
                                                          HBFontCache &fc) {
    const char *language = "fi";
    HBMeasurer shaper(fc, language);
    std::vector<LinePenaltyStatistics> penalties;
    penalties.reserve(lines.size());
    Length indent = par.indent;
    for(const auto &line : lines) {
        const Length w = shaper.text_width(line, par.font);
        const Length delta = w - (paragraph_width - indent);
        indent = Length::zero();
        penalties.emplace_back(LinePenaltyStatistics{delta, pow(delta.mm(), 2)});
    }
    // The last line does not get a length penalty unless it is the only line.
    if(penalties.size() > 1) {
        penalties.back() = LinePenaltyStatistics{Length(), 0};
    }
    return penalties;
}

bool line_ends_in_dash(const std::string &line) {
    // FIXME: Unicode.
    return !line.empty() && line.back() == '-';
}

bool line_ends_in_dash(const LineStats &line) { return line.ends_in_dash; }

static double compute_dash_penalty(size_t num_dashes, const double base_amount) {
    if(num_dashes < 2) {
        return 0;
    }
    return base_amount * pow(5, num_dashes - 1);
}

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
            if(dashes >= 2) {
                penalties.emplace_back(
                    ExtraPenaltyStatistics{ExtraPenaltyTypes::ConsecutiveDashes,
                                           int(i) - dashes,
                                           compute_dash_penalty(dashes, amounts.multiple_dashes)});
            }
            dashes = 0;
        }
    }
    if(dashes >= 2) {
        penalties.emplace_back(
            ExtraPenaltyStatistics{ExtraPenaltyTypes::ConsecutiveDashes,
                                   int(lines.size()) - 1 - dashes,
                                   compute_dash_penalty(dashes, amounts.multiple_dashes)});
    }
    return penalties;
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

void toggle_format(StyleStack &current_style, const char format_to_toggle) {
    if(current_style.contains(format_to_toggle)) {
        current_style.pop(format_to_toggle);
        switch(format_to_toggle) {
        case ITALIC_S:
            break;
        case BOLD_S:
            break;
        case TT_S:
            break;
        case SMALLCAPS_S:
            break;
        case SUPERSCRIPT_S:
            break;
        case SUBSCRIPT_S:
            break;
        default:
            printf("Bad style end bit.\n");
            std::abort();
        }
    } else {
        current_style.push(format_to_toggle);
        switch(format_to_toggle) {
        case ITALIC_S:
            break;
        case BOLD_S:
            break;
        case TT_S:
            break;
        case SMALLCAPS_S:
            break;
        case SUPERSCRIPT_S:
            break;
        case SUBSCRIPT_S:
            break;
        default:
            printf("Bad style start bit.\n");
            std::abort();
        }
    }
}

HBWord wordfragment2runs(const HBTextParameters &original_par,
                         StyleStack &sstack,
                         const EnrichedWord &w,
                         size_t start,
                         size_t end,
                         bool add_space,
                         bool add_dash) {
    HBWord word;
    HBTextParameters active_par = original_par;
    {
        HBStyleApplier tmp(sstack);
        tmp.apply_to_base_style(active_par.par);
    }
    std::string current_run;

    std::string_view view = std::string_view{w.text}.substr(start, end);
    assert(g_utf8_validate(view.data(), view.length(), nullptr));
    size_t style_point = 0;
    while(style_point < w.f.size() && w.f[style_point].offset < start) {
        ++style_point;
    }
    for(size_t i = 0; i < view.size(); ++i) {
        while(style_point < w.f.size() && w.f[style_point].offset == start + i) {
            if(current_run.empty()) {
                // Skip multiple style changes in a row.
            } else {
                word.runs.emplace_back(active_par, std::move(current_run));
                current_run.clear();
            }
            toggle_format(sstack, w.f[style_point].format);
            HBStyleApplier applier(sstack);
            active_par = original_par;
            applier.apply_to_base_style(active_par.par);
            ++style_point;
        }
        current_run += view[i];
    }

    if(add_dash) {
        current_run += '-';
    }
    while(style_point < w.f.size()) {
        toggle_format(sstack, w.f[style_point].format);
        HBStyleApplier applier(sstack);
        applier.apply_to_base_style(active_par.par);
        ++style_point;
    }
    if(add_space) {
        current_run += ' ';
    }
    if(!current_run.empty()) {
        word.runs.emplace_back(active_par, std::move(current_run));
    }
    return word;
}

} // namespace

PenaltyStatistics compute_stats(const std::vector<std::string> &lines,
                                const Length paragraph_width,
                                const HBChapterParameters &par,
                                const ExtraPenaltyAmounts &amounts,
                                HBFontCache &fc) {

    return PenaltyStatistics{compute_line_penalties(lines, par, paragraph_width, fc),
                             compute_extra_penalties(lines, amounts)};
}

ParagraphFormatter::ParagraphFormatter(const std::vector<EnrichedWord> &words_,
                                       const Length target_width,
                                       const HBChapterParameters &in_params,
                                       const ExtraPenaltyAmounts &ea,
                                       HBFontCache &fc_)
    : paragraph_width(target_width), words{words_}, params{in_params}, extras(ea), fc(fc_) {}

std::vector<std::string> ParagraphFormatter::split_lines() {
    precompute();
    HBMeasurer shaper(fc, "fi");
    best_penalty = 1e100;
    best_split.clear();
    if(false) {
        best_split = simple_split(shaper);
        best_penalty = total_penalty(best_split);
        std::abort();
    } else {
        std::abort();
    }
}

std::vector<HBLine> ParagraphFormatter::split_formatted_lines() {
    precompute();
    HBMeasurer shaper{fc, "fi"};
    best_penalty = 1e100;
    best_split.clear();
    return global_split_runs(shaper);
}

std::vector<LineStats> ParagraphFormatter::simple_split(HBMeasurer &shaper) {
    std::vector<LineStats> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    while(current_split < split_points.size() - 1) {
        auto line_end = get_closest_line_end(current_split, shaper, lines.size());
        lines.emplace_back(line_end);
        current_split = line_end.end_split;
    }
    const auto total = total_penalty(lines);
    printf("Total penalty: %.1f\n", total);
    return lines;
}

std::vector<HBLine>
ParagraphFormatter::stats_to_markup_lines(const std::vector<LineStats> &linestats) const {
    std::vector<HBLine> lines;
    lines.reserve(linestats.size());
    lines.push_back(build_line_words_runs(0, linestats[0].end_split));
    for(size_t i = 1; i < linestats.size(); ++i) {
        lines.push_back(build_line_words_runs(linestats[i - 1].end_split, linestats[i].end_split));
    }
    return lines;
}

Length ParagraphFormatter::current_line_width(size_t line_num) const {
    if(line_num == 0) {
        return paragraph_width - params.indent;
    }
    return paragraph_width;
}

std::vector<HBLine> ParagraphFormatter::global_split_runs(const HBMeasurer &shaper) {
    std::vector<std::string> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    std::vector<LineStats> line_stats;

    global_split_recursive(shaper, line_stats, current_split);
    // printf("Total penalty: %.2f\n", best_penalty);
    // for(size_t i = 0; i < split_points.size(); ++i) {
    //    const auto line = build_line_text_debug(i, split_points.size() - 1);
    //    printf("%s\n", line.c_str());
    //}
    return stats_to_markup_lines(best_split);
}

void ParagraphFormatter::global_split_recursive(const HBMeasurer &shaper,
                                                std::vector<LineStats> &line_stats,
                                                size_t current_split) {
    if(state_cache.abandon_search(line_stats, total_penalty(line_stats))) {
        return;
    }
    auto line_end_choices = get_line_end_choices(current_split, shaper, line_stats.size());
    if(line_end_choices.front().end_split == split_points.size() - 1) {
        // Text exhausted.
        line_stats.emplace_back(line_end_choices.front());
        const auto current_penalty = total_penalty(line_stats, true);
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

double ParagraphFormatter::paragraph_end_penalty(const std::vector<LineStats> &lines) const {
    if(lines.size() < 2) {
        return 0;
    }
    const auto &last_split_var = split_points[lines[lines.size() - 1].end_split];
    const auto &penultimate_split_var = split_points[lines[lines.size() - 2].end_split];
    assert(std::holds_alternative<BetweenWordSplit>(last_split_var));
    const auto &last_split = std::get<BetweenWordSplit>(last_split_var);
    assert(last_split.word_index ==
           words.size()); // The last one is a sentinel that points one-past-the-end.
    if(std::holds_alternative<BetweenWordSplit>(penultimate_split_var)) {
        const auto &penultimate_split = std::get<BetweenWordSplit>(penultimate_split_var);
        if(penultimate_split.word_index + 1 == last_split.word_index) {
            return extras.single_word_line;
        }
    } else {
        const auto &penultimate_split = std::get<WithinWordSplit>(penultimate_split_var);
        if(penultimate_split.word_index + 1 == last_split.word_index) {
            return extras.single_split_word_line;
        }
    }
    return 0.0;
}

double ParagraphFormatter::total_penalty(const std::vector<LineStats> &lines,
                                         bool is_complete) const {
    double total = 0;
    double last_line_penalty = 0;
    size_t line_number = 0;
    for(const auto &l : lines) {
        last_line_penalty = line_penalty(l, current_line_width(line_number));
        total += last_line_penalty;
        ++line_number;
    }
    const auto line_penalty = params.indent_last_line ? total : total - last_line_penalty;
    const auto extra_penalties = compute_multihyphen_penalties(lines, extras);
    auto extra_penalty = std::accumulate(
        extra_penalties.begin(),
        extra_penalties.end(),
        0.0,
        [](double i, const ExtraPenaltyStatistics &stats) { return i + stats.penalty; });
    if(is_complete) {
        extra_penalty += paragraph_end_penalty(lines);
    }
    return line_penalty + extra_penalty;
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

WordsOnLine ParagraphFormatter::words_for_splits(size_t from_split_ind, size_t to_split_ind) const {
    WordsOnLine w;
    const auto &from_split = split_points[from_split_ind];
    const auto &to_split = split_points[to_split_ind];
    const auto &from_loc = split_locations[from_split_ind];
    const auto &to_loc = split_locations[to_split_ind];

    if(std::holds_alternative<WithinWordSplit>(from_split)) {
        w.first = WordStart{from_loc.word_index, from_loc.offset + 1};
        w.full_word_begin = from_loc.word_index + 1;
    } else {
        w.full_word_begin = from_loc.word_index;
    }

    w.full_word_end = to_loc.word_index;
    if(std::holds_alternative<WithinWordSplit>(to_split)) {
        const auto &fs = std::get<WithinWordSplit>(to_split);

        w.last =
            WordEnd{fs.word_index,
                    to_loc.offset + 1,
                    words[fs.word_index].hyphen_points[fs.hyphen_index].type == SplitType::Regular};
    }
    return w;
}

std::string ParagraphFormatter::build_line_text_debug(size_t from_split_ind,
                                                      size_t to_split_ind) const {
    const auto w = words_for_splits(from_split_ind, to_split_ind);
    std::string result;
    if(w.first) {
        result = words[w.first->word].text.substr(w.first->from_bytes);
        result += ' ';
        assert(g_utf8_validate(result.c_str(), result.size(), nullptr));
    }
    for(size_t i = w.full_word_begin; i < w.full_word_end; ++i) {
        result += words[i].text;
        if(i + 1 != w.full_word_end) {
            result += ' ';
        }
    }
    assert(g_utf8_validate(result.c_str(), result.size(), nullptr));
    if(w.last) {
        if(!result.empty()) {
            result += ' ';
        }
        result += words[w.last->word].text.substr(0, w.last->to_bytes);
        assert(g_utf8_validate(result.c_str(), result.size(), nullptr));
        if(w.last->add_dash) {
            result += '-';
        }
    }

    return result;
}

HBLine ParagraphFormatter::build_line_markup(size_t from_split_ind, size_t to_split_ind) const {
    return build_line_words_runs(from_split_ind, to_split_ind);
}

StyleStack ParagraphFormatter::determine_style(TextLocation t) const {
    const auto &current_word = words[t.word_index];
    StyleStack style = words[t.word_index].start_style;
    size_t i = 0;
    size_t style_point = 0;
    std::string dummy;
    while(i < t.offset) {
        while(style_point < current_word.f.size() && current_word.f[style_point].offset == i) {
            toggle_format(style, current_word.f[style_point].format);
            ++style_point;
        }
        ++i;
    }
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
                                                   const HBMeasurer &shaper,
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
                                                       const HBMeasurer &shaper,
                                                       size_t line_num) const {
    assert(start_split < split_points.size() - 1);
    const Length target_line_width_mm = current_line_width(line_num);
    size_t chosen_point = -1;
    auto ppoint = std::partition_point(
        split_points.begin() + start_split + 2,
        split_points.end(),
        [this, &shaper, start_split, target_line_width_mm](const SplitPoint &p) {
            const auto loc = &p - split_points.data();
            const auto trial_line = build_line_words_runs(start_split, loc);
            const auto trial_width = shaper.text_width(trial_line);
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
    const auto final_width = shaper.text_width(final_line);
    // FIXME, check whether the word ends in a dash.
    return LineStats{chosen_point,
                     final_width,
                     std::holds_alternative<WithinWordSplit>(split_points[chosen_point])};
}

// Sorted by decreasing fitness.
std::vector<LineStats> ParagraphFormatter::get_line_end_choices(size_t start_split,
                                                                const HBMeasurer &shaper,
                                                                size_t line_num) const {
    std::vector<LineStats> potentials;
    potentials.reserve(5);
    auto tightest_split = get_closest_line_end(start_split, shaper, line_num);
    potentials.push_back(tightest_split);

    bool word_split_seen = false;
    // Lambdas, yo!
    auto check_word_split = [&word_split_seen, this](size_t split_point) {
        if(std::holds_alternative<BetweenWordSplit>(split_points[split_point])) {
            word_split_seen = true;
        }
    };
    check_word_split(tightest_split.end_split);
    auto add_point = [&](size_t split_point) {
        const auto trial_split = split_point;
        const auto trial_line = build_line_markup(start_split, trial_split);
        const auto trial_width = shaper.text_width(trial_line);
        potentials.emplace_back(
            LineStats{trial_split,
                      trial_width,
                      std::holds_alternative<WithinWordSplit>(
                          split_points[trial_split])}); // FIXME, check if word ends with a dash
                                                        // character.
    };

    if(tightest_split.end_split > start_split + 2) {
        check_word_split(tightest_split.end_split - 1);
        add_point(tightest_split.end_split - 1);
    }
    if(tightest_split.end_split + 2 < split_points.size()) {
        // Do not look forward for word splits.
        // This is based on nothing but a hunch.
        add_point(tightest_split.end_split + 1);
    }

    if(tightest_split.end_split > start_split + 3) {
        check_word_split(tightest_split.end_split - 2);
        add_point(tightest_split.end_split - 2);
    }

    if(!word_split_seen) {
        // For languages with long words we need to go backwards
        // until we find a break between words and add that. Do not
        // add entries in between to keep computational complexity
        // down.
        //
        // This is to prevent lines that look like this:
        //
        // Some text and so on and at the very end we have a verylongsingle-
        // word.
        if(tightest_split.end_split > 3) {
            size_t i = tightest_split.end_split - 3;
            while(i > start_split) {
                if(std::holds_alternative<BetweenWordSplit>(split_points[i])) {
                    add_point(i);
                    break;
                }
                --i;
            }
        }
    }
    return potentials;
}

bool SplitStates::abandon_search(const std::vector<LineStats> &new_splits,
                                 const double new_penalty) {
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

HBLine ParagraphFormatter::build_line_words_runs(size_t from_split_ind, size_t to_split_ind) const {
    HBLine line;
    if(to_split_ind == from_split_ind) {
        return line;
    }

    const WordsOnLine line_words = words_for_splits(from_split_ind, to_split_ind);
    const auto &from_loc = split_locations[from_split_ind];

    StyleStack current_style = determine_style(from_loc);

    if(line_words.first) {
        auto new_word = wordfragment2runs(params.font,
                                          current_style,
                                          words[line_words.first->word],
                                          line_words.first->from_bytes,
                                          std::string::npos,
                                          true,
                                          false);
        line.words.push_back(std::move(new_word));
    }
    for(size_t i = line_words.full_word_begin; i < line_words.full_word_end; ++i) {
        const bool add_space = i + 1 != line_words.full_word_end || line_words.last;
        auto new_word = wordfragment2runs(
            params.font, current_style, words[i], 0, std::string::npos, add_space, false);
        line.words.push_back(std::move(new_word));
    }
    if(line_words.last) {
        auto new_word = wordfragment2runs(params.font,
                                          current_style,
                                          words[line_words.last->word],
                                          0,
                                          line_words.last->to_bytes,
                                          false,
                                          line_words.last->add_dash);
        line.words.push_back(std::move(new_word));
    }
    return line;
}
