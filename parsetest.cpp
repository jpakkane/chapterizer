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

#include <pango/pangocairo.h>
#include <cairo-pdf.h>
#include <glib.h>

#include <vector>
#include <string>

#include <clocale>
#include <cassert>

double mm2pt(const double x) { return x * 2.8346456693; }
double pt2mm(const double x) { return x / 2.8346456693; }

const std::vector<std::string> lines{
    {"This chapter is in /italic/."},
    {"The second chapter be *written in bold*."},
    {"The third chapter be `written in typewriter`."},
    {"The fourth chapter be ¤written in Small Caps¤."},
};

#define ITALIC_BIT 1
#define BOLD_BIT (1 << 1)
#define SMALLCAPS_BIT (1 << 2)

enum class LetterFormat : char { None, Italic, Bold, SmallCaps };

struct Formatting {
    size_t offset;
    int formats;
};

struct FormattedWord {
    std::string word;
    std::vector<Formatting> blob;
};

std::vector<std::string> split_to_words(const char *u8string) {
    GRegex *re = g_regex_new(" ", GRegexCompileFlags(0), GRegexMatchFlags(0), nullptr);
    std::vector<std::string> words;
    gchar **parts = g_regex_split(re, u8string, GRegexMatchFlags(0));

    for(int i = 0; parts[i]; ++i) {
        if(parts[i][0]) {
            words.emplace_back(parts[i]);
        }
    }
    g_regex_unref(re);
    return words;
}

int main() {
    setlocale(LC_ALL, "");
    //    cairo_status_t status;
    cairo_surface_t *surface = cairo_pdf_surface_create("parsingtest.pdf", 595, 842);
    cairo_t *cr = cairo_create(surface);
    cairo_save(cr);
    // cairo_set_source_rgb(cr, 1.0, 0.2, 0.1);
    printf("PANGO_SCALE = %d\n", PANGO_SCALE);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc;
    int line_num = -1;
    desc = pango_font_description_from_string("Gentium");
    assert(desc);
    pango_font_description_set_absolute_size(desc, 12 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    for(const auto &line : lines) {
        ++line_num;
        int word_num = -1;
        for(const auto &word : split_to_words(line.c_str())) {
            ++word_num;
            cairo_move_to(cr, 72 + 50 * word_num, 72 + 14 * line_num);
            pango_layout_set_markup(layout, word.c_str(), -1);
            pango_cairo_update_layout(cr, layout);
            pango_cairo_show_layout(cr, layout);
        }
    }

    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    return 0;
}
