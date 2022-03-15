#include "wordhyphenator.hpp"
#include <cassert>
#include <cctype>

namespace {

std::string lowerword(const std::string &w) {
    std::string lw;
    lw.reserve(w.size());
    for(const auto c : w) {
        lw.push_back(tolower(c));
    }
    return lw;
}

} // namespace

WordHyphenator::WordHyphenator() {
    dict = hnj_hyphen_load("/usr/share/hyphen/hyph_en.dic");
    assert(dict);
}

WordHyphenator::~WordHyphenator() { hnj_hyphen_free(dict); }

HyphenatedWord WordHyphenator::hyphenate(const std::string &word) const {
    assert(word.find(' ') == std::string::npos);
    std::vector<char> output(word.size() * 2 + 1, '\0');
    char **rep = nullptr;
    int *pos = nullptr;
    int *cut = nullptr;
    std::vector<char> hyphens(word.size() + 5, (char)-1);
    // The hyphenation function only deals with lower case single words.
    // Trailing punctuation, capital letters and hyphens within words break it.
    const auto lw = lowerword(word);
    const auto rc = hnj_hyphen_hyphenate2(
        dict, lw.c_str(), (int)lw.size(), hyphens.data(), output.data(), &rep, &pos, &cut);
    assert(rc == 0);

    HyphenatedWord result;
    result.word = word;
    for(size_t i = 0; i < word.size(); ++i) {
        if(hyphens[i] & 1) {
            result.hyphens.push_back(i);
        }
    }

    free(rep);
    free(pos);
    free(cut);
    return result;
}
