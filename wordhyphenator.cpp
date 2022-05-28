#include "wordhyphenator.hpp"
#include <cassert>
#include <cctype>

struct WordPieces {
    std::string prefix;
    std::string core;
    std::string suffix;
};

namespace {

std::string lowerword(const std::string &w) {
    std::string lw;
    lw.reserve(w.size());
    for(const auto c : w) {
        lw.push_back(tolower(c));
    }
    return lw;
}

const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

WordPieces tripartite(const std::string &word) {
    std::string_view letterview(letters);
    const auto p1 = word.find_first_of(letterview);
    if(p1 == std::string::npos) {
        return WordPieces{word, "", ""};
    }
    const auto p2 = word.find_last_of(letterview);
    assert(p2 != std::string::npos);

    return WordPieces{word.substr(0, p1), word.substr(p1, p2 + 1), word.substr(p2 + 1)};
};

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
    // Attached punctuation, quotes, capital letters etc break it.
    // For words like spatio-temporal it splits the individual words but not the hyphen.
    const auto trips = tripartite(word);
    const auto lw = lowerword(trips.core);
    // printf("X %s\n", lw.c_str());
    const auto rc = hnj_hyphen_hyphenate2(
        dict, lw.c_str(), (int)lw.size(), hyphens.data(), output.data(), &rep, &pos, &cut);
    assert(rc == 0);

    HyphenatedWord result;
    result.word = word;
    size_t previous_point = 0;
    for(size_t i = 0; i < word.size(); ++i) {
        if(hyphens[i] & 1) {
            auto dash_point = word.find('-', previous_point);
            while(dash_point != std::string::npos && dash_point < i) {
                result.hyphen_points.emplace_back(
                    HyphenPoint{dash_point + trips.prefix.length(), SplitType::NoHyphen});
                dash_point = word.find('-', dash_point + 1);
            }
            result.hyphen_points.emplace_back(
                HyphenPoint{i + trips.prefix.length(), SplitType::Regular});
            previous_point = dash_point;
        }
    }

    free(rep);
    free(pos);
    free(cut);
    return result;
}
