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
#include <optional>

struct Paragraph {
    std::vector<std::string> lines;
    double indent = 30.0;
    double narrowing = 0.0;
};

struct Heading {
    std::vector<std::string> lines;
};

struct Figure {
    double h;
    std::string text;
};

struct EmptyLine {};

typedef std::variant<Paragraph, Heading, Figure, EmptyLine> Element;

size_t lines_in_element(const Element &e) {
    if(std::holds_alternative<Paragraph>(e)) {
        return std::get<Paragraph>(e).lines.size();
    }
    if(std::holds_alternative<EmptyLine>(e)) {
        return 1;
    }
    std::abort();
}

struct PageLoc {
    size_t element;
    size_t line;
};

struct PageContent {
    PageLoc start;
    PageLoc end;
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
    double different_spread = 5;
    double missing_target = 1;
    double last_page_widow = 80;
};

struct PotentialSplits {
    std::optional<PageLoc> one_less;
    PageLoc locally_optimal;
    int32_t optimal_lines;
    std::optional<PageLoc> one_more;
};

namespace {

double mm2pt(const double x) { return x * 2.8346456693; }

Paragraph dummy_paragraph(int32_t num_lines, double narrowing = 0.0) {
    Paragraph p;
    p.narrowing = narrowing;
    if(narrowing > 0) {
        p.indent = 0;
    }
    for(int32_t i = 0; i < num_lines; ++i) {
        p.lines.emplace_back("");
    }
    return p;
}

std::vector<Element> create_basic_chapter() {
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
    es.emplace_back(EmptyLine{});
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
    es.emplace_back(EmptyLine{});
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
    es.emplace_back(EmptyLine{});
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

std::vector<Element> create_quote_chapter() {
    std::vector<Element> es;
    es.emplace_back(dummy_paragraph(10));
    es.emplace_back(dummy_paragraph(15));
    es.emplace_back(dummy_paragraph(12));
    es.emplace_back(dummy_paragraph(10));
    es.emplace_back(dummy_paragraph(10));
    es.emplace_back(EmptyLine{});
    es.emplace_back(dummy_paragraph(1, 0.2));
    es.emplace_back(EmptyLine{});
    es.emplace_back(dummy_paragraph(3, 0.2));
    es.emplace_back(EmptyLine{});
    es.emplace_back(dummy_paragraph(2, 0.2));
    es.emplace_back(EmptyLine{});
    es.emplace_back(dummy_paragraph(1, 0.2));
    es.emplace_back(EmptyLine{});
    es.emplace_back(dummy_paragraph(1, 0.2));
    es.emplace_back(EmptyLine{});
    es.emplace_back(dummy_paragraph(5));
    es.emplace_back(dummy_paragraph(10));
    es.emplace_back(dummy_paragraph(13));
    return es;
}

std::vector<Element> create_test_chapter() {
    std::vector<Element> es;
    es.emplace_back(dummy_paragraph(40));
    return es;
}

std::vector<std::vector<Element>> create_document() {
    std::vector<std::vector<Element>> chapters;
    chapters.emplace_back(create_basic_chapter());
    chapters.emplace_back(create_quote_chapter());
    chapters.emplace_back(create_test_chapter());
    return chapters;
}

} // namespace

class PageSplitter {
public:
    PageSplitter(const std::vector<Element> &e, int32_t target)
        : elements(e), line_target{target} {}

    std::vector<PaginationData> split_to_pages() {
        std::vector<PaginationData> pages;

        split_recursive(pages);
        return std::move(best_split);
    }

private:
    PotentialSplits compute_splits(const PageLoc &from) {
        PotentialSplits splits;
        int num_lines = 0;
        size_t lind, eind;
        bool first_line_on_page = true;
        for(eind = from.element; eind < elements.size(); ++eind) {
            const auto &e = elements[eind];
            const auto lind_start = eind == from.element ? from.line : 0;
            const auto element_lines = lines_in_element(e);
            for(lind = lind_start; lind < element_lines; ++lind) {
                if(first_line_on_page && std::holds_alternative<EmptyLine>(e)) {
                    // Whitespace at the beginning of the page does not count.
                } else {
                    if(num_lines > line_target) {
                        splits.one_more = PageLoc{eind, lind};
                        return splits;
                    } else if(num_lines == line_target) {
                        splits.locally_optimal = PageLoc{eind, lind};
                        splits.optimal_lines = num_lines;
                    } else {
                        splits.one_less = PageLoc(eind, lind);
                    }
                    ++num_lines;
                }
                first_line_on_page = false;
            }
        }
        // The last paragraph overflowed the page
        splits.one_less.reset();
        splits.locally_optimal = PageLoc{eind - 1, lind};
        splits.one_more.reset();
        return splits;
    }

