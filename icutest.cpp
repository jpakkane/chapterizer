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
    int num_lines = 0;
    for(std::string line; std::getline(ifile, line);) {
        ++num_lines;
        if(line.empty()) {
            continue;
        }
        auto tmpstr = icu::UnicodeString::fromUTF8(line);
        if(ustr.isBogus()) {
            fprintf(stderr, "Fail.\n");
            return 1;
        }
        if(tmpstr.length() > ustr.length()) {
            ustr = tmpstr;
        }
    }
    printf("%d lines.\n", num_lines);
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
