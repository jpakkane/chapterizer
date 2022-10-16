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
#include <array>
#include <memory>
#include <optional>

#include <glib.h>
#include <cassert>
#include <cctype>

struct WordPieces {
    std::string prefix;
    std::string core;
    std::string suffix;
};

namespace {

const std::array<uint32_t, 4> dash_codepoints{0x2d, 0x2012, 0x2014, 0x2212};

struct DashSplit {
    std::vector<std::string> words;
    std::vector<uint32_t> separators;
};

bool is_dashlike(uint32_t uchar) {
    for(const auto c : dash_codepoints) {
        if(c == uchar) {
            return true;
        }
    }
    return false;
}

DashSplit split_at_dashes(const std::string &word) {
    DashSplit splits;
    std::unique_ptr<char[]> bufd{new char[word.size() + 10]};
    char *buf = bufd.get();
    int chars_in_buf = 0;
    const char *in = word.c_str();
    while(*in) {
        auto c = g_utf8_get_char(in);
        if(is_dashlike(c)) {
            buf[chars_in_buf] = '\0';
            splits.words.emplace_back(buf);
            chars_in_buf = 0;
            splits.separators.push_back(c);
        } else {
            chars_in_buf += g_unichar_to_utf8(c, buf + chars_in_buf);
        }
        in = g_utf8_next_char(in);
    }
    assert(splits.words.size() == splits.separators.size());
    buf[chars_in_buf] = '\0';
    splits.words.emplace_back(buf);
    return splits;
}

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

    return WordPieces{word.substr(0, p1), word.substr(p1, p2 - p1 + 1), word.substr(p2 + 1)};
};

std::vector<HyphenPoint> build_hyphenation_data(const std::string &word,
                                                const std::vector<char> &hyphens,
                                                size_t prefix_length) {
    std::vector<HyphenPoint> hyphen_points;
    for(size_t i = 0; i < word.size(); ++i) {
        if(hyphens[i] == char(-1)) {
            // The hyphenator library's output is a bit weird.
            // When we reach an item it has not touched (i.e. is still 255)
            // exit out.
            break;
        }
        if(hyphens[i] & 1) {
            hyphen_points.emplace_back(HyphenPoint{i + prefix_length, SplitType::Regular});
        }
    }
    return hyphen_points;
}

void hyphenate_and_append(std::string &reconstructed_word,
                          std::vector<HyphenPoint> &hyphen_points,
                          const std::string &word,
                          std::optional<uint32_t> separator,
                          HyphenDict *dict) {
    const auto trips = tripartite(word);
    char buf[7] = {0, 0, 0, 0, 0, 0, 0};
    if(trips.core.empty()) {
        reconstructed_word += word;
        if(separator) {
            g_unichar_to_utf8(*separator, buf);
            reconstructed_word += buf;
        }
        return;
    }
    reconstructed_word += trips.prefix;
    std::string lowercase_word = lowerword(trips.core);
    std::vector<char> output(word.size() * 2 + 1, '\0');
    std::vector<char> hyphens(word.size() + 5, (char)-1);
    char **rep = nullptr;
    int *pos = nullptr;
    int *cut = nullptr;
    // NOTE: it seems that this function only works with ASCII
    // characters. At least feeding it some UTF-8 text produced
    // weird results. Consider checking for this if weird
    // errors pop up.
    const auto rc = hnj_hyphen_hyphenate2(dict,
                                          lowercase_word.c_str(),
                                          (int)lowercase_word.size(),
                                          hyphens.data(),
                                          output.data(),
                                          &rep,
                                          &pos,
                                          &cut);
    assert(rc == 0);
    auto subhyphens = build_hyphenation_data(word, hyphens, reconstructed_word.length());
    free(rep);
    free(pos);
    free(cut);
    hyphen_points.insert(hyphen_points.cend(), subhyphens.begin(), subhyphens.end());
    reconstructed_word += trips.core;
    reconstructed_word += trips.suffix;
    if(separator) {
        g_unichar_to_utf8(*separator, buf);
        reconstructed_word += buf;
        hyphen_points.emplace_back(HyphenPoint{reconstructed_word.size() - 1, SplitType::NoHyphen});
    }
}

char *discard_one_letter_syllables(char *hyphen_str) {
    const auto word_len = strlen(hyphen_str);
    if(word_len < 3) {
        return hyphen_str;
    }
    if(hyphen_str[1] == '-') {
        hyphen_str[1] = ' ';
    }
    if(hyphen_str[word_len - 1] == '-') {
        hyphen_str[word_len - 1] = ' ';
    }
    for(size_t i = 1; i < word_len - 3; ++i) {
        if(hyphen_str[i] == '=') {
            if(hyphen_str[i - 1] == '-') {
                hyphen_str[i - 1] = ' ';
            }
            if(hyphen_str[i + 2] == '-') {
                hyphen_str[i + 2] = ' ';
            }
        }
    }
    return hyphen_str;
}

} // namespace

