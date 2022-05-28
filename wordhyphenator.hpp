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
};

class WordHyphenator {
public:
    WordHyphenator();
    ~WordHyphenator();

    HyphenatedWord hyphenate(const std::string &word) const;

private:
    HyphenatedWord build_hyphenation_data(const std::string &word,
                                          const std::vector<char> &hyphens,
                                          size_t prefix_length) const;
    HyphenDict *dict;
};
