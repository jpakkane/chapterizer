#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

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

std::vector<std::string> split(const std::string &text) {
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

void print_output(const std::vector<std::string> &lines, const size_t target_width) {
    for(const auto &l : lines) {
        printf("%-75s %3d %3d\n", l.c_str(), int(l.length()), int(l.length() - target_width));
    }
}

int main() {
    const size_t target_width = 66;
    std::string text{raw_text};
    for(size_t i = 0; i < text.size(); ++i) {
        if(text[i] == '\n') {
            text[i] = ' ';
        }
    }

    auto words = split(text);
    printf("Text has %d words.\n\n", (int)words.size());
    const auto eager_lines = eager_split(words, target_width);
    print_output(eager_lines, target_width);
    printf("\n\n");
    const auto smarter_lines = slightly_smarter_split(words, target_width);
    print_output(smarter_lines, target_width);
    return 0;
}
