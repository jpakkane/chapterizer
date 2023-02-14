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

#include <wordhyphenator.hpp>
#include <glib.h>

#define CHECK(cond)                                                                                \
    if(!(cond)) {                                                                                  \
        printf("Fail %s:%d\n", __PRETTY_FUNCTION__, __LINE__);                                     \
        std::abort();                                                                              \
    }

void test_hyphenation_simple() {
    WordHyphenator h;
    auto w = h.hyphenate("morning", Language::English);
    HyphenPoint expected{3, SplitType::Regular};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_hyphenation_dash() {
    WordHyphenator h;
    auto w = h.hyphenate("hi-ho", Language::English);
    HyphenPoint expected{2, SplitType::NoHyphen};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_hyphenation_emdash() {
    WordHyphenator h;
    auto w = h.hyphenate("us—more", Language::English); // Unicode
    HyphenPoint expected{4, SplitType::NoHyphen};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_hyphenation_prefix() {
    WordHyphenator h;
    auto w = h.hyphenate("“morning", Language::English);
    HyphenPoint expected{6,
                         SplitType::Regular}; // First character is Unicode and takes three bytes.
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_hyphenation_underscore() {
    WordHyphenator h;
    auto w = h.hyphenate("_Nature_", Language::English);
    HyphenPoint expected{2, SplitType::Regular};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_utf8_splitting() {
    WordHyphenator h;
    const std::string text{"emerge—possibly"};
    auto w = h.hyphenate(text, Language::English); // Note: has an em-dash.
    for(size_t i = 0; i < w.size(); ++i) {
        auto sub = text.substr(0, w[i].loc + 1);
        CHECK(g_utf8_validate(sub.c_str(), -1, nullptr));
    }
    for(size_t i = 0; i < w.size(); ++i) {
        auto sub = text.substr(w[i].loc + 1);
        CHECK(g_utf8_validate(sub.c_str(), -1, nullptr));
    }
}

void test_strange_combo() {
    WordHyphenator h;
    const std::string text{"impact—“splashed”"}; // impact—“splashed”"};
    auto w = h.hyphenate(text, Language::English);
    HyphenPoint expected{1, SplitType::Regular};
    HyphenPoint expected2{8, SplitType::NoHyphen};
    CHECK(w.size() == 2);
    CHECK(w.front() == expected);
    CHECK(w.back() == expected2);
}

void test_dualhyphen() {
    // Not sure if correct, this could also be hyphenless.
    WordHyphenator h;
    const std::string text{"maybe——"};
    auto w = h.hyphenate(text, Language::English);
    HyphenPoint expected{7, SplitType::NoHyphen};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_utf8_impl(const char *t, Language lang) {
    WordHyphenator h;
    const std::string text(t);
    const auto w = h.hyphenate(text, lang);
    for(const auto &hp : w) {
        std::string pre = text.substr(0, hp.loc + 1);
        CHECK(g_utf8_validate(pre.c_str(), pre.length(), nullptr));
        std::string post = text.substr(hp.loc + 1, std::string::npos);
        CHECK(g_utf8_validate(post.c_str(), post.length(), nullptr));
    }
}

void test_utf8() {
    test_utf8_impl("kansikuvapönöttäjästä", Language::Finnish);
    test_utf8_impl("päämajaksi", Language::Finnish);
    test_utf8_impl("silkkiäis", Language::Finnish);
}

void test_finhyphen() {
    WordHyphenator h;
    const std::string text("juna-UV");
    const auto w = h.hyphenate(text, Language::Finnish);
    HyphenPoint expected{1, SplitType::Regular};
    HyphenPoint expected2{4, SplitType::NoHyphen};
    CHECK(w.size() == 2);
    CHECK(w.front() == expected);
    CHECK(w.back() == expected2);
}

void test_singleletter() {
    WordHyphenator h;
    const std::string text{"oliivi"};
    HyphenPoint expected{3, SplitType::Regular};
    const auto w = h.hyphenate(text, Language::Finnish);
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_singleletter_end() {
    WordHyphenator h;
    const std::string text{"tarttua,"};
    HyphenPoint expected{3, SplitType::Regular};
    const auto w = h.hyphenate(text, Language::Finnish);
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_singleletter_dash() {
    WordHyphenator h;
    const std::string text{"junaolio-oliivi"};
    HyphenPoint expected0{1, SplitType::Regular};
    HyphenPoint expected1{3, SplitType::Regular};
    HyphenPoint expected2{4, SplitType::Regular};
    HyphenPoint expected3{8, SplitType::NoHyphen};
    HyphenPoint expected4{12, SplitType::Regular};
    const auto w = h.hyphenate(text, Language::Finnish);
    CHECK(w.size() == 5);
    CHECK(w[0] == expected0);
    CHECK(w[1] == expected1);
    CHECK(w[2] == expected2);
    CHECK(w[3] == expected3);
    CHECK(w[4] == expected4);
}

void test_hyphenation() {
    test_hyphenation_simple();
    test_hyphenation_dash();
    test_hyphenation_emdash();
    test_hyphenation_prefix();
    test_hyphenation_underscore();
    test_utf8_splitting();
    test_strange_combo();
    test_dualhyphen();
    test_utf8();
    test_finhyphen();
    test_singleletter();
    test_singleletter_end();
    test_singleletter_dash();
}

int main(int, char **) {
    printf("Running hyphenation tests.\n");
    test_hyphenation();
}