WordHyphenator::WordHyphenator() {
    dict = hnj_hyphen_load("/usr/share/hyphen/hyph_en.dic");
    if(!dict) {
        dict = hnj_hyphen_load("/usr/share/hyphen/hyph_en_US.dic");
    }
    if(!dict) {
        printf("Could not load english hyphenation data.\n");
        std::abort();
    }
    const char *error;
    voikko = voikkoInit(&error, "fi", nullptr);
    if(!voikko) {
        printf("Voikko init failed: %s\n", error);
        std::abort();
    }
}

WordHyphenator::~WordHyphenator() {
    hnj_hyphen_free(dict);
    voikkoTerminate(voikko);
}

std::vector<HyphenPoint> WordHyphenator::hyphenate(const std::string &word,
                                                   const Language lang) const {
    assert(word.find(' ') == std::string::npos);
    g_utf8_validate(word.c_str(), word.length(), nullptr);
    std::vector<HyphenPoint> hyphen_points;
    if(lang == Language::Unset) {
        // FIXME, split at dashes.
    } else if(lang == Language::English) {
        std::string reconstructed_word;
        reconstructed_word.reserve(word.size());
        // The hyphenation function only deals with lower case single words.
        // Attached punctuation, quotes, capital letters etc break it.
        // For words like spatio-temporal it splits the individual words but not the hyphen.
        std::string_view letterview(letters);
        const auto p1 = word.find_first_of(letterview);
        if(p1 == std::string::npos) {
            // Non-word such as a number or other weird character combinations.
            // FIXME to add dashelss hyphenation points for things like
            // 1,000,000.
            return {};
        }
        const auto subwords = split_at_dashes(word);
        assert(subwords.words.size() == subwords.separators.size() + 1);
        for(size_t ind = 0; ind < subwords.words.size(); ++ind) {
            hyphenate_and_append(reconstructed_word,
                                 hyphen_points,
                                 subwords.words[ind],
                                 ind < subwords.separators.size()
                                     ? std::optional<uint32_t>{subwords.separators[ind]}
                                     : std::optional<uint32_t>{},
                                 dict);
        }
        assert(reconstructed_word == word);
    } else if(lang == Language::Finnish) {
        // The return value is an ASCII string. It lists the hyphenation points
        // in characters, but we need them in bytes.
        char *hyphenation = discard_one_letter_syllables(voikkoHyphenateCstr(voikko, word.c_str()));

        const char *character_location = word.c_str();
        for(size_t i = 0; hyphenation[i]; ++i) {
            auto byte_offset = size_t(character_location - word.c_str());
            switch(hyphenation[i]) {
            case '-':
                // The weird offset comes from the fact that we need to mimic how the
                // english language hyphenator has chosen to set up its offsets.
                hyphen_points.emplace_back(HyphenPoint{byte_offset - 1, SplitType::Regular});
                break;
            case '=':
                hyphen_points.emplace_back(HyphenPoint{byte_offset, SplitType::NoHyphen});
                break;
            case ' ':
                break;
            default:
                printf("Unexpected output from hyphenator: %c\n", hyphenation[i]);
                std::abort();
            }
            character_location = g_utf8_find_next_char(character_location, nullptr);
        }
        assert(character_location == word.c_str() + word.length());
        voikkoFreeCstr(hyphenation);
    } else {
        printf("Unkown hyphenation language.\n");
        std::abort();
    }
    HyphenatedWord tmp;
    tmp.word = word;
    tmp.hyphen_points = hyphen_points;
    tmp.sanity_check();
    return hyphen_points;
}

std::vector<std::vector<HyphenPoint>>
WordHyphenator::hyphenate(const std::vector<std::string> &words, const Language lang) const {
    std::vector<std::vector<HyphenPoint>> hyphs;
    hyphs.reserve(words.size());
    for(const auto &w : words) {
        hyphs.emplace_back(hyphenate(w, lang));
    }
    return hyphs;
}

std::string get_visual_string(const std::string &word,
                              const std::vector<HyphenPoint> hyphen_points) {
    std::string dashed_word;
    dashed_word.reserve(word.size() + hyphen_points.size());
    size_t hyphen_index = 0;
    for(size_t i = 0; i < word.size(); ++i) {
        dashed_word += word[i];
        if(hyphen_index < hyphen_points.size() && i == hyphen_points[hyphen_index].loc) {
            dashed_word += "⬧";
            ++hyphen_index;
        }
    }
    return dashed_word;
}

std::string HyphenatedWord::get_visual_string() const {
    std::string dashed_word;
    dashed_word.reserve(word.size() + hyphen_points.size());
    size_t hyphen_index = 0;
    for(size_t i = 0; i < word.size(); ++i) {
        dashed_word += word[i];
        if(hyphen_index < hyphen_points.size() && i == hyphen_points[hyphen_index].loc) {
            dashed_word += "⬧";
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
