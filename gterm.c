#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <pwd.h>

#include <sys/types.h>

#include <gtk/gtk.h>
#include <vte/vte.h>

#define ARRAY_SIZE(_x) (sizeof(_x)/sizeof(_x[0]))

/* ------------------------------------------------------------------------ */

#define GTERM_CFG_FILENAME              ".config/gterm.conf"
#define GTERM_CFG_GROUP_DEFAULT         "default"
#define GTERM_CFG_GROUP_CMDLINE         "_cmdline_"

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
#define GTERM_CFG_KEY_PROFILE           "profile"
#define GTERM_CFG_KEY_FULLSCREEN        "fullscreen"
#define GTERM_CFG_KEY_VISUAL_BELL       "visualBell"

typedef struct gterm_opt {
    char *opt;
    char *key;
    bool is_bool;
} gterm_opt;

typedef enum gterm_bool {
    GTERM_BOOL_UNSET  = -1,
    GTERM_BOOL_FALSE  = 0,
    GTERM_BOOL_TRUE   = 1,
} gterm_bool;

static const gterm_opt gterm_opts[] = {
    { .opt = "fa",            .key = GTERM_CFG_KEY_FONT_FACE     },
    { .opt = "fs",            .key = GTERM_CFG_KEY_FONT_SIZE     },
    { .opt = "geometry",      .key = GTERM_CFG_KEY_GEOMETRY      },
    { .opt = "T",             .key = GTERM_CFG_KEY_TITLE         },
    { .opt = "title",         .key = GTERM_CFG_KEY_TITLE         },
    { .opt = "cr",            .key = GTERM_CFG_KEY_CURSOR_COLOR  },
    { .opt = "fg",            .key = GTERM_CFG_KEY_FOREGROUND    },
    { .opt = "bg",            .key = GTERM_CFG_KEY_BACKGROUND    },
    { .opt = "name",          .key = GTERM_CFG_KEY_PROFILE       },
    { .opt = "class",         .key = GTERM_CFG_KEY_PROFILE       },

    { .opt = "bc",            .key = GTERM_CFG_KEY_CURSOR_BLINK, .is_bool = true  },
    { .opt = "fullscreen",    .key = GTERM_CFG_KEY_FULLSCREEN,   .is_bool = true  },
    { .opt = "vb",            .key = GTERM_CFG_KEY_VISUAL_BELL,  .is_bool = true  },
};

static const gterm_opt *gterm_opt_find(char *arg)
{
    const gterm_opt *opt = NULL;
    int i;

    if (arg[0] != '-' && arg[0] != '+')
        return NULL;
    for (i = 0; i < ARRAY_SIZE(gterm_opts); i++) {
        if (strcmp(gterm_opts[i].opt, arg + 1) == 0) {
            opt = gterm_opts + i;
            break;
        }
    }
    if (!opt)
        return NULL;
    if (arg[0] == '+' && !opt->is_bool)
        return NULL;
    return opt;
}

static void gterm_cfg_set(GKeyFile *cfg, char *key, char *value)
{
    g_key_file_set_string(cfg, GTERM_CFG_GROUP_CMDLINE, key, value);
}

static char *gterm_cfg_get(GKeyFile *cfg, const char *key)
{
    char *profile;
    char *value;

    profile = g_key_file_get_string(cfg, GTERM_CFG_GROUP_CMDLINE,
                                    GTERM_CFG_KEY_PROFILE, NULL);

    value = g_key_file_get_string(cfg, GTERM_CFG_GROUP_CMDLINE, key, NULL);
    if (value)
        return value;

    if (profile) {
        value = g_key_file_get_string(cfg, profile, key, NULL);
        if (value)
            return value;
    }

    value = g_key_file_get_string(cfg, GTERM_CFG_GROUP_DEFAULT, key, NULL);
    if (value)
        return value;

    return NULL;
}

static gterm_bool gterm_cfg_get_bool(GKeyFile *cfg, const char *key)
{
    char *value;

    value = gterm_cfg_get(cfg, key);
    if (!value)
        return GTERM_BOOL_UNSET;

    if (strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "on") == 0)
        return GTERM_BOOL_TRUE;

    if (strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "off") == 0)
        return GTERM_BOOL_FALSE;

    return GTERM_BOOL_UNSET;
}

/* ------------------------------------------------------------------------ */

typedef struct gterm {
    GtkWidget *window;
    GtkWidget *terminal;
    GtkWidget *popup;

    GtkWidget *fullscreen;
    GtkWidget *bell;
    GSList *fontgrp;

    GKeyFile *cfg;
    GPid pid;
    gint exit_code;
} gterm;

static void gterm_spawn_cb(VteTerminal *terminal, GPid pid,
                           GError *error, gpointer user_data)
{
    gterm *gt = user_data;

    if (error) {
        fprintf(stderr, "ERROR: %s\n", error->message);
        gt->exit_code = 1;
        gtk_main_quit();
    } else {
        gt->pid = pid;
    }
}

