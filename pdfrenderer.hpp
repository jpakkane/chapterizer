/*
 * Copyright 2022 Jussi Pakkanen
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

#pragma once

#include <chaptercommon.hpp>
#include <utils.hpp>

#include <cairo.h>
#include <pango/pangocairo.h>

#include <vector>
#include <string>

enum class TextAlignment : int {
    Left,
    Centered,
    Right,
    // Justified is stored in a separate struct.
};

struct ImageInfo {
    cairo_surface_t *surf;
    int w, h;
};

struct Coord {
    Length x;
    Length y;
};

class PdfRenderer {
public:
    explicit PdfRenderer(
        const char *ofname, Length pagew, Length pageh, const char *title, const char *author);
    ~PdfRenderer();

    void render_line_justified(const std::string &text,
                               const FontParameters &par,
                               Length line_width_mm,
                               Length x,
                               Length y);

    void render_line_justified(const std::vector<std::string> &markup_words,
                               const FontParameters &par,
                               Length line_width,
                               Length x,
                               Length y);

    void render_text_as_is(const char *line, const FontParameters &par, Length x, Length y);

    void render_markup_as_is(
        const char *line, const FontParameters &par, Length x, Length y, TextAlignment alignment);
    void render_markup_as_is(const std::vector<std::string> markup_words,
                             const FontParameters &par,
                             Length x,
                             Length y,
                             TextAlignment alignment);
    void render_line_centered(const char *line, const FontParameters &par, Length x, Length y);

    void new_page();
    int page_num() const { return pages; }

    void draw_box(Length x, Length y, Length w, Length h, Length thickness);
    void fill_box(Length x, Length y, Length w, Length h, double color);

    void draw_line(Length x0, Length y0, Length x1, Length y1, Length thickness);

    ImageInfo get_image(const std::string &path);

    void draw_image(const ImageInfo &image, Length x, Length y, Length w, Length h);

    void draw_dash_line(const std::vector<Coord> &points);

    void draw_arc(Length x, Length y, Length r, double angle1, double angle2, Length thickness);

    void draw_poly_line(const std::vector<Coord> &points, Length thickness);

    void add_section_outline(int section_number, const std::string &text);

private:
    void draw_grid();
    void setup_pango(const FontParameters &par);

    int pages = 1;
    cairo_t *cr;
    cairo_surface_t *surf;
    PangoLayout *layout;
    std::unordered_map<std::string, cairo_surface_t *> loaded_images;
    std::string outname;
};
