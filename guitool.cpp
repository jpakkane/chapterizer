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

#include <utils.hpp>
#include <paragraphformatter.hpp>
#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <fontconfig/fontconfig.h>
#include <fchelpers.hpp>
#include <cassert>

namespace {

const char preformatted_text[] =
    R"(From the corner of the divan of Persian
saddle-bags on which he was lying, smoking,
as was his custom, innumerable cigarettes,
Lord Henry Wotton could just catch the gleam
of the honey-sweet and honey-coloured
blossoms of a laburnum, whose tremulous
branches seemed hardly able to bear the
burden of a beauty so flamelike as theirs; and
now and then the fantastic shadows of birds
in flight flitted across the long tussore-silk
curtains that were stretched in front of the
huge window, producing a kind of momentary
Japanese effect, and making him think of
those pallid, jade-faced painters of Tokyo who,
through the medium of an art that is
necessarily immobile, seek to convey the
sense of swiftness and motion. The sullen
murmur of the bees shouldering their way
through the long unmown grass, or circling
with monotonous insistence round the dusty
gilt horns of the straggling woodbine, seemed
to make the stillness more oppressive. The
dim roar of London was like the bourdon note
of a distant organ.)";

static const char *extra_penalty_strings[3] = {
    "Consecutive dashes", "Single word on line", "Single hyphenated word on line"};

void draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data);

enum {
    LINENUM_LINE_COLUMN,
    TEXT_LINE_COLUMN,
    DELTA_LINE_COLUMN,
    PENALTY_LINE_COLUMN,
    N_LINE_COLUMNS
};

enum { LINENUM_EXTRA_COLUMN, TYPE_EXTRA_COLUMN, PENALTY_EXTRA_COLUMN, N_EXTRA_COLUMNS };

struct App {
    GtkApplication *app;
    GtkWindow *win;
    GtkTextView *textview;
    GtkTreeView *statview;
    GtkTreeStore *linestore;
    GtkTreeView *extraview;
    GtkTreeStore *extrastore;
    GtkLabel *status;
    GtkDrawingArea *draw;
    GtkSpinButton *zoom;
    GtkComboBoxText *fonts;
    GtkComboBoxText *font_style;
    GtkSpinButton *ptsize;
    GtkSpinButton *row_height;
    GtkSpinButton *chapter_width;
    GtkSpinButton *indent;
    GtkNotebook *note;
    GtkButton *store_text;
    GtkButton *reset;
    GtkButton *optimize;
    GtkToggleButton *justify;
    GtkEntry *hyp_entry;
    GtkLabel *hyp_output;
    std::string default_text{preformatted_text};

    // Penalty amounts
    GtkSpinButton *dash_penalty;
    GtkSpinButton *single_word_penalty;
    GtkSpinButton *single_split_word_penalty;

    GtkTextBuffer *buf() { return gtk_text_view_get_buffer(textview); }
};

struct Outdents {
    Length left;
    Length right;
};

/*
static void quitfunc(GtkButton *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    g_main_loop_quit(nullptr);
}
*/
double mm2screenpt(double mm) { return mm / 25.4 * 72; }

Outdents compute_outdents(const std::string &s, Length point_size) {
    Outdents r;
    if(s.size() < 2) {
        return r;
    }
    switch(s.front()) {
    case '-':
        r.left = 0.2 * point_size;
        break;

    case '.':
    case ',':
        r.left = 0.15 * point_size;
        break;

    case 'd':
        r.left = 0.15 * point_size;

    case 'o':
    case 'O':
    case '0':
        r.left = 0.15 * point_size;
        break;

    case '"':
    case '\'':
        r.left = 0.15 * point_size;
        break;

    case '/':
    case '\\':
        r.left = 0.15 * point_size;
        break;

    default:;
    }

    switch(s.back()) {
    case '-':
        r.right = 0.15 * point_size;
        break;

    case '.':
    case ',':
        r.right = 0.15 * point_size;
        break;

    case 'b':
        r.right = 0.15 * point_size;

    case 'o':
    case 'O':
    case '0':
        r.right = 0.2 * point_size;
        break;

    case '"':
    case '\'':
        r.right = 0.2 * point_size;
        break;

    case '/':
    case '\\':
        r.right = 0.15 * point_size;
        break;

    default:;
    }

    return r;
}