    void descend(std::vector<PaginationData> &pages,
                 const PageLoc &split_point,
                 const int32_t optimal_lines,
                 const PageLoc &previous) {
        auto page_stats = compute_page_stats(previous, split_point, optimal_lines);
        pages.emplace_back(std::move(page_stats));
        compute_interpage_stats(pages);
        const auto split_point_lines = lines_in_element(elements[split_point.element]);
        if(split_point.element < elements.size() && split_point.line < split_point_lines) {
            auto current_penalty = compute_penalties(pages, false);
            if(current_penalty < best_penalty) {
                split_recursive(pages);
            }
        } else {
            const double total_penalty = compute_penalties(pages, true);
            if(total_penalty < best_penalty) {
                best_penalty = total_penalty;
                best_split = pages;
            }
        }
        pages.pop_back();
    }

    void split_recursive(std::vector<PaginationData> &pages) {
        PageLoc previous{0, 0};
        const auto num_pages = pages.size();
        if(!pages.empty()) {
            previous = pages.back().c.end;
        }
        const auto potentials = compute_splits(previous);
        descend(pages, potentials.locally_optimal, potentials.optimal_lines, previous);
        assert(pages.size() == num_pages);
        if(potentials.one_less) {
            descend(pages, *potentials.one_less, potentials.optimal_lines - 1, previous);
        }
        if(potentials.one_more) {
            descend(pages, *potentials.one_more, potentials.optimal_lines + 1, previous);
        }
        assert(pages.size() == num_pages);
    }

    double compute_penalties(const std::vector<PaginationData> &pages, bool is_complete) const {
        assert(!pages.empty());
        double total_penalty = 0;
        int page_num = 0;
        for(const auto &page : pages) {
            ++page_num;
            if(page.s.has_widow) {
                total_penalty += penalties.widow;
            }
            if(page.s.has_orphan) {
                total_penalty += penalties.orphan;
            }
            if(page_num > 1 && page_num != (int32_t)pages.size() && page_num % 2 == 1) {
                total_penalty +=
                    penalties.different_spread * abs(pages[page_num - 1].s.height_delta);
            }
            if(page.c.num_lines != line_target && page_num != (int32_t)pages.size() &&
               page_num != 1) {
                total_penalty += penalties.missing_target;
            }
        }
        if(is_complete && pages.back().c.num_lines == 1) {
            total_penalty += penalties.last_page_widow;
        }
        return total_penalty;
    }

    PaginationData
    compute_page_stats(const PageLoc &from, const PageLoc &to, int32_t num_lines) const {
        PaginationData pd;
        pd.c.start = from;
        pd.c.end = to;
        pd.c.num_lines = num_lines;
        const auto from_el_lines = lines_in_element(elements[from.element]);
        const auto to_el_lines = lines_in_element(elements[to.element]);
        if(from.line == from_el_lines - 1 && from_el_lines > 1) {
            pd.s.has_widow = true;
        }
        if(to.line == 1 && to_el_lines > 1) {
            pd.s.has_orphan = true;
        }

        return pd;
    }

    void compute_interpage_stats(std::vector<PaginationData> &pages) const {
        for(size_t i = 1; i < pages.size(); ++i) {
            pages[i].s.height_delta =
                int32_t(pages[i].c.num_lines) - int32_t(pages[i - 1].c.num_lines);
        }
    }

    LayoutPenalties penalties;
    const std::vector<Element> &elements;
    const int32_t line_target;

    std::vector<PaginationData> best_split;
    double best_penalty = 1e100;
};

class DraftPaginator {
public:
    DraftPaginator() {
        w = mm2pt(135);
        h = mm2pt(210);
        inner = mm2pt(20);
        outer = mm2pt(15);
        top = mm2pt(15);
        bottom = mm2pt(25);
        bleed = mm2pt(20);

        textblock_width = w - inner - outer;
        textblock_height = h - top - bottom;

        line_height = 14;
        line_target = textblock_height / line_height;

        assert(int32_t(textblock_height / line_height) == line_target);

        page_number = 1;
        surf = cairo_pdf_surface_create("paginationtest.pdf", w + 2 * bleed, h + 2 * bleed);
        cr = cairo_create(surf);
    }

