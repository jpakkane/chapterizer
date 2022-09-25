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
                                const ChapterParameters &text_par);
    std::vector<EnrichedWord> text_to_formatted_words(const std::string &text);

    const Document &doc;
    PageSize page;
    FontStyles font_styles;
    margins m;
    std::unique_ptr<PdfRenderer> rend;
    WordHyphenator hyphen;
    int current_page = 1;
};