void resize_canvas(App *app) {
    const double zoom = gtk_spin_button_get_value(app->zoom);
    const double text_width = mm2screenpt(gtk_spin_button_get_value(app->chapter_width));
    const double row_height = gtk_spin_button_get_value(app->row_height);
    const int num_rows = 40; // FIXme
    const int boundary = 10;
    const int magic_extra_space_to_the_right = 50;
    const int canvas_w = int(2 * boundary + text_width * zoom + magic_extra_space_to_the_right);
    const int canvas_h = int(2 * boundary + num_rows * row_height * zoom);
    gtk_drawing_area_set_content_height(app->draw, canvas_h);
    gtk_drawing_area_set_content_width(app->draw, canvas_w);
}

std::vector<std::string> get_entry_widget_text_lines(App *app) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(app->buf(), &start, &end);
    char *text = gtk_text_buffer_get_text(app->buf(), &start, &end, 0);
    std::string t(text);
    auto lines = split_to_lines(t);
    g_free(text);
    return lines;
}

std::vector<std::string> get_entry_widget_text_words(App *app) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(app->buf(), &start, &end);
    char *text = gtk_text_buffer_get_text(app->buf(), &start, &end, 0);
    std::string_view t(text);
    auto words = split_to_words(t);
    g_free(text);
    return words;
}

ChapterParameters get_params(App *app) {
    ChapterParameters par;
    auto *tmp = gtk_combo_box_text_get_active_text(app->fonts);
    par.font.name = tmp;
    g_free(tmp);
    par.font.size = Length::from_pt(gtk_spin_button_get_value(app->ptsize));
    par.font.type = (FontStyle)gtk_combo_box_get_active(GTK_COMBO_BOX(app->font_style));
    par.line_height = Length::from_pt(gtk_spin_button_get_value(app->row_height));
    par.indent = Length::from_mm(gtk_spin_button_get_value(app->indent));
    return par;
}

ExtraPenaltyAmounts get_penalties(App *app) {
    ExtraPenaltyAmounts pen_amounts;
    pen_amounts.multiple_dashes = gtk_spin_button_get_value(app->dash_penalty);
    pen_amounts.single_word_line = gtk_spin_button_get_value(app->single_word_penalty);
    pen_amounts.single_split_word_line = gtk_spin_button_get_value(app->single_split_word_penalty);
    return pen_amounts;
}

double populate_line_store(App *app,
                           const std::vector<std::string> &lines,
                           const std::vector<LinePenaltyStatistics> &penalties) {
    GtkTreeIter iter;
    const int sample_len = 25;
    std::string workarea;
    double total_penalty = 0;
    gtk_tree_store_clear(app->linestore);
    size_t i = 0;
    for(const auto &stats : penalties) {
        workarea = lines[i];
        if(workarea.length() > sample_len) {
            workarea.erase(workarea.begin() + sample_len - 4, workarea.end());
            workarea += " ...";
        }
        gtk_tree_store_append(app->linestore, &iter, nullptr);
        total_penalty += stats.penalty;
        gtk_tree_store_set(app->linestore,
                           &iter,
                           LINENUM_LINE_COLUMN,
                           int(i) + 1,
                           TEXT_LINE_COLUMN,
                           workarea.c_str(),
                           DELTA_LINE_COLUMN,
                           stats.delta.mm(),
                           PENALTY_LINE_COLUMN,
                           stats.penalty,
                           -1);
        ++i;
    }
    return total_penalty;
}

double populate_extra_store(App *app, const std::vector<ExtraPenaltyStatistics> &penalties) {
    GtkTreeIter iter;
    std::string workarea;
    double total_penalty = 0;
    gtk_tree_store_clear(app->extrastore);
    for(const auto &stats : penalties) {
        gtk_tree_store_append(app->extrastore, &iter, nullptr);
        gtk_tree_store_set(app->extrastore,
                           &iter,
                           LINENUM_EXTRA_COLUMN,
                           stats.line,
                           TYPE_EXTRA_COLUMN,
                           extra_penalty_strings[int(stats.type)],
                           PENALTY_EXTRA_COLUMN,
                           stats.penalty,
                           -1);
        total_penalty += stats.penalty;
    }
    return total_penalty;
}

