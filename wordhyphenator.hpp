#pragma once

#include <hyphen.h>

#include <string>
#include <vector>

class WordHyphenator {
public:
    WordHyphenator();
    ~WordHyphenator();

    std::string hyphenate(const std::string &word) const;

private:
    HyphenDict *dict;
};
