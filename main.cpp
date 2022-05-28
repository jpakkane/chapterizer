#include <wordhyphenator.hpp>
#include <chapterbuilder.hpp>
#include <pdfrenderer.hpp>
#include <cstdio>
#include <cassert>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#if 0
const char raw_text[] =
    R"(From the corner of the divan of Persian saddle-bags on which he was lying, smoking, as
was his custom, innumerable cigarettes, Lord Henry Wotton could just catch the gleam of
the honey-sweet and honey-coloured blossoms of a laburnum, whose tremulous branches
seemed hardly able to bear the burden of a beauty so flamelike as theirs.)";

#else

const char raw_text[] =
    R"(From the corner of the divan of Persian saddle-bags on which he was lying, smoking, as
was his custom, innumerable cigarettes, Lord Henry Wotton could just catch the gleam of
the honey-sweet and honey-coloured blossoms of a laburnum, whose tremulous branches
seemed hardly able to bear the burden of a beauty so flamelike as theirs; and now and
then the fantastic shadows of birds in flight flitted across the long tussore-silk curtains
that were stretched in front of the huge window, producing a kind of momentary Japanese
effect, and making him think of those pallid, jade-faced painters of Tokyo who, through
the medium of an art that is necessarily immobile, seek to convey the sense of swiftness
and motion. The sullen murmur of the bees shouldering their way through the long
unmown grass, or circling with monotonous insistence round the dusty gilt horns of the
straggling woodbine, seemed to make the stillness more oppressive. The dim roar of
London was like the bourdon note of a distant organ.)";

#endif

std::vector<std::string> split(const std::string &in_text) {
    std::string text;
    text.reserve(in_text.size());
    for(size_t i = 0; i < in_text.size(); ++i) {
        if(in_text[i] == '\n') {
            text.push_back(' ');
        } else {
            text.push_back(in_text[i]);
        }
    }
    std::string val;
    const char separator = ' ';
    std::vector<std::string> words;
    std::stringstream sstream(text);
    while(std::getline(sstream, val, separator)) {
        words.push_back(val);
    }
    return words;
}

std::vector<std::string> eager_split(const std::vector<std::string> &words,
                                     const size_t target_width) {
    std::vector<std::string> lines;
    std::string current_line;
    for(const auto &w : words) {
        if(current_line.empty()) {
            current_line = w;
            continue;
        }
        if(current_line.length() + w.length() >= target_width) {
            lines.emplace_back(std::move(current_line));
            current_line = w;
        } else {
            current_line += ' ';
            current_line += w;
        }
    }
    if(!current_line.empty()) {
        lines.emplace_back(std::move(current_line));
    }
    return lines;
}

std::vector<std::string> slightly_smarter_split(const std::vector<std::string> &words,
                                                const size_t target_width) {
    std::vector<std::string> lines;
    std::string current_line;
    for(const auto &w : words) {
        if(current_line.empty()) {
            current_line = w;
            continue;
        }
        const auto current_w = int(current_line.length());
        const auto appended_w = int(current_line.length() + w.length() + 1);

        if(appended_w >= (int)target_width) {
            const auto current_delta = abs(current_w - (int)target_width);
            const auto appended_delta = abs(appended_w - (int)target_width);
            if(current_delta <= appended_delta) {
                lines.emplace_back(std::move(current_line));
                current_line = w;
            } else {
                current_line += ' ';
                current_line += w;
                lines.emplace_back(std::move(current_line));
                current_line = "";
            }
        } else {
            current_line += ' ';
            current_line += w;
        }
    }
    if(!current_line.empty()) {
        lines.emplace_back(std::move(current_line));
    }
    return lines;
}

size_t detect_optimal_split_point(const HyphenatedWord &word,
                                  const int line_width,
                                  const int target_width) {
    const int hyphen_width = 1;
    auto loc = std::find_if(
        word.hyphens.begin(), word.hyphens.end(), [&line_width, &target_width](size_t hyph_index) {
            return int(line_width + hyph_index + hyphen_width) > target_width;
        });
    if(loc == word.hyphens.end()) {
        return word.hyphens.back();
    }
    // Check if the split point before is better than the current one.
    // I.e. if "overshoot" is better than "undershoot".
    return *loc;
}

