#include "splitter.hpp"
#include <textshaper.hpp>

#include <cassert>

Splitter::Splitter(const std::vector<HyphenatedWord> &words_, double paragraph_width_mm)
    : words{words_}, target_width{paragraph_width_mm} {}

std::vector<std::string> Splitter::split_lines() {
    precompute();
    TextShaper shaper;
    std::vector<std::string> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    while(current_split < split_points.size() - 1) {
        auto line_end = get_line_end(current_split, shaper);
        lines.emplace_back(build_line(current_split, line_end));
        current_split = line_end;
    }
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

size_t Splitter::get_line_end(size_t start_split, TextShaper &shaper) const {
    assert(start_split < split_points.size() - 1);
    size_t trial = start_split + 2;
    while(trial < split_points.size()) {
        const auto trial_line = build_line(start_split, trial);
        const auto trial_length = shaper.text_width(trial_line.c_str());
        if(trial_length >= target_width) {
            --trial;
            break;
        }
        ++trial;
    }
    return std::min(trial, split_points.size() - 1);
}
