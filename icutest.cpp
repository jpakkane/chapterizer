#include <unicode/unistr.h>
#include <unicode/search.h>
#include <unicode/regex.h>
#include <unicode/normalizer2.h>

#include <fstream>
#include <vector>

int main(int argc, char **argv) {
    std::ifstream ifile(argv[1]);
    icu::UnicodeString ustr;

    if(argc != 2) {
        fprintf(stderr, "Fail\n");
        return 1;
    }

    //    std::vector<std::string> paragraphs;
    for(std::string line; std::getline(ifile, line);) {
        if(line.empty()) {
            continue;
        }
        ustr = icu::UnicodeString::fromUTF8(line);
        if(ustr.isBogus()) {
            fprintf(stderr, "Fail.\n");
            return 1;
        }
        if(ustr.length() > 40) {
            break;
        }
    }
    // https://www.unicode.org/reports/tr15/
    UErrorCode status = U_ZERO_ERROR;
    auto *nrml = icu::Normalizer2::getNFCInstance(status);
    if(U_FAILURE(status)) {
        fprintf(stderr, "Fail %d.\n\n%s\n", status, u_errorName(status));
        return 1;
    };
    auto normalized = nrml->normalize(ustr, status);
    if(U_FAILURE(status)) {
        fprintf(stderr, "Fail %d.\n\n%s\n", status, u_errorName(status));
        return 1;
    };
    ustr = normalized;

    //  icu::Locale lang = icu::Locale::getUS();
    status = U_ZERO_ERROR;
    //    icu::BreakIterator *word_breaker = icu::BreakIterator::createWordInstance(lang, status);
    if(U_FAILURE(status)) {
        fprintf(stderr, "Fail %d.\n\n%s\n", status, u_errorName(status));
        return 1;
    };
    status = U_ZERO_ERROR;
    icu::RegexMatcher matcher("\\s+", 0, status);
    if(U_FAILURE(status)) {
        fprintf(stderr, "Fail %d.\n\n%s\n", status, u_errorName(status));
        return 1;
    }
    const int max_words = 100;
    icu::UnicodeString buf[max_words];
    int num_matches = matcher.split(ustr, buf, max_words, status);
    if(U_FAILURE(status)) {
        fprintf(stderr, "Fail %d.\n\n%s\n", status, u_errorName(status));
        return 1;
    }
    std::vector<icu::UnicodeString> words;
    for(int i = 0; i < num_matches; ++i) {
        words.emplace_back(buf[i]);
    }
    std::string out;
    for(const auto &w : words) {
        w.toUTF8String(out);
        printf("%s\n", out.c_str());
        out.clear();
    }
    return 0;
}