static void gterm_spawn(gterm *gt, char *argv[])
{
#if VTE_CHECK_VERSION(0,48,0)
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
#else
    GError *error = NULL;
    GPid pid = -1;

    vte_terminal_spawn_sync(VTE_TERMINAL(gt->terminal),
                            VTE_PTY_DEFAULT,
                            NULL,
                            argv,
                            NULL,
                            G_SPAWN_SEARCH_PATH,
                            NULL,
                            NULL,
                            &pid,
                            NULL,
                            &error);
    gterm_spawn_cb(gt->terminal, pid, error, gt);
#endif
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
    gtk_main_quit();
}

static void gterm_vte_window_title_changed(VteTerminal *vteterminal,
                                           gpointer     user_data)
{
    gterm *gt = user_data;
    const char *str;

    str = vte_terminal_get_window_title(VTE_TERMINAL(gt->terminal));
    gtk_window_set_title(GTK_WINDOW(gt->window), str);
}

static gboolean gterm_vte_button_press_event(GtkWidget *widget,
                                             GdkEvent  *event,
                                             gpointer   user_data)
{
    GdkEventButton *btn = (GdkEventButton *)event;
    gterm *gt = user_data;

    if (btn->type != GDK_BUTTON_PRESS)
        return FALSE;
    if (!(btn->state & GDK_CONTROL_MASK))
        return FALSE;

    if (!(btn->button == 1 ||
          btn->button == 2 ||
          btn->button == 3))
        return FALSE;

    gtk_menu_popup_at_pointer(GTK_MENU(gt->popup), event);
    return TRUE;
}

static void gterm_vte_configure(gterm *gt)
{
    char *fontdesc;
    char *fontname;
    char *fontsize;
    char *str;
    gterm_bool b;
    gboolean state;
    GdkRGBA color;
    unsigned int cols, rows;

    fontname = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_FONT_FACE);
    fontsize = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_FONT_SIZE);
    if (fontname && fontsize) {
        fontdesc = g_strdup_printf("%s %s", fontname, fontsize);
    } else if (fontname) {
        fontdesc = g_strdup_printf("%s", fontname);
    } else if (fontsize) {
        fontdesc = g_strdup_printf("%s", fontsize);
    } else {
        fontdesc = NULL;
    }
    if (fontdesc) {
        PangoFontDescription *font;
        font = pango_font_description_from_string(fontdesc);
        vte_terminal_set_font(VTE_TERMINAL(gt->terminal), font);
        g_free(fontdesc);
    }

    str = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_GEOMETRY);
    if (str && sscanf(str, "%dx%d", &cols, &rows) == 2) {
        vte_terminal_set_size(VTE_TERMINAL(gt->terminal), cols, rows);
    }

    b = gterm_cfg_get_bool(gt->cfg, GTERM_CFG_KEY_CURSOR_BLINK);
    if (b == GTERM_BOOL_TRUE) {
        vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(gt->terminal),
                                           VTE_CURSOR_BLINK_ON);
    } else if (b == GTERM_BOOL_FALSE) {
        vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(gt->terminal),
                                           VTE_CURSOR_BLINK_OFF);
    }

    b = gterm_cfg_get_bool(gt->cfg, GTERM_CFG_KEY_VISUAL_BELL);
    if (b == GTERM_BOOL_TRUE) {
        state = false;
    } else if (b == GTERM_BOOL_FALSE) {
        state = true;
    } else {
        state = vte_terminal_get_audible_bell(VTE_TERMINAL(gt->terminal));
    }
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gt->bell), state);
    vte_terminal_set_audible_bell(VTE_TERMINAL(gt->terminal), state);

    str = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_CURSOR_COLOR);
    if (str) {
        gdk_rgba_parse(&color, str);
        vte_terminal_set_color_cursor(VTE_TERMINAL(gt->terminal), &color);
    }
    str = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_FOREGROUND);
    if (str) {
        gdk_rgba_parse(&color, str);
        vte_terminal_set_color_foreground(VTE_TERMINAL(gt->terminal), &color);
    }
    str = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_BACKGROUND);
    if (str) {
        gdk_rgba_parse(&color, str);
        vte_terminal_set_color_background(VTE_TERMINAL(gt->terminal), &color);
    }
}

/* ------------------------------------------------------------------------ */

static void gterm_menu_fullscreen(GtkCheckMenuItem *item,
                                  gpointer user_data)
{
    gterm *gt = user_data;

    if (gtk_check_menu_item_get_active(item)) {
        gtk_window_fullscreen(GTK_WINDOW(gt->window));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(gt->window));
    }
}

static void gterm_menu_bell(GtkCheckMenuItem *item,
                            gpointer user_data)
{
    gterm *gt = user_data;
    gboolean state;

    state = gtk_check_menu_item_get_active(item);
    vte_terminal_set_audible_bell(VTE_TERMINAL(gt->terminal), state);
}

static void gterm_menu_font(GtkCheckMenuItem *item,
                            gpointer user_data)
{
    gterm *gt = user_data;
    PangoFontDescription *font;
    gboolean state;
    const char *name;

    state = gtk_check_menu_item_get_active(item);
    if (state) {
        name = gtk_menu_item_get_label(GTK_MENU_ITEM(item));
        font = pango_font_description_from_string(name);
        vte_terminal_set_font(VTE_TERMINAL(gt->terminal), font);
    }
}

