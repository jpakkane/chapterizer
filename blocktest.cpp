#include <cstdio>
#include <cassert>
#include <vector>
#include <string>

const int space_width = 1;

double best_penalty = 1e9;
std::vector<int> best_splits;

int64_t total_width;

int64_t width(const std::string &s) { return s.length(); }

int64_t width_between(const std::vector<std::string> &words, size_t f, size_t t) {
    if(f == t) {
        return 0;
    }
    assert(f < t);
    int64_t total_w = 0;
    for(size_t i = f; i < t; ++i) {
        total_w += width(words[i]);
    }
    total_w += std::max(int64_t(t) - int64_t(f) - 1, int64_t(0));
    return total_w;
}

double penalty_for_splits(const std::vector<std::string> &words, const std::vector<int> &splits) {
    const double average_width = total_width / (splits.size() + 1.0);
    double total_penalty = 0.0;
    assert(!splits.empty());
    const double first_error = width_between(words, 0, splits[0]) - average_width;
    total_penalty += first_error * first_error;
    for(size_t i = 1; i < splits.size() - 1; ++i) {
        const double line_error = width_between(words, splits[i], splits[i + 1]) - average_width;
        total_penalty += line_error * line_error;
    }
    const double last_error = width_between(words, splits.back(), words.size()) - average_width;
    total_penalty += last_error * last_error;
    return total_penalty;
}

void determine_best_recursive(const std::vector<std::string> &words,
                              const int64_t target_width,
                              const int64_t line_estimate,
                              const int word_index,
                              std::vector<int> &splits) {
    if((int64_t)splits.size() > line_estimate + 1) {
        return;
    }

    int i = word_index;
    int64_t running_width = width(words[i]);
    int overflow_steps = 0;
    while(i < (int)words.size() && overflow_steps < 2) {
        ++i;
        if(i < (int)words.size()) {
            splits.push_back(i);
            determine_best_recursive(words, target_width, line_estimate, i + 1, splits);
            splits.pop_back();
            running_width += space_width + width(words[i]);
        } else {
            // End reached.
            const double total_penalty = penalty_for_splits(words, splits);
            if(total_penalty < best_penalty) {
                best_penalty = total_penalty;
                best_splits = splits;
            }
        }
        if(running_width > target_width) {
            ++overflow_steps;
        }
    }
}

std::vector<std::string> splits_to_lines(const std::vector<std::string> &words,
                                         const std::vector<int> &splits) {
    std::vector<std::string> lines;
    lines.push_back("");
    size_t split_ind = 0;
    for(size_t i = 0; i < words.size(); ++i) {
        if(split_ind < splits.size() && i >= (size_t)splits[split_ind]) {
            lines.push_back("");
            ++split_ind;
        }
        lines.back() += words[i];
        lines.back() += ' ';
    }
    return lines;
}
std::vector<std::string> determine_best(const std::vector<std::string> &words,
                                        const int64_t target_width,
                                        const int64_t line_estimate) {
    std::vector<int> splits;
    determine_best_recursive(words, target_width, line_estimate, 0, splits);
    return splits_to_lines(words, best_splits);
}

std::vector<std::string> spread(const std::vector<std::string> &words, const int target_width) {
    std::vector<std::string> lines;
    total_width = space_width * (words.size() - 1);
    for(const auto &w : words) {
        total_width += width(w);
    }
    const auto line_estimate = total_width / target_width + 1;
    // FIXME, try line_estimate \pm 1 and pick the "best".
    lines = determine_best(words, target_width, line_estimate);
    return lines;
}

void do_it(const std::vector<std::string> &words, const int target_width) {
    best_penalty = 1e9;
    best_splits.clear();
    auto spread_lines = spread(words, target_width);
    for(const auto &l : spread_lines) {
        printf("%s\n", l.c_str());
    }
}

void test1() {
    std::vector<std::string> text{
        "Aaaaaaa", "aaaaa",  "aaaaaa",      "aaaaaaaaaaaaa,", "aaaaa",       "aaaaaaaaa",
        "aaaaa",   "aaaaa",  "aaaaaaaaaaa", "aaaaaaaaa",      "aaa",         "aaaaaa,",
        "aaaa",    "aaaaaa", "aaaa",        "aaaaaaaaaaa",    "aaaaaaaaaaa", "aaaaaaa.",
        "Aa",      "aaaaa",  "aaaaaaaaaaa", "aaaaa",          "aaaaaaaaa",   "aaaaa.",
    };
    const int target_width = 60;
    do_it(text, target_width);
}

void test2() {
    std::vector<std::string> text{"Pitkadana", "jotain", "pidempaeae", "kaikkeinpisin"};
    const int target_width = 20;
    do_it(text, target_width);
}

int main(int, char **) {
    test1();
    test2();
    return 0;
}
