/*
 * Copyright 2022 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

HyphenatedWord WordHyphenator::build_hyphenation_data(const std::string &word,
                                                      const std::vector<char> &hyphens,
                                                      size_t prefix_length) const {
    HyphenatedWord result;
    result.word = word;
    size_t previous_point = 0;
    for(size_t i = 0; i < word.size(); ++i) {
        if(hyphens[i] == char(-1)) {
            // The hyphenator library's output is a bit weird.
            // When we reach an item it has not touched (i.e. is still 255)
            // exit out.
            break;
        }
        if(hyphens[i] & 1 || i == word.size() - 1) {
            auto dash_point = word.find('-', previous_point);
            while(dash_point != std::string::npos) {
                result.hyphen_points.emplace_back(
                    HyphenPoint{dash_point + prefix_length, SplitType::NoHyphen});
                dash_point = word.find('-', dash_point + 1);
            }
            previous_point = dash_point;
        }
        if(hyphens[i] & 1) {
            result.hyphen_points.emplace_back(HyphenPoint{i + prefix_length, SplitType::Regular});
        }
    }
    return result;
}

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
    if(trips.core.empty()) {
        // Non-word such as a number or other weird character combinations.
        // FIXME to add dashelss hyphenation points for things like
        // 1,000,000.
        return HyphenatedWord{{}, word};
    }
    const auto lw = lowerword(trips.core);
    // printf("X %s\n", lw.c_str());
    const auto rc = hnj_hyphen_hyphenate2(
        dict, lw.c_str(), (int)lw.size(), hyphens.data(), output.data(), &rep, &pos, &cut);
    assert(rc == 0);

    auto result = build_hyphenation_data(word, hyphens, trips.prefix.length());

    result.sanity_check();

    free(rep);
    free(pos);
    free(cut);
    return result;
}

std::vector<HyphenatedWord> WordHyphenator::hyphenate(const std::vector<std::string> &words) const {
    std::vector<HyphenatedWord> hyphs;
    hyphs.reserve(words.size());
    for(const auto &w : words) {
        hyphs.emplace_back(hyphenate(w));
    }
    return hyphs;
}

std::string HyphenatedWord::get_visual_string() const {
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

void HyphenatedWord::sanity_check() const {
    for(const auto &h : hyphen_points) {
        assert(h.loc < word.length());
    }
}
