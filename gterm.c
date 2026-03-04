#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <gtk/gtk.h>
#include <vte/vte.h>

#include "gcfg.h"

/* ------------------------------------------------------------------------ */

#define GTERM_CFG_FILENAME              ".config/gterm.conf"

#define GTERM_CFG_KEY_FONT_FACE         "faceName"
#define GTERM_CFG_KEY_FONT_SIZE         "faceSize"
#define GTERM_CFG_KEY_FONT_SIZE_1       "faceSize1"
#define GTERM_CFG_KEY_FONT_SIZE_2       "faceSize2"
#define GTERM_CFG_KEY_FONT_SIZE_3       "faceSize3"
#define GTERM_CFG_KEY_FONT_SIZE_4       "faceSize4"
#define GTERM_CFG_KEY_FONT_SIZE_5       "faceSize5"
#define GTERM_CFG_KEY_FONT_SIZE_6       "faceSize6"
#define GTERM_CFG_KEY_GEOMETRY          "geometry"
#define GTERM_CFG_KEY_TITLE             "title"
#define GTERM_CFG_KEY_CURSOR_BLINK      "cursorBlink"
#define GTERM_CFG_KEY_CURSOR_COLOR      "cursorColor"
#define GTERM_CFG_KEY_FOREGROUND        "foreground"
#define GTERM_CFG_KEY_BACKGROUND        "background"
#define GTERM_CFG_KEY_FULLSCREEN        "fullscreen"
#define GTERM_CFG_KEY_VISUAL_BELL       "visualBell"
#define GTERM_CFG_KEY_SCROLLBACK_LINES  "saveLines"

static const gcfg_opt gterm_opts[] = {
    { .opt = "fa",            .key = GTERM_CFG_KEY_FONT_FACE     },
    { .opt = "fs",            .key = GTERM_CFG_KEY_FONT_SIZE     },
    { .opt = "geometry",      .key = GTERM_CFG_KEY_GEOMETRY      },
    { .opt = "T",             .key = GTERM_CFG_KEY_TITLE         },
    { .opt = "title",         .key = GTERM_CFG_KEY_TITLE         },
    { .opt = "cr",            .key = GTERM_CFG_KEY_CURSOR_COLOR  },
    { .opt = "fg",            .key = GTERM_CFG_KEY_FOREGROUND    },
    { .opt = "bg",            .key = GTERM_CFG_KEY_BACKGROUND    },
    { .opt = "name",          .key = GCFG_KEY_PROFILE            },
    { .opt = "class",         .key = GCFG_KEY_PROFILE            },
    { .opt = "sl",            .key = GTERM_CFG_KEY_SCROLLBACK_LINES },

    { .opt = "bc",            .key = GTERM_CFG_KEY_CURSOR_BLINK, .is_bool = true  },
    { .opt = "fullscreen",    .key = GTERM_CFG_KEY_FULLSCREEN,   .is_bool = true  },
    { .opt = "vb",            .key = GTERM_CFG_KEY_VISUAL_BELL,  .is_bool = true  },
};

/* ------------------------------------------------------------------------ */

typedef struct gterm {
    GtkWidget *window;
    GtkWidget *terminal;
    GtkWidget *popup;

    GtkWidget *fullscreen;
    GtkWidget *bell;
    GtkCheckButton *fontgrp;

    GKeyFile *cfg;
    GPid pid;
    gint exit_code;
    GMainLoop *loop;
} gterm;

static void gterm_spawn_cb(VteTerminal *terminal, GPid pid,
                           GError *error, gpointer user_data)
{
    gterm *gt = user_data;

    if (error) {
        fprintf(stderr, "ERROR: %s\n", error->message);
        gt->exit_code = 1;
        g_main_loop_quit(gt->loop);
    } else {
        gt->pid = pid;
    }
}

static void gterm_spawn(gterm *gt, char *argv[])
{
    vte_terminal_spawn_async(VTE_TERMINAL(gt->terminal),
                             VTE_PTY_DEFAULT,
                             NULL,
                             argv,
                             NULL,
                             G_SPAWN_SEARCH_PATH,
                             NULL,
                             NULL,
                             NULL,
                             -1,
                             NULL,
                             gterm_spawn_cb,
                             gt);
}

