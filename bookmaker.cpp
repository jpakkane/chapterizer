#include <pdfrenderer.hpp>
#include <utils.hpp>
#include <chaptercommon.hpp>
#include <paragraphformatter.hpp>
#include <wordhyphenator.hpp>

#include <tinyxml2.h>
#include <filesystem>

#include <glib.h>
#include <cassert>
#include <fstream>
#include <cstring>

struct Chapter {
    std::string title;
    std::vector<std::string> paragraphs;
};

namespace fs = std::filesystem;

namespace {

const char mimetext[] = "application/epub+zip";

bool looks_like_title(const std::string &line) {
    if(line.empty()) {
        return false;
    }
    if(line.size() < 6 && line.back() == '.' &&
       (line.front() == 'I' || line.front() == 'V' || line.front() == 'X')) {
        return true;
    }
    return false;
}

} // namespace

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
        // FIXME, strip.
        if(line.empty()) {
            if(reading_title) {
                reading_title = false;
            } else {
                if(!paragraph_text.empty()) {
                    paragraphs.emplace_back(std::move(paragraph_text));
                    paragraph_text.clear();
                }
            }
            continue;
        }
        if(!g_utf8_validate(line.c_str(), line.length(), nullptr)) {
            printf("Line %d not valid UTF-8.\n", num_lines);
            exit(1);
        }
        gchar *normalized_text = g_utf8_normalize(line.c_str(), line.length(), G_NORMALIZE_NFC);
        line = normalized_text;
        g_free(normalized_text);
        if(!reading_title) {
            if(looks_like_title(line)) {
                if(!paragraph_text.empty()) {
                    paragraphs.emplace_back(std::move(paragraph_text));
                    paragraph_text.clear();
                }
                if(!paragraphs.empty()) {
                    chapters.emplace_back(Chapter{std::move(title_text), std::move(paragraphs)});
                    title_text.clear();
                    paragraphs.clear();
                }
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

struct PageSize {
    int w_mm;
    int h_mm;
};

void render_page_num(
    PdfRenderer &book, FontParameters &par, int page_num, const PageSize &p, const margins &m) {
    char buf[128];
    snprintf(buf, 128, "%d", page_num);
    const double yloc = p.h_mm - 2 * m.lower / 3;
    const double leftmargin = page_num % 2 ? m.inner : m.outer;
    const double xloc = leftmargin + (p.w_mm - m.inner - m.outer) / 2;
    book.render_line_centered(buf, par, mm2pt(xloc), mm2pt(yloc));
}

void create_pdf(const char *ofilename, std::vector<Chapter> &chapters) {
    PageSize page;

    page.w_mm = 110;
    page.h_mm = 175;
    PdfRenderer book(ofilename, mm2pt(page.w_mm), mm2pt(page.h_mm));
    margins m;
    FontParameters title_font;
    ChapterParameters chapter_par;
    chapter_par.font.name = "Gentium";
    chapter_par.font.point_size = 10;
    chapter_par.font.type = FontStyle::Regular;
    chapter_par.indent = 5; // First chapter does not get indented.
    chapter_par.line_height_pt = 12;
    chapter_par.paragraph_width_mm = page.w_mm - m.inner - m.outer;
    ExtraPenaltyAmounts extras;
    const double bottom_watermark = page.h_mm - m.lower - pt2mm(chapter_par.line_height_pt);
    const double title_above_space = 30;
    const double title_below_space = 10;
    title_font.name = "Noto sans";
    title_font.point_size = 14;
    title_font.type = FontStyle::Bold;
    double x = m.inner;
    double y = m.upper;
    const double indent = 5;
    WordHyphenator hyphen;
    int current_page = 1;
    bool first_chapter = true;
    int chapter_num = 0;
    for(const auto &c : chapters) {
        printf("Processing chapter %d/%d.\n", ++chapter_num, (int)chapters.size());
        if(!first_chapter) {
            render_page_num(book, chapter_par.font, current_page, page, m);
            book.new_page();
            ++current_page;
            if(current_page % 2 == 0) {
                book.new_page();
                ++current_page;
            }
            y = m.upper;
            x = current_page % 2 ? m.inner : m.outer;
        }
        y += title_above_space;
        book.render_line_as_is(c.title.c_str(), title_font, mm2pt(x), mm2pt(y));
        y += pt2mm(title_font.point_size + 1);
        y += title_below_space;
        bool first_paragraph = true;
        for(const auto &p : c.paragraphs) {
            chapter_par.indent = first_paragraph ? 0 : indent;
            auto plain_words = split_to_words(std::string_view(p));
            auto hyphenated_words = hyphen.hyphenate(plain_words);
            ParagraphFormatter b(hyphenated_words, chapter_par, extras);
            auto lines = b.split_lines();
            size_t line_num = 0;
            for(const auto &line : lines) {
                assert(g_utf8_validate(line.c_str(), -1, nullptr));
                double current_indent = line_num == 0 ? chapter_par.indent : 0;
                if(y >= bottom_watermark) {
                    render_page_num(book, chapter_par.font, current_page, page, m);
                    book.new_page();
                    ++current_page;
                    y = m.upper;
                    x = current_page % 2 ? m.inner : m.outer;
                }
                if(line_num < lines.size() - 1) {
                    book.render_line_justified(line,
                                               chapter_par.font,
                                               chapter_par.paragraph_width_mm - current_indent,
                                               mm2pt(x + current_indent),
                                               mm2pt(y));
                } else {
                    book.render_line_as_is(
                        line.c_str(), chapter_par.font, mm2pt(x + current_indent), mm2pt(y));
                }
                line_num++;
                y += pt2mm(chapter_par.line_height_pt);
            }
            first_paragraph = false;
        }
        first_chapter = false;
    }
    render_page_num(book, chapter_par.font, current_page, page, m);
}

void create_epub(const char *ofilename, std::vector<Chapter> &chapters) {
    fs::path outdir{"epubtmp"};
    fs::remove_all(outdir);
    auto metadir = outdir / "META-INF";
    auto oebpsdir = outdir / "OEBPS";
    auto mimefile = outdir / "mimetype";
    auto containerfile = metadir / "container.xml";

    fs::create_directories(metadir);
    fs::create_directory(oebpsdir);

    FILE *f = fopen(mimefile.c_str(), "w");
    fwrite(mimetext, 1, strlen(mimetext), f);
    fclose(f);
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <input text file>\n", argv[0]);
        return 1;
    }
    auto chapters = load_text(argv[1]);
    /*
    printf("The file had %d chapters.\n", (int)chapters.size());
    printf("%s: %d paragraphs.\n",
           chapters.front().title.c_str(),
           (int)chapters.front().paragraphs.size());
    // printf("%s\n", chapters.front().paragraphs.back().c_str());
    */
    // create_pdf("bookout.pdf", chapters);
    create_epub("war_test.epub", chapters);
    return 0;
}
