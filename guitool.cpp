#include <gtk/gtk.h>

struct App {
    GtkApplication *app;
    GtkWidget *win;
};

static void activate(GtkApplication *, gpointer user_data) {
    auto *app = static_cast<App *>(user_data);
    app->win = gtk_application_window_new(app->app);
    gtk_window_set_title(GTK_WINDOW(app->win), "Chapterizer devtool");
    gtk_window_set_default_size(GTK_WINDOW(app->win), 640, 480);

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
