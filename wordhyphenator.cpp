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
    // Attached punctuation, quotes, capital letters, hyphens within words break it.
    // Among other things.
    const auto trips = tripartite(word);
    const auto lw = lowerword(trips.core);
    printf("X %s\n", lw.c_str());
    const auto rc = hnj_hyphen_hyphenate2(
        dict, lw.c_str(), (int)lw.size(), hyphens.data(), output.data(), &rep, &pos, &cut);
    assert(rc == 0);

    HyphenatedWord result;
    result.word = word;
    for(size_t i = 0; i < word.size(); ++i) {
        if(hyphens[i] & 1) {
            result.hyphens.push_back(i + trips.prefix.length());
        }
    }

    free(rep);
    free(pos);
    free(cut);
    return result;
}
