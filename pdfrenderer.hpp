#pragma once

#include <cairo.h>

#include <vector>
#include <string>

class PdfRenderer {
public:
    explicit PdfRenderer(const char *ofname);
    ~PdfRenderer();

    void render(const std::vector<std::string> &lines);

private:
    cairo_t *cr;
    cairo_surface_t *surf;
};
