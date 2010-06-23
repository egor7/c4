#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "graph.h"
#include "graph_list.h"
#include "graph_ident.h"
#include "graph_def.h"
#include "graph_config.h"
#include "common.h"
#include "filesystem.h"
#include "utils_cgi.h"

#include <fcgiapp.h>
#include <fcgi_stdio.h>

/*
 * Data types
 */
struct graph_config_s /* {{{ */
{
  graph_ident_t *select;

  char *title;
  char *vertical_label;
  _Bool show_zero;

  graph_def_t *defs;

  graph_instance_t **instances;
  size_t instances_num;
}; /* }}} struct graph_config_s */

/*
 * Private functions
 */

/*
 * Config functions
 */
static graph_ident_t *graph_config_get_selector (const oconfig_item_t *ci) /* {{{ */
{
  char *host = NULL;
  char *plugin = NULL;
  char *plugin_instance = NULL;
  char *type = NULL;
  char *type_instance = NULL;
  graph_ident_t *ret;
  int i;

  for (i = 0; i < ci->children_num; i++)
  {
    oconfig_item_t *child;

    child = ci->children + i;

    if (strcasecmp ("Host", child->key) == 0)
      graph_config_get_string (child, &host);
    else if (strcasecmp ("Plugin", child->key) == 0)
      graph_config_get_string (child, &plugin);
    else if (strcasecmp ("PluginInstance", child->key) == 0)
      graph_config_get_string (child, &plugin_instance);
    else if (strcasecmp ("Type", child->key) == 0)
      graph_config_get_string (child, &type);
    else if (strcasecmp ("TypeInstance", child->key) == 0)
      graph_config_get_string (child, &type_instance);
    /* else: ignore all other directives here. */
  } /* for */

  ret = ident_create (host, plugin, plugin_instance, type, type_instance);

  free (host);
  free (plugin);
  free (plugin_instance);
  free (type);
  free (type_instance);

  return (ret);
} /* }}} int graph_config_get_selector */

/*
 * Global functions
 */
graph_config_t *graph_create (const graph_ident_t *selector) /* {{{ */
{
  graph_config_t *cfg;

  cfg = malloc (sizeof (*cfg));
  if (cfg == NULL)
    return (NULL);
  memset (cfg, 0, sizeof (*cfg));

  if (selector != NULL)
    cfg->select = ident_clone (selector);
  else
    cfg->select = NULL;

  cfg->title = NULL;
  cfg->vertical_label = NULL;
  cfg->defs = NULL;
  cfg->instances = NULL;

  return (cfg);
} /* }}} int graph_create */

void graph_destroy (graph_config_t *cfg) /* {{{ */
{
  size_t i;

  if (cfg == NULL)
    return;

  ident_destroy (cfg->select);

  free (cfg->title);
  free (cfg->vertical_label);

  def_destroy (cfg->defs);

  for (i = 0; i < cfg->instances_num; i++)
    inst_destroy (cfg->instances[i]);
  free (cfg->instances);
} /* }}} void graph_destroy */

int graph_config_add (const oconfig_item_t *ci) /* {{{ */
{
  graph_ident_t *select;
  graph_config_t *cfg = NULL;
  int i;

  select = graph_config_get_selector (ci);
  if (select == NULL)
    return (EINVAL);

  cfg = graph_create (/* selector = */ NULL);
  if (cfg == NULL)
    return (ENOMEM);

  cfg->select = select;

  for (i = 0; i < ci->children_num; i++)
  {
    oconfig_item_t *child;

    child = ci->children + i;

    if (strcasecmp ("Title", child->key) == 0)
      graph_config_get_string (child, &cfg->title);
    else if (strcasecmp ("VerticalLabel", child->key) == 0)
      graph_config_get_string (child, &cfg->vertical_label);
    else if (strcasecmp ("ShowZero", child->key) == 0)
      graph_config_get_bool (child, &cfg->show_zero);
    else if (strcasecmp ("DEF", child->key) == 0)
      def_config (cfg, child);
  } /* for */

  gl_add_graph (cfg);

  return (0);
} /* }}} graph_config_add */

