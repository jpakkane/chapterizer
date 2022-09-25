#include <pdfrenderer.hpp>
#include <utils.hpp>
#include <chaptercommon.hpp>
#include <paragraphformatter.hpp>
#include <wordhyphenator.hpp>
#include <formatting.hpp>
#include <bookparser.hpp>
#include <epub.hpp>

#include <glib.h>
#include <cassert>
#include <fstream>
#include <cstring>

struct margins {
    Millimeter inner = Millimeter::from_value(15);
    Millimeter outer = Millimeter::from_value(10);
    Millimeter upper = Millimeter::from_value(10);
    Millimeter lower = Millimeter::from_value(20);
};

struct PageSize {
    Millimeter w;
    Millimeter h;
};

void render_page_num(PdfRenderer &book,
                     const FontParameters &par,
                     int page_num,
                     const PageSize &p,
                     const margins &m) {
    char buf[128];
    snprintf(buf, 128, "%d", page_num);
    const Millimeter yloc = p.h - 2.0 * m.lower / 3.0;
    const Millimeter leftmargin = page_num % 2 ? m.inner : m.outer;
    const Millimeter xloc = leftmargin + (p.w - m.inner - m.outer) / 2;
    book.render_line_centered(buf, par, xloc.topt(), yloc.topt());
}

template<typename T> void style_change(T &stack, typename T::value_type val) {
    if(stack.contains(val)) {
        stack.pop(val);
        // If the
    } else {
        stack.push(val);
    }
}

// NOTE: mutates the input words.
std::vector<FormattingChange> extract_styling(StyleStack &current_style, std::string &word) {
    std::vector<FormattingChange> changes;
    std::string buf;
    const char *word_start = word.c_str();
    const char *in = word_start;
    int num_changes = 0;

    while(*in) {
        auto c = g_utf8_get_char(in);

        switch(c) {
        case italic_codepoint:
            style_change(current_style, ITALIC_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), ITALIC_S});
            ++num_changes;
            break;
        case bold_codepoint:
            style_change(current_style, BOLD_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), BOLD_S});
            ++num_changes;
            break;
        case tt_codepoint:
            style_change(current_style, TT_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), TT_S});
            ++num_changes;
            break;
        case smallcaps_codepoint:
            style_change(current_style, SMALLCAPS_S);
            changes.push_back(FormattingChange{size_t(in - word_start - num_changes), SMALLCAPS_S});
            ++num_changes;
            break;
        case superscript_codepoint:
            style_change(current_style, SUPERSCRIPT_S);
            changes.push_back(
                FormattingChange{size_t(in - word_start - num_changes), SUPERSCRIPT_S});
            ++num_changes;
            break;
        default:
            char tmp[10];
            const int bytes_written = g_unichar_to_utf8(c, tmp);
            tmp[bytes_written] = '\0';
            buf += tmp;
        }
        in = g_utf8_next_char(in);
    }
    word = buf;
    return changes;
}

void render_formatted_lines(const std::vector<std::vector<std::string>> &lines,
                            Millimeter &x,
                            Millimeter &y,
                            const Millimeter &bottom_watermark,
                            int &current_page,
                            const margins &m,
                            const PageSize &page,
                            const ChapterParameters &text_par,
                            PdfRenderer &book) {
    const bool debug_draw = true;
    size_t line_num = 0;
    for(const auto &markup_words : lines) {
        Millimeter current_indent = line_num == 0 ? text_par.indent : Millimeter{};
        if(y >= bottom_watermark) {
            render_page_num(book, text_par.font, current_page, page, m);
            book.new_page();
            ++current_page;
            y = m.upper;
            x = current_page % 2 ? m.inner : m.outer;
            if(debug_draw) {
                if(current_page % 2) {
                    book.draw_box(m.inner.topt(),
                                  m.upper.topt(),
                                  (page.w - m.inner - m.outer).topt(),
                                  (page.h - m.upper - m.lower).topt());
                } else {
                    book.draw_box(m.outer.topt(),
                                  m.upper.topt(),
                                  (page.w - m.inner - m.outer).topt(),
                                  (page.h - m.upper - m.lower).topt());
                }
            }
        }
        if(line_num < lines.size() - 1) {
            book.render_line_justified(markup_words,
                                       text_par.font,
                                       text_par.paragraph_width - current_indent,
                                       (x + current_indent).topt(),
                                       y.topt());
        } else {
            book.render_markup_as_is(
                markup_words, text_par.font, (x + current_indent).topt(), y.topt());
        }
        line_num++;
        y += text_par.line_height.tomm();
    }
}

std::vector<EnrichedWord> text_to_formatted_words(const std::string &text,
                                                  const WordHyphenator &hyphen) {
    StyleStack current_style;
    auto plain_words = split_to_words(std::string_view(text));
    std::vector<EnrichedWord> processed_words;
    for(const auto &word : plain_words) {
        auto working_word = word;
        auto start_style = current_style;
        auto formatting_data = extract_styling(current_style, working_word);
        auto hyphenation_data = hyphen.hyphenate(working_word);
        processed_words.emplace_back(EnrichedWord{std::move(working_word),
                                                  std::move(hyphenation_data),
                                                  std::move(formatting_data),
                                                  start_style});
    }
    return processed_words;
}