void text_changed(GtkTextBuffer *, gpointer data) {
    auto app = static_cast<App *>(data);
    ChapterParameters par = get_params(app);
    auto paragraph_width = Length::from_mm(gtk_spin_button_get_value(app->chapter_width));
    double total_penalty = 0;
    auto lines = get_entry_widget_text_lines(app);
    ExtraPenaltyAmounts pen_amounts = get_penalties(app);
    auto penalties = compute_stats(lines, paragraph_width, par, pen_amounts);
    const int BIGBUF = 1024;
    char buf[BIGBUF];
    total_penalty += populate_line_store(app, lines, penalties.lines);
    total_penalty += populate_extra_store(app, penalties.extras);
    snprintf(buf, BIGBUF, "Total penalty is %.2f.", total_penalty);
    gtk_label_set_text(app->status, buf);
    gtk_widget_queue_draw(GTK_WIDGET(app->draw));
}

void penalty_changed(GtkSpinButton *, gpointer data) {
    // Not correct, but does the job for now.
    text_changed(nullptr, data);
}

void zoom_changed(GtkSpinButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    resize_canvas(app);
}

void font_changed(GtkComboBox *, gpointer data) {
    auto app = static_cast<App *>(data);
    gtk_widget_queue_draw(GTK_WIDGET(app->draw));
}

void font_size_changed(GtkSpinButton *new_size, gpointer data) {
    auto app = static_cast<App *>(data);
    // gtk_widget_queue_draw(GTK_WIDGET(app->draw));
    gtk_spin_button_set_value(app->row_height, 1.1 * gtk_spin_button_get_value(new_size));
}

void row_height_changed(GtkSpinButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    resize_canvas(app);
}

void chapter_width_changed(GtkSpinButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    resize_canvas(app);
}

void indent_changed(GtkSpinButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    gtk_widget_queue_draw(GTK_WIDGET(app->draw));
}

void reset_text_cb(GtkButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    gtk_text_buffer_set_text(app->buf(), app->default_text.c_str(), -1);
}

void store_text_cb(GtkButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(app->buf(), &start, &end);
    char *text = gtk_text_buffer_get_text(app->buf(), &start, &end, 0);
    app->default_text = text;
    g_free(text);
}

void run_optimization_cb(GtkButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    // FIXME: run as an async task.
    WordHyphenator hyp;
    ChapterParameters params = get_params(app);
    auto paragraph_width = Length::from_mm(gtk_spin_button_get_value(app->chapter_width));

    ExtraPenaltyAmounts extras = get_penalties(app);
    auto words = get_entry_widget_text_words(app);
    auto hyphenated_words = hyp.hyphenate(words, Language::English);
    std::vector<EnrichedWord> rich_words;
    rich_words.reserve(hyphenated_words.size());
    StyleStack empty_style;
    for(size_t i = 0; i < words.size(); ++i) {
        rich_words.emplace_back(
            EnrichedWord{std::move(words[i]), std::move(hyphenated_words[i]), {}, empty_style});
    }
    ParagraphFormatter builder{rich_words, paragraph_width, params, extras};
    auto new_lines = builder.split_lines();
    std::string collator;
    for(const auto &l : new_lines) {
        collator += l;
        collator += '\n';
    }
    gtk_text_buffer_set_text(app->buf(), collator.c_str(), collator.size());
}

void justify_toggle_cb(GtkToggleButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    gtk_widget_queue_draw(GTK_WIDGET(app->draw));
}

void hyphenword_changed_cb(GtkEditable *, gpointer data) {
    auto app = static_cast<App *>(data);
    auto b = gtk_entry_get_buffer(app->hyp_entry);
    std::string word = gtk_entry_buffer_get_text(b);
    WordHyphenator hyphenator; // Not very efficient, but whatever.
    auto hyphen_points = hyphenator.hyphenate(word, Language::English);
    auto visual = get_visual_string(word, hyphen_points);
    gtk_label_set_text(app->hyp_output, visual.c_str());
}