int graph_add_file (graph_config_t *cfg, const graph_ident_t *file) /* {{{ */
{
  graph_instance_t *inst;

  inst = graph_inst_find_matching (cfg, file);
  if (inst == NULL)
  {
    graph_instance_t **tmp;

    tmp = realloc (cfg->instances,
        sizeof (*cfg->instances) * (cfg->instances_num + 1));
    if (tmp == NULL)
      return (ENOMEM);
    cfg->instances = tmp;

    inst = inst_create (cfg, file);
    if (inst == NULL)
      return (ENOMEM);

    cfg->instances[cfg->instances_num] = inst;
    cfg->instances_num++;
  }

  return (inst_add_file (inst, file));
} /* }}} int graph_add_file */

int graph_get_title (graph_config_t *cfg, /* {{{ */
    char *buffer, size_t buffer_size)
{
  if ((cfg == NULL) || (buffer == NULL) || (buffer_size < 1))
    return (EINVAL);

  if (cfg->title == NULL)
    cfg->title = ident_to_string (cfg->select);

  if (cfg->title == NULL)
    return (ENOMEM);

  strncpy (buffer, cfg->title, buffer_size);
  buffer[buffer_size - 1] = 0;

  return (0);
} /* }}} int graph_get_title */

int graph_get_params (graph_config_t *cfg, /* {{{ */
    char *buffer, size_t buffer_size)
{
  buffer[0] = 0;

#define COPY_FIELD(field) do {                                       \
  const char *str = ident_get_##field (cfg->select);                 \
  char uri_str[1024];                                                \
  uri_escape (uri_str, str, sizeof (uri_str));                       \
  strlcat (buffer, #field, buffer_size);                             \
  strlcat (buffer, "=", buffer_size);                                \
  strlcat (buffer, uri_str, buffer_size);                            \
} while (0)

  COPY_FIELD(host);
  strlcat (buffer, ";", buffer_size);
  COPY_FIELD(plugin);
  strlcat (buffer, ";", buffer_size);
  COPY_FIELD(plugin_instance);
  strlcat (buffer, ";", buffer_size);
  COPY_FIELD(type);
  strlcat (buffer, ";", buffer_size);
  COPY_FIELD(type_instance);

#undef COPY_FIELD

  return (0);
} /* }}} int graph_get_params */

graph_ident_t *graph_get_selector (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (ident_clone (cfg->select));
} /* }}} graph_ident_t *graph_get_selector */

graph_def_t *graph_get_defs (graph_config_t *cfg) /* {{{ */
{
  if (cfg == NULL)
    return (NULL);

  return (cfg->defs);
} /* }}} graph_def_t *graph_get_defs */

int graph_add_def (graph_config_t *cfg, graph_def_t *def) /* {{{ */
{
  if ((cfg == NULL) || (def == NULL))
    return (EINVAL);

  if (cfg->defs == NULL)
  {
    cfg->defs = def;
    return (0);
  }

  return (def_append (cfg->defs, def));
} /* }}} int graph_add_def */

_Bool graph_matches_ident (graph_config_t *cfg, const graph_ident_t *ident) /* {{{ */
{
  if ((cfg == NULL) || (ident == NULL))
    return (0);

  return (ident_matches (cfg->select, ident));
} /* }}} _Bool graph_matches_ident */

_Bool graph_matches_field (graph_config_t *cfg, /* {{{ */
    graph_ident_field_t field, const char *field_value)
{
  const char *selector_value;

  if ((cfg == NULL) || (field_value == NULL))
    return (0);

  selector_value = ident_get_field (cfg->select, field);
  if (selector_value == NULL)
    return (0);

  if (IS_ALL (selector_value) || IS_ANY (selector_value))
    return (1);
  else if (strcasecmp (selector_value, field_value) == 0)
    return (1);

  return (0);
} /* }}} _Bool graph_matches_field */

