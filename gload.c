#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

#include <sys/utsname.h>

#include <gtk/gtk.h>

/* ------------------------------------------------------------------------ */

typedef struct gload {
    GtkWidget *window;
    GtkWidget *label;
    GtkWidget *graph;
} gload;

/* ------------------------------------------------------------------------ */

static int gload_read()
{
    char line[80];
    int file, ret, val, pos;
    int vals[3] = {0, 0, 0};

    /* read proc file */
    file = open("/proc/loadavg", O_RDONLY);
    if (file < 0)
        return -1;
    ret = read(file, line, sizeof(line) - 1);
    if (ret < 0) {
        close(file);
        return -1;
    }
    line[ret] = 0;
    close(file);

    /* parse load values */
    for (val = 0, pos = 0;
         val < 3 && pos < ret;
         pos++) {
        if (line[pos] == ' ') {
            val++;
            continue;
        }
        if (line[pos] == '.') {
            continue;
        }
        if (line[pos] < '0'  || line[pos] > '9') {
            return -1;
        }
        vals[val] *= 10;
        vals[val] += line[pos] - '0';
    }
    if (val != 3)
        return -1;

    fprintf(stderr, "%s: %d %d %d\n", __func__,
            vals[0], vals[1], vals[2]);
    return 0;
}

static gboolean gload_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
//    gload *gl = data;
    GtkStyleContext *context;
    guint width, height;
    GdkRGBA color;

    context = gtk_widget_get_style_context(widget);
    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

#if 1 /* gtk doc example code */
    gtk_render_background(context, cr, 0, 0, width, height);

    cairo_arc(cr,
              width / 2.0, height / 2.0,
              MIN (width, height) / 2.0,
              0, 2 * G_PI);

    gtk_style_context_get_color(context,
                                gtk_style_context_get_state(context),
                                &color);
    gdk_cairo_set_source_rgba(cr, &color);
    cairo_fill(cr);
#endif

    return FALSE;
}

static void gload_window_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static gload *gload_new(void)
{
    struct utsname uts;
    GtkWidget *vbox;
    gload *gl = g_new0(gload, 1);

    gl->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(gl->window), "destroy",
                     G_CALLBACK(gload_window_destroy), gl);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(gl->window), vbox);

    uname(&uts);
    gl->label = gtk_label_new(uts.nodename);
    gtk_box_pack_start(GTK_BOX(vbox), gl->label, false, false, 0);

    gl->graph = gtk_drawing_area_new();
    gtk_widget_set_size_request(gl->graph, 100, 70);
    g_signal_connect(G_OBJECT(gl->graph), "draw",
                     G_CALLBACK(gload_draw), gl);
    gtk_box_pack_start(GTK_BOX(vbox), gl->graph, true, true, 0);

    gtk_widget_show_all(gl->window);
    return gl;
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    gload_new();
    gload_read();
    gtk_main();
    return 0;
}
