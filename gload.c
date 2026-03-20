#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

#include <sys/utsname.h>

#include <gtk/gtk.h>

#include "gcfg.h"

/* ------------------------------------------------------------------------ */

#define GLOAD_CFG_FILENAME              ".config/gload.conf"

#define GLOAD_CFG_KEY_LABEL             "label"
#define GLOAD_CFG_KEY_UPDATE            "update"
#define GLOAD_CFG_KEY_HIGHLIGHT         "highlight"
#define GLOAD_CFG_KEY_FONTNAME          "fontname"
#define GLOAD_CFG_KEY_ALPHA             "alpha"

static const gcfg_opt gload_opts[] = {
    /* xload style */
    { .opt = "label",         .key = GLOAD_CFG_KEY_LABEL         },
    { .opt = "update",        .key = GLOAD_CFG_KEY_UPDATE        },
    { .opt = "name",          .key = GCFG_KEY_PROFILE            },
    { .opt = "class",         .key = GCFG_KEY_PROFILE            },
    { .opt = "hl",            .key = GLOAD_CFG_KEY_HIGHLIGHT     },
    { .opt = "highlight",     .key = GLOAD_CFG_KEY_HIGHLIGHT     },

    /* gload only */
    { .opt = "font",          .key = GLOAD_CFG_KEY_FONTNAME      },
    { .opt = "alpha",         .key = GLOAD_CFG_KEY_ALPHA         },
};

/* ------------------------------------------------------------------------ */

typedef struct gload {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *label;
    GtkWidget *graph;

    int *load1;
    uint32_t used, total;

    GKeyFile *cfg;
} gload;

/* ------------------------------------------------------------------------ */

static void gload_resize(gload *gl, uint32_t size)
{
    int *ptr;

    if (size <= gl->total)
        return;

    ptr = calloc(size, sizeof(int));
    if (gl->load1) {
        memcpy(ptr, gl->load1, gl->total * sizeof(int));
        free(gl->load1);
    }
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
    if (gl->graph)
        gtk_widget_queue_draw(gl->graph);
    return G_SOURCE_CONTINUE;
}

/* ------------------------------------------------------------------------ */

static void gload_draw(GtkDrawingArea *drawing_area,
                       cairo_t        *cr,
                       int             width,
                       int             height,
                       gpointer        user_data)
{
    gload *gl = user_data;
    GdkRGBA normal, dimmed;
    char *highlight, *alpha;
    guint i, idx, max;

    highlight = gcfg_get(gl->cfg, GLOAD_CFG_KEY_HIGHLIGHT);
    if (highlight) {
        gdk_rgba_parse(&normal, highlight);
        g_free(highlight);
    } else {
        normal.red = 0; normal.green = 0; normal.blue = 0; normal.alpha = 1.0;
    }
    normal.alpha = 1.0;

    dimmed = normal;
    alpha = gcfg_get(gl->cfg, GLOAD_CFG_KEY_ALPHA);
    if (alpha) {
        dimmed.alpha = atoi(alpha) / 100.0;
        g_free(alpha);
    } else {
        dimmed.alpha = 0.6;
    }

    gload_resize(gl, width);
    for (i = 0, max = 0; i < gl->used && i < (guint)width; i++) {
        idx = gl->used > (guint)width ? i + gl->used - width : i;
        if (max < (guint)gl->load1[idx])
            max = gl->load1[idx];
    }
    max += 100;
    max -= (max % 100);

    cairo_set_line_width(cr, 1);
    gdk_cairo_set_source_rgba(cr, &dimmed);
    for (i = 0; i < gl->used && i < (guint)width; i++) {
        idx = gl->used > (guint)width ? i + gl->used - width : i;
        cairo_move_to(cr, i - 0.5, (max - gl->load1[idx]) * (double)height / max - 0.5);
        cairo_line_to(cr, i - 0.5, height - 0.5);
    }
    cairo_stroke(cr);

    gdk_cairo_set_source_rgba(cr, &normal);
    for (i = 100; i < max; i += 100) {
        double y = (max - i) * (double)height / max;
        cairo_move_to(cr, 0, y - 0.5);
        cairo_line_to(cr, width, y - 0.5);
    }
    cairo_stroke(cr);
}

