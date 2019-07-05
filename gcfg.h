#include <glib.h>

#define ARRAY_SIZE(_x) (sizeof(_x)/sizeof(_x[0]))

#define GCFG_GROUP_DEFAULT         "default"
#define GCFG_GROUP_CMDLINE         "_cmdline_"
#define GCFG_KEY_PROFILE           "profile"

/* ------------------------------------------------------------------------ */

typedef struct gcfg_opt {
    char *opt;
    char *key;
    bool is_bool;
} gcfg_opt;

typedef enum gcfg_bool {
    GCFG_BOOL_UNSET  = -1,
    GCFG_BOOL_FALSE  = 0,
    GCFG_BOOL_TRUE   = 1,
} gcfg_bool;

const gcfg_opt *gcfg_opt_find(const gcfg_opt *opts, int nopts,
                              char *arg);
void gcfg_set(GKeyFile *cfg, char *key, char *value);
char *gcfg_get(GKeyFile *cfg, const char *key);
gcfg_bool gcfg_get_bool(GKeyFile *cfg, const char *key);