static void gterm_spawn_shell(gterm *gt)
{
    struct passwd *pwent;
    char *argv[2];

    pwent = getpwent();
    argv[0] = strdup(pwent->pw_shell);
    argv[1] = NULL;
    gterm_spawn(gt, argv);
}

static void gterm_vte_child_exited(VteTerminal *vteterminal,
                                   gint         status,
                                   gpointer     user_data)
{
    gterm *gt = user_data;

    if (WIFEXITED(status))
        gt->exit_code = WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        gt->exit_code = 1;
    g_main_loop_quit(gt->loop);
}

static void gterm_vte_window_title_changed(VteTerminal *vteterminal,
                                           gpointer     user_data)
{
    gterm *gt = user_data;
    const char *str;

    str = vte_terminal_get_window_title(VTE_TERMINAL(gt->terminal));
    gtk_window_set_title(GTK_WINDOW(gt->window), str);
}

static void gterm_vte_button_press(GtkGestureClick *gesture,
                                   gint n_press,
                                   gdouble x,
                                   gdouble y,
                                   gpointer user_data)
{
    GdkModifierType state;
    guint button;
    gterm *gt = user_data;
    graphene_rect_t point_to;

    if (n_press != 1)
        return;

    button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    if (!(button == 1 || button == 2 || button == 3))
        return;

    state = gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
    if (!(state & GDK_CONTROL_MASK))
        return;

    graphene_rect_init(&point_to, x, y, 1, 1);
    gtk_popover_set_pointing_to(GTK_POPOVER(gt->popup), &point_to);
    gtk_popover_popup(GTK_POPOVER(gt->popup));
}

static void gterm_vte_configure(gterm *gt)
{
    char *fontdesc;
    char *fontname;
    char *fontsize;
    char *str;
    gcfg_bool b;
    gboolean state;
    GdkRGBA color;
    unsigned int cols, rows;

    fontname = gcfg_get(gt->cfg, GTERM_CFG_KEY_FONT_FACE);
    fontsize = gcfg_get(gt->cfg, GTERM_CFG_KEY_FONT_SIZE);
    if (fontname && fontsize) {
        fontdesc = g_strdup_printf("%s %s", fontname, fontsize);
    } else if (fontname) {
        fontdesc = g_strdup_printf("%s", fontname);
    } else if (fontsize) {
        fontdesc = g_strdup_printf("mono %s", fontsize);
    } else {
        fontdesc = NULL;
    }
    if (fontdesc) {
        PangoFontDescription *font;
        font = pango_font_description_from_string(fontdesc);
        vte_terminal_set_font(VTE_TERMINAL(gt->terminal), font);
        g_free(fontdesc);
    }

    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_GEOMETRY);
    if (str && sscanf(str, "%dx%d", &cols, &rows) == 2) {
        vte_terminal_set_size(VTE_TERMINAL(gt->terminal), cols, rows);
    }

    b = gcfg_get_bool(gt->cfg, GTERM_CFG_KEY_CURSOR_BLINK);
    if (b == GCFG_BOOL_TRUE) {
        vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(gt->terminal),
                                           VTE_CURSOR_BLINK_ON);
    } else if (b == GCFG_BOOL_FALSE) {
        vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(gt->terminal),
                                           VTE_CURSOR_BLINK_OFF);
    }

    b = gcfg_get_bool(gt->cfg, GTERM_CFG_KEY_VISUAL_BELL);
    if (b == GCFG_BOOL_TRUE) {
        state = false;
    } else if (b == GCFG_BOOL_FALSE) {
        state = true;
    } else {
        state = vte_terminal_get_audible_bell(VTE_TERMINAL(gt->terminal));
    }
    gtk_check_button_set_active(GTK_CHECK_BUTTON(gt->bell), state);
    vte_terminal_set_audible_bell(VTE_TERMINAL(gt->terminal), state);

    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_CURSOR_COLOR);
    if (str) {
        gdk_rgba_parse(&color, str);
        vte_terminal_set_color_cursor(VTE_TERMINAL(gt->terminal), &color);
    }
    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_FOREGROUND);
    if (str) {
        gdk_rgba_parse(&color, str);
        vte_terminal_set_color_foreground(VTE_TERMINAL(gt->terminal), &color);
    }
    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_BACKGROUND);
    if (str) {
        gdk_rgba_parse(&color, str);
        vte_terminal_set_color_background(VTE_TERMINAL(gt->terminal), &color);
    }
    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_SCROLLBACK_LINES);
    if (str) {
        vte_terminal_set_scrollback_lines(VTE_TERMINAL(gt->terminal),
                                          atoi(str));
    }
}

