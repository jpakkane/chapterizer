#include <gtk/gtk.h>
#include <vector>
#include <string>
#include <fontconfig/fontconfig.h>
#include <fchelpers.hpp>

namespace {

const char preformatted_text[] =
    R"(From the corner of the divan of Persian
saddle-bags on which he was lying, smok-
ing, as was his custom, innumerable ciga-
rettes, Lord Henry Wotton could just
catch the gleam of the honey-sweet and
honey-coloured blossoms of a laburnum,
whose tremulous branches seemed hardly
able to bear the burden of a beauty so
flamelike as theirs; and now and then the
fantastic shadows of birds in flight flitted
across the long tussore-silk curtains that
were stretched in front of the huge win-
dow, producing a kind of momentary Ja-
panese effect, and making him think of
those pallid, jade-faced painters of Tokyo
who, through the medium of an art that is
necessarily immobile, seek to convey the
sense of swiftness and motion. The sullen
murmur of the bees shouldering their way
through the long unmown grass, or cir-
cling with monotonous insistence round
the dusty gilt horns of the straggling wood-
bine, seemed to make the stillness more
oppressive. The dim roar of London was
like the bourdon note of a distant organ.)";

void draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data);

enum { LINENUM_COLUMN, TEXT_COLUMN, DELTA_COLUMN, PENALTY_COLUMN, N_COLUMNS };

struct App {
    int target_width = 66;
    GtkApplication *app;
    GtkWindow *win;
    GtkTextView *textview;
    GtkTreeView *statview;
    GtkTreeStore *store;
    GtkLabel *status;
    GtkDrawingArea *draw;
    GtkSpinButton *zoom;
    GtkComboBoxText *fonts;

    GtkTextBuffer *buf() { return gtk_text_view_get_buffer(textview); }
};

/*
static void quitfunc(GtkButton *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    g_main_loop_quit(nullptr);
}
*/

std::vector<std::string> split_to_lines(const char *text) {
    if(!text) {
        return {};
    }
    size_t i = 0;
    std::string_view v(text);
    std::vector<std::string> lines;
    while(true) {
        auto next = v.find('\n', i);
        if(next == std::string_view::npos) {
            lines.emplace_back(v.substr(i));
            break;
        }
        lines.emplace_back(v.substr(i, next - i));
        i = next + 1;
    }
    return lines;
}

std::vector<std::string> get_entry_widget_text(App *app) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(app->buf(), &start, &end);
    char *text = gtk_text_buffer_get_text(app->buf(), &start, &end, 0);
    auto r = split_to_lines(text);
    g_free(text);
    return r;
}

void text_changed(GtkTextBuffer *, gpointer data) {
    auto app = static_cast<App *>(data);
    gtk_tree_store_clear(app->store);
    GtkTreeIter iter;
    size_t i = 0;
    int total_penalty = 0;
    auto lines = get_entry_widget_text(app);
    for(const auto &l : lines) {
        const int BUFSIZE = 128;
        char deltabuf[BUFSIZE];
        char penaltybuf[BUFSIZE];
        gtk_tree_store_append(app->store, &iter, nullptr);
        int delta = int(l.length()) - app->target_width;
        int penalty;
        if(lines.size() - 1 == i) {
            penalty =
                int(l.length()) <= app->target_width ? 0 : int(l.length()) - app->target_width;
        } else {
            penalty = int(l.length()) - app->target_width;
        }
        penalty = penalty * penalty;
        total_penalty += penalty;
        snprintf(deltabuf, BUFSIZE, "%d", delta);
        snprintf(penaltybuf, BUFSIZE, "%d", penalty);
        gtk_tree_store_set(app->store,
                           &iter,
                           LINENUM_COLUMN,
                           int(i) + 1,
                           TEXT_COLUMN,
                           l.c_str(),
                           DELTA_COLUMN,
                           deltabuf,
                           PENALTY_COLUMN,
                           penaltybuf,
                           -1);
        ++i;
    }
    const int BIGBUF = 1024;
    char buf[BIGBUF];
    snprintf(buf, BIGBUF, "Total penalty is %d.", total_penalty);
    gtk_label_set_text(app->status, buf);
    gtk_widget_queue_draw(GTK_WIDGET(app->draw));
}

void zoom_changed(GtkSpinButton *, gpointer data) {
    auto app = static_cast<App *>(data);
    gtk_widget_queue_draw(GTK_WIDGET(app->draw));
}

void font_changed(GtkComboBox *, gpointer data) {
    auto app = static_cast<App *>(data);
    gtk_widget_queue_draw(GTK_WIDGET(app->draw));
}

void connect_stuffs(App *app) {
    g_signal_connect(app->buf(), "changed", G_CALLBACK(text_changed), static_cast<gpointer>(app));
    g_signal_connect(
        app->zoom, "value-changed", G_CALLBACK(zoom_changed), static_cast<gpointer>(app));
    g_signal_connect(app->fonts, "changed", G_CALLBACK(font_changed), static_cast<gpointer>(app));
}

