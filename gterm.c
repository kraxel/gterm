#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#define GTERM_CFG_KEY_GEOMETRY          "geometry"
#define GTERM_CFG_KEY_TITLE             "title"

typedef struct gterm_opt {
    char *opt;
    char *key;
    bool is_bool;
} gterm_opt;

static const gterm_opt gterm_opts[] = {
    { .opt = "fa",            .key = GTERM_CFG_KEY_FONT_FACE },
    { .opt = "fs",            .key = GTERM_CFG_KEY_FONT_SIZE },
    { .opt = "geometry",      .key = GTERM_CFG_KEY_GEOMETRY  },
    { .opt = "T",             .key = GTERM_CFG_KEY_TITLE     },
    { .opt = "title",         .key = GTERM_CFG_KEY_TITLE     },
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

static char *gterm_cfg_get(GKeyFile *cfg, char *key)
{
    char *value;

    value = g_key_file_get_string(cfg, GTERM_CFG_GROUP_CMDLINE, key, NULL);
    if (value)
        return value;

    value = g_key_file_get_string(cfg, GTERM_CFG_GROUP_DEFAULT, key, NULL);
    if (value)
        return value;

    return NULL;
}

/* ------------------------------------------------------------------------ */

typedef struct gterm {
    GtkWidget *window;
    GtkWidget *terminal;
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
    gtk_main_quit();
}

static void gterm_vte_configure(gterm *gt)
{
    char *fontdesc;
    char *fontname;
    char *fontsize;
    char *str;
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
}

static void gterm_window_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static void gterm_window_configure(gterm *gt)
{
    char *str;

    str = gterm_cfg_get(gt->cfg, GTERM_CFG_KEY_TITLE);
    if (str) {
        gtk_window_set_title(GTK_WINDOW(gt->window), str);
    }
}

static gterm *gterm_new(GKeyFile *cfg)
{
    gterm *gt = g_new0(gterm, 1);

    gt->cfg = cfg;

    gt->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(gt->window), "destroy",
                     G_CALLBACK(gterm_window_destroy), gt);
    gterm_window_configure(gt);

    gt->terminal = vte_terminal_new();
    g_signal_connect(G_OBJECT(gt->terminal), "child-exited",
                     G_CALLBACK(gterm_vte_child_exited), gt);
    gterm_vte_configure(gt);

    gtk_container_add(GTK_CONTAINER(gt->window), gt->terminal);
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