/* ------------------------------------------------------------------------ */

static void gload_window_destroy(GtkWidget *widget, gpointer data)
{
    gload *gl = data;
    if (gl->app)
        g_application_quit(G_APPLICATION(gl->app));
}

static void gload_app_activate(GApplication *app, gpointer user_data)
{
    gload *gl = user_data;
    struct utsname uts;
    GtkWidget *vbox;
    char *label, *fontname, *highlight;
    char *markup;

    if (gl->window) {
        gtk_window_present(GTK_WINDOW(gl->window));
        return;
    }

    gl->window = gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_title(GTK_WINDOW(gl->window), "gload");
    g_signal_connect(G_OBJECT(gl->window), "destroy",
                     G_CALLBACK(gload_window_destroy), gl);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(gl->window), vbox);

    label = gcfg_get(gl->cfg, GLOAD_CFG_KEY_LABEL);
    if (!label) {
        uname(&uts);
        label = g_strdup(uts.nodename);
    }
    gl->label = gtk_label_new(label);
    if (gl->label) {
        gtk_label_set_xalign(GTK_LABEL(gl->label), 0);
        gtk_box_append(GTK_BOX(vbox), gl->label);

        fontname = gcfg_get(gl->cfg, GLOAD_CFG_KEY_FONTNAME);
        highlight = gcfg_get(gl->cfg, GLOAD_CFG_KEY_HIGHLIGHT);
        markup = g_strdup_printf("<span%s%s%s%s%s%s>%s</span>",
                                 fontname  ? " font='"  : "",
                                 fontname  ? fontname   : "",
                                 fontname  ? "'"        : "",
                                 highlight ? " color='" : "",
                                 highlight ? highlight  : "",
                                 highlight ? "'"        : "",
                                 label ? label : "");
        if (markup) {
            gtk_label_set_markup(GTK_LABEL(gl->label), markup);
            g_free(markup);
        }
        g_free(fontname);
        g_free(highlight);
    }
    g_free(label);

    gl->graph = gtk_drawing_area_new();
    gtk_widget_set_size_request(gl->graph, 200, 100);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(gl->graph), gload_draw, gl, NULL);
    gtk_box_append(GTK_BOX(vbox), gl->graph);
    gtk_widget_set_vexpand(gl->graph, TRUE);

    gtk_window_present(GTK_WINDOW(gl->window));
}

int main(int argc, char *argv[])
{
    char *filename;
    GKeyFile *cfg;
    gload *gl;
    const gcfg_opt *opt;
    char *valstr;
    int i, value;

    gl = g_new0(gload, 1);

    cfg = g_key_file_new();
    filename = g_strdup_printf("%s/%s", g_get_home_dir(), GLOAD_CFG_FILENAME);
    if (filename) {
        g_key_file_load_from_file(cfg, filename, G_KEY_FILE_NONE, NULL);
        g_free(filename);
    }
    gl->cfg = cfg;

    for (i = 1; i < argc;) {
        opt = gcfg_opt_find(gload_opts, ARRAY_SIZE(gload_opts), argv[i]);
        if (!opt) {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            exit(1);
        }
        if (opt->is_bool) {
            if (argv[i][0] == '-')
                gcfg_set(cfg, opt->key, "true");
            i++;
        } else {
            if (i + 1 == argc) {
                fprintf(stderr, "missing argument for: %s\n", argv[i]);
                exit(1);
            }
            gcfg_set(cfg, opt->key, argv[i+1]);
            i += 2;
        }
    }

    gl->app = gtk_application_new("org.gterm.load", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(gl->app, "activate", G_CALLBACK(gload_app_activate), gl);

    gload_read(gl);

    valstr = gcfg_get(gl->cfg, GLOAD_CFG_KEY_UPDATE);
    value = valstr ? atoi(valstr) : 10;
    g_free(valstr);
    g_timeout_add_seconds(value, gload_timer, gl);

    g_application_run(G_APPLICATION(gl->app), 0, NULL);

    free(gl->load1);
    g_key_file_free(gl->cfg);
    g_object_unref(gl->app);
    g_free(gl);
    return 0;
}