void connect_stuffs(App *app) {
    g_signal_connect(app->buf(), "changed", G_CALLBACK(text_changed), static_cast<gpointer>(app));
    // Parameters.
    g_signal_connect(
        app->zoom, "value-changed", G_CALLBACK(zoom_changed), static_cast<gpointer>(app));
    g_signal_connect(app->fonts, "changed", G_CALLBACK(font_changed), static_cast<gpointer>(app));
    g_signal_connect(
        app->font_style, "changed", G_CALLBACK(font_changed), static_cast<gpointer>(app));
    g_signal_connect(
        app->ptsize, "changed", G_CALLBACK(font_size_changed), static_cast<gpointer>(app));
    g_signal_connect(
        app->row_height, "changed", G_CALLBACK(row_height_changed), static_cast<gpointer>(app));
    g_signal_connect(app->chapter_width,
                     "changed",
                     G_CALLBACK(chapter_width_changed),
                     static_cast<gpointer>(app));
    g_signal_connect(
        app->indent, "changed", G_CALLBACK(indent_changed), static_cast<gpointer>(app));

    g_signal_connect(
        app->dash_penalty, "changed", G_CALLBACK(penalty_changed), static_cast<gpointer>(app));

    // Penalties

    // Bottom buttons
    g_signal_connect(app->reset, "clicked", G_CALLBACK(reset_text_cb), static_cast<gpointer>(app));
    g_signal_connect(
        app->optimize, "clicked", G_CALLBACK(run_optimization_cb), static_cast<gpointer>(app));
    g_signal_connect(
        app->store_text, "clicked", G_CALLBACK(store_text_cb), static_cast<gpointer>(app));
    g_signal_connect(
        app->justify, "toggled", G_CALLBACK(justify_toggle_cb), static_cast<gpointer>(app));
    g_signal_connect(
        app->hyp_entry, "changed", G_CALLBACK(hyphenword_changed_cb), static_cast<gpointer>(app));
}

void render_line_justified(cairo_t *cr,
                           PangoLayout *layout,
                           Length size,
                           const std::string &line_text,
                           Length x,
                           Length y,
                           Length chapter_width) {
    assert(!line_text.empty());
    const auto words = split_to_words(std::string_view(line_text));
    assert(!words.empty());
    const int num_spaces = int(words.size() - 1);
    Length text_width;
    cairo_move_to(cr, -1000, -1000);
    const auto outdents = compute_outdents(line_text, size);
    const auto total_outdent = outdents.left + outdents.right;
    // Measure the total width of printed words.
    for(const auto &word : words) {
        PangoRectangle r;
        pango_layout_set_text(layout, word.c_str(), -1);
        pango_layout_get_extents(layout, nullptr, &r);
        pango_cairo_update_layout(cr, layout);
        text_width += Length::from_pt(double(r.width) / PANGO_SCALE);
    }
    const Length space_extra_width =
        num_spaces > 0 ? (chapter_width - text_width + total_outdent) / num_spaces : Length{};

    x -= outdents.left;
    for(size_t i = 0; i < words.size(); ++i) {
        cairo_move_to(cr, x.pt(), y.pt());
        PangoRectangle r;

        pango_layout_set_text(layout, words[i].c_str(), -1);
        pango_layout_get_extents(layout, nullptr, &r);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
        x += Length::from_pt(double(r.width) / PANGO_SCALE);
        x += space_extra_width;
    }
}

void render_line_as_is(
    cairo_t *cr, PangoLayout *layout, const std::string &line, double x, double y) {
    cairo_move_to(cr, x, y);
    pango_layout_set_text(layout, line.c_str(), line.length());
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
}

