#include "value-pairs/cmdline.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/*******************************************************************************
 * Command line parser
 *******************************************************************************/

static void
vp_cmdline_parse_rekey_finish (gpointer data)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];

  if (vpts)
    value_pairs_add_transforms (vp, args[2]);
  args[2] = NULL;
  g_free(args[3]);
  args[3] = NULL;
}

/* parse a value-pair specification from a command-line like environment */
static gboolean
vp_cmdline_parse_scope(const gchar *option_name, const gchar *value,
                       gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];
  gchar **scopes;
  gint i;

  vp_cmdline_parse_rekey_finish (data);

  scopes = g_strsplit (value, ",", -1);
  for (i = 0; scopes[i] != NULL; i++)
    {
      if (!value_pairs_add_scope (vp, scopes[i]))
        {
          g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                       "Error parsing value-pairs: unknown scope %s", scopes[i]);
          g_strfreev (scopes);
          return FALSE;
        }
    }
  g_strfreev (scopes);

  return TRUE;
}

static gboolean
vp_cmdline_parse_exclude(const gchar *option_name, const gchar *value,
                         gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];
  gchar **excludes;
  gint i;

  vp_cmdline_parse_rekey_finish (data);

  excludes = g_strsplit(value, ",", -1);
  for (i = 0; excludes[i] != NULL; i++)
    value_pairs_add_glob_pattern(vp, excludes[i], FALSE);
  g_strfreev(excludes);

  return TRUE;
}

static void
vp_cmdline_start_key(gpointer data, const gchar *key)
{
  gpointer *args = (gpointer *) data;

  vp_cmdline_parse_rekey_finish (data);
  args[3] = g_strdup(key);
}

static gboolean
vp_cmdline_parse_key(const gchar *option_name, const gchar *value,
		      gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];
  gchar **keys;
  gint i;

  vp_cmdline_start_key(data, value);

  keys = g_strsplit(value, ",", -1);
  for (i = 0; keys[i] != NULL; i++)
    value_pairs_add_glob_pattern(vp, keys[i], TRUE);
  g_strfreev(keys);

  return TRUE;
}

static gboolean
vp_cmdline_parse_rekey(const gchar *option_name, const gchar *value,
                       gpointer data, GError **error)
{
  vp_cmdline_start_key(data, value);
  return TRUE;
}

static void
value_pairs_parse_type(gchar *spec, gchar **value, gchar **type)
{
  char *sp, *ep;

  *type = NULL;
  sp = spec;

  while (g_ascii_isalnum(*sp) || (*sp) == '_')
    sp++;

  while (*sp == ' ' || *sp == '\t')
    sp++;

  if (*sp != '(' ||
      !((g_ascii_toupper(spec[0]) >= 'A' &&
         g_ascii_toupper(spec[0]) <= 'Z') ||
        spec[0] == '_'))
    {
      *value = spec;
      return;
    }

  ep = strchr(sp, ')');
  if (ep == NULL || ep[1] != '\0')
    {
      *value = spec;
      return;
    }

  *value = sp + 1;
  *type = spec;
  sp[0] = '\0';
  ep[0] = '\0';
}

static gboolean
vp_cmdline_parse_pair (const gchar *option_name, const gchar *value,
		       gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairs *vp = (ValuePairs *) args[1];
  GlobalConfig *cfg = (GlobalConfig *) args[0];
  gchar **kv, *v, *t;
  gboolean res = FALSE;
  LogTemplate *template;

  vp_cmdline_parse_rekey_finish (data);

  if (strchr(value, '=') == NULL)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Error parsing value-pairs: expected an equal sign in key=value pair");
      return FALSE;
    }

  kv = g_strsplit(value, "=", 2);
  value_pairs_parse_type(kv[1], &v, &t);

  template = log_template_new(cfg, NULL);
  if (!log_template_compile(template, v, error))
    goto error;
  if (!log_template_set_type_hint(template, t, error))
    goto error;

  value_pairs_add_pair(vp, kv[0], template);

  res = TRUE;
 error:
  log_template_unref(template);
  g_strfreev(kv);

  return res;
}

static gboolean
vp_cmdline_parse_pair_or_key (const gchar *option_name, const gchar *value,
		              gpointer data, GError **error)
{
  if (strchr(value, '=') == NULL)
    return vp_cmdline_parse_key(option_name, value, data, error);
  else
    return vp_cmdline_parse_pair(option_name, value, data, error);
}

