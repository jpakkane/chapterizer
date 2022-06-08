#include "chapterbuilder.hpp"
#include <textstats.hpp>
#include <algorithm>
#include <cassert>

namespace {

double line_penalty(const LineStats &line, double target_width) {
    const auto delta = abs(line.text_width - target_width);
    return delta * delta;
}

double total_penalty(const std::vector<LineStats> &lines, double target_width) {
    double total = 0;
    double last_line_penalty = 0;
    for(const auto &l : lines) {
        last_line_penalty = line_penalty(l, target_width);
        total += last_line_penalty;
    }
    return total - last_line_penalty;
}

} // namespace

ChapterBuilder::ChapterBuilder(const std::vector<HyphenatedWord> &words_, double paragraph_width_mm)
    : words{words_}, target_width{paragraph_width_mm} {}

std::vector<std::string> ChapterBuilder::split_lines() {
    precompute();
    TextStats shaper;
    best_penalty = 1e100;
    best_split.clear();
    if(false) {
        best_split = simple_split(shaper);
        best_penalty = total_penalty(best_split, target_width);
        return stats_to_lines(best_split);
    } else {
        return global_split(shaper);
    }
}

std::vector<LineStats> ChapterBuilder::simple_split(TextStats &shaper) {
    std::vector<LineStats> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    while(current_split < split_points.size() - 1) {
        auto line_end = get_line_end(current_split, shaper);
        lines.emplace_back(line_end);
        current_split = line_end.end_split;
    }
    const auto total = total_penalty(lines, target_width);
    printf("Total penalty: %.1f\n", total);
    return lines;
}

std::vector<std::string>
ChapterBuilder::stats_to_lines(const std::vector<LineStats> &linestats) const {
    std::vector<std::string> lines;
    lines.reserve(linestats.size());
    lines.emplace_back(build_line(0, linestats[0].end_split));
    for(size_t i = 1; i < linestats.size(); ++i) {
        lines.emplace_back(build_line(linestats[i - 1].end_split, linestats[i].end_split));
    }
    return lines;
}

std::vector<std::string> ChapterBuilder::global_split(const TextStats &shaper) {
    std::vector<std::string> lines;
    std::vector<TextLocation> splits;
    size_t current_split = 0;
    std::vector<LineStats> line_stats;

    global_split_recursive(shaper, line_stats, current_split);
    printf("Total penalty: %.2f\n", best_penalty);
    return stats_to_lines(best_split);
}

void ChapterBuilder::global_split_recursive(const TextStats &shaper,
                                            std::vector<LineStats> &line_stats,
                                            size_t current_split) {
    if(state_cache.abandon_search(line_stats, target_width)) {
        return;
    }
    auto line_end_choices = get_line_end_choices(current_split, shaper);
    if(line_end_choices.front().end_split == split_points.size() - 1) {
        // Text exhausted.
        line_stats.emplace_back(line_end_choices.front());
        const auto current_penalty = total_penalty(line_stats, target_width);
        // printf("Total penalty: %.1f\n", current_penalty);
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
    // return lines;
}

void ChapterBuilder::precompute() {
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

std::string ChapterBuilder::build_line(size_t from_split_ind, size_t to_split_ind) const {
    assert(to_split_ind >= from_split_ind);
    std::string line;
    if(to_split_ind == from_split_ind) {
        return line;
    }
    const auto &from_split = split_points[from_split_ind];
    const auto &to_split = split_points[to_split_ind];
    const auto from_loc = split_locations[from_split_ind];
    const auto to_loc = split_locations[to_split_ind];

    // A single word spans the entire line.
    const bool pathological_single_word = from_loc.word_index == to_loc.word_index;
    if(pathological_single_word) {
        const auto &bad_word = words[from_loc.word_index].word;
        line = bad_word.substr(from_loc.offset, to_loc.offset - from_loc.offset + 1);
        if(std::holds_alternative<WithinWordSplit>(to_split)) {
            const auto &source_loc = std::get<WithinWordSplit>(to_split);
            if(words[source_loc.word_index].hyphen_points[source_loc.hyphen_index].type ==
               SplitType::Regular) {
                line += '-';
            }
        }
        return line;
    }

    // First word.
    if(std::holds_alternative<BetweenWordSplit>(from_split)) {
        line = words[from_loc.word_index].word;
    } else {
        const auto &word_to_split = words[from_loc.word_index];
        const auto sv = std::string_view(word_to_split.word);
        line = sv.substr(from_loc.offset + 1);
    }

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

    // Final word.
    if(std::holds_alternative<BetweenWordSplit>(to_split)) {
        // Do nothing, I guess?
    } else {
        const auto &source_loc = std::get<WithinWordSplit>(to_split);
        line += ' ';
        const auto &word_to_split = words[to_loc.word_index];
        const auto sv = std::string_view(word_to_split.word);
        line += sv.substr(0, to_loc.offset + 1);
        if(words[source_loc.word_index].hyphen_points[source_loc.hyphen_index].type ==
           SplitType::Regular) {
            line += '-';
        }
    }

    return line;
}

TextLocation ChapterBuilder::point_to_location(const SplitPoint &p) const {
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

LineStats ChapterBuilder::get_line_end(size_t start_split, const TextStats &shaper) const {
    assert(start_split < split_points.size() - 1);
    size_t trial = start_split + 2;
    double previous_width = -100.0;
    double final_width = -1000000.0;
    while(trial < split_points.size()) {
        const auto trial_line = build_line(start_split, trial);
        const auto trial_width = shaper.text_width(trial_line);
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

// Sorted by decreasing fitness.
std::vector<LineStats> ChapterBuilder::get_line_end_choices(size_t start_split,
                                                            const TextStats &shaper) const {
    std::vector<LineStats> potentials;
    potentials.reserve(5);
    auto tightest_split = get_line_end(start_split, shaper);
    potentials.push_back(tightest_split);

    // Lambdas, yo!
    auto add_point = [&](size_t split_point) {
        const auto trial_split = split_point;
        const auto trial_line = build_line(start_split, trial_split);
        const auto trial_width = shaper.text_width(trial_line);
        potentials.emplace_back(LineStats{trial_split, trial_width});
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
    if(tightest_split.end_split + 3 < split_points.size()) {
        add_point(tightest_split.end_split + 2);
    }

    return potentials;
}

bool SplitStates::abandon_search(const std::vector<LineStats> &new_splits,
                                 const double target_width) {
    const double new_penalty = total_penalty(new_splits, target_width);
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