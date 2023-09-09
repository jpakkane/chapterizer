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
    // std::vector<size_t> images;
    // std::something footnotes;
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
        line_target = 34;

        assert(int32_t(textblock_height / line_height) == line_target);

        page = 1;
        surf = cairo_pdf_surface_create("paginationtest.pdf", w, h);
        cr = cairo_create(surf);
    }

    ~Paginator() {
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }

    std::vector<TextLoc> split_to_pages(const std::vector<Element> &elements) {
        std::vector<TextLoc> splits;
        int num_lines = 0;
        for(size_t eind = 0; eind < elements.size(); ++eind) {
            const auto &e = elements[eind];
            const auto &p = std::get<Paragraph>(e);
            for(size_t lind = 0; lind < p.lines.size(); ++lind) {
                if(num_lines >= line_target) {
                    splits.emplace_back(TextLoc{eind, lind});
                    num_lines = 0;
                }
                ++num_lines;
            }
        }
        if(num_lines > 0) {
            splits.emplace_back(
                TextLoc{elements.size() - 1, std::get<Paragraph>(elements.back()).lines.size()});
        }
        return splits;
    }

    void draw_page(const std::vector<Element> &elements, const TextLoc &start, const TextLoc &end) {
        const double indent = 30;
        double y = top;
        size_t eind = start.element;
        size_t lind = start.line;
        while(eind <= end.element) {
            if(eind == end.element && lind == end.line) {
                break;
            }
            const auto &p = std::get<Paragraph>(elements[eind]);
            assert(eind <= end.element);
            if(eind == end.element) {
                // Last thing on this page.
                while(lind < end.line) {
                    draw_textline(y, lind == 0 ? indent : 0.0, lind == p.lines.size() - 1);
                    y += line_height;
                    ++lind;
                }
                eind = end.element;
                lind = end.line;
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

    void print_stats(const std::vector<Element> &elements,
                     const std::vector<TextLoc> &splitpoints) {
        TextLoc previous{0, 0};
        int page_num = 1;
        for(const auto &next : splitpoints) {
            const auto &prev_el = std::get<Paragraph>(elements[previous.element]);
            const auto &next_el = std::get<Paragraph>(elements[next.element]);
            if(previous.line == prev_el.lines.size() - 1 && prev_el.lines.size() > 1) {
                printf("%d: widow line.\n", page_num);
            }
            if(next.line == 1 && next_el.lines.size() > 1) {
                printf("%d: orphan line.\n", page_num);
            }

            ++page_num;
            previous = next;
        }
    }

    void create() {
        auto elements = create_document();
        auto splitpoints = split_to_pages(elements);
        print_stats(elements, splitpoints);
        draw_textbox();
        TextLoc previous{0, 0};
        for(const auto &current : splitpoints) {
            draw_page(elements, previous, current);
            cairo_show_page(cr);
            ++page;
            previous = current;
        }
    }

    void draw_page_number() {
        const double line_width = 20;
        const double pointsize = 12;
        const double yref = h / 2;
        const double texty = yref + pointsize / 2.5;
        const double linex = page % 2 ? w - line_width : -line_width;
        const double line_length = 2 * line_width;
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, line_width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, linex, yref);
        cairo_rel_line_to(cr, line_length, 0);
        cairo_stroke(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        const double textx = page % 2 ? w - line_width : line_width / 4; // FIXME, alignment
        cairo_move_to(cr, textx, texty);
        cairo_select_font_face(cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, pointsize);
        char buf[128];
        snprintf(buf, 128, "%d", page);
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

    double left_margin() const { return (page % 2 == 1) ? inner : outer; }

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
    int32_t page;
};

int main() {
    Paginator p;
    p.create();
    return 0;
}