int graph_inst_foreach (graph_config_t *cfg, /* {{{ */
		inst_callback_t cb, void *user_data)
{
  size_t i;
  int status;

  for (i = 0; i < cfg->instances_num; i++)
  {
    status = (*cb) (cfg->instances[i], user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int graph_inst_foreach */

graph_instance_t *graph_inst_find_exact (graph_config_t *cfg, /* {{{ */
    graph_ident_t *ident)
{
  size_t i;

  if ((cfg == NULL) || (ident == NULL))
    return (NULL);

  for (i = 0; i < cfg->instances_num; i++)
    if (inst_compare_ident (cfg->instances[i], ident) == 0)
      return (cfg->instances[i]);

  return (NULL);
} /* }}} graph_instance_t *graph_inst_find_exact */

graph_instance_t *graph_inst_find_matching (graph_config_t *cfg, /* {{{ */
    const graph_ident_t *ident)
{
  size_t i;

  if ((cfg == NULL) || (ident == NULL))
    return (NULL);

  for (i = 0; i < cfg->instances_num; i++)
    if (inst_matches_ident (cfg->instances[i], ident))
      return (cfg->instances[i]);

  return (NULL);
} /* }}} graph_instance_t *graph_inst_find_matching */

int graph_inst_search (graph_config_t *cfg, const char *term, /* {{{ */
    graph_inst_callback_t cb,
    void *user_data)
{
  char buffer[1024];
  int status;
  size_t i;

  status = graph_get_title (cfg, buffer, sizeof (buffer));
  if (status != 0)
  {
    fprintf (stderr, "graph_inst_search: graph_get_title failed\n");
    return (status);
  }

  strtolower (buffer);

  if (strstr (buffer, term) != NULL)
  {
    for (i = 0; i < cfg->instances_num; i++)
    {
      status = (*cb) (cfg, cfg->instances[i], user_data);
      if (status != 0)
        return (status);
    }
  }
  else
  {
    for (i = 0; i < cfg->instances_num; i++)
    {
      if (inst_matches_string (cfg, cfg->instances[i], term))
      {
        status = (*cb) (cfg, cfg->instances[i], user_data);
        if (status != 0)
          return (status);
      }
    }
  }

  return (0);
} /* }}} int graph_inst_search */

int graph_inst_search_field (graph_config_t *cfg, /* {{{ */
    graph_ident_field_t field, const char *field_value,
    graph_inst_callback_t callback, void *user_data)
{
  size_t i;
  const char *selector_field;
  _Bool need_check_instances = 0;

  if ((cfg == NULL) || (field_value == NULL) || (callback == NULL))
    return (EINVAL);

  if (!graph_matches_field (cfg, field, field_value))
    return (0);

  selector_field = ident_get_field (cfg->select, field);
  if (selector_field == NULL)
    return (-1);

  if (IS_ALL (selector_field) || IS_ANY (selector_field))
    need_check_instances = 1;

  for (i = 0; i < cfg->instances_num; i++)
  {
    int status;

    if (need_check_instances
        && !inst_matches_field (cfg->instances[i], field, field_value))
      continue;

    status = (*callback) (cfg, cfg->instances[i], user_data);
    if (status != 0)
      return (status);
  }

  return (0);
} /* }}} int graph_inst_search_field */

int graph_compare (graph_config_t *cfg, const graph_ident_t *ident) /* {{{ */
{
  if ((cfg == NULL) || (ident == NULL))
    return (0);

  return (ident_compare (cfg->select, ident));
} /* }}} int graph_compare */

int graph_clear_instances (graph_config_t *cfg) /* {{{ */
{
  size_t i;

  if (cfg == NULL)
    return (EINVAL);

  for (i = 0; i < cfg->instances_num; i++)
    inst_destroy (cfg->instances[i]);
  free (cfg->instances);
  cfg->instances = NULL;
  cfg->instances_num = 0;

  return (0);
} /* }}} int graph_clear_instances */

int graph_get_rrdargs (graph_config_t *cfg, graph_instance_t *inst, /* {{{ */
    str_array_t *args)
{
  if ((cfg == NULL) || (inst == NULL) || (args == NULL))
    return (EINVAL);

  if (cfg->title != NULL)
  {
    array_append (args, "-t");
    array_append (args, cfg->title);
  }

  if (cfg->vertical_label != NULL)
  {
    array_append (args, "-v");
    array_append (args, cfg->vertical_label);
  }

  if (cfg->show_zero)
  {
    array_append (args, "-l");
    array_append (args, "0");
  }

  return (0);
} /* }}} int graph_get_rrdargs */

/* vim: set sw=2 sts=2 et fdm=marker : */
