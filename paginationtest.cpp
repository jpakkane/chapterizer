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
namespace {

double mm2pt(const double x) { return x * 2.8346456693; }

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
        draw_textlines(top, 3, 30);
    }

    void draw_textlines(double y, int32_t num_lines, double first_line_indent) {
        const double left = left_margin();
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        for(int32_t i = 0; i < num_lines; ++i) {
            const double indent = i == 0 ? first_line_indent : 0;
            cairo_rectangle(cr,
                            left + indent,
                            y + i * line_height,
                            textblock_width - indent,
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
    int32_t page;
};

int main() {
    Paginator p;
    p.create();
    return 0;
}
