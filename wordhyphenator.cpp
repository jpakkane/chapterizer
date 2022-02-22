#include "wordhyphenator.hpp"
#include <cassert>

WordHyphenator::WordHyphenator() {
    dict = hnj_hyphen_load("/usr/share/hyphen/hyph_en.dic");
    assert(dict);
}

WordHyphenator::~WordHyphenator() { hnj_hyphen_free(dict); }

std::string WordHyphenator::hyphenate(const std::string &word) const {
    assert(word.find(' ') == std::string::npos);
    std::vector<char> output(word.size() * 2 + 1, '\0');
    char **rep = nullptr;
    int *pos = nullptr;
    int *cut = nullptr;
    std::vector<char> hyphens(word.size() + 5, (char)-1);
    // The hyphenation function only deals with lower case single words.
    // Trailing punctuation, capital letters and hyphens within words break it.
    const auto rc = hnj_hyphen_hyphenate2(
        dict, word.c_str(), (int)word.size(), hyphens.data(), output.data(), &rep, &pos, &cut);
    assert(rc == 0);

    std::string result;
    for(size_t i = 0; i < word.size(); ++i) {
        result.push_back(word[i]);
        if(hyphens[i] & 1) {
            result.push_back('-');
        }
    }

    free(rep);
    free(pos);
    free(cut);
    return result;
}
