#pragma once

#include <cairo.h>
#include <pango/pangocairo.h>

#include <vector>
#include <string>

class PdfRenderer {
public:
    explicit PdfRenderer(const char *ofname);
    ~PdfRenderer();

    void render(const std::vector<std::string> &lines);

private:
    void temp();
    void render_line(const std::string &text, double x, double y);
    void render_line_as_is(const char *line, double x, double y);

    void draw_box(double x, double y, double w, double h);
    void draw_grid();

    cairo_t *cr;
    cairo_surface_t *surf;
    PangoLayout *layout;
};