std::vector<std::string> hyphenation_split(const std::vector<HyphenatedWord> &words,
                                           const size_t target_width) {
    std::vector<std::string> lines;
    std::string current_line;
    const int space_width = 1;
    const int hyphen_width = 1;
    for(const auto &hw : words) {
        if(current_line.empty()) {
            current_line = hw.word;
            continue;
        }
        const auto current_w = int(current_line.length());
        const auto appended_w = int(current_w + space_width + hw.word.length());

        if(appended_w >= (int)target_width) {
            if(hw.hyphens.empty() ||
               (current_w + space_width + hw.hyphens[0] + hyphen_width >= target_width)) {
                // Even the first hyphenation split overshoots.
                lines.emplace_back(std::move(current_line));
                current_line = hw.word;
            } else {
                const auto splitpoint =
                    detect_optimal_split_point(hw, current_w + space_width, target_width);
                current_line += ' ';
                current_line += hw.word.substr(0, splitpoint);
                current_line += '-';
                lines.emplace_back(std::move(current_line));
                current_line = hw.word.substr(splitpoint);
            }
        } else {
            current_line += ' ';
            current_line += hw.word;
        }
    }
    if(!current_line.empty()) {
        lines.emplace_back(std::move(current_line));
    }
    return lines;
}

void print_output(const std::vector<std::string> &lines, const size_t target_width) {
    for(const auto &l : lines) {
        printf("%-75s %3d %3d\n", l.c_str(), int(l.length()), int(l.length() - target_width));
    }
}

#include <wordhyphenator.hpp>
std::vector<HyphenatedWord> do_hyphenstuff(const std::vector<std::string> &plain_words) {
    WordHyphenator wp;
    std::vector<HyphenatedWord> result;
    result.reserve(plain_words.size());
    for(const auto &w : plain_words) {
        result.emplace_back(wp.hyphenate(w));
    }
    return result;
}

void hyphentest() {
    WordHyphenator wp;
    std::string test_word{"shouldering"};
    const auto result = wp.hyphenate(test_word);
    printf("The hyphenated form is ");
    size_t hyphenloc = 0;
    for(size_t i = 0; i < result.word.length(); ++i) {
        printf("%c", result.word[i]);
        if(hyphenloc < result.hyphens.size() && result.hyphens[hyphenloc] == i) {
            printf("-");
            ++hyphenloc;
        }
    }
    printf(".\n");
    exit(0);
}

template<typename T1, typename T2>
void monospacetest(const T1 &plain_words, const T2 &hyphenated_words) {
    const size_t target_width = 66;
    printf("Text has %d words.\n\n", (int)plain_words.size());
    const auto eager_lines = eager_split(plain_words, target_width);
    printf("-- dumb ---\n\n");
    print_output(eager_lines, target_width);
    printf("\n--- slightly smarter ---\n\n");
    const auto smarter_lines = slightly_smarter_split(plain_words, target_width);
    print_output(smarter_lines, target_width);
    printf("\n-- hyphenation ---\n\n");
    const auto hyph_lines = hyphenation_split(hyphenated_words, target_width);
    print_output(hyph_lines, target_width);
}
#include <textstats.hpp>
static TextStats hack;
template<typename T1> void full_test(const T1 &hyphenated_words) {
    const double paragraph_width = 60.0;
    ChapterBuilder spl{hyphenated_words, paragraph_width};
    const auto lines = spl.split_lines();

    for(const auto &line : lines) {
        printf("%40s %.2f\n", line.c_str(), hack.text_width(line.c_str()));
    }

    PdfRenderer r("texttest.pdf");
    r.render(lines);
}

int main() {
    setlocale(LC_ALL, "");
    // hyphentest();
    std::string text{raw_text};

    auto plain_words = split(raw_text);
    auto hyphenated_words = do_hyphenstuff(plain_words);

    if(false) {
        monospacetest(plain_words, hyphenated_words);
        printf("\n\n");
    }
    full_test(hyphenated_words);
    return 0;
}
