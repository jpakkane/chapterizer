#pragma once

#include <bookparser.hpp>
#include <pdfrenderer.hpp>
#include <formatting.hpp>

#include <memory>

struct margins {
    Millimeter inner = Millimeter::from_value(20);
    Millimeter outer = Millimeter::from_value(15);
    Millimeter upper = Millimeter::from_value(15);
    Millimeter lower = Millimeter::from_value(15);
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

struct ImageCommand {
    ImageInfo i;
    Millimeter display_height;
    Millimeter display_width;
};

typedef std::variant<MarkupDrawCommand, JustifiedMarkupDrawCommand> TextCommands;

struct PageLayout {
    std::vector<ImageCommand> images;
    std::vector<TextCommands> text;
    std::vector<TextCommands> footnote;

    bool empty() const { return text.empty() && footnote.empty(); }

    void clear() {
        images.clear();
        text.clear();
        footnote.clear();
    }
};

struct Heights {
    Millimeter figure_height;
    Millimeter text_height;
    Millimeter footnote_height;
    Millimeter whitespace_height;

    Millimeter total_height() const {
        return figure_height + text_height + footnote_height + whitespace_height;
    }

    void clear() {
        figure_height = text_height = footnote_height = whitespace_height =
            Millimeter::from_value(0);
    }
};

class Paginator {
public:
    Paginator(const Document &d);

    void generate_pdf(const char *outfile);

private:
    void render_page_num(const FontParameters &par);
    std::vector<TextCommands>
    build_formatted_lines(const std::vector<std::vector<std::string>> &lines,
                          const ChapterParameters &text_par);
    std::vector<EnrichedWord> text_to_formatted_words(const std::string &text);

    Millimeter current_left_margin() const { return current_page % 2 ? m.inner : m.outer; }

    void new_page(bool draw_page_num);

    void flush_draw_commands();

    Millimeter textblock_width() const { return page.w - m.inner - m.outer; }

    void add_pending_figure(const ImageInfo &f);
    void add_top_image(const ImageInfo &image);

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
    std::optional<ImageInfo> pending_figure;
};