static ValuePairsTransformSet *
vp_cmdline_rekey_verify (gchar *key, ValuePairsTransformSet *vpts,
                         gpointer data)
{
  gpointer *args = (gpointer *)data;

  if (!vpts)
    {
      if (!key)
        return NULL;
      vpts = value_pairs_transform_set_new (key);
      vp_cmdline_parse_rekey_finish (data);
      args[2] = vpts;
      return vpts;
    }
  return vpts;
}


static gboolean
vp_cmdline_parse_rekey_replace_prefix (const gchar *option_name, const gchar *value,
                                       gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];
  gchar *key = (gchar *) args[3];
  gchar **kv;

  vpts = vp_cmdline_rekey_verify (key, vpts, data);
  if (!vpts)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "Error parsing value-pairs: --replace-prefix used without --key or --rekey");
      return FALSE;
    }

  if (!g_strstr_len (value, strlen (value), "="))
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                   "Error parsing value-pairs: rekey replace-prefix construct should be in the format string=replacement");
      return FALSE;
    }

  kv = g_strsplit(value, "=", 2);
  value_pairs_transform_set_add_func
    (vpts, value_pairs_new_transform_replace_prefix (kv[0], kv[1]));

  g_free (kv[0]);
  g_free (kv[1]);
  g_free (kv);

  return TRUE;
}

static gboolean
vp_cmdline_parse_rekey_add_prefix (const gchar *option_name, const gchar *value,
                                   gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];
  gchar *key = (gchar *) args[3];

  vpts = vp_cmdline_rekey_verify (key, vpts, data);
  if (!vpts)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "Error parsing value-pairs: --add-prefix used without --key or --rekey");
      return FALSE;
    }

  value_pairs_transform_set_add_func
    (vpts, value_pairs_new_transform_add_prefix (value));
  return TRUE;
}

static gboolean
vp_cmdline_parse_rekey_shift (const gchar *option_name, const gchar *value,
                              gpointer data, GError **error)
{
  gpointer *args = (gpointer *) data;
  ValuePairsTransformSet *vpts = (ValuePairsTransformSet *) args[2];
  gchar *key = (gchar *) args[3];

  vpts = vp_cmdline_rekey_verify (key, vpts, data);
  if (!vpts)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   "Error parsing value-pairs: --shift used without --key or --rekey");
      return FALSE;
    }

  value_pairs_transform_set_add_func
    (vpts, value_pairs_new_transform_shift (atoi (value)));
  return TRUE;
}

ValuePairs *
value_pairs_new_from_cmdline (GlobalConfig *cfg,
			      gint argc, gchar **argv,
			      GError **error)
{
  ValuePairs *vp;
  GOptionContext *ctx;

  GOptionEntry vp_options[] = {
    { "scope", 's', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_scope,
      NULL, NULL },
    { "exclude", 'x', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_exclude,
      NULL, NULL },
    { "key", 'k', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_key,
      NULL, NULL },
    { "rekey", 'r', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_rekey,
      NULL, NULL },
    { "pair", 'p', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_pair,
      NULL, NULL },
    { "shift", 'S', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_rekey_shift,
      NULL, NULL },
    { "add-prefix", 'A', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_rekey_add_prefix,
      NULL, NULL },
    { "replace-prefix", 'R', 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_rekey_replace_prefix,
      NULL, NULL },
    { "replace", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK,
      vp_cmdline_parse_rekey_replace_prefix, NULL, NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_CALLBACK, vp_cmdline_parse_pair_or_key,
      NULL, NULL },
    { NULL }
  };
  GOptionGroup *og;
  gpointer user_data_args[4];
  gboolean success;

  vp = value_pairs_new();
  user_data_args[0] = cfg;
  user_data_args[1] = vp;
  user_data_args[2] = NULL;
  user_data_args[3] = NULL;

  ctx = g_option_context_new ("value-pairs");
  og = g_option_group_new (NULL, NULL, NULL, user_data_args, NULL);
  g_option_group_add_entries (og, vp_options);
  g_option_context_set_main_group (ctx, og);

  success = g_option_context_parse (ctx, &argc, &argv, error);
  vp_cmdline_parse_rekey_finish (user_data_args);
  g_option_context_free (ctx);

  if (!success)
    {
      value_pairs_unref (vp);
      vp = NULL;
    }

  return vp;
}
