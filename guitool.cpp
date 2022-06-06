#include <gtk/gtk.h>
#include <vector>
#include <string>

enum { TEXT_COLUMN, DELTA_COLUMN, PENALTY_COLUMN, N_COLUMNS };

struct App {
    int target_width = 66;
    GtkApplication *app;
    GtkWindow *win;
    GtkTextView *textview;
    GtkTextBuffer *buf;
    GtkTreeView *statview;
    GtkTreeStore *store;
    GtkLabel *status;
    GtkDrawingArea *draw;
};

/*
static void quitfunc(GtkButton *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    g_main_loop_quit(nullptr);
}
*/

std::vector<std::string> split_to_lines(const char *text) {
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

void text_changed(GtkTextBuffer *, gpointer data) {
    auto app = static_cast<App *>(data);
    gtk_tree_store_clear(app->store);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(app->buf, &start, &end);
    char *text = gtk_text_buffer_get_text(app->buf, &start, &end, 0);
    const auto lines = split_to_lines(text);
    g_free(text);
    GtkTreeIter iter;
    size_t i = 0;
    int total_penalty = 0;
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
}

void connect_stuffs(App *app) {
    g_signal_connect(app->buf, "changed", G_CALLBACK(text_changed), static_cast<gpointer>(app));
}

static void draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    App *a = static_cast<App *>(data);
    GdkRGBA color;

    color.red = color.green = color.blue = 1.0f;
    color.alpha = 1.0f;
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    const double xoff = 10;
    const double yoff = 10;
    const double parwid = 50 / 25.4 * 72;
    const double parhei = 200;
    cairo_rectangle(cr, xoff, yoff, parwid, parhei);
    color.red = color.green = 0.0;
    gdk_cairo_set_source_rgba(cr, &color);

    cairo_stroke(cr);
}

static void activate(GtkApplication *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    app->win = GTK_WINDOW(gtk_application_window_new(app->app));
    gtk_window_set_title(app->win, "Chapterizer devtool");
    gtk_window_set_default_size(app->win, 800, 480);
    GtkGrid *grid = GTK_GRID(gtk_grid_new());
    app->store = gtk_tree_store_new(
        N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING); // Should be int but...
    app->statview = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->store)));
    GtkCellRenderer *r;
    GtkTreeViewColumn *c;
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
    app->buf = gtk_text_view_get_buffer(app->textview);
    app->status = GTK_LABEL(gtk_label_new("Total error is ?."));

    app->draw = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_drawing_area_set_content_height(app->draw, 600);
    gtk_drawing_area_set_content_width(app->draw, 300);
    gtk_drawing_area_set_draw_func(app->draw, draw_function, &app, nullptr);

    gtk_widget_set_vexpand(GTK_WIDGET(app->textview), 1);
    gtk_widget_set_vexpand(GTK_WIDGET(app->statview), 1);
    gtk_widget_set_hexpand(GTK_WIDGET(app->textview), 1);
    // gtk_widget_set_hexpand(GTK_WIDGET(app->statview), 1);
    gtk_widget_set_vexpand(GTK_WIDGET(app->draw), 1);
    gtk_widget_set_hexpand(GTK_WIDGET(app->draw), 1);

    gtk_grid_attach(grid, GTK_WIDGET(app->textview), 0, 0, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(app->draw), 1, 0, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(app->statview), 2, 0, 1, 1);
    gtk_grid_attach(grid, GTK_WIDGET(app->status), 0, 1, 2, 1);

    connect_stuffs(app);
    gtk_text_buffer_set_text(app->buf, "Yes, this is dog.", -1);
    gtk_window_set_child(app->win, GTK_WIDGET(grid));
    gtk_window_present(GTK_WINDOW(app->win));
}

int main(int argc, char **argv) {
    App app;

    app.app = gtk_application_new("io.github.jpakkane.chapterizer", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app.app, "activate", G_CALLBACK(activate), static_cast<gpointer>(&app));
    int status = g_application_run(G_APPLICATION(app.app), argc, argv);
    g_object_unref(app.app);
    return status;
}