void draw_function(GtkDrawingArea *, cairo_t *cr, int width, int height, gpointer data) {
    App *a = static_cast<App *>(data);
    GdkRGBA color;
    ChapterParameters cp = get_params(a);
    const auto paragraph_width = Length::from_mm(gtk_spin_button_get_value(a->chapter_width));

    auto text = get_entry_widget_text_lines(a);
    auto *layout = pango_cairo_create_layout(cr);
    PangoContext *context = pango_layout_get_context(layout);
    pango_context_set_round_glyph_positions(context, FALSE);
    PangoFontDescription *desc =
        pango_font_description_from_string(gtk_combo_box_text_get_active_text(a->fonts));
    if(cp.font.type == FontStyle::Bold || cp.font.type == FontStyle::BoldItalic) {
        pango_font_description_set_weight(desc, PANGO_WEIGHT_BOLD);
    } else {
        pango_font_description_set_weight(desc, PANGO_WEIGHT_NORMAL);
    }
    if(cp.font.type == FontStyle::Italic || cp.font.type == FontStyle::BoldItalic) {
        pango_font_description_set_style(desc, PANGO_STYLE_ITALIC);
    } else {
        pango_font_description_set_style(desc, PANGO_STYLE_NORMAL);
    }
    pango_font_description_set_absolute_size(desc, cp.font.size.pt() * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    color.red = color.green = color.blue = 1.0f;
    color.alpha = 1.0f;
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    const double zoom_ratio = gtk_spin_button_get_value(a->zoom);
    const double xoff = 10;
    const double yoff = 10;
    const double parwid = zoom_ratio * mm2screenpt(paragraph_width.pt());
    const double parhei = zoom_ratio * text.size() * cp.line_height.pt();
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, xoff, yoff, parwid, parhei);
    color.red = color.green = 0.0;
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_scale(cr, zoom_ratio, zoom_ratio);

    if(!text.empty()) {
        color.blue = 0;
        gdk_cairo_set_source_rgba(cr, &color);
        if(gtk_toggle_button_get_active(a->justify)) {
            for(size_t i = 0; i < text.size() - 1; ++i) {
                double indent = i == 0 ? zoom_ratio * mm2screenpt(cp.indent.mm()) : 0;
                render_line_justified(
                    cr,
                    layout,
                    cp.font.size,
                    text[i],
                    Length::from_pt((xoff + indent) / zoom_ratio),
                    Length::from_pt((yoff / zoom_ratio + cp.line_height.pt() * i)),
                    Length::from_pt((parwid - indent) / zoom_ratio));
            }
            render_line_as_is(cr,
                              layout,
                              text.back(),
                              xoff / zoom_ratio,
                              yoff / zoom_ratio + cp.line_height.pt() * (text.size() - 1));
        } else {
            for(size_t i = 0; i < text.size(); ++i) {
                double indent = i == 0 ? zoom_ratio * mm2screenpt(cp.indent.mm()) : 0;
                render_line_as_is(cr,
                                  layout,
                                  text[i],
                                  (xoff + indent) / zoom_ratio,
                                  yoff / zoom_ratio + cp.line_height.pt() * i);
            }
        }
    }
    g_object_unref(G_OBJECT(layout));
}

void populate_fontlist(App *app) {
    int active_id = 0;
    app->fonts = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
    int i = 0;
    for(const auto &f : get_fontnames_smart()) {
        if(f == "Gentium") {
            active_id = i;
        }
        gtk_combo_box_text_append_text(app->fonts, f.c_str());
        ++i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->fonts), active_id);
}

void add_property(GtkGrid *grid, const char *label_text, GtkWidget *w, int yloc) {
    GtkWidget *l = gtk_label_new(label_text);
    gtk_widget_set_halign(l, GTK_ALIGN_START);
    gtk_grid_attach(grid, l, 0, yloc, 1, 1);
    gtk_grid_attach(grid, w, 1, yloc, 1, 1);
}

