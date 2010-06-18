#ifndef GRAPH_IDENT_H
#define GRAPH_IDENT_H 1

#include <time.h>

#define ANY_TOKEN "/any/"
#define ALL_TOKEN "/all/"

#define IS_ANY(str) (((str) != NULL) && (strcasecmp (ANY_TOKEN, (str)) == 0))
#define IS_ALL(str) (((str) != NULL) && (strcasecmp (ALL_TOKEN, (str)) == 0))

struct graph_ident_s;
typedef struct graph_ident_s graph_ident_t;

graph_ident_t *ident_create (const char *host,
    const char *plugin, const char *plugin_instance,
    const char *type, const char *type_instance);
graph_ident_t *ident_clone (const graph_ident_t *ident);

#define IDENT_FLAG_REPLACE_ALL 0x01
#define IDENT_FLAG_REPLACE_ANY 0x02
graph_ident_t *ident_copy_with_selector (const graph_ident_t *selector,
    const graph_ident_t *ident, unsigned int flags);

void ident_destroy (graph_ident_t *ident);

const char *ident_get_host (graph_ident_t *ident);
const char *ident_get_plugin (graph_ident_t *ident);
const char *ident_get_plugin_instance (graph_ident_t *ident);
const char *ident_get_type (graph_ident_t *ident);
const char *ident_get_type_instance (graph_ident_t *ident);

int ident_set_host (graph_ident_t *ident, const char *host);
int ident_set_plugin (graph_ident_t *ident, const char *plugin);
int ident_set_plugin_instance (graph_ident_t *ident,
    const char *plugin_instance);
int ident_set_type (graph_ident_t *ident, const char *type);
int ident_set_type_instance (graph_ident_t *ident,
    const char *type_instance);

int ident_compare (const graph_ident_t *i0,
    const graph_ident_t *i1);

_Bool ident_matches (const graph_ident_t *selector,
		const graph_ident_t *ident);

char *ident_to_string (const graph_ident_t *ident);
char *ident_to_file (const graph_ident_t *ident);
char *ident_to_json (const graph_ident_t *ident);

time_t ident_get_mtime (const graph_ident_t *ident);

/* vim: set sw=2 sts=2 et fdm=marker : */
#endif /* GRAPH_IDENT_H */
