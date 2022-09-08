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

template<typename T, int max_elements> class SmallStack final {

public:
    typedef T value_type;

    SmallStack() = default;

    bool empty() const { return size == 0; }

    bool contains(T val) const {
        for(int i = 0; i < size; ++i) {
            if(arr[i] == val) {
                return true;
            }
        }
        return false;
    }

    void push(T new_val) {
        if(contains(new_val)) {
            printf("Tried to push an element that is already in the stack.\n");
            std::abort();
        }
        if(size >= max_elements) {
            printf("Stack overflow.\n");
            std::abort();
        }
        arr[size] = new_val;
        ++size;
    }

    void pop(T new_val) {
        if(empty()) {
            printf("Tried to pop an empty stack.\n");
            std::abort();
        }
        if(arr[size - 1] != new_val) {
            printf("Tried to pop a different value than is at the end of the stack.\n");
            std::abort();
        }
        --size;
    }

    const T *cbegin() const { return arr; }
    const T *cend() const { return arr + size; }

    const T *crbegin() const { return arr + size - 1; } // FIXME, need to use -- to progress.
    const T *crend() const { return arr - 1; }

private:
    T arr[max_elements];
    int size = 0;
};

typedef SmallStack<int, 4> StyleStack;

double mm2pt(const double x) { return x * 2.8346456693; }
double pt2mm(const double x) { return x / 2.8346456693; }

const std::vector<std::string> lines{{"Some /italic text/ here."},
                                     {"The second chapter be *written in bold*."},
                                     {"The third chapter be `written in typewriter`."},
                                     {"The fourth chapter be |written in Small Caps|."},
                                     {"a/b/c*d*e/f*g*h/i."},
                                     {"Death|death|"},
                                     {"# This is a top level header"},
                                     {"Some text with /emphasis/."},
                                     {"## This is a second level header"},
                                     {"Again some text."},
                                     {"# This heading should have a 2."},
                                     {"## Did it reset the numbering?"},
                                     {"This concludes our *broadcast day*."}};

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
    StyleStack styles;
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
    StyleStack styles;
};

template<typename T> void style_change(T &stack, typename T::value_type val) {
    if(stack.contains(val)) {
        stack.pop(val);
        // If the
    } else {
        stack.push(val);
    }
}

void style_and_append(FormatJiggy &fwords, const std::vector<std::string> in_words) {
    std::string buf;
    for(const auto &word : in_words) {
        auto start_style = fwords.styles;
        buf.clear();
        std::vector<Formatting> changes;
        for(const char c : word) {
            switch(c) {
            case italic_char:
                style_change(fwords.styles, ITALIC_BIT);
                changes.emplace_back(Formatting{buf.size(), ITALIC_BIT});
                break;
            case bold_char:
                style_change(fwords.styles, BOLD_BIT);
                changes.emplace_back(Formatting{buf.size(), BOLD_BIT});
                break;
            case tt_char:
                style_change(fwords.styles, TT_BIT);
                changes.emplace_back(Formatting{buf.size(), TT_BIT});
                break;
            case smallcaps_char:
                style_change(fwords.styles, SMALLCAPS_BIT);
                changes.emplace_back(Formatting{buf.size(), SMALLCAPS_BIT});
                break;
            default:
                buf.push_back(c);
            }
        }
        fwords.formatted_words.emplace_back(FormattedWord{buf, std::move(changes), start_style});
    }
}

void append_markup_start(std::string &buf, int style) {
    switch(style) {
    case ITALIC_BIT:
        buf.append("<i>");
        break;
    case BOLD_BIT:
        buf.append("<b>");
        break;
    case TT_BIT:
        buf.append("<tt>");
        break;
    case SMALLCAPS_BIT:
        buf.append("<span variant=\"small-caps\" letter_spacing=\"100\">");
        break;
    default:
        printf("Bad style start bit.\n");
        std::abort();
    }
}

void append_markup_end(std::string &buf, int style) {
    switch(style) {
    case ITALIC_BIT:
        buf.append("</i>");
        break;
    case BOLD_BIT:
        buf.append("</b>");
        break;
    case TT_BIT:
        buf.append("</tt>");
        break;
    case SMALLCAPS_BIT:
        buf.append("</span>");
        break;
    default:
        printf("Bad style end bit.\n");
        std::abort();
    }
}

void set_font(PangoLayout *layout, int level) {
    PangoFontDescription *desc;
    switch(level) {
    case 0:
        desc = pango_font_description_from_string("Gentium");
        pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
        break;
    case 1:
        desc = pango_font_description_from_string("Noto sans");
        pango_font_description_set_absolute_size(desc, 14 * PANGO_SCALE);
        break;
    case 2:
        desc = pango_font_description_from_string("Noto sans");
        pango_font_description_set_absolute_size(desc, 12 * PANGO_SCALE);
        break;
    }
    assert(desc);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);
}

int main() {
    setlocale(LC_ALL, "");
    //    cairo_status_t status;
    cairo_surface_t *surface = cairo_pdf_surface_create("parsingtest.pdf", 595, 842);
    cairo_t *cr = cairo_create(surface);
    cairo_save(cr);
    // cairo_set_source_rgb(cr, 1.0, 0.2, 0.1);
    PangoLayout *layout = pango_cairo_create_layout(cr);
    int line_num = -1;

    int section_number = 0;
    int subsection_number = 0;

    int y = 72;

    for(const auto &line : lines) {
        int delta_y = 0;
        pango_layout_set_attributes(layout, nullptr);
        ++line_num;
        FormatJiggy jg;
        style_and_append(jg, split_to_words(line.c_str()));
        if(jg.formatted_words.empty()) {
            continue;
        }

        if(jg.formatted_words.front().word == "#") {
            ++section_number;
            subsection_number = 0;
            set_font(layout, 1);
            jg.formatted_words.front().word = std::to_string(section_number);
            delta_y = 16;
        } else if(jg.formatted_words.front().word == "##") {
            ++subsection_number;
            set_font(layout, 2);
            jg.formatted_words.front().word = std::to_string(section_number);
            jg.formatted_words.front().word += '.';
            jg.formatted_words.front().word += std::to_string(subsection_number);
            delta_y = 14;
        } else {
            set_font(layout, 0);
            delta_y = 12;
        }

        int word_num = -1;
        std::string markup_buf;
        cairo_move_to(cr, 72, y);
        markup_buf.clear();
        for(const auto &word : jg.formatted_words) {
            ++word_num;
            if(word_num != 0) {
                markup_buf += ' ';
            }
            for(auto *it = word.styles.cbegin(); it != word.styles.cend(); ++it) {
                append_markup_start(markup_buf, *it);
            }
            auto current_styles = word.styles;

            size_t style_index = 0;
            for(size_t i = 0; i < word.word.size(); ++i) {
                while(style_index < word.blob.size() && word.blob[style_index].offset == i) {
                    const auto &format = word.blob[style_index];
                    if(current_styles.contains(format.formats)) {
                        append_markup_end(markup_buf, format.formats);
                        current_styles.pop(format.formats);
                    } else {
                        append_markup_start(markup_buf, format.formats);
                        current_styles.push(format.formats);
                    }
                    ++style_index;
                }
                markup_buf += word.word[i];
            }
            for(auto *it = current_styles.crbegin(); it != current_styles.crend(); --it) {
                append_markup_end(markup_buf, *it);
            }
        }
        pango_layout_set_markup(layout, markup_buf.c_str(), -1);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        y += delta_y;
    }

    cairo_surface_destroy(surface);
    cairo_destroy(cr);
    return 0;
}
