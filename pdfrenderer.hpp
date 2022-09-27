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

struct ImageInfo {
    cairo_surface_t *surf;
    int w, h;
};

class PdfRenderer {
public:
    explicit PdfRenderer(const char *ofname, Point pagew, Point pageh);
    ~PdfRenderer();

    void render_line_justified(const std::string &text,
                               const FontParameters &par,
                               Millimeter line_width_mm,
                               Point x,
                               Point y);

    void render_line_justified(const std::vector<std::string> &markup_words,
                               const FontParameters &par,
                               Millimeter line_width,
                               Point x,
                               Point y);

    void render_text_as_is(const char *line, const FontParameters &par, Point x, Point y);

    void render_markup_as_is(const char *line, const FontParameters &par, Point x, Point y);
    void render_markup_as_is(const std::vector<std::string> markup_words,
                             const FontParameters &par,
                             Point x,
                             Point y);
    void render_line_centered(const char *line, const FontParameters &par, Point x, Point y);

    void new_page();
    int page_num() const { return pages; }

    void draw_box(Point x, Point y, Point w, Point h);

    void draw_line(Point x0, Point y0, Point x1, Point y1, Point thickness);

    ImageInfo get_image(const std::string &path);

    void draw_image(const ImageInfo &image, Millimeter x, Millimeter y, Millimeter w, Millimeter h);

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
