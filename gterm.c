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
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *terminal;
    GtkWidget *popover;

    GSimpleActionGroup *actions;
    GMenu *menu;

    GKeyFile *cfg;
    GPid pid;
    gint exit_code;
} gterm;

typedef struct gterm_spawn_data {
    gterm *gt;
    char **argv;
} gterm_spawn_data;

static void gterm_spawn_cb(VteTerminal *terminal, GPid pid,
                           GError *error, gpointer user_data)
{
    gterm_spawn_data *sd = user_data;
    gterm *gt = sd->gt;

    if (error) {
        fprintf(stderr, "ERROR: %s\n", error->message);
        gt->exit_code = 1;
        g_application_quit(G_APPLICATION(gt->app));
    } else {
        gt->pid = pid;
    }
    g_strfreev(sd->argv);
    g_free(sd);
}

static void gterm_spawn(gterm *gt, char *argv[])
{
    gterm_spawn_data *sd = g_new0(gterm_spawn_data, 1);
    sd->gt = gt;
    sd->argv = g_strdupv(argv);

    vte_terminal_spawn_async(VTE_TERMINAL(gt->terminal),
                             VTE_PTY_DEFAULT,
                             NULL,
                             sd->argv,
                             NULL,
                             G_SPAWN_SEARCH_PATH,
                             NULL,
                             NULL,
                             NULL,
                             -1,
                             NULL,
                             gterm_spawn_cb,
                             sd);
}

static void gterm_spawn_shell(gterm *gt)
{
    struct passwd *pwent;
    const char *shell;
    char *argv[2];

    shell = getenv("SHELL");
    if (!shell) {
        pwent = getpwuid(getuid());
        if (pwent)
            shell = pwent->pw_shell;
    }
    if (!shell)
        shell = "/bin/sh";

    argv[0] = (char*)shell;
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
    g_application_quit(G_APPLICATION(gt->app));
}

static void gterm_vte_window_title_changed(VteTerminal *vteterminal,
                                           gpointer     user_data)
{
    gterm *gt = user_data;
    const char *str;

    str = vte_terminal_get_window_title(VTE_TERMINAL(gt->terminal));
    if (str) {
        gtk_window_set_title(GTK_WINDOW(gt->window), str);
    }
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
        pango_font_description_free(font);
        g_free(fontdesc);
    }
    g_free(fontname);
    g_free(fontsize);

    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_GEOMETRY);
    if (str) {
        if (sscanf(str, "%dx%d", &cols, &rows) == 2) {
            vte_terminal_set_size(VTE_TERMINAL(gt->terminal), cols, rows);
        }
        g_free(str);
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

    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(gt->actions), "bell");
    if (action) {
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(state));
    }
    vte_terminal_set_audible_bell(VTE_TERMINAL(gt->terminal), state);

    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_CURSOR_COLOR);
    if (str) {
        if (gdk_rgba_parse(&color, str))
            vte_terminal_set_color_cursor(VTE_TERMINAL(gt->terminal), &color);
        g_free(str);
    }
    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_FOREGROUND);
    if (str) {
        if (gdk_rgba_parse(&color, str))
            vte_terminal_set_color_foreground(VTE_TERMINAL(gt->terminal), &color);
        g_free(str);
    }
    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_BACKGROUND);
    if (str) {
        if (gdk_rgba_parse(&color, str))
            vte_terminal_set_color_background(VTE_TERMINAL(gt->terminal), &color);
        g_free(str);
    }
    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_SCROLLBACK_LINES);
    if (str) {
        vte_terminal_set_scrollback_lines(VTE_TERMINAL(gt->terminal),
                                          atoi(str));
        g_free(str);
    }
}

/* ------------------------------------------------------------------------ */

static void gterm_menu_fullscreen_cb(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    gterm *gt = user_data;
    gboolean fullscreen = g_variant_get_boolean(state);

    if (fullscreen) {
        gtk_window_fullscreen(GTK_WINDOW(gt->window));
    } else {
        gtk_window_unfullscreen(GTK_WINDOW(gt->window));
    }
    g_simple_action_set_state(action, state);
}

