#include <gtk/gtk.h>

#include <guidata.hpp>

struct App {
    GtkApplication *app;
    GtkWindow *win;
    GtkTextView *textview;
    GtkTreeView *statview;
};

static void quitfunc(GtkButton *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    g_main_loop_quit(nullptr);
}

static void activate(GtkApplication *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    GtkBuilder *b = gtk_builder_new_from_string(ui_text, -1);
    app->win = GTK_WINDOW(gtk_builder_get_object(b, "mainwin"));
    gtk_window_set_title(app->win, "Chapterizer devtool");
    gtk_window_set_default_size(app->win, 640, 480);
    /*
        g_signal_connect(gtk_builder_get_object(b, "hackbutton"),
                         "clicked",
                         G_CALLBACK(quitfunc),
                         static_cast<gpointer>(app));
                         */
    gtk_window_present(GTK_WINDOW(app->win));
    g_object_unref(b);
}

int main(int argc, char **argv) {
    App app;

    app.app = gtk_application_new("io.github.jpakkane.chapterizer", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app.app, "activate", G_CALLBACK(activate), static_cast<gpointer>(&app));
    int status = g_application_run(G_APPLICATION(app.app), argc, argv);
    g_object_unref(app.app);
    return status;
}
