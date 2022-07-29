#include <wordhyphenator.hpp>
#include <glib.h>

#define CHECK(cond)                                                                                \
    if(!(cond)) {                                                                                  \
        printf("Fail %s:%d\n", __PRETTY_FUNCTION__, __LINE__);                                     \
        std::abort();                                                                              \
    }

void test_hyphenation_simple() {
    WordHyphenator h;
    HyphenatedWord w = h.hyphenate("morning");
    HyphenPoint expected{3, SplitType::Regular};
    CHECK(w.hyphen_points.size() == 1);
    CHECK(w.hyphen_points.front() == expected);
}

void test_hyphenation_dash() {
    WordHyphenator h;
    HyphenatedWord w = h.hyphenate("hi-ho");
    HyphenPoint expected{2, SplitType::NoHyphen};
    CHECK(w.hyphen_points.size() == 1);
    CHECK(w.hyphen_points.front() == expected);
}

void test_hyphenation_emdash() {
    WordHyphenator h;
    HyphenatedWord w = h.hyphenate("us—more"); // Unicode
    HyphenPoint expected{4, SplitType::NoHyphen};
    CHECK(w.hyphen_points.size() == 1);
    CHECK(w.hyphen_points.front() == expected);
}

void test_hyphenation_prefix() {
    WordHyphenator h;
    HyphenatedWord w = h.hyphenate("“morning");
    HyphenPoint expected{6,
                         SplitType::Regular}; // First character is Unicode and takes three bytes.
    CHECK(w.hyphen_points.size() == 1);
    CHECK(w.hyphen_points.front() == expected);
}

void test_hyphenation_underscore() {
    WordHyphenator h;
    HyphenatedWord w = h.hyphenate("_Nature_");
    HyphenPoint expected{2, SplitType::Regular};
    CHECK(w.hyphen_points.size() == 1);
    CHECK(w.hyphen_points.front() == expected);
}

void test_utf8_splitting() {
    WordHyphenator h;
    const std::string text{"emerge—possibly"};
    HyphenatedWord w = h.hyphenate(text); // Note: has an em-dash.
    for(size_t i = 0; i < w.hyphen_points.size(); ++i) {
        auto sub = text.substr(0, w.hyphen_points[i].loc + 1);
        CHECK(g_utf8_validate(sub.c_str(), -1, nullptr));
    }
    for(size_t i = 0; i < w.hyphen_points.size(); ++i) {
        auto sub = text.substr(w.hyphen_points[i].loc + 1);
        CHECK(g_utf8_validate(sub.c_str(), -1, nullptr));
    }
}

void test_hyphenation() {
    test_hyphenation_simple();
    test_hyphenation_dash();
    test_hyphenation_emdash();
    test_hyphenation_prefix();
    test_hyphenation_underscore();
    test_utf8_splitting();
}

int main(int, char **) {
    printf("Running hyphenation tests.\n");
    test_hyphenation();
}