    ~DraftPaginator() {
        cairo_destroy(cr);
        cairo_surface_destroy(surf);
    }

    double compute_indent(const std::vector<Element> &elements, size_t eind, size_t lind) const {
        if(eind == 0 && lind == 0) {
            return 0;
        }
        if(eind > 0 && lind == 0 && std::holds_alternative<EmptyLine>(elements[eind - 1])) {
            return 0;
        }
        if(!std::holds_alternative<Paragraph>(elements[eind])) {
            return 0;
        }
        return lind == 0 ? std::get<Paragraph>(elements[eind]).indent : 0.0;
    }

    void draw_page(const std::vector<Element> &elements, const PageContent &page) {
        double y = top;
        size_t eind = page.start.element;
        size_t lind = page.start.line;
        bool first_line = true;
        while(eind <= page.end.element) {
            if(eind == page.end.element && lind == page.end.line) {
                break;
            }
            const auto &e = elements[eind];
            if(first_line) {
                // Whitespace at the beginning of the page is not rendered.
                first_line = false;
                if(std::holds_alternative<EmptyLine>(e)) {
                    ++eind;
                    continue;
                }
            }
            if(std::holds_alternative<Paragraph>(e)) {
                const auto &p = std::get<Paragraph>(e);
                assert(eind <= page.end.element);
                if(eind == page.end.element) {
                    // Last thing on this page.
                    while(lind < page.end.line) {
                        const auto current_indent = compute_indent(elements, eind, lind);
                        draw_textline(y, current_indent, lind == p.lines.size() - 1, p.narrowing);
                        y += line_height;
                        ++lind;
                    }
                    eind = page.end.element;
                    lind = page.end.line;
                } else {
                    while(lind < p.lines.size()) {
                        const auto current_indent = compute_indent(elements, eind, lind);
                        draw_textline(y, current_indent, lind == p.lines.size() - 1, p.narrowing);
                        y += line_height;
                        if(++lind >= p.lines.size()) {
                            ++eind;
                            lind = 0;
                            break;
                        }
                    }
                }
            } else if(std::holds_alternative<EmptyLine>(e)) {
                y += line_height;
                ++eind;
                lind = 0;
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
            if(page.c.num_lines != line_target && page_num != (int32_t)pages.size() &&
               page_num != 1) {
                printf("%d: line target not met.\n", page_num);
            }
        }
        if(pages.back().c.num_lines == 1) {
            printf("%d: last page only has one line.\n", page_num);
        }
    }

    void create() {
        // auto elements = create_basic_chapter();
        auto chapters = create_document();
        num_chapters = (int32_t)chapters.size();
        chapter_number = 0;
        for(const auto &elements : chapters) {
            if(page_number > 1 && page_number % 2 == 0) {
                cairo_show_page(cr);
                ++page_number;
            }
            PageSplitter splitter(elements, line_target);
            auto pages = splitter.split_to_pages();
            print_stats(pages);
            for(const auto &current : pages) {
                cairo_save(cr);
                draw_trims();
                cairo_translate(cr, bleed, bleed);
                draw_textbox();
                draw_cross();
                draw_page(elements, current.c);
                cairo_restore(cr);
                cairo_show_page(cr);
                ++page_number;
            }
            ++chapter_number;
        }
    }

    double y_offset_for_tab(const double line_width) {
        const double total_height = num_chapters * line_width;
        return h / 2 - total_height / 2 + 0.5 * line_width + chapter_number * line_width;
    }

    void draw_page_number() {
        const double line_width = 20;
        const double pointsize = 10;
        const double yref = y_offset_for_tab(line_width);
        const double texty = yref + pointsize / 2.9;
        const double line_center_x = page_number % 2 ? w : 0;
        const double line_length = 2.8 * line_width;
        const double corner = 0.2 * line_width;

        const double line_top = yref - line_width / 2;
        const double line_bottom = yref + line_width / 2;
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
        const auto line_left = line_center_x - line_length / 2;
        const auto line_right = line_center_x + line_length / 2;
        cairo_move_to(cr, line_left + corner, line_top);
        cairo_curve_to(cr,
                       line_left + 0.5 * corner,
                       line_top,
                       line_left,
                       line_top + 0.5 * corner,
                       line_left,
                       line_top + corner);
        cairo_line_to(cr, line_left, line_bottom - corner);
        cairo_curve_to(cr,
                       line_left,
                       line_bottom - 0.5 * corner,
                       line_left + 0.5 * corner,
                       line_bottom,
                       line_left + corner,
                       line_bottom);
        cairo_line_to(cr, line_right - corner, line_bottom);
        cairo_curve_to(cr,
                       line_right - 0.5 * corner,
                       line_bottom,
                       line_right,
                       line_bottom - 0.5 * corner,
                       line_right,
                       line_bottom - corner);
        cairo_line_to(cr, line_right, line_top + corner);
        cairo_curve_to(cr,
                       line_right,
                       line_top + 0.5 * corner,
                       line_right - 0.5 * corner,
                       line_top,
                       line_right - corner,
                       line_top);

        cairo_close_path(cr);
        cairo_fill(cr);
        cairo_set_source_rgb(cr, 1, 1, 1);
        const double textx =
            page_number % 2 ? w - 1.2 * line_width : line_width / 4; // FIXME, alignment
        cairo_move_to(cr, textx, texty);
        cairo_select_font_face(cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, pointsize);
        char buf[128];
        snprintf(buf, 128, "%d", 100 + page_number);
        cairo_text_path(cr, buf);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 0.2);
        cairo_set_source_rgb(cr, 0, 0, 0);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    void draw_textline(double y, double indent, bool is_last_line, double narrowing) {
        const double left = left_margin();
        const double narrow_width = narrowing * textblock_width;
        cairo_save(cr);
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        const double last_line_multiplier = is_last_line ? 0.8 : 1.0;
        cairo_rectangle(cr,
                        left + indent + narrow_width,
                        y,
                        last_line_multiplier * (textblock_width - indent - 2 * narrow_width),
                        0.7 * line_height);
        cairo_fill(cr);
        cairo_restore(cr);
    }

    double left_margin() const { return (page_number % 2 == 1) ? inner : outer; }

    void draw_trims() {
        cairo_move_to(cr, bleed, 0);
        cairo_rel_line_to(cr, 0, bleed / 2);
        cairo_move_to(cr, w + bleed, 0);
        cairo_rel_line_to(cr, 0, bleed / 2);
        cairo_move_to(cr, bleed, h + 2 * bleed);
        cairo_rel_line_to(cr, 0, -bleed / 2);
        cairo_move_to(cr, w + bleed, h + 2 * bleed);
        cairo_rel_line_to(cr, 0, -bleed / 2);

        cairo_move_to(cr, 0, bleed);
        cairo_rel_line_to(cr, bleed / 2, 0);
        cairo_move_to(cr, w + 2 * bleed, bleed);
        cairo_rel_line_to(cr, -bleed / 2, 0);
        cairo_move_to(cr, 0, h + bleed);
        cairo_rel_line_to(cr, bleed / 2, 0);
        cairo_move_to(cr, w + 2 * bleed, h + bleed);
        cairo_rel_line_to(cr, -bleed / 2, 0);

        cairo_stroke(cr);

        cairo_save(cr);
        cairo_set_line_width(cr, 0.5);
        cairo_rectangle(cr, bleed, bleed, w, h);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    void draw_textbox() {
        double left = left_margin();
        // double right = (page % 2 == 1) ? outer : inner;
        cairo_save(cr);
        cairo_set_line_width(cr, 1);
        cairo_rectangle(cr, left, top, textblock_width, textblock_height);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    void draw_cross() {
        cairo_save(cr);
        cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, w / 2, 0);
        cairo_rel_line_to(cr, 0, h);
        cairo_move_to(cr, 0, h / 2);
        cairo_rel_line_to(cr, w, 0);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

private:
    cairo_t *cr;
    cairo_surface_t *surf;

    double w, h, bleed;
    double inner, outer, top, bottom;
    double textblock_width, textblock_height;
    double line_height;
    int32_t line_target;
    int32_t page_number; // 1-based
    int32_t chapter_number;
    int32_t num_chapters;
};

int main() {
    DraftPaginator p;
    p.create();
    return 0;
}
