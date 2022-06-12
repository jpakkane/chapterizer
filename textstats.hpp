#pragma once

#include <pango/pangocairo.h>
#include <string>
#include <unordered_map>

class TextStats {
public:
    TextStats(const std::string &font, int fontsize);
    ~TextStats();

    double text_width(const char *utf8_text) const;

    double text_width(const std::string &s) const { return text_width(s.c_str()); };

private:
    cairo_t *cr;
    cairo_surface_t *surface;
    PangoLayout *layout;
    PangoFontDescription *desc;
    mutable std::unordered_map<std::string, double> widths;
};
