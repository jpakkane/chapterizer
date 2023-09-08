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
    es.emplace_back(dummy_paragraph(3));
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(2));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(6));
    es.emplace_back(dummy_paragraph(1));
    es.emplace_back(dummy_paragraph(10));
    es.emplace_back(dummy_paragraph(4));
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

    void create() {
        draw_textbox();
        auto elements = create_document();
        const double indent = 30;
        double y = top;
        for(const auto &e : elements) {
            const auto &p = std::get<Paragraph>(e);
            draw_textlines(y, p.lines.size(), indent);
            draw_page_number();
            y += p.lines.size() * line_height;
        }
    }

    void draw_page_number() {
        const double line_width = 24;
        const double pointsize = 12;
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_set_line_width(cr, line_width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, -line_width, h / 2);
        cairo_rel_line_to(cr, 2 * line_width, 0);
        cairo_stroke(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        cairo_move_to(cr, line_width / 2 - pointsize / 2.5, h / 2 + pointsize / 2.5);
        cairo_select_font_face(cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, pointsize);
        cairo_show_text(cr, "888");
        cairo_restore(cr);
    }

    void draw_textlines(double y, int32_t num_lines, double first_line_indent) {
        const double left = left_margin();
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        for(int32_t i = 0; i < num_lines; ++i) {
            const double indent = i == 0 ? first_line_indent : 0;
            const double last_line_multiplier = i + 1 == num_lines ? 0.8 : 1.0;
            cairo_rectangle(cr,
                            left + indent,
                            y + i * line_height,
                            last_line_multiplier * (textblock_width - indent),
                            0.7 * line_height);
            cairo_fill(cr);
        }
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