static void gterm_menu_reset(GtkMenuItem *item,
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
    GtkWidget *item;
    char *fontdesc;
    char *fontname;
    char *fontsize;
    int i;

    gt->fullscreen = gtk_check_menu_item_new_with_label("Fullscreen");
    g_signal_connect(G_OBJECT(gt->fullscreen), "toggled",
                     G_CALLBACK(gterm_menu_fullscreen), gt);
    gtk_container_add(GTK_CONTAINER(gt->popup), gt->fullscreen);

    gt->bell = gtk_check_menu_item_new_with_label("Audible bell");
    g_signal_connect(G_OBJECT(gt->bell), "toggled",
                     G_CALLBACK(gterm_menu_bell), gt);
    gtk_container_add(GTK_CONTAINER(gt->popup), gt->bell);

    item = gtk_separator_menu_item_new();
    gtk_container_add(GTK_CONTAINER(gt->popup), item);

    fontname = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_FONT_FACE);
    if (!fontname)
        fontname = "monospace";
    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        fontsize = gterm_cfg_get(gt->cfg, sizes[i]);
        if (!fontsize)
            continue;
        fontdesc = g_strdup_printf("%s %s", fontname, fontsize);
        item = gtk_radio_menu_item_new_with_label(gt->fontgrp, fontdesc);
        gt->fontgrp = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
        g_signal_connect(G_OBJECT(item), "toggled",
                         G_CALLBACK(gterm_menu_font), gt);
        gtk_container_add(GTK_CONTAINER(gt->popup), item);
        g_free(fontdesc);
    }

    item = gtk_separator_menu_item_new();
    gtk_container_add(GTK_CONTAINER(gt->popup), item);

    item = gtk_menu_item_new_with_label("Terminal reset");
    g_signal_connect(G_OBJECT(item), "activate",
                     G_CALLBACK(gterm_menu_reset), gt);
    gtk_container_add(GTK_CONTAINER(gt->popup), item);

    gtk_widget_show_all(gt->popup);
}

/* ------------------------------------------------------------------------ */

static void gterm_window_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static void gterm_window_configure(gterm *gt)
{
    gterm_bool b;
    char *str;

    str = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_TITLE);
    if (str) {
        gtk_window_set_title(GTK_WINDOW(gt->window), str);
    }

    b = gterm_cfg_get_bool(gt->cfg, GTERM_CFG_KEY_FULLSCREEN);
    if (b == GTERM_BOOL_TRUE) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gt->fullscreen), true);
    }
}

static gterm *gterm_new(GKeyFile *cfg)
{
    gterm *gt = g_new0(gterm, 1);

    gt->cfg = cfg;

    gt->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(gt->window), "destroy",
                     G_CALLBACK(gterm_window_destroy), gt);

    gt->terminal = vte_terminal_new();
    g_signal_connect(G_OBJECT(gt->terminal), "child-exited",
                     G_CALLBACK(gterm_vte_child_exited), gt);
    g_signal_connect(G_OBJECT(gt->terminal), "window-title-changed",
                     G_CALLBACK(gterm_vte_window_title_changed), gt);
    g_signal_connect(G_OBJECT(gt->terminal), "button-press-event",
                     G_CALLBACK(gterm_vte_button_press_event), gt);
    gtk_container_add(GTK_CONTAINER(gt->window), gt->terminal);

    gt->popup = gtk_menu_new();
    gterm_fill_menu(gt);

    gterm_window_configure(gt);
    gterm_vte_configure(gt);
    gtk_widget_show_all(gt->window);

    return gt;
}

int main(int argc, char *argv[])
{
    char *filename;
    GKeyFile *cfg;
    gterm *gt;
    const gterm_opt *opt;
    int i, eopt = 0;

    gtk_init(&argc, &argv);

    cfg = g_key_file_new();
    filename = g_strdup_printf("%s/%s", getenv("HOME"), GTERM_CFG_FILENAME);
    g_key_file_load_from_file(cfg, filename, G_KEY_FILE_NONE, NULL);
    g_free(filename);

    for (i = 1; i < argc;) {
        if (strcmp(argv[i], "-e") == 0) {
            eopt = i + 1;
            break;
        }
        opt = gterm_opt_find(argv[i]);
        if (!opt) {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            exit(1);
        }
        if (opt->is_bool) {
            if (argv[i][0] == '-')
                gterm_cfg_set(cfg, opt->key, "true");
            else
                gterm_cfg_set(cfg, opt->key, "false");
            i++;
        } else {
            if (i + 1 == argc) {
                fprintf(stderr, "missing argument for: %s\n", argv[i]);
                exit(1);
            }
            gterm_cfg_set(cfg, opt->key, argv[i+1]);
            i += 2;
        }
    }

    gt = gterm_new(cfg);
    if (eopt) {
        gterm_spawn(gt, argv + eopt);
    } else {
        gterm_spawn_shell(gt);
    }

    gtk_main();
    return gt->exit_code;
}