static void gterm_menu_bell_cb(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    gterm *gt = user_data;
    gboolean audible = g_variant_get_boolean(state);

    vte_terminal_set_audible_bell(VTE_TERMINAL(gt->terminal), audible);
    g_simple_action_set_state(action, state);
}

static void gterm_menu_font_cb(GSimpleAction *action, GVariant *state, gpointer user_data)
{
    gterm *gt = user_data;
    const char *name = g_variant_get_string(state, NULL);
    PangoFontDescription *font;

    font = pango_font_description_from_string(name);
    vte_terminal_set_font(VTE_TERMINAL(gt->terminal), font);
    pango_font_description_free(font);

    g_simple_action_set_state(action, state);
}

static void gterm_menu_reset_cb(GSimpleAction *action, GVariant *parameter, gpointer user_data)
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
    char *fontname;
    char *fontsize;
    int i;

    gt->actions = g_simple_action_group_new();

    GSimpleAction *fullscreen_action = g_simple_action_new_stateful("fullscreen", NULL, g_variant_new_boolean(FALSE));
    g_signal_connect(fullscreen_action, "change-state", G_CALLBACK(gterm_menu_fullscreen_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->actions), G_ACTION(fullscreen_action));

    GSimpleAction *bell_action = g_simple_action_new_stateful("bell", NULL, g_variant_new_boolean(TRUE));
    g_signal_connect(bell_action, "change-state", G_CALLBACK(gterm_menu_bell_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->actions), G_ACTION(bell_action));

    GSimpleAction *font_action = g_simple_action_new_stateful("font", G_VARIANT_TYPE_STRING, g_variant_new_string(""));
    g_signal_connect(font_action, "change-state", G_CALLBACK(gterm_menu_font_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->actions), G_ACTION(font_action));

    GSimpleAction *reset_action = g_simple_action_new("reset", NULL);
    g_signal_connect(reset_action, "activate", G_CALLBACK(gterm_menu_reset_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->actions), G_ACTION(reset_action));

    gtk_widget_insert_action_group(gt->window, "menu", G_ACTION_GROUP(gt->actions));

    gt->menu = g_menu_new();
    g_menu_append(gt->menu, "Fullscreen", "menu.fullscreen");
    g_menu_append(gt->menu, "Audible bell", "menu.bell");

    GMenu *font_menu = g_menu_new();
    fontname = gcfg_get(gt->cfg, GTERM_CFG_KEY_FONT_FACE);
    if (!fontname)
        fontname = g_strdup("monospace");
    for (i = 0; i < (int)ARRAY_SIZE(sizes); i++) {
        fontsize = gcfg_get(gt->cfg, sizes[i]);
        if (!fontsize)
            continue;
        char *fontdesc = g_strdup_printf("%s %s", fontname, fontsize);
        GMenuItem *item = g_menu_item_new(fontdesc, NULL);
        g_menu_item_set_action_and_target(item, "menu.font", "s", fontdesc);
        g_menu_append_item(font_menu, item);
        g_object_unref(item);
        g_free(fontdesc);
        g_free(fontsize);
    }
    g_free(fontname);
    g_menu_append_section(gt->menu, NULL, G_MENU_MODEL(font_menu));
    g_object_unref(font_menu);

    g_menu_append(gt->menu, "Terminal reset", "menu.reset");

    gt->popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(gt->menu));
    gtk_widget_set_parent(gt->popover, gt->terminal);
}

