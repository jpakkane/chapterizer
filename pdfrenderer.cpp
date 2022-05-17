#include "pdfrenderer.hpp"
#include <utils.hpp>

#include <cairo-pdf.h>
#include <cassert>

#include <textshaper.hpp>
#include <algorithm>

static TextShaper hack;
#include <sstream>

namespace {

std::vector<std::string> hack_split(const std::string &in_text) {
    std::string text;
    text.reserve(in_text.size());
    for(size_t i = 0; i < in_text.size(); ++i) {
        if(in_text[i] == '\n') {
            text.push_back(' ');
        } else {
            text.push_back(in_text[i]);
        }
    }
    std::string val;
    const char separator = ' ';
    std::vector<std::string> words;
    std::stringstream sstream(text);
    while(std::getline(sstream, val, separator)) {
        words.push_back(val);
    }
    return words;
}
} // namespace

PdfRenderer::PdfRenderer(const char *ofname) {
    surf = cairo_pdf_surface_create(ofname, 595, 842);
    cr = cairo_create(surf);
    layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc;
    desc = pango_font_description_from_string("Gentium");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    // Can't use scale to draw in millimeters because it also scales text size.
    // cairo_scale(cr, 595.0 / 21.0, 595.0 / 21.0);

    //    cairo_select_font_face(cr, "Gentium", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    //    cairo_set_font_size(cr, 10.0);
}

PdfRenderer::~PdfRenderer() {
    g_object_unref(G_OBJECT(layout));
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

void PdfRenderer::render(const std::vector<std::string> &lines) {
    draw_grid();
    const double line_height = 11.0;
    const double target_width_pt = mm2pt(50.0);
    cairo_set_source_rgb(cr, 0, 0, 0);
    for(size_t i = 0; i < lines.size(); ++i) {
        if(i < lines.size() - 1) {
            render_line(lines[i], mm2pt(20), mm2pt(20) + i * line_height);
        } else {
            render_line_as_is(lines[i].c_str(), mm2pt(20), mm2pt(20) + i * line_height);
        }
        render_line_as_is(lines[i].c_str(), mm2pt(80), mm2pt(20.0) + i * line_height);
    }
    cairo_set_source_rgb(cr, 0, 0, 1.0);
    cairo_set_line_width(cr, 0.5);
    const double box_height = line_height * lines.size();
    draw_box(mm2pt(20), mm2pt(20), target_width_pt, box_height);
    draw_box(mm2pt(80), mm2pt(20), target_width_pt, box_height);

    temp();
}

void PdfRenderer::draw_grid() {
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 0.1);
    for(double x = 0.0; x < mm2pt(210); x += mm2pt(10)) {
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, mm2pt(300));
    }
    for(double y = 0.0; y < mm2pt(300); y += mm2pt(10)) {
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, mm2pt(210), y);
    }
    cairo_stroke(cr);
}

void PdfRenderer::draw_box(double x, double y, double w, double h) {
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x + w, y);
    cairo_line_to(cr, x + w, y + h);
    cairo_line_to(cr, x, y + h);
    cairo_close_path(cr);
    cairo_stroke(cr);
}

void PdfRenderer::temp() {
    cairo_set_source_rgb(cr, 0, 0, 0);

    printf("\n'space ship' %.2f\n", hack.text_width("space ship"));
    printf("'space' %.2f\n", hack.text_width("space"));
    printf("'space ' %.2f\n", hack.text_width("space "));
    printf("'space ' + 'ship' %.2f\n", hack.text_width("space ") + hack.text_width("ship"));
    printf("'space' + ' '  + 'ship' %.2f\n",
           hack.text_width("space") + hack.text_width(" ") + hack.text_width("ship"));
    render_line_as_is("space ship", 300, 500);

    const char *line2 = "Persian saddle-bags on which he";
    PangoRectangle logical, ink;
    cairo_move_to(cr, mm2pt(20), 411);
    pango_layout_set_justify(layout, FALSE);
    pango_layout_set_text(layout, line2, -1);
    // pango_cairo_update_layout(cr, layout);
    pango_layout_get_extents(layout, &ink, &logical);
    pango_cairo_show_layout(cr, layout);
    cairo_set_line_width(cr, 1 / 2.83);
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    draw_box(mm2pt(20), 411, double(ink.width) / PANGO_SCALE, 11);
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    draw_box(mm2pt(20), 410, double(logical.width) / PANGO_SCALE, 13);
    printf("Line 2:   %.2f\n", hack.text_width(line2));
    printf(" logical: %.2f\n", pt2mm(double(logical.width) / PANGO_SCALE));
    printf("     ink: %.2f\n", pt2mm(double(ink.width) / PANGO_SCALE));
}

void PdfRenderer::render_line(const std::string &line_text, double x, double y) {
    assert(line_text.find('\n') == std::string::npos);
    const auto words = hack_split(line_text);
    const double target_width_pt = mm2pt(50.0);
    double text_width_mm = hack.text_width(line_text.c_str());
    const double text_width_pt = mm2pt(text_width_mm);
    const double num_spaces = std::count(line_text.begin(), line_text.end(), ' ');
    const double space_extra_width =
        num_spaces > 0 ? (target_width_pt - text_width_pt) / num_spaces : 0.0;

    std::string tmp;
    for(size_t i = 0; i < words.size(); ++i) {
        cairo_move_to(cr, x, y);
        PangoRectangle r;

        tmp = words[i];
        tmp += ' ';
        pango_layout_set_text(layout, tmp.c_str(), -1);
        pango_layout_get_extents(layout, nullptr, &r);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        x += double(r.width) / PANGO_SCALE;
        x += space_extra_width;

        /*
        cairo_show_text(cr, words[i].c_str());
        cairo_show_text(cr, " ");
        cairo_get_current_point(cr, &x, &y);
        x += space_extra_width;
        cairo_move_to(cr, x, y);
        */
    }
}

void PdfRenderer::render_line_as_is(const char *line, double x, double y) {
    cairo_move_to(cr, x, y);
    pango_layout_set_text(layout, line, -1);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
}
