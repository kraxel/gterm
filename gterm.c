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
    GtkWidget *popup;

    GSimpleAction *copy_action;
    GSimpleAction *fullscreen_action;
    GSimpleAction *bell_action;
    GSimpleAction *font_action;

    GKeyFile *cfg;
    char **exec;
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
        g_application_quit(G_APPLICATION(gt->app));
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

    pwent = getpwuid(getuid());
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
    g_application_quit(G_APPLICATION(gt->app));
}

static void gterm_vte_termprop_changed(VteTerminal *vteterminal,
                                       const char   *name,
                                       gpointer     user_data)
{
    gterm *gt = user_data;
    const char *str;

    str = vte_terminal_get_termprop_string(vteterminal, name, NULL);
    if (str)
        gtk_window_set_title(GTK_WINDOW(gt->window), str);
}

static gboolean gterm_vte_key_pressed(GtkEventControllerKey *controller,
                                     guint                  keyval,
                                     guint                  keycode,
                                     GdkModifierType        state,
                                     gpointer               user_data)
{
    gterm *gt = user_data;
    GdkModifierType mask = GDK_CONTROL_MASK | GDK_SHIFT_MASK;

    if ((state & mask) == mask) {
        switch (keyval) {
        case GDK_KEY_C:
        case GDK_KEY_c:
            vte_terminal_copy_clipboard_format(VTE_TERMINAL(gt->terminal), VTE_FORMAT_TEXT);
            return TRUE;
        case GDK_KEY_V:
        case GDK_KEY_v:
            vte_terminal_paste_clipboard(VTE_TERMINAL(gt->terminal));
            return TRUE;
        }
    }
    return FALSE;
}

static void gterm_vte_gesture_pressed(GtkGestureMultiPress *gesture,
                                      gint                  n_press,
                                      gdouble               x,
                                      gdouble               y,
                                      gpointer              user_data)
{
    gterm *gt = user_data;
    GdkModifierType state;
    guint button;
    GdkEventSequence *sequence;
    const GdkEvent *event;

    if (n_press > 1)
        return;

    sequence = gtk_gesture_get_last_updated_sequence(GTK_GESTURE(gesture));
    event = gtk_gesture_get_last_event(GTK_GESTURE(gesture), sequence);

    if (!event || !gdk_event_get_state(event, &state))
        return;

    if (!(state & GDK_CONTROL_MASK))
        return;

    button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
    if (button == 1) {
        char *url;
        url = vte_terminal_hyperlink_check_event(VTE_TERMINAL(gt->terminal), (GdkEvent*)event);
        if (!url) {
            url = vte_terminal_match_check_event(VTE_TERMINAL(gt->terminal), (GdkEvent*)event, NULL);
        }
        if (url) {
            gtk_show_uri_on_window(GTK_WINDOW(gt->window), url, gdk_event_get_time(event), NULL);
            g_free(url);
            gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
            return;
        }
    }

    if (button < 1 || button > 3)
        return;

    GdkRectangle rect = { .x = (int)x, .y = (int)y, .width = 1, .height = 1 };

    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    g_simple_action_set_enabled(gt->copy_action, vte_terminal_get_has_selection(VTE_TERMINAL(gt->terminal)));
    gtk_popover_set_pointing_to(GTK_POPOVER(gt->popup), &rect);
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
        pango_font_description_free(font);
        g_simple_action_set_state(gt->font_action, g_variant_new_string(fontdesc));
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
    g_simple_action_set_state(gt->bell_action, g_variant_new_boolean(state));
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
    gint cw = vte_terminal_get_char_width(VTE_TERMINAL(gt->terminal));
    gint ch = vte_terminal_get_char_height(VTE_TERMINAL(gt->terminal));
    GdkGeometry hints = {
        .min_width   = cw,
        .min_height  = ch,
        .base_width  = cw,
        .base_height = ch,
        .width_inc   = cw,
        .height_inc  = ch,
    };

    gtk_window_set_geometry_hints(GTK_WINDOW(gt->window),
                                  GTK_WIDGET(gt->terminal),
                                  &hints,
                                  GDK_HINT_RESIZE_INC |
                                  GDK_HINT_MIN_SIZE |
                                  GDK_HINT_BASE_SIZE);
}

/* ------------------------------------------------------------------------ */

static void gterm_action_fullscreen(GSimpleAction *action,
                                    GVariant      *state,
                                    gpointer       user_data)
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

static void gterm_action_bell(GSimpleAction *action,
                              GVariant      *state,
                              gpointer       user_data)
{
    gterm *gt = user_data;
    gboolean state_bool = g_variant_get_boolean(state);

    vte_terminal_set_audible_bell(VTE_TERMINAL(gt->terminal), state_bool);
    g_simple_action_set_state(action, state);
}

