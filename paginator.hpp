#pragma once

#include <bookparser.hpp>
#include <pdfrenderer.hpp>
#include <formatting.hpp>

#include <memory>

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

struct MarkupDrawCommand {
    std::string markup;
    const FontParameters *font;
    Millimeter x;
    Millimeter y;
};

struct JustifiedMarkupDrawCommand {
    std::vector<std::string> markup_words;
    const FontParameters *font;
    Millimeter x;
    Millimeter y;
    Millimeter width;
};

typedef std::variant<MarkupDrawCommand, JustifiedMarkupDrawCommand> TextCommands;

struct PageLayout {
    // Picture.
    std::vector<TextCommands> text;
    std::vector<TextCommands> footnote;

    void clear() {
        text.clear();
        footnote.clear();
    }
};

struct Heights {
    Millimeter text_height;
    Millimeter footnote_height;
    Millimeter whitespace_height;

    Millimeter total_height() const { return text_height + footnote_height + whitespace_height; }

    void clear() { text_height = footnote_height = whitespace_height = Millimeter::from_value(0); }
};

class Paginator {
public:
    Paginator(const Document &d);

    void generate_pdf(const char *outfile);

private:
    void render_page_num(const FontParameters &par);
    void render_formatted_lines(const std::vector<std::vector<std::string>> &lines,
                                Millimeter &x,
                                Millimeter &y,
                                const Millimeter &bottom_watermark,
                                const ChapterParameters &text_par,
                                Millimeter &height_counter);
    void build_formatted_lines(const std::vector<std::vector<std::string>> &lines,
                               Millimeter &x,
                               Millimeter &rel_y,
                               const ChapterParameters &text_par,
                               Millimeter &height_counter);
    std::vector<EnrichedWord> text_to_formatted_words(const std::string &text);

    Millimeter current_left_margin() const { return current_page % 2 ? m.inner : m.outer; }

    void new_page(bool draw_page_num);

    void flush_draw_commands();

    const Document &doc;
    PageSize page;
    FontStyles font_styles;
    margins m;
    std::unique_ptr<PdfRenderer> rend;
    WordHyphenator hyphen;
    int current_page = 1;

    // These keep track of the current page stats.
    PageLayout layout;
    Heights heights;
};
