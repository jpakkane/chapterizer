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

#include <draftparagraphformatter.hpp>
#include <hbfontcache.hpp>
#include <textstats.hpp>
#include <algorithm>
#include <optional>
#include <cassert>
#include <cmath>

namespace {

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
            line.append(current_style.inline_code_end_tag());
            break;
        case SMALLCAPS_S:
            line.append("</span>");
            break;
        case SUPERSCRIPT_S:
            line.append("</sup>");
            break;
        case SUBSCRIPT_S:
            line.append("</sub>");
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
            line.append(current_style.inline_code_start_tag());
            break;
        case SMALLCAPS_S:
            line.append("<span variant=\"small-caps\" letter_spacing=\"100\">");
            break;
        case SUPERSCRIPT_S:
            line.append("<sup>");
            break;
        case SUBSCRIPT_S:
            line.append("<sub>");
            break;
        default:
            printf("Bad style start bit.\n");
            std::abort();
        }
    }
}

std::string wordfragment2markup(StyleStack &current_style,
                                const EnrichedWord &w,
                                size_t start,
                                size_t end,
                                bool add_space,
                                bool add_dash) {
    std::string markup;
    current_style.write_buildup_markup(markup);

    std::string_view view = std::string_view{w.text}.substr(start, end);
    assert(g_utf8_validate(view.data(), view.length(), nullptr));
    size_t style_point = 0;
    while(style_point < w.f.size() && w.f[style_point].offset < start) {
        ++style_point;
    }
    for(size_t i = 0; i < view.size(); ++i) {
        while(style_point < w.f.size() && w.f[style_point].offset == start + i) {
            toggle_format(current_style, markup, w.f[style_point].format);
            ++style_point;
        }
        if(view[i] == '<') {
            markup += "&lt;";
        } else if(view[i] == '>') {
            markup += "&gt;";
        } else if(view[i] == '&') {
            markup += "&amp;";
        } else {
            markup += view[i];
        }
    }

    if(add_dash) {
        markup += '-';
    }
    while(style_point < w.f.size()) {
        toggle_format(current_style, markup, w.f[style_point].format);
        ++style_point;
    }
    if(add_space) {
        markup += ' '; // The space must be inside the markup to get the correct width.
    }
    current_style.write_teardown_markup(markup);
    assert(g_utf8_validate(markup.c_str(), -1, nullptr));
    return markup;
}

} // namespace

DraftParagraphFormatter::DraftParagraphFormatter(const std::vector<EnrichedWord> &words_,
                                       const Length target_width,
                                       const ChapterParameters &in_params,
                                       HBFontCache &hbfc)
    : paragraph_width(target_width), words{words_}, params{in_params}, fc(hbfc) {}

std::vector<std::vector<std::string>> DraftParagraphFormatter::split_formatted_lines() {
    TextStats shaper;
    return stats_to_markup_lines(simple_split(shaper));
}

std::vector<LineStats> DraftParagraphFormatter::simple_split(TextStats &shaper) {
    std::vector<LineStats> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    while(current_split < split_points.size() - 1) {
        auto line_end = get_closest_line_end(current_split, shaper, lines.size());
        lines.emplace_back(line_end);
        current_split = line_end.end_split;
    }
    return lines;
}

std::vector<std::vector<std::string>>
DraftParagraphFormatter::stats_to_markup_lines(const std::vector<LineStats> &linestats) const {
    std::vector<std::vector<std::string>> lines;
    lines.reserve(linestats.size());
    lines.emplace_back(build_line_words_markup(0, linestats[0].end_split));
    for(size_t i = 1; i < linestats.size(); ++i) {
        lines.emplace_back(
            build_line_words_markup(linestats[i - 1].end_split, linestats[i].end_split));
    }
    return lines;
}

Length DraftParagraphFormatter::current_line_width(size_t line_num) const {
    if(line_num == 0) {
        return paragraph_width - params.indent;
    }
    return paragraph_width;
}

WordsOnLine DraftParagraphFormatter::words_for_splits(size_t from_split_ind, size_t to_split_ind) const {
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

std::string DraftParagraphFormatter::build_line_text_debug(size_t from_split_ind,
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

std::string DraftParagraphFormatter::build_line_markup(size_t from_split_ind,
                                                  size_t to_split_ind) const {
    std::string line;
    const auto markup_words = build_line_words_markup(from_split_ind, to_split_ind);
    for(const auto &w : markup_words) {
        line += w;
    }
    return line;
}

std::vector<std::string> DraftParagraphFormatter::build_line_words_markup(size_t from_split_ind,
                                                                     size_t to_split_ind) const {
    assert(to_split_ind >= from_split_ind);
    std::vector<std::string> line;
    std::vector<std::string> markup_words;
    if(to_split_ind == from_split_ind) {
        return line;
    }
    const WordsOnLine line_words = words_for_splits(from_split_ind, to_split_ind);
    const auto &from_loc = split_locations[from_split_ind];

    StyleStack current_style = determine_style(from_loc);

    if(line_words.first) {
        markup_words.emplace_back(wordfragment2markup(current_style,
                                                      words[line_words.first->word],
                                                      line_words.first->from_bytes,
                                                      std::string::npos,
                                                      true,
                                                      false));
    }
    for(size_t i = line_words.full_word_begin; i < line_words.full_word_end; ++i) {
        const bool add_space = i + 1 != line_words.full_word_end || line_words.last;
        markup_words.emplace_back(
            wordfragment2markup(current_style, words[i], 0, std::string::npos, add_space, false));
    }
    if(line_words.last) {
        markup_words.emplace_back(wordfragment2markup(current_style,
                                                      words[line_words.last->word],
                                                      0,
                                                      line_words.last->to_bytes,
                                                      false,
                                                      line_words.last->add_dash));
    }

    return markup_words;
}

StyleStack DraftParagraphFormatter::determine_style(TextLocation t) const {
    const auto &current_word = words[t.word_index];
    StyleStack style = words[t.word_index].start_style;
    size_t i = 0;
    size_t style_point = 0;
    std::string dummy;
    while(i < t.offset) {
        while(style_point < current_word.f.size() && current_word.f[style_point].offset == i) {
            toggle_format(style, dummy, current_word.f[style_point].format);
            ++style_point;
        }
        ++i;
    }
    return style;
}

TextLocation DraftParagraphFormatter::point_to_location(const SplitPoint &p) const {
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

LineStats DraftParagraphFormatter::get_closest_line_end(size_t start_split,
                                                   const TextStats &shaper,
                                                   size_t line_num) const {
    auto val = compute_closest_line_end(start_split, shaper, line_num);
    return val;
}

LineStats DraftParagraphFormatter::compute_closest_line_end(size_t start_split,
                                                       const TextStats &shaper,
                                                       size_t line_num) const {
    assert(start_split < split_points.size() - 1);
    const Length target_line_width_mm = current_line_width(line_num);
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