static void gterm_vte_geometry_hints(gterm *gt)
{
    /*
     * GtkWindow geometry hints from GTK3 were removed in GTK4.
     * Keep this helper as a no-op so existing callsites still
     * document intent without relying on removed APIs.
     */
    (void)gt;
}

/* ------------------------------------------------------------------------ */

static void gterm_menu_fullscreen(GtkCheckButton *item,
                                  gpointer user_data)
{
    gterm *gt = user_data;

    if (gtk_check_button_get_active(item)) {
        gtk_window_fullscreen(GTK_WINDOW(gt->window));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(gt->window));
    }
}

static void gterm_menu_bell(GtkCheckButton *item,
                            gpointer user_data)
{
    gterm *gt = user_data;
    gboolean state;

    state = gtk_check_button_get_active(item);
    vte_terminal_set_audible_bell(VTE_TERMINAL(gt->terminal), state);
}

static void gterm_menu_font(GtkCheckButton *item,
                            gpointer user_data)
{
    gterm *gt = user_data;
    PangoFontDescription *font;
    gboolean state;
    const char *name;

    state = gtk_check_button_get_active(item);
    if (!state)
        return;

    name = gtk_check_button_get_label(item);
    font = pango_font_description_from_string(name);
    vte_terminal_set_font(VTE_TERMINAL(gt->terminal), font);
    gterm_vte_geometry_hints(gt);
}

static void gterm_menu_reset(GtkButton *item,
                             gpointer user_data)
{
    gterm *gt = user_data;

    vte_terminal_reset(VTE_TERMINAL(gt->terminal), true, true);
}

static void gterm_fill_menu(gterm *gt)
{
    static const char *sizes[] = {
        GTERM_CFG_KEY_FONT_SIZE,
        GTERM_CFG_KEY_FONT_SIZE_1,
        GTERM_CFG_KEY_FONT_SIZE_2,
        GTERM_CFG_KEY_FONT_SIZE_3,
        GTERM_CFG_KEY_FONT_SIZE_4,
        GTERM_CFG_KEY_FONT_SIZE_5,
        GTERM_CFG_KEY_FONT_SIZE_6,
    };
    GtkWidget *menu_box;
    GtkWidget *item;
    char *fontdesc;
    char *fontname;
    char *fontsize;
    int i;

    menu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gt->fullscreen = gtk_check_button_new_with_label("Fullscreen");
    g_signal_connect(G_OBJECT(gt->fullscreen), "toggled",
                     G_CALLBACK(gterm_menu_fullscreen), gt);
    gtk_box_append(GTK_BOX(menu_box), gt->fullscreen);

    gt->bell = gtk_check_button_new_with_label("Audible bell");
    g_signal_connect(G_OBJECT(gt->bell), "toggled",
                     G_CALLBACK(gterm_menu_bell), gt);
    gtk_box_append(GTK_BOX(menu_box), gt->bell);

    item = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(menu_box), item);

    fontname = gcfg_get(gt->cfg, GTERM_CFG_KEY_FONT_FACE);
    if (!fontname)
        fontname = "monospace";
    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        fontsize = gcfg_get(gt->cfg, sizes[i]);
        if (!fontsize)
            continue;
        fontdesc = g_strdup_printf("%s %s", fontname, fontsize);
        item = gtk_check_button_new_with_label(fontdesc);
        gtk_check_button_set_group(GTK_CHECK_BUTTON(item), gt->fontgrp);
        gt->fontgrp = GTK_CHECK_BUTTON(item);
        g_signal_connect(G_OBJECT(item), "toggled",
                         G_CALLBACK(gterm_menu_font), gt);
        gtk_box_append(GTK_BOX(menu_box), item);
        g_free(fontdesc);
    }

    item = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(menu_box), item);

    item = gtk_button_new_with_label("Terminal reset");
    g_signal_connect(G_OBJECT(item), "clicked",
                     G_CALLBACK(gterm_menu_reset), gt);
    gtk_box_append(GTK_BOX(menu_box), item);

    gtk_popover_set_child(GTK_POPOVER(gt->popup), menu_box);
}

