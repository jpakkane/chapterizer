#pragma once

#include <hyphen.h>

#include <string>
#include <vector>

enum class SplitType : int {
    Regular,
    NoHyphen,
};

struct HyphenPoint {
    size_t loc;
    SplitType type;
};

struct HyphenatedWord {
    std::vector<HyphenPoint> hyphen_points;
    std::string word;

    std::string get_visual_string() const {
        std::string dashed_word;
        dashed_word.reserve(word.size() + hyphen_points.size());
        size_t hyphen_index = 0;
        for(size_t i = 0; i < word.size(); ++i) {
            dashed_word += word[i];
            if(hyphen_index < hyphen_points.size() && i == hyphen_points[hyphen_index].loc) {
                if(hyphen_points[hyphen_index].type == SplitType::Regular) {
                    dashed_word += '-';
                }
                ++hyphen_index;
            }
        }
        return dashed_word;
    }
};

class WordHyphenator {
public:
    WordHyphenator();
    ~WordHyphenator();

    HyphenatedWord hyphenate(const std::string &word) const;
    std::vector<HyphenatedWord> hyphenate(const std::vector<std::string> &words) const;

private:
    HyphenatedWord build_hyphenation_data(const std::string &word,
                                          const std::vector<char> &hyphens,
                                          size_t prefix_length) const;
    HyphenDict *dict;
};
