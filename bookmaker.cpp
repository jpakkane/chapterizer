#include <pdfrenderer.hpp>
#include <utils.hpp>
#include <chaptercommon.hpp>
#include <chapterbuilder.hpp>
#include <wordhyphenator.hpp>

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

struct margins {
    double inner = 15;
    double outer = 10;
    double upper = 10;
    double lower = 20;
};

void render(const char *ofilename, std::vector<Chapter> &chapters) {
    const int pagew_mm = 110;
    const int pageh_mm = 175;
    PdfRenderer book(ofilename, mm2pt(pagew_mm), mm2pt(pageh_mm));
    margins m;
    const Chapter &c = chapters.front(); // Just do onw.
    FontParameters title_font;
    ChapterParameters chapter_par;
    chapter_par.font.name = "Gentium";
    chapter_par.font.point_size = 10;
    chapter_par.font.type = FontStyle::Regular;
    chapter_par.indent = 5; // First chapter does not get indented.
    chapter_par.line_height_pt = 12;
    chapter_par.paragraph_width_mm = pagew_mm - m.inner - m.outer;
    const double bottom_watermark = pageh_mm - m.lower - pt2mm(chapter_par.line_height_pt);
    const double title_above_space = 50;
    const double title_below_space = 10;
    title_font.name = "Noto sans";
    title_font.point_size = 14;
    title_font.type = FontStyle::Bold;
    double x = m.inner;
    double y = m.upper;
    y += title_above_space;
    book.render_line_as_is(c.title.c_str(), title_font, mm2pt(x), mm2pt(y));
    y += pt2mm(title_font.point_size + 1);
    y += title_below_space;
    const double indent = 5;
    WordHyphenator hyphen;
    int current_page = 1;
    while(chapters[0].paragraphs.size() > 1) {
        chapters[0].paragraphs.pop_back();
    }
    for(const auto &p : c.paragraphs) {
        auto plain_words = split_to_words(std::string_view(p));
        auto hyphenated_words = hyphen.hyphenate(plain_words);
        ChapterBuilder b(hyphenated_words, chapter_par);
        auto lines = b.split_lines();
        size_t line_num = 0;
        for(const auto &line : lines) {
            double current_indent = line_num == 0 ? chapter_par.indent : 0;
            if(y >= bottom_watermark) {
                book.new_page();
                ++current_page;
                y = m.upper;
                x = current_page % 2 ? m.inner : m.outer;
            }
            if(line_num < lines.size() - 1) {
                book.render_line_justified(line,
                                           chapter_par.font,
                                           chapter_par.paragraph_width_mm,
                                           mm2pt(x + current_indent),
                                           mm2pt(y));
            } else {
                book.render_line_as_is(
                    line.c_str(), chapter_par.font, mm2pt(x + current_indent), mm2pt(y));
            }
            line_num++;
            y += pt2mm(chapter_par.line_height_pt);
        }
        chapter_par.indent = indent;
    }
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <input text file>\n", argv[0]);
        return 1;
    }
    auto chapters = load_text(argv[1]);
    printf("The file had %d chapters.\n", (int)chapters.size());
    printf("%s: %d paragraphs.\n",
           chapters.front().title.c_str(),
           (int)chapters.front().paragraphs.size());
    // printf("%s\n", chapters.front().paragraphs.back().c_str());
    render("bookout.pdf", chapters);
    return 0;
}