void build_treeviews(App *app) {
    GtkCellRenderer *r;
    GtkTreeViewColumn *c;

    // Line penalties
    app->linestore =
        gtk_tree_store_new(N_LINE_COLUMNS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
    app->statview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->linestore)));
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes(
        "Line number", r, "text", LINENUM_LINE_COLUMN, nullptr);
    gtk_tree_view_append_column(app->statview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Text", r, "text", TEXT_LINE_COLUMN, nullptr);
    gtk_tree_view_append_column(app->statview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Delta", r, "text", DELTA_LINE_COLUMN, nullptr);
    gtk_tree_view_append_column(app->statview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes(
        "Penalty", r, "text", PENALTY_LINE_COLUMN, nullptr);
    gtk_tree_view_append_column(app->statview, c);

    // Extra penalties
    app->extrastore = gtk_tree_store_new(N_EXTRA_COLUMNS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_DOUBLE);
    app->extraview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->extrastore)));
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes(
        "Line number", r, "text", LINENUM_EXTRA_COLUMN, nullptr);
    gtk_tree_view_append_column(app->extraview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Type", r, "text", TYPE_EXTRA_COLUMN, nullptr);
    gtk_tree_view_append_column(app->extraview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes(
        "Penalty", r, "text", PENALTY_EXTRA_COLUMN, nullptr);
    gtk_tree_view_append_column(app->extraview, c);
}

void populate_parameter_grid(App *app, GtkGrid *parameter_grid) {
    int i = 0;
    add_property(parameter_grid, "Zoom", GTK_WIDGET(app->zoom), i++);
    add_property(parameter_grid, "Font size", GTK_WIDGET(app->ptsize), i++);
    add_property(parameter_grid, "Row height", GTK_WIDGET(app->row_height), i++);
    add_property(parameter_grid, "Chapter width (mm)", GTK_WIDGET(app->chapter_width), i++);
    add_property(parameter_grid, "Indent (mm)", GTK_WIDGET(app->indent), i++);
    add_property(parameter_grid, "Font", GTK_WIDGET(app->fonts), i++);
    add_property(parameter_grid, "Font style", GTK_WIDGET(app->font_style), i++);
    add_property(parameter_grid, "Alignment", GTK_WIDGET(app->justify), i++);

    add_property(parameter_grid, "Dash penalty", GTK_WIDGET(app->dash_penalty), i++);
    add_property(parameter_grid, "Single word penalty", GTK_WIDGET(app->single_word_penalty), i++);
    add_property(parameter_grid,
                 "Single split word penalty",
                 GTK_WIDGET(app->single_split_word_penalty),
                 i++);
}

void activate(GtkApplication *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    app->win = GTK_WINDOW(gtk_application_window_new(app->app));
    app->note = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_window_set_title(app->win, "Chapterizer devtool");
    gtk_window_set_default_size(app->win, 800, 480);
    build_treeviews(app);
    GtkGrid *main_grid = GTK_GRID(gtk_grid_new());
    GtkGrid *parameter_grid = GTK_GRID(gtk_grid_new());
    auto *stat_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(stat_scroll), GTK_WIDGET(app->statview));
    gtk_notebook_append_page(app->note, stat_scroll, gtk_label_new("Line Statistics"));
    auto *extra_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(extra_scroll), GTK_WIDGET(app->extraview));
    gtk_notebook_append_page(app->note, extra_scroll, gtk_label_new("Extra penalties"));
    auto *par_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(par_scroll), GTK_WIDGET(parameter_grid));
    gtk_notebook_append_page(app->note, par_scroll, gtk_label_new("Parameters"));
    gtk_widget_set_vexpand(GTK_WIDGET(app->note), 1);
    gtk_widget_set_hexpand(GTK_WIDGET(app->note), 0);
    // gtk_widget_set_size_request(GTK_WIDGET(app->note), 600, 600);

    app->textview = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_monospace(app->textview, 1);
    app->status = GTK_LABEL(gtk_label_new("Total error is ?."));

    app->zoom = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1.0, 4.0, 0.1));
    app->ptsize = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(6.0, 18.0, 0.5));
    app->row_height = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(4.0, 20, 0.1));
    app->chapter_width = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(30, 100, 1));
    app->indent = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 20, 1));
    app->justify = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label("Justify"));
    gtk_spin_button_set_value(app->zoom, 2);
    gtk_spin_button_set_value(app->ptsize, 10.0);
    gtk_spin_button_set_value(app->row_height, 12.0);
    gtk_spin_button_set_value(app->chapter_width, 66);
    gtk_spin_button_set_value(app->indent, 5);

    app->dash_penalty = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 100, 1));
    app->single_word_penalty = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 100, 1));
    app->single_split_word_penalty = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0, 1000, 1));
    gtk_spin_button_set_value(app->dash_penalty, 10);
    gtk_spin_button_set_value(app->single_word_penalty, 10);
    gtk_spin_button_set_value(app->single_split_word_penalty, 50);

    populate_fontlist(app);
    app->font_style = GTK_COMBO_BOX_TEXT(gtk_combo_box_text_new());
    gtk_combo_box_text_append_text(app->font_style, "Regular");
    gtk_combo_box_text_append_text(app->font_style, "Italic");
    gtk_combo_box_text_append_text(app->font_style, "Bold");
    gtk_combo_box_text_append_text(app->font_style, "Bold italic");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->font_style), 0);

    app->draw = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_drawing_area_set_draw_func(app->draw, draw_function, app, nullptr);
    resize_canvas(app);

    auto *text_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(text_scroll), GTK_WIDGET(app->textview));
    gtk_widget_set_size_request(text_scroll, 400, 600);
    auto *draw_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(draw_scroll), GTK_WIDGET(app->draw));
    gtk_widget_set_vexpand(draw_scroll, 1);
    gtk_widget_set_hexpand(draw_scroll, 1);
    gtk_widget_set_size_request(draw_scroll, 300, 600);

    gtk_grid_attach(main_grid, text_scroll, 0, 0, 1, 1);
    gtk_grid_attach(main_grid, draw_scroll, 1, 0, 1, 1);
    gtk_grid_attach(main_grid, GTK_WIDGET(app->note), 2, 0, 1, 1);

    populate_parameter_grid(app, parameter_grid);
    gtk_widget_set_vexpand(GTK_WIDGET(main_grid), 1);
    gtk_widget_set_hexpand(GTK_WIDGET(main_grid), 1);

    auto *hyphen_grid = GTK_GRID(gtk_grid_new());
    app->hyp_entry = GTK_ENTRY(gtk_entry_new());
    app->hyp_output = GTK_LABEL(gtk_label_new(""));
    gtk_grid_attach(hyphen_grid, gtk_label_new("Word to hyphenate"), 0, 0, 1, 1);
    gtk_grid_attach(hyphen_grid, gtk_label_new("Hyphenated form"), 0, 1, 1, 1);
    gtk_grid_attach(hyphen_grid, GTK_WIDGET(app->hyp_entry), 1, 0, 1, 1);
    gtk_grid_attach(hyphen_grid, GTK_WIDGET(app->hyp_output), 1, 1, 1, 1);
    gtk_notebook_append_page(app->note, GTK_WIDGET(hyphen_grid), gtk_label_new("Hyphenation tool"));

    GtkGrid *button_grid = GTK_GRID(gtk_grid_new());
    app->reset = GTK_BUTTON(gtk_button_new_with_label("Reset text"));
    app->optimize = GTK_BUTTON(gtk_button_new_with_label("Optimize"));
    app->store_text = GTK_BUTTON(gtk_button_new_with_label("Store current text"));
    gtk_grid_attach(button_grid, GTK_WIDGET(app->reset), 0, 0, 1, 1);
    gtk_grid_attach(button_grid, GTK_WIDGET(app->optimize), 1, 0, 1, 1);
    gtk_grid_attach(button_grid, GTK_WIDGET(app->store_text), 2, 0, 1, 1);
    gtk_grid_attach(button_grid, GTK_WIDGET(app->status), 3, 0, 1, 1);
    gtk_grid_attach(main_grid, GTK_WIDGET(button_grid), 0, 1, 1, 3);

    connect_stuffs(app);
    gtk_window_set_child(app->win, GTK_WIDGET(main_grid));
    gtk_window_present(GTK_WINDOW(app->win));
    gtk_text_buffer_set_text(app->buf(), preformatted_text, -1);
}

} // namespace

int main(int argc, char **argv) {
    App app;
    FcInit();
    app.app = gtk_application_new("io.github.jpakkane.chapterizer", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app.app, "activate", G_CALLBACK(activate), static_cast<gpointer>(&app));
    int status = g_application_run(G_APPLICATION(app.app), argc, argv);
    g_object_unref(app.app);
    // FcFini();
    return status;
}
