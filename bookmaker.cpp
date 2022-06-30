#include <pdfrenderer.hpp>
#include <utils.hpp>

#include <cassert>
#include <fstream>

struct Chapter {
    std::string title;
    std::vector<std::string> paragraphs;
};

bool looks_like_title(const std::string &line) {
    if(line.empty()) {
        return false;
    }
    if(line.size() < 6 && line.back() == '.' && line.front() == 'I') {
        return true;
    }
    return false;
}

std::vector<Chapter> load_text(const char *fname) {
    std::vector<Chapter> chapters;
    std::ifstream ifile(fname);

    bool reading_title = false;
    int num_lines = 0;
    std::string title_text;
    std::string paragraph_text;
    std::vector<std::string> paragraphs;
    for(std::string line; std::getline(ifile, line);) {
        ++num_lines;
        if(line.empty()) {
            if(reading_title) {
                reading_title = false;
            } else {
                if(!paragraph_text.empty()) {
                    paragraphs.push_back(paragraph_text);
                    paragraph_text.clear();
                }
            }
            continue;
        }
        if(!reading_title) {
            if(looks_like_title(line)) {
                reading_title = true;
                title_text = line;
                title_text += ' ';
                continue;
            } else {
                paragraph_text += line;
                paragraph_text += ' ';
            }
        } else {
            title_text += line;
            title_text += ' ';
        }
    }
    if(!paragraph_text.empty()) {
        paragraphs.emplace_back(std::move(paragraph_text));
    }
    printf("File had %d lines.\n", num_lines);
    chapters.emplace_back(Chapter{std::move(title_text), std::move(paragraphs)});
    return chapters;
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <input text file>\n", argv[0]);
        return 1;
    }
    PdfRenderer book("bookout.pdf", mm2pt(130), mm2pt(200));
    auto chapters = load_text(argv[1]);
    printf("The file had %d chapters.\n", (int)chapters.size());
    printf("%s: %d paragraphs.\n",
           chapters.front().title.c_str(),
           (int)chapters.front().paragraphs.size());
    printf("%s\n", chapters.front().paragraphs.back().c_str());
    return 0;
}