void create_pdf(const char *ofilename, const Document &doc) {
    PageSize page;
    page.w = Millimeter::from_value(110);
    page.h = Millimeter::from_value(175);
    PdfRenderer book(ofilename, page.w.topt(), page.h.topt());
    margins m;
    FontStyles font_styles;
    font_styles.basic.name = "Gentium";
    font_styles.basic.size = Point::from_value(10);
    font_styles.code.name = "Liberation Mono";
    font_styles.code.size = Point::from_value(8);
    font_styles.footnote.size = Point::from_value(9);
    font_styles.heading.name = "Noto sans";
    font_styles.heading.size = Point::from_value(14);
    font_styles.heading.type = FontStyle::Bold;

    ChapterParameters text_par;
    text_par.indent = Millimeter::from_value(5);
    text_par.line_height = Point::from_value(12);
    text_par.paragraph_width = page.w - m.inner - m.outer;
    text_par.font = font_styles.basic;

    ChapterParameters code_par = text_par;
    code_par.font = font_styles.code;

    ChapterParameters footnote_par = text_par;
    footnote_par.font = font_styles.footnote;
    footnote_par.line_height = Point::from_value(11);
    footnote_par.indent = Millimeter::from_value(4);

    ExtraPenaltyAmounts extras;
    const Millimeter bottom_watermark = page.h - m.lower - text_par.line_height.tomm();
    const Millimeter title_above_space = Millimeter::from_value(30);
    const Millimeter title_below_space = Millimeter::from_value(10);
    const Millimeter different_paragraph_space = Millimeter::from_value(2);
    Millimeter x = m.inner;
    Millimeter y = m.upper;
    const Millimeter indent = Millimeter::from_value(5);
    WordHyphenator hyphen;
    int current_page = 1;
    bool first_paragraph = true;
    bool first_section = true;
    for(const auto &e : doc.elements) {
        if(std::holds_alternative<Section>(e)) {
            const Section &s = std::get<Section>(e);
            if(!first_section) {
                render_page_num(book, text_par.font, current_page, page, m);
                book.new_page();
                ++current_page;
                if(current_page % 2 == 0) {
                    book.new_page();
                    ++current_page;
                }
            }
            first_section = false;
            y = m.upper;
            x = current_page % 2 ? m.inner : m.outer;
            y += title_above_space;
            assert(s.level == 1);
            std::string full_title = std::to_string(s.number);
            full_title += ". ";
            full_title += s.text;
            book.render_markup_as_is(full_title.c_str(), font_styles.heading, x.topt(), y.topt());
            y += font_styles.heading.size.tomm();
            y += title_below_space;
            first_paragraph = true;
        } else if(std::holds_alternative<Paragraph>(e)) {
            const Paragraph &p = std::get<Paragraph>(e);
            StyleStack current_style;
            text_par.indent = first_paragraph ? Millimeter::from_value(0) : indent;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(p.text, hyphen);
            ParagraphFormatter b(processed_words, text_par, extras);
            auto lines = b.split_formatted_lines();
            render_formatted_lines(
                lines, x, y, bottom_watermark, current_page, m, page, text_par, book);
            first_paragraph = false;
        } else if(std::holds_alternative<Footnote>(e)) {
            const Footnote &f = std::get<Footnote>(e);
            StyleStack current_style;
            std::vector<EnrichedWord> processed_words = text_to_formatted_words(f.text, hyphen);
            ParagraphFormatter b(processed_words, footnote_par, extras);
            auto lines = b.split_formatted_lines();
            std::string fnum = std::to_string(f.number);
            fnum += '.';
            book.render_text_as_is(fnum.c_str(), footnote_par.font, x.topt(), y.topt());
            render_formatted_lines(
                lines, x, y, bottom_watermark, current_page, m, page, footnote_par, book);
        } else if(std::holds_alternative<SceneChange>(e)) {
            y += text_par.line_height.tomm();
            if(y >= bottom_watermark) {
                render_page_num(book, text_par.font, current_page, page, m);
                book.new_page();
                ++current_page;
                y = m.upper;
                x = current_page % 2 ? m.inner : m.outer;
            }
            first_paragraph = true;
        } else if(std::holds_alternative<CodeBlock>(e)) {
            const CodeBlock &cb = std::get<CodeBlock>(e);
            y += different_paragraph_space;
            for(const auto &line : cb.raw_lines) {
                if(y >= bottom_watermark) {
                    render_page_num(book, text_par.font, current_page, page, m);
                    book.new_page();
                    ++current_page;
                    y = m.upper;
                    x = current_page % 2 ? m.inner : m.outer;
                }
                book.render_text_as_is(line.c_str(), code_par.font, x.topt(), y.topt());
                y += code_par.line_height.tomm();
            }
            first_paragraph = true;
            y += different_paragraph_space;
        } else {
            std::abort();
        }
    }
}

Document load_document(const char *fname) {
    Document doc;
    MMapper map(fname);
    if(!g_utf8_validate(map.data(), map.size(), nullptr)) {
        printf("Invalid utf-8.\n");
        std::abort();
    }
    GError *err = nullptr;
    if(err) {
        std::abort();
    }

    LineParser linep(map.data(), map.size());
    StructureParser strucp;

    line_token token = linep.next();
    while(!std::holds_alternative<EndOfFile>(token)) {
        strucp.push(token);
        token = linep.next();
    }
    strucp.push(token);

    // FIXME, add stored data if any.
    return strucp.get_document();
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <input text file>\n", argv[0]);
        return 1;
    }
    auto doc = load_document(argv[1]);
    /*
    printf("The file had %d chapters.\n", (int)chapters.size());
    printf("%s: %d paragraphs.\n",
           chapters.front().title.c_str(),
           (int)chapters.front().paragraphs.size());
    // printf("%s\n", chapters.front().paragraphs.back().c_str());
    */
    create_pdf("bookout.pdf", doc);
    Epub epub(doc);
    epub.generate("epub_test.epub");
    return 0;
}
