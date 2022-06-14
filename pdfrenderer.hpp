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

#include <cairo.h>
#include <pango/pangocairo.h>

#include <vector>
#include <string>

class PdfRenderer {
public:
    explicit PdfRenderer(const char *ofname);
    ~PdfRenderer();

    void render(const std::vector<std::string> &lines, const double target_width_mm);

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
