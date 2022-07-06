#include <wordhyphenator.hpp>

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

void test_hyphenation() {
    test_hyphenation_simple();
    test_hyphenation_dash();
    test_hyphenation_emdash();
    test_hyphenation_prefix();
    test_hyphenation_underscore();
}

int main(int, char **) {
    printf("Running hyphenation tests.\n");
    test_hyphenation();
}
