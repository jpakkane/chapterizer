#pragma once

#include <pango/pangocairo.h>
#include <string>
// Bad name, should be something like "TextStatisticsCalculator".

class TextShaper {
public:
    TextShaper();
    ~TextShaper();

    double text_width(const char *utf8_text) const;

    double text_width(const std::string &s) const { return text_width(s.c_str()); };

private:
    cairo_t *cr;
    cairo_surface_t *surface;
    PangoLayout *layout;
    PangoFontDescription *desc;
};