/* ------------------------------------------------------------------------ */

static gboolean gterm_window_close_request(GtkWindow *window, gpointer data)
{
    gterm *gt = data;

    g_main_loop_quit(gt->loop);
    return FALSE;
}

static void gterm_window_configure(gterm *gt)
{
    gcfg_bool b;
    char *str;

    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_TITLE);
    if (str) {
        gtk_window_set_title(GTK_WINDOW(gt->window), str);
    }

    b = gcfg_get_bool(gt->cfg, GTERM_CFG_KEY_FULLSCREEN);
    if (b == GCFG_BOOL_TRUE) {
        gtk_check_button_set_active(GTK_CHECK_BUTTON(gt->fullscreen), true);
    }
}

static gterm *gterm_new(GKeyFile *cfg)
{
    GtkGesture *click;
    gterm *gt = g_new0(gterm, 1);

    gt->cfg = cfg;
    gt->loop = g_main_loop_new(NULL, FALSE);

    gt->window = gtk_window_new();
    g_signal_connect(G_OBJECT(gt->window), "close-request",
                     G_CALLBACK(gterm_window_close_request), gt);

    gt->terminal = vte_terminal_new();
    g_signal_connect(G_OBJECT(gt->terminal), "child-exited",
                     G_CALLBACK(gterm_vte_child_exited), gt);
    g_signal_connect(G_OBJECT(gt->terminal), "window-title-changed",
                     G_CALLBACK(gterm_vte_window_title_changed), gt);

    click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 0);
    g_signal_connect(G_OBJECT(click), "pressed",
                     G_CALLBACK(gterm_vte_button_press), gt);
    gtk_widget_add_controller(gt->terminal, GTK_EVENT_CONTROLLER(click));

    gtk_window_set_child(GTK_WINDOW(gt->window), gt->terminal);

    gt->popup = gtk_popover_new();
    gtk_widget_set_parent(gt->popup, gt->terminal);
    gterm_fill_menu(gt);

    gterm_window_configure(gt);
    gterm_vte_configure(gt);
    gterm_vte_geometry_hints(gt);
    gtk_window_present(GTK_WINDOW(gt->window));

    return gt;
}

int main(int argc, char *argv[])
{
    char *filename;
    GKeyFile *cfg;
    gterm *gt;
    const gcfg_opt *opt;
    int i, eopt = 0;

    gtk_init();

    cfg = g_key_file_new();
    filename = g_strdup_printf("%s/%s", getenv("HOME"), GTERM_CFG_FILENAME);
    g_key_file_load_from_file(cfg, filename, G_KEY_FILE_NONE, NULL);
    g_free(filename);

    for (i = 1; i < argc;) {
        if (strcmp(argv[i], "-e") == 0) {
            eopt = i + 1;
            break;
        }
        opt = gcfg_opt_find(gterm_opts, ARRAY_SIZE(gterm_opts), argv[i]);
        if (!opt) {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            exit(1);
        }
        if (opt->is_bool) {
            if (argv[i][0] == '-')
                gcfg_set(cfg, opt->key, "true");
            else
                gcfg_set(cfg, opt->key, "false");
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

    gt = gterm_new(cfg);
    if (eopt) {
        if (!gcfg_get(cfg, GTERM_CFG_KEY_TITLE))
            gtk_window_set_title(GTK_WINDOW(gt->window), argv[eopt]);
        gterm_spawn(gt, argv + eopt);
    } else {
        gterm_spawn_shell(gt);
    }

    g_main_loop_run(gt->loop);
    return gt->exit_code;
}