static void gterm_action_font(GSimpleAction *action,
                              GVariant      *state,
                              gpointer       user_data)
{
    gterm *gt = user_data;
    const char *name = g_variant_get_string(state, NULL);
    PangoFontDescription *font;
    GtkRequisition min, nat;

    font = pango_font_description_from_string(name);
    vte_terminal_set_font(VTE_TERMINAL(gt->terminal), font);
    pango_font_description_free(font);
    gterm_vte_geometry_hints(gt);

    /*
     * Force window resize.  Not sure why this is needed, shouldn't
     * the window automatically respond to terminal size requests?
     */
    gtk_widget_get_preferred_size(GTK_WIDGET(gt->terminal), &min, &nat);
    gtk_window_resize(GTK_WINDOW(gt->window), nat.width, nat.height);

    g_simple_action_set_state(action, state);
}

static void gterm_action_copy(GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       user_data)
{
    gterm *gt = user_data;

    vte_terminal_copy_clipboard_format(VTE_TERMINAL(gt->terminal), VTE_FORMAT_TEXT);
}

static void gterm_action_paste(GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
    gterm *gt = user_data;

    vte_terminal_paste_clipboard(VTE_TERMINAL(gt->terminal));
}

static void gterm_action_reset(GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       user_data)
{
    gterm *gt = user_data;

    vte_terminal_reset(VTE_TERMINAL(gt->terminal), true, true);
}

static void gterm_popdown_cb(GAction *action, GVariant *param, gpointer user_data)
{
    gterm *gt = user_data;

    if (gt->popup)
        gtk_popover_popdown(GTK_POPOVER(gt->popup));
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
    char *fontdesc;
    char *fontname;
    char *fontsize;
    int i;

    gt->fullscreen_action = g_simple_action_new_stateful("fullscreen", NULL, g_variant_new_boolean(FALSE));
    g_signal_connect(gt->fullscreen_action, "change-state", G_CALLBACK(gterm_action_fullscreen), gt);
    g_signal_connect(gt->fullscreen_action, "change-state", G_CALLBACK(gterm_popdown_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->window), G_ACTION(gt->fullscreen_action));

    gt->bell_action = g_simple_action_new_stateful("bell", NULL, g_variant_new_boolean(TRUE));
    g_signal_connect(gt->bell_action, "change-state", G_CALLBACK(gterm_action_bell), gt);
    g_signal_connect(gt->bell_action, "change-state", G_CALLBACK(gterm_popdown_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->window), G_ACTION(gt->bell_action));

    gt->copy_action = g_simple_action_new("copy", NULL);
    g_signal_connect(gt->copy_action, "activate", G_CALLBACK(gterm_action_copy), gt);
    g_signal_connect(gt->copy_action, "activate", G_CALLBACK(gterm_popdown_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->window), G_ACTION(gt->copy_action));

    GSimpleAction *paste_action = g_simple_action_new("paste", NULL);
    g_signal_connect(paste_action, "activate", G_CALLBACK(gterm_action_paste), gt);
    g_signal_connect(paste_action, "activate", G_CALLBACK(gterm_popdown_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->window), G_ACTION(paste_action));

    gt->font_action = g_simple_action_new_stateful("font", G_VARIANT_TYPE_STRING, g_variant_new_string(""));
    g_signal_connect(gt->font_action, "change-state", G_CALLBACK(gterm_action_font), gt);
    g_signal_connect(gt->font_action, "change-state", G_CALLBACK(gterm_popdown_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->window), G_ACTION(gt->font_action));

    GSimpleAction *reset_action = g_simple_action_new("reset", NULL);
    g_signal_connect(reset_action, "activate", G_CALLBACK(gterm_action_reset), gt);
    g_signal_connect(reset_action, "activate", G_CALLBACK(gterm_popdown_cb), gt);
    g_action_map_add_action(G_ACTION_MAP(gt->window), G_ACTION(reset_action));

    GMenu *menu = g_menu_new();

    GMenu *s1 = g_menu_new();
    g_menu_append(s1, "Fullscreen", "win.fullscreen");
    g_menu_append(s1, "Audible bell", "win.bell");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(s1));
    g_object_unref(s1);

    GMenu *s2 = g_menu_new();
    g_menu_append(s2, "Copy", "win.copy");
    g_menu_append(s2, "Paste", "win.paste");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(s2));
    g_object_unref(s2);

    GMenu *s3 = g_menu_new();
    fontname = gcfg_get(gt->cfg, GTERM_CFG_KEY_FONT_FACE);
    if (!fontname)
        fontname = "monospace";
    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        fontsize = gcfg_get(gt->cfg, sizes[i]);
        if (!fontsize)
            continue;
        fontdesc = g_strdup_printf("%s %s", fontname, fontsize);
        GMenuItem *item = g_menu_item_new(fontdesc, NULL);
        g_menu_item_set_action_and_target(item, "win.font", "s", fontdesc);
        g_menu_append_item(s3, item);
        g_object_unref(item);
        g_free(fontdesc);
    }
    g_menu_append_section(menu, NULL, G_MENU_MODEL(s3));
    g_object_unref(s3);

    GMenu *s4 = g_menu_new();
    g_menu_append(s4, "Terminal reset", "win.reset");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(s4));
    g_object_unref(s4);

    gt->popup = gtk_popover_new_from_model(gt->terminal, G_MENU_MODEL(menu));
    gtk_popover_set_modal(GTK_POPOVER(gt->popup), TRUE);
    g_object_unref(menu);
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
    }

    b = gcfg_get_bool(gt->cfg, GTERM_CFG_KEY_FULLSCREEN);
    if (b == GCFG_BOOL_TRUE) {
        g_action_group_change_action_state(G_ACTION_GROUP(gt->window), "fullscreen", g_variant_new_boolean(TRUE));
    }
}

