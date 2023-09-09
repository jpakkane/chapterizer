/*
 * Copyright 2023 Jussi Pakkanen
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

#include <cairo.h>
#include <cairo-pdf.h>
#include <cstdlib>
#include <cassert>

#include <vector>
#include <string>
#include <variant>

struct Paragraph {
    std::vector<std::string> lines;
};

struct Heading {
    std::vector<std::string> lines;
};

struct Figure {
    double h;
    std::string text;
};

typedef std::variant<Paragraph, Heading, Figure> Element;

struct TextLoc {
    size_t element;
    size_t line;
};

struct PageContent {
    TextLoc start;
    TextLoc end;
    int32_t num_lines;
    // std::vector<size_t> images;
    // std::something footnotes;
};

struct PageStats {
    bool has_widow = false;
    bool has_orphan = false;
    int32_t height_delta = 0; // line count difference to previous page
};

struct PaginationData {
    PageContent c;
    PageStats s;
};

struct LayoutPenalties {
    double widow = 20;
    double orphan = 10;
    double different_spread = 10;
    double missing_target = 5;
    double last_page_widow = 100;
};

namespace {

double mm2pt(const double x) { return x * 2.8346456693; }

Paragraph dummy_paragraph(int32_t num_lines) {
    Paragraph p;
    for(int32_t i = 0; i < num_lines; ++i) {
        p.lines.emplace_back("");
    }
    return p;
}

std::vector<Element> create_document() {
    std::vector<Element> es;
    es.emplace_back(dummy_paragraph(8));
    es.emplace_back(dummy_paragraph(15));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(3));
    // scene
    es.emplace_back(dummy_paragraph(6));

    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(7));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(7));
    es.emplace_back(dummy_paragraph(15));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(3));
    // scene
    es.emplace_back(dummy_paragraph(13));
    es.emplace_back(dummy_paragraph(6));

    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(12));
    es.emplace_back(dummy_paragraph(13));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(8));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));

    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(7));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(4));

    es.emplace_back(dummy_paragraph(6));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(6));
    es.emplace_back(dummy_paragraph(13));
    es.emplace_back(dummy_paragraph(8));

    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(6));

    es.emplace_back(dummy_paragraph(3));
    // scene
    es.emplace_back(dummy_paragraph(15));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(6));

    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(1));

    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(8));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(1));
    // scene
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));

    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(7));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(4));

    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(7));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(3));

    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(4));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(7));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(4));

    es.emplace_back(dummy_paragraph(13));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(2));

    return es;
}

} // namespace

class PageSplitter {
public:
    PageSplitter(const std::vector<Element> &e, int32_t target)
        : elements(e), line_target{target} {}

    std::vector<PaginationData> split_to_pages() {
        std::vector<PaginationData> pages;
        int num_lines = 0;
        TextLoc previous{0, 0};
        for(size_t eind = 0; eind < elements.size(); ++eind) {
            const auto &e = elements[eind];
            const auto &p = std::get<Paragraph>(e);
            for(size_t lind = 0; lind < p.lines.size(); ++lind) {
                if(num_lines >= line_target) {
                    assert(num_lines == line_target);
                    TextLoc next{eind, lind};
                    pages.emplace_back(compute_page_stats(previous, next, num_lines));
                    num_lines = 1; // Will be correct on the next round.
                    previous = next;
                } else {
                    ++num_lines;
                }
            }
        }
        if(num_lines > 0) {
            TextLoc next{elements.size() - 1, std::get<Paragraph>(elements.back()).lines.size()};
            pages.emplace_back(compute_page_stats(previous, next, num_lines));
        }
        compute_interpage_stats(pages);
        return pages;
    }

private:
    PaginationData compute_page_stats(const TextLoc &from, const TextLoc &to, int32_t num_lines) {
        PaginationData pd;
        pd.c.start = from;
        pd.c.end = to;
        pd.c.num_lines = num_lines;
        const auto &from_el = std::get<Paragraph>(elements[from.element]);
        const auto &to_el = std::get<Paragraph>(elements[to.element]);
        if(from.line == from_el.lines.size() - 1 && from_el.lines.size() > 1) {
            pd.s.has_widow = true;
        }
        if(to.line == 1 && to_el.lines.size() > 1) {
            pd.s.has_orphan = true;
        }

        return pd;
    }

    void compute_interpage_stats(std::vector<PaginationData> &pages) {
        for(size_t i = 1; i < pages.size(); ++i) {
            pages[i].s.height_delta =
                int32_t(pages[i].c.num_lines) - int32_t(pages[i - 1].c.num_lines);
        }
    }

    LayoutPenalties penalties;
    const std::vector<Element> &elements;
    const int32_t line_target;
};

class Paginator {
public:
    Paginator() {
        w = mm2pt(135);
        h = mm2pt(210);
        inner = mm2pt(20);
        outer = mm2pt(15);
        top = mm2pt(15);
        bottom = mm2pt(25);

        textblock_width = w - inner - outer;
        textblock_height = h - top - bottom;

        line_height = 14;
        line_target = textblock_height / line_height;

        assert(int32_t(textblock_height / line_height) == line_target);

        page_number = 1;
        surf = cairo_pdf_surface_create("paginationtest.pdf", w, h);
        cr = cairo_create(surf);
    }

    ~Paginator() {
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }

    void draw_page(const std::vector<Element> &elements, const PageContent &page) {
        const double indent = 30;
        double y = top;
        size_t eind = page.start.element;
        size_t lind = page.start.line;
        while(eind <= page.end.element) {
            if(eind == page.end.element && lind == page.end.line) {
                break;
            }
            const auto &p = std::get<Paragraph>(elements[eind]);
            assert(eind <= page.end.element);
            if(eind == page.end.element) {
                // Last thing on this page.
                while(lind < page.end.line) {
                    draw_textline(y, lind == 0 ? indent : 0.0, lind == p.lines.size() - 1);
                    y += line_height;
                    ++lind;
                }
                eind = page.end.element;
                lind = page.end.line;
            } else {
                while(lind < p.lines.size()) {
                    draw_textline(y, lind == 0 ? indent : 0.0, lind == p.lines.size() - 1);
                    y += line_height;
                    if(++lind >= p.lines.size()) {
                        ++eind;
                        lind = 0;
                        break;
                    }
                }
            }
        }
        draw_page_number();
    }

    void print_stats(const std::vector<PaginationData> &pages) {
        assert(!pages.empty());
        int page_num = 0;
        for(const auto &page : pages) {
            ++page_num;
            if(page.s.has_widow) {
                printf("%d: widow line.\n", page_num);
            }
            if(page.s.has_orphan) {
                printf("%d: orphan line.\n", page_num);
            }
            if(page_num > 1 && page_num != (int32_t)pages.size() && page_num % 2 == 1 &&
               pages[page_num - 1].s.height_delta != 0) {
                printf("%d: spread height difference.\n", page_num);
            }
            if(page.c.num_lines != line_target && page_num != (int32_t)pages.size()) {
                printf("%d: line target not met.\n", page_num);
            }
        }
        if(pages.back().c.num_lines == 1) {
            printf("%d: last page only has one line.\n", page_num);
        }
    }

    void create() {
        auto elements = create_document();
        PageSplitter splitter(elements, line_target);
        auto pages = splitter.split_to_pages();
        print_stats(pages);
        for(const auto &current : pages) {
            draw_textbox();
            draw_page(elements, current.c);
            cairo_show_page(cr);
            ++page_number;
        }
    }

    void draw_page_number() {
        const double line_width = 20;
        const double pointsize = 12;
        const double yref = h / 2;
        const double texty = yref + pointsize / 2.5;
        const double linex = page_number % 2 ? w - line_width : -line_width;
        const double line_length = 2 * line_width;
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, line_width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, linex, yref);
        cairo_rel_line_to(cr, line_length, 0);
        cairo_stroke(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        const double textx = page_number % 2 ? w - line_width : line_width / 4; // FIXME, alignment
        cairo_move_to(cr, textx, texty);
        cairo_select_font_face(cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, pointsize);
        char buf[128];
        snprintf(buf, 128, "%d", page_number);
        cairo_show_text(cr, buf);
        cairo_restore(cr);
    }

    void draw_textline(double y, double indent, bool is_last_line) {
        const double left = left_margin();
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        const double last_line_multiplier = is_last_line ? 0.8 : 1.0;
        cairo_rectangle(cr,
                        left + indent,
                        y,
                        last_line_multiplier * (textblock_width - indent),
                        0.7 * line_height);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    double left_margin() const { return (page_number % 2 == 1) ? inner : outer; }

    void draw_textbox() {
        double left = left_margin();
        // double right = (page % 2 == 1) ? outer : inner;
        cairo_save(cr);
        cairo_set_line_width(cr, 1);
        cairo_rectangle(cr, left, top, textblock_width, textblock_height);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

private:
    cairo_t *cr;
    cairo_surface_t *surf;

    double w, h;
    double inner, outer, top, bottom;
    double textblock_width, textblock_height;
    double line_height;
    int32_t line_target;
    int32_t page_number;
};

int main() {
    Paginator p;
    p.create();
    return 0;
}
