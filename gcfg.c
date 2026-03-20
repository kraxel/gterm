#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "gcfg.h"

const gcfg_opt *gcfg_opt_find(const gcfg_opt *opts, int nopts,
                              char *arg)
{
    const gcfg_opt *opt = NULL;
    int i;

    if (!arg || (arg[0] != '-' && arg[0] != '+'))
        return NULL;
    for (i = 0; i < nopts; i++) {
        if (opts[i].opt && strcmp(opts[i].opt, arg + 1) == 0) {
            opt = opts + i;
            break;
        }
    }
    if (!opt)
        return NULL;
    if (arg[0] == '+' && !opt->is_bool)
        return NULL;
    return opt;
}

void gcfg_set(GKeyFile *cfg, char *key, char *value)
{
    if (cfg && key && value)
        g_key_file_set_string(cfg, GCFG_GROUP_CMDLINE, key, value);
}

char *gcfg_get(GKeyFile *cfg, const char *key)
{
    char *profile = NULL;
    char *value = NULL;

    if (!cfg || !key)
        return NULL;

    if (g_key_file_has_group(cfg, GCFG_GROUP_CMDLINE)) {
        profile = g_key_file_get_string(cfg, GCFG_GROUP_CMDLINE,
                                        GCFG_KEY_PROFILE, NULL);
        value = g_key_file_get_string(cfg, GCFG_GROUP_CMDLINE, key, NULL);
    }

    if (!value && profile && *profile && g_key_file_has_group(cfg, profile)) {
        value = g_key_file_get_string(cfg, profile, key, NULL);
    }

    if (!value && g_key_file_has_group(cfg, GCFG_GROUP_DEFAULT)) {
        value = g_key_file_get_string(cfg, GCFG_GROUP_DEFAULT, key, NULL);
    }

    g_free(profile);
    return value;
}

gcfg_bool gcfg_get_bool(GKeyFile *cfg, const char *key)
{
    char *value;
    gcfg_bool result = GCFG_BOOL_UNSET;

    value = gcfg_get(cfg, key);
    if (!value)
        return GCFG_BOOL_UNSET;

    if (g_ascii_strcasecmp(value, "true") == 0 ||
        g_ascii_strcasecmp(value, "on") == 0)
        result = GCFG_BOOL_TRUE;
    else if (g_ascii_strcasecmp(value, "false") == 0 ||
             g_ascii_strcasecmp(value, "off") == 0)
        result = GCFG_BOOL_FALSE;

    g_free(value);
    return result;
}
