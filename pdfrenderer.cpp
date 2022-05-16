#include "pdfrenderer.hpp"
#include <cairo-pdf.h>

PdfRenderer::PdfRenderer(const char *ofname) {
    surf = cairo_pdf_surface_create(ofname, 595, 842);
    cr = cairo_create(surf);

    // Can't use scale to draw in millimeters because it also scales text size.
    // cairo_scale(cr, 595.0 / 21.0, 595.0 / 21.0);

    cairo_select_font_face(cr, "Gentium", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
}

PdfRenderer::~PdfRenderer() {
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

void PdfRenderer::render(const std::vector<std::string> &lines) {
    const double line_height = 11.0;
    const double target_width = 50.0 * 595.0 / 210.0;
    for(size_t i = 0; i < lines.size(); ++i) {
        cairo_move_to(cr, 100.0, 100.0 + (i + 1) * line_height);
        cairo_show_text(cr, lines[i].c_str());
    }
    const double box_height = line_height * lines.size();
    cairo_set_source_rgb(cr, 0, 0, 1.0);
    cairo_set_line_width(cr, 0.5);
    cairo_move_to(cr, 100.0, 100.0);
    cairo_line_to(cr, 100.0 + target_width, 100.0);
    cairo_line_to(cr, 100.0 + target_width, 100.0 + box_height);
    cairo_line_to(cr, 100.0, 100 + box_height);
    cairo_close_path(cr);
    cairo_stroke(cr);
}
