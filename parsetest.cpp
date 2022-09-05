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
    {"Some /italic text/ here."},
    {"The second chapter be *written in bold*."},
    {"The third chapter be `written in typewriter`."},
    {"The fourth chapter be |written in Small Caps|."},
};

#define ITALIC_BIT 1
#define BOLD_BIT (1 << 1)
#define TT_BIT (1 << 2)
#define SMALLCAPS_BIT (1 << 3)

const char italic_char = '/';
const char bold_char = '*';
const char tt_char = '`';
const char smallcaps_char = '|';

enum class LetterFormat : char { None, Italic, Bold, SmallCaps };

struct Formatting {
    size_t offset;
    int formats;
};

struct FormattedWord {
    std::string word;
    std::vector<Formatting> blob;
    int style_bits = 0;
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

struct FormatJiggy {
    std::vector<FormattedWord> formatted_words;
    int enabled_styles = 0;
};

void style_and_append(FormatJiggy &fwords, const std::vector<std::string> in_words) {
    std::string buf;
    int current_style = fwords.enabled_styles;
    for(const auto &word : in_words) {
        buf.clear();
        std::vector<Formatting> changes;
        for(const char c : word) {
            switch(c) {
            case italic_char:
                changes.emplace_back(Formatting{buf.size(), ITALIC_BIT});
                current_style ^= ITALIC_BIT;
                break;
            case bold_char:
                changes.emplace_back(Formatting{buf.size(), BOLD_BIT});
                current_style ^= BOLD_BIT;
                break;
            case tt_char:
                changes.emplace_back(Formatting{buf.size(), TT_BIT});
                current_style ^= TT_BIT;
                break;
            case smallcaps_char:
                changes.emplace_back(Formatting{buf.size(), SMALLCAPS_BIT});
                current_style ^= SMALLCAPS_BIT;
                break;
            default:
                buf.push_back(c);
            }
        }
        int word_start_style = fwords.enabled_styles;
        while(!changes.empty() && changes.front().offset == 0) {
            word_start_style ^= changes.front().formats;
            changes.erase(changes.begin());
        }
        fwords.formatted_words.emplace_back(
            FormattedWord{buf, std::move(changes), word_start_style});
        fwords.enabled_styles = current_style;
    }
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
        FormatJiggy jg;
        style_and_append(jg, split_to_words(line.c_str()));

        int word_num = -1;
        std::string markup_buf;
        for(const auto &word : jg.formatted_words) {
            pango_layout_set_attributes(layout, nullptr);
            ++word_num;
            cairo_move_to(cr, 72 + 50 * word_num, 72 + 14 * line_num);
            if(word.style_bits == 0) {
                pango_layout_set_text(layout, word.word.c_str(), -1);
            } else {
                markup_buf.clear();
                // FIXME, replace < and >.
                if(word.style_bits & ITALIC_BIT) {
                    markup_buf = "<i>";
                    markup_buf += word.word;
                    markup_buf += "</i>";
                    pango_layout_set_markup(layout, markup_buf.c_str(), -1);
                } else if(word.style_bits & BOLD_BIT) {
                    markup_buf = "<b>";
                    markup_buf += word.word;
                    markup_buf += "</b>";
                    pango_layout_set_markup(layout, markup_buf.c_str(), -1);
                }
                if(word.style_bits & TT_BIT) {
                    markup_buf = "<tt>";
                    markup_buf += word.word;
                    markup_buf += "</tt>";
                    pango_layout_set_markup(layout, markup_buf.c_str(), -1);
                }
                if(word.style_bits & SMALLCAPS_BIT) {
                    markup_buf = "<span variant=\"small-caps\" letter_spacing=\"100\">";
                    markup_buf += word.word;
                    markup_buf += "</span>";
                    pango_layout_set_markup(layout, markup_buf.c_str(), -1);
                }
            }
            pango_cairo_update_layout(cr, layout);
            pango_cairo_show_layout(cr, layout);
        }
    }

    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    return 0;
}