void draw_function(GtkDrawingArea *, cairo_t *cr, int width, int height, gpointer data) {
    App *a = static_cast<App *>(data);
    GdkRGBA color;
    const int line_height = 11;
    auto *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *desc =
        pango_font_description_from_string(gtk_combo_box_text_get_active_text(a->fonts));
    pango_font_description_set_absolute_size(desc, 10 * PANGO_SCALE);
    pango_layout_set_font_description(layout, desc);
    pango_font_description_free(desc);

    auto text = get_entry_widget_text(a);
    color.red = color.green = color.blue = 1.0f;
    color.alpha = 1.0f;
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    const double zoom_ratio = gtk_spin_button_get_value(a->zoom);
    cairo_scale(cr, zoom_ratio, zoom_ratio);
    const double xoff = 10;
    const double yoff = 10;
    const double parwid = 60 / 25.4 * 72;
    const double parhei = text.size() * line_height;
    cairo_set_line_width(cr, 1.0 / zoom_ratio);
    cairo_rectangle(cr, xoff, yoff, parwid, parhei);
    color.red = color.green = 0.0;
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_stroke(cr);
    cairo_set_line_width(cr, 1.0);

    color.blue = 0;
    gdk_cairo_set_source_rgba(cr, &color);
    if(!text.empty())
        cairo_select_font_face(cr,
                               gtk_combo_box_text_get_active_text(a->fonts),
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);
    for(size_t i = 0; i < text.size(); ++i) {
        cairo_move_to(cr, xoff, yoff + line_height * i);
        pango_layout_set_text(layout, text[i].c_str(), -1);
        pango_cairo_update_layout(cr, layout);
        pango_cairo_show_layout(cr, layout);
    }
    g_object_unref(G_OBJECT(layout));
}

void populate_fontlist(App *app) {
    int active_id = 0;
    app->zoom = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(1.0, 4.0, 0.1));
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

void activate(GtkApplication *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    app->win = GTK_WINDOW(gtk_application_window_new(app->app));
    gtk_window_set_title(app->win, "Chapterizer devtool");
    gtk_window_set_default_size(app->win, 800, 480);
    GtkGrid *grid = GTK_GRID(gtk_grid_new());
    app->store = gtk_tree_store_new(
        N_COLUMNS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING); // Should be int but...
    app->statview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->store)));
    GtkCellRenderer *r;
    GtkTreeViewColumn *c;
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Line number", r, "text", LINENUM_COLUMN, nullptr);
    gtk_tree_view_append_column(app->statview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Text", r, "text", TEXT_COLUMN, nullptr);
    gtk_tree_view_append_column(app->statview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Delta", r, "text", DELTA_COLUMN, nullptr);
    gtk_tree_view_append_column(app->statview, c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Penalty", r, "text", PENALTY_COLUMN, nullptr);
    gtk_tree_view_append_column(app->statview, c);

    app->textview = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_monospace(app->textview, 1);
    app->status = GTK_LABEL(gtk_label_new("Total error is ?."));

    app->draw = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_drawing_area_set_content_height(app->draw, 600);
    gtk_drawing_area_set_content_width(app->draw, 300);
    gtk_drawing_area_set_draw_func(app->draw, draw_function, app, nullptr);

    populate_fontlist(app);

    auto *text_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(text_scroll), GTK_WIDGET(app->textview));
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(text_scroll), GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
    gtk_widget_set_size_request(text_scroll, 400, 600);
    auto *draw_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(draw_scroll), GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(draw_scroll), GTK_WIDGET(app->draw));
    gtk_widget_set_vexpand(draw_scroll, 1);
    gtk_widget_set_hexpand(draw_scroll, 1);
    gtk_widget_set_size_request(draw_scroll, 400, 600);

    gtk_grid_attach(grid, text_scroll, 0, 0, 1, 1);
    gtk_grid_attach(grid, draw_scroll, 1, 0, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(app->statview), 2, 0, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(app->zoom), 0, 1, 3, 1);
    gtk_grid_attach(grid, GTK_WIDGET(app->fonts), 0, 2, 3, 1);
    gtk_grid_attach(grid, GTK_WIDGET(app->status), 0, 3, 3, 1);

    gtk_widget_set_vexpand(GTK_WIDGET(grid), 1);
    gtk_widget_set_hexpand(GTK_WIDGET(grid), 1);

    connect_stuffs(app);
    gtk_window_set_child(app->win, GTK_WIDGET(grid));
    gtk_window_present(GTK_WINDOW(app->win));
    gtk_text_buffer_set_text(app->buf(), preformatted_text, -1);
}

} // namespace

int main(int argc, char **argv) {
    App app;
    FcInit();
    app.app = gtk_application_new("io.github.jpakkane.chapterizer", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app.app, "activate", G_CALLBACK(activate), static_cast<gpointer>(&app));
    int status = g_application_run(G_APPLICATION(app.app), argc, argv);
    g_object_unref(app.app);
    // FcFini();
    return status;
}
