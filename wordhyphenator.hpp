#pragma once

#include <hyphen.h>

#include <string>
#include <vector>

struct HyphenatedWord {
    std::vector<size_t> hyphens;
    std::string word;
};

class WordHyphenator {
public:
    WordHyphenator();
    ~WordHyphenator();

    HyphenatedWord hyphenate(const std::string &word) const;

private:
    HyphenDict *dict;
};
