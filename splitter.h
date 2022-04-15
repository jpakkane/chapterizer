#pragma once

#include <wordhyphenator.hpp>

class Splitter {
public:
    Splitter(const std::vector<HyphenatedWord> &words, double paragraph_width_mm);

    std::vector<std::string> split_lines();

private:
    std::vector<HyphenatedWord> words;
    double target; // in mm
};
