#include "splitter.hpp"
#include <textshaper.hpp>

#include <cassert>

Splitter::Splitter(const std::vector<HyphenatedWord> &words_, double paragraph_width_mm)
    : words{words_}, target_width{paragraph_width_mm} {}

std::vector<std::string> Splitter::split_lines() {
    precompute();
    TextShaper shaper;
    best_penalty = 1e100;
    best_split.clear();
    if(true) {
        best_split = simple_split(shaper);
        best_penalty = total_penalty(best_split);
        return stats_to_lines(best_split);
    } else {
        return global_split(shaper);
    }
}

std::vector<LineStats> Splitter::simple_split(TextShaper &shaper) {
    std::vector<LineStats> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    while(current_split < split_points.size() - 1) {
        auto line_end = get_line_end(current_split, shaper);
        lines.emplace_back(line_end);
        current_split = line_end.end_split;
    }
    const auto total = total_penalty(lines);
    printf("Total penalty: %.1f\n", total);
    return lines;
}

std::vector<std::string> Splitter::stats_to_lines(const std::vector<LineStats> &linestats) const {
    std::vector<std::string> lines;
    lines.reserve(linestats.size());
    lines.emplace_back(build_line(0, linestats[0].end_split));
    for(size_t i = 1; i < linestats.size(); ++i) {
        lines.emplace_back(build_line(linestats[i - 1].end_split, linestats[i].end_split));
    }
    return lines;
}

std::vector<std::string> Splitter::global_split(TextShaper &shaper) {
    std::vector<std::string> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    std::vector<LineStats> line_stats;
    while(current_split < split_points.size() - 1) {
        auto line_end = get_line_end(current_split, shaper);
        auto current_line = build_line(current_split, line_end.end_split);
        line_stats.emplace_back(line_end);
        lines.push_back(current_line);
        current_split = line_end.end_split;
    }
    const auto total = total_penalty(line_stats);
    printf("Total penalty: %.1f\n", total);
    return lines;
}

void Splitter::precompute() {
    split_points.clear();
    split_points.reserve(words.size() * 3);
    for(size_t word_index = 0; word_index < words.size(); ++word_index) {
        split_points.emplace_back(BetweenWordSplit{word_index});
        for(size_t hyphen_index = 0; hyphen_index < words[word_index].hyphens.size();
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
}

std::string Splitter::build_line(size_t from_split_ind, size_t to_split_ind) const {
    assert(to_split_ind >= from_split_ind);
    std::string line;
    if(to_split_ind == from_split_ind) {
        return line;
    }
    const auto &from_split = split_points[from_split_ind];
    const auto &to_split = split_points[to_split_ind];
    const auto start_loc = split_locations[from_split_ind];
    const auto end_loc = split_locations[to_split_ind];

    // First word.
    if(std::holds_alternative<BetweenWordSplit>(from_split)) {
        line = words[start_loc.word_index].word;
    } else {
        const auto &word_to_split = words[start_loc.word_index];
        const auto sv = std::string_view(word_to_split.word);
        line = sv.substr(start_loc.offset + 1);
    }
    size_t current_word_index = start_loc.word_index + 1;

    // Intermediate words.
    while(current_word_index < end_loc.word_index && current_word_index < words.size()) {
        line += ' ';
        line += words[current_word_index].word;
        ++current_word_index;
    }

    // Final word.
    if(std::holds_alternative<BetweenWordSplit>(to_split)) {
        // Do nothing, I guess?
    } else {
        line += ' ';
        const auto &word_to_split = words[end_loc.word_index];
        const auto sv = std::string_view(word_to_split.word);
        line += sv.substr(0, end_loc.offset + 1);
        line += '-';
    }

    return line;
}

TextLocation Splitter::point_to_location(const SplitPoint &p) const {
    if(std::holds_alternative<BetweenWordSplit>(p)) {
        const auto &r = std::get<BetweenWordSplit>(p);
        return TextLocation{r.word_index, 0};
    } else if(std::holds_alternative<WithinWordSplit>(p)) {
        const auto &r = std::get<WithinWordSplit>(p);
        return TextLocation{r.word_index, words[r.word_index].hyphens[r.hyphen_index]};
    } else {
        assert(false);
    }
}

LineStats Splitter::get_line_end(size_t start_split, TextShaper &shaper) const {
    assert(start_split < split_points.size() - 1);
    size_t trial = start_split + 2;
    double previous_width = -100.0;
    double final_width = -1000000.0;
    while(trial < split_points.size()) {
        const auto trial_line = build_line(start_split, trial);
        const auto trial_width = shaper.text_width(trial_line.c_str());
        if(trial_width >= target_width) {
            if(abs(trial_width - target_width) > abs(previous_width - target_width)) {
                --trial;
                final_width = previous_width;
            } else {
                final_width = trial_width;
            }
            break;
        }
        ++trial;
        previous_width = trial_width;
        final_width = previous_width;
    }
    return LineStats{std::min(trial, split_points.size() - 1), final_width};
}

double Splitter::total_penalty(const std::vector<LineStats> &lines) const {
    double total = 0;
    for(const auto &l : lines) {
        total += line_penalty(l);
    }
    return total;
}

double Splitter::line_penalty(const LineStats &line) const {
    const auto delta = abs(line.text_width - target_width);
    return delta * delta;
}
