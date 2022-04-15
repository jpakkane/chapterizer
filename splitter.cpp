#include "splitter.h"

Splitter::Splitter(const std::vector<HyphenatedWord> &words, double paragraph_width_mm)
    : words{words}, target{paragraph_width_mm} {}

std::vector<std::string> Splitter::split_lines() {
    std::vector<std::string> lines;
    lines.push_back("Text.");
    return lines;
}
