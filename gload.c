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

    int *load1;
    uint32_t used, total;
} gload;

/* ------------------------------------------------------------------------ */

static void gload_resize(gload *gl, uint32_t size)
{
    int *ptr;

    if (size < gl->total)
        return;

    ptr = calloc(size, sizeof(int));
    memcpy(ptr, gl->load1, gl->total * sizeof(int));
    free(gl->load1);
    gl->load1 = ptr;
    gl->total = size;
}

static void gload_store(gload *gl, int load1)
{
    if (!gl->total)
        gload_resize(gl, 10);
    if (gl->used == gl->total) {
        gl->used--;
        memmove(gl->load1, gl->load1 + 1, gl->used * sizeof(int));
    }
    gl->load1[gl->used++] = load1;
}

static void gload_read(gload *gl)
{
    char line[80];
    int file, ret, val, pos;
    int vals[3] = {0, 0, 0};

    /* read proc file */
    file = open("/proc/loadavg", O_RDONLY);
    if (file < 0)
        return;
    ret = read(file, line, sizeof(line) - 1);
    if (ret < 0) {
        close(file);
        return;
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
            return;
        }
        vals[val] *= 10;
        vals[val] += line[pos] - '0';
    }
    if (val != 3)
        return;

    /* store value */
    gload_store(gl, vals[0]);
}

static gboolean gload_timer(gpointer user_data)
{
    gload *gl = user_data;

    gload_read(gl);
    gtk_widget_queue_draw(gl->graph);
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------------ */

static int gload_brightness(GdkRGBA *color)
{
    /* scale: 0 -> 100 */
    return (color->red * 30 +
            color->green * 60 +
            color->blue * 10);
}

static void gload_darken(GdkRGBA *src, GdkRGBA *dst, int percent)
{
    dst->red   = src->red   - src->red   * percent / 100;
    dst->green = src->green - src->green * percent / 100;
    dst->blue  = src->blue  - src->blue  * percent / 100;
    dst->alpha = 1;
}

static void gload_lighten(GdkRGBA *src, GdkRGBA *dst, int percent)
{
    dst->red   = src->red   + (1-src->red)   * percent / 100;
    dst->green = src->green + (1-src->green) * percent / 100;
    dst->blue  = src->blue  + (1-src->blue)  * percent / 100;
    dst->alpha = 1;
}

static gboolean gload_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    gload *gl = data;
    GtkStyleContext *context;
    GdkRGBA normal, dimmed;
    guint width, height, i, idx, max;
    bool darkmode;

    context = gtk_widget_get_style_context(widget);
    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_style_context_get_color(context, GTK_STATE_FLAG_NORMAL, &normal);
    darkmode = gload_brightness(&normal) > 50;
    if (darkmode)
        gload_darken(&normal, &dimmed, 30);
    else
        gload_lighten(&normal, &dimmed, 30);

    gload_resize(gl, width);
    for (i = 0, max = 0; i < gl->used && i < width; i++) {
        idx = gl->used > width ? i + gl->used - width : i;
        if (max < gl->load1[idx])
            max = gl->load1[idx];
    }
    max += 100;
    max -= (max % 100);

    cairo_set_line_width(cr, 1);
    gdk_cairo_set_source_rgba(cr, &dimmed);
    for (i = 0; i < gl->used && i < width; i++) {
        idx = gl->used > width ? i + gl->used - width : i;
        cairo_move_to(cr, i - 0.5, (max - gl->load1[idx]) * height / max - 0.5);
        cairo_line_to(cr, i - 0.5, height - 0.5);
    }
    cairo_stroke(cr);

    gdk_cairo_set_source_rgba(cr, &normal);
    for (i = 100; i < max; i += 100) {
        int y = (max - i) * height / max;
        cairo_move_to(cr, 0, y + 0.5);
        cairo_line_to(cr, width, y + 0.5);
    }
    cairo_stroke(cr);

    return FALSE;
}

/* ------------------------------------------------------------------------ */

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
    gtk_label_set_xalign(GTK_LABEL(gl->label), 0);
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
    gload *gl;

    gtk_init(&argc, &argv);

    gl = gload_new();
    gload_read(gl);
    g_timeout_add_seconds(10, gload_timer, gl);

    gtk_main();
    return 0;
}