static void gterm_click_pressed_cb(GtkGestureClick *gesture,
                                   int               n_press,
                                   double            x,
                                   double            y,
                                   gpointer          user_data)
{
    gterm *gt = user_data;
    GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(gesture));
    GdkModifierType state = gdk_event_get_modifier_state(event);
    guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));

    if (!(state & GDK_CONTROL_MASK))
        return;

    if (button >= 1 && button <= 3) {
        GdkRectangle rect = { (int)x, (int)y, 1, 1 };
        gtk_popover_set_pointing_to(GTK_POPOVER(gt->popover), &rect);
        gtk_popover_popup(GTK_POPOVER(gt->popover));
    }
}

/* ------------------------------------------------------------------------ */

static void gterm_window_destroy(GtkWidget *widget, gpointer data)
{
    gterm *gt = data;
    g_application_quit(G_APPLICATION(gt->app));
}

static void gterm_window_configure(gterm *gt)
{
    gcfg_bool b;
    char *str;

    str = gcfg_get(gt->cfg, GTERM_CFG_KEY_TITLE);
    if (str) {
        gtk_window_set_title(GTK_WINDOW(gt->window), str);
        g_free(str);
    }

    b = gcfg_get_bool(gt->cfg, GTERM_CFG_KEY_FULLSCREEN);
    if (b == GCFG_BOOL_TRUE) {
        GAction *action = g_action_map_lookup_action(G_ACTION_MAP(gt->actions), "fullscreen");
        if (action) {
            g_action_change_state(action, g_variant_new_boolean(TRUE));
        }
    }
}

static gterm *gterm_new(GtkApplication *app, GKeyFile *cfg)
{
    gterm *gt = g_new0(gterm, 1);

    gt->app = app;
    gt->cfg = cfg;

    gt->window = gtk_application_window_new(app);
    g_signal_connect(G_OBJECT(gt->window), "destroy",
                     G_CALLBACK(gterm_window_destroy), gt);

    gt->terminal = vte_terminal_new();
    g_signal_connect(G_OBJECT(gt->terminal), "child-exited",
                     G_CALLBACK(gterm_vte_child_exited), gt);
    g_signal_connect(G_OBJECT(gt->terminal), "window-title-changed",
                     G_CALLBACK(gterm_vte_window_title_changed), gt);

    GtkGesture *gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 0); // any button
    g_signal_connect(gesture, "pressed", G_CALLBACK(gterm_click_pressed_cb), gt);
    gtk_widget_add_controller(gt->terminal, GTK_EVENT_CONTROLLER(gesture));

    gtk_window_set_child(GTK_WINDOW(gt->window), gt->terminal);

    gterm_fill_menu(gt);
    gterm_window_configure(gt);
    gterm_vte_configure(gt);

    gtk_window_present(GTK_WINDOW(gt->window));

    return gt;
}

static void gterm_app_activate(GApplication *app, gpointer user_data)
{
    // Do nothing, we handle everything in main for now to keep the logic similar
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
    filename = g_strdup_printf("%s/%s", g_get_home_dir(), GTERM_CFG_FILENAME);
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

    GtkApplication *app = gtk_application_new("org.gterm.terminal", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(gterm_app_activate), NULL);

    // We need to run g_application_register to use it without g_application_run if we want to stay close to original flow,
    // but GTK 4 really wants g_application_run.
    // Let's use a hybrid approach or just put everything in activate.
    // Actually, gterm_new creates the window.

    g_application_register(G_APPLICATION(app), NULL, NULL);

    gt = gterm_new(app, cfg);
    if (eopt) {
        char *title = gcfg_get(cfg, GTERM_CFG_KEY_TITLE);
        if (!title && argv[eopt])
            gtk_window_set_title(GTK_WINDOW(gt->window), argv[eopt]);
        g_free(title);
        gterm_spawn(gt, argv + eopt);
    } else {
        gterm_spawn_shell(gt);
    }

    while (g_list_length(gtk_application_get_windows(app)) > 0) {
        g_main_context_iteration(NULL, TRUE);
    }

    int exit_code = gt->exit_code;
    g_free(gt);
    g_object_unref(app);
    g_key_file_free(cfg);
    return exit_code;
}
