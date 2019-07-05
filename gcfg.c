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

    if (arg[0] != '-' && arg[0] != '+')
        return NULL;
    for (i = 0; i < nopts; i++) {
        if (strcmp(opts[i].opt, arg + 1) == 0) {
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
    g_key_file_set_string(cfg, GCFG_GROUP_CMDLINE, key, value);
}

char *gcfg_get(GKeyFile *cfg, const char *key)
{
    char *profile;
    char *value;

    profile = g_key_file_get_string(cfg, GCFG_GROUP_CMDLINE,
                                    GCFG_KEY_PROFILE, NULL);

    value = g_key_file_get_string(cfg, GCFG_GROUP_CMDLINE, key, NULL);
    if (value)
        return value;

    if (profile) {
        value = g_key_file_get_string(cfg, profile, key, NULL);
        if (value)
            return value;
    }

    value = g_key_file_get_string(cfg, GCFG_GROUP_DEFAULT, key, NULL);
    if (value)
        return value;

    return NULL;
}

gcfg_bool gcfg_get_bool(GKeyFile *cfg, const char *key)
{
    char *value;

    value = gcfg_get(cfg, key);
    if (!value)
        return GCFG_BOOL_UNSET;

    if (strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "on") == 0)
        return GCFG_BOOL_TRUE;

    if (strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "off") == 0)
        return GCFG_BOOL_FALSE;

    return GCFG_BOOL_UNSET;
}
