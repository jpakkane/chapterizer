#include <wordhyphenator.hpp>
#include <glib.h>

#define CHECK(cond)                                                                                \
    if(!(cond)) {                                                                                  \
        printf("Fail %s:%d\n", __PRETTY_FUNCTION__, __LINE__);                                     \
        std::abort();                                                                              \
    }

void test_hyphenation_simple() {
    WordHyphenator h;
    auto w = h.hyphenate("morning");
    HyphenPoint expected{3, SplitType::Regular};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_hyphenation_dash() {
    WordHyphenator h;
    auto w = h.hyphenate("hi-ho");
    HyphenPoint expected{2, SplitType::NoHyphen};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_hyphenation_emdash() {
    WordHyphenator h;
    auto w = h.hyphenate("us—more"); // Unicode
    HyphenPoint expected{4, SplitType::NoHyphen};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_hyphenation_prefix() {
    WordHyphenator h;
    auto w = h.hyphenate("“morning");
    HyphenPoint expected{6,
                         SplitType::Regular}; // First character is Unicode and takes three bytes.
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_hyphenation_underscore() {
    WordHyphenator h;
    auto w = h.hyphenate("_Nature_");
    HyphenPoint expected{2, SplitType::Regular};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
}

void test_utf8_splitting() {
    WordHyphenator h;
    const std::string text{"emerge—possibly"};
    auto w = h.hyphenate(text); // Note: has an em-dash.
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
    auto w = h.hyphenate(text);
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
    auto w = h.hyphenate(text);
    HyphenPoint expected{7, SplitType::NoHyphen};
    CHECK(w.size() == 1);
    CHECK(w.front() == expected);
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
}

int main(int, char **) {
    printf("Running hyphenation tests.\n");
    test_hyphenation();
}
