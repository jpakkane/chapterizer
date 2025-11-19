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
#include <hbfontcache.hpp>
#include <hbmeasurer.hpp>

#include <capypdf.hpp>

#include <filesystem>
#include <vector>
#include <string>

struct CapyImageInfo {
    CapyPDF_ImageId id;
    int w, h;
};

class CapyPdfRenderer {
public:
    explicit CapyPdfRenderer(const char *ofname,
                             Length pagew_,
                             Length pageh_,
                             Length bleed,
                             const capypdf::DocumentProperties &docprop,
                             HBFontCache &fc_);
    ~CapyPdfRenderer();

    void render_line_justified(
        const HBLine line, const HBTextParameters &par, Length line_width_mm, Length x, Length y);

    void render_text_as_is(const char *line, const HBTextParameters &par, Length x, Length y);
    void render_text_as_is(
        const char *line, const HBTextParameters &par, Length x, Length y, TextAlignment align);
    void render_text(
        const char *line, const HBTextParameters &par, Length x, Length y, TextAlignment alignment);

    void render_runs(const std::vector<HBRun> &runs, Length x, Length y, TextAlignment alignment);
    void render_run(const HBRun &runs, Length x, Length y);

    void render_wonky_text(const char *text,
                           const HBTextParameters &par,
                           Length raise,
                           Length shift,
                           double tilt,
                           double color,
                           Length x,
                           Length y);

    void new_page();
    int page_num() const { return pages; }

    void draw_box(Length x, Length y, Length w, Length h, double g, Length thickness);
    void fill_box(Length x, Length y, Length w, Length h, double color);

    void fill_rounded_corner_box(Length x, Length y, Length w, Length h, double color);

    void draw_line(Length x0, Length y0, Length x1, Length y1, Length thickness);

    void draw_line(Length x0,
                   Length y0,
                   Length x1,
                   Length y1,
                   Length thickness,
                   double g,
                   CapyPDF_Line_Cap cap);

    CapyImageInfo get_image(const std::filesystem::path &path);

    void draw_image(const CapyImageInfo &image, Length x, Length y, Length w, Length h);

    void draw_dash_line(const std::vector<Coord> &points, double line_width);

    void draw_poly_line(const std::vector<Coord> &points, Length thickness);

    void add_section_outline(int section_number, const std::string &text);

    void init_page();
    void finalize_page();

private:
    void draw_grid();
    void draw_cropmarks();

    void serialize_single_run(const HBRun &run, capypdf::Text &tobj, hb_buffer_t *buf);

    CapyPDF_FontId hbfont2capyfont(const HBTextParameters &par, const FontInfo &fontinfo);

    capypdf::Generator capygen;
    capypdf::DrawContext ctx;
    int pages = 1;
    double bleed;
    double pagew, pageh;
    double mediaw, mediah;
    std::unordered_map<hb_font_t *, CapyPDF_FontId> loaded_fonts;
    std::unordered_map<std::filesystem::path, CapyImageInfo> loaded_images;
    std::string outname;
    HBFontCache &fc;
    HBMeasurer meas;
};
