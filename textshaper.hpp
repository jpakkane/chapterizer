#pragma once

#include <pango/pangocairo.h>

// Bad name, should be something like "TextStatisticsCalculator".

class TextShaper {
public:
    TextShaper();
    ~TextShaper();

    double text_width(const char *utf8_text) const;

private:
    cairo_t *cr;
    cairo_surface_t *surface;
    PangoLayout *layout;
    PangoFontDescription *desc;
};