static void gterm_new(gterm *gt)
{
    gt->window = gtk_application_window_new(gt->app);
    g_signal_connect(G_OBJECT(gt->window), "destroy",
                     G_CALLBACK(gterm_window_destroy), gt);

    gt->terminal = vte_terminal_new();
    vte_terminal_set_allow_hyperlink(VTE_TERMINAL(gt->terminal), TRUE);

    VteRegex *url_regex;
    GError *err = NULL;
    url_regex = vte_regex_new_for_match("((https?|file)://|mailto:)[^\\s'\"<>]+",
                                        -1, VTE_REGEX_FLAGS_DEFAULT, &err);
    g_assert(url_regex != NULL);
    int tag = vte_terminal_match_add_regex(VTE_TERMINAL(gt->terminal), url_regex, 0);
    vte_terminal_match_set_cursor_name(VTE_TERMINAL(gt->terminal), tag, "pointer");
    vte_regex_unref(url_regex);

    g_signal_connect(G_OBJECT(gt->terminal), "child-exited",
                     G_CALLBACK(gterm_vte_child_exited), gt);
    g_signal_connect(G_OBJECT(gt->terminal), "termprop-changed::xterm.title",
                     G_CALLBACK(gterm_vte_termprop_changed), gt);

    GtkGesture *gesture = gtk_gesture_multi_press_new(gt->terminal);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(gesture),
                                               GTK_PHASE_TARGET);
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 0);
    g_signal_connect(gesture, "pressed",
                     G_CALLBACK(gterm_vte_gesture_pressed), gt);

    GtkEventController *key_controller = gtk_event_controller_key_new(gt->terminal);
    gtk_event_controller_set_propagation_phase(key_controller,
                                               GTK_PHASE_TARGET);
    g_signal_connect(key_controller, "key-pressed",
                     G_CALLBACK(gterm_vte_key_pressed), gt);

    gtk_container_add(GTK_CONTAINER(gt->window), gt->terminal);

    gterm_fill_menu(gt);

    gterm_window_configure(gt);
    gterm_vte_configure(gt);
    gterm_vte_geometry_hints(gt);
    gtk_widget_show_all(gt->window);
}

static void gterm_activate(GApplication *app, gpointer data)
{
    gterm *gt = data;

    gterm_new(gt);

    if (gt->exec) {
        if (!gcfg_get(gt->cfg, GTERM_CFG_KEY_TITLE))
            gtk_window_set_title(GTK_WINDOW(gt->window), gt->exec[0]);
        gterm_spawn(gt, gt->exec);
    } else {
        gterm_spawn_shell(gt);
    }
}

int main(int argc, char *argv[])
{
    char *filename;
    GKeyFile *cfg;
    gterm *gt;
    const gcfg_opt *opt;
    int i, eopt = 0;

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

    gt = g_new0(gterm, 1);
    gt->cfg = cfg;
    if (eopt) {
        if (eopt == argc) {
            fprintf(stderr, "missing command to execute\n");
            exit(1);
        }
        gt->exec = argv + eopt;
    }
    gt->app = gtk_application_new("org.kraxel.gterm", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(gt->app, "activate", G_CALLBACK(gterm_activate), gt);

    g_application_run(G_APPLICATION(gt->app), 0, NULL);
    return gt->exit_code;
}
