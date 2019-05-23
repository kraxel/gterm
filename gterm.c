#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

#include <sys/types.h>

#include <gtk/gtk.h>
#include <vte/vte.h>

typedef struct gterm {
    GtkWidget *window;
    GtkWidget *terminal;
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

static void gterm_window_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

static gterm *gterm_new(void)
{
    gterm *gt = g_new0(gterm, 1);

    gt->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(gt->window), "destroy",
                     G_CALLBACK(gterm_window_destroy), gt);

    gt->terminal = vte_terminal_new();
    g_signal_connect(G_OBJECT(gt->terminal), "child-exited",
                     G_CALLBACK(gterm_vte_child_exited), gt);

    gtk_container_add(GTK_CONTAINER(gt->window), gt->terminal);
    gtk_widget_show_all(gt->window);
    return gt;
}

int main(int argc, char *argv[])
{
    gterm *gt;

    gtk_init(&argc, &argv);

    gt = gterm_new();
    gterm_spawn_shell(gt);

    gtk_main();
    return gt->exit_code;
}
