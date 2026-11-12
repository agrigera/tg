/*
    tg
    Copyright (C) 2015 Marcello Mamino

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "tg.h"

#if !GLIB_CHECK_VERSION(2,40,0)
static gboolean
g_key_file_save_to_file (GKeyFile     *key_file,
                         const gchar  *filename,
                         GError      **error)
{
  gchar *contents;
  gboolean success;
  gsize length;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  contents = g_key_file_to_data (key_file, &length, NULL);
  g_assert (contents != NULL);

  success = g_file_set_contents (filename, contents, length, error);
  g_free (contents);

  return success;
}
#endif

#define g_key_file_get_int g_key_file_get_integer
#define g_key_file_set_int g_key_file_set_integer // the devil may take glib

void load_config(struct main_window *w)
{
	w->config_file = g_key_file_new();
	w->config_file_name = g_build_filename(g_get_user_config_dir(), CONFIG_FILE_NAME, NULL);
	w->conf_data = malloc(sizeof(struct conf_data));
#define SETUP(NAME,PLACE,TYPE) \
	w -> conf_data -> PLACE = w -> PLACE;

	CONFIG_FIELDS(SETUP);

	debug("Config: loading configuration file %s\n", w->config_file_name);
	gboolean ret = g_key_file_load_from_file(w->config_file, w->config_file_name, G_KEY_FILE_KEEP_COMMENTS, NULL);
	if(!ret) {
		debug("Config: failed to load config file");
		return;
	}
#define LOAD(NAME,PLACE,TYPE) \
	{ \
		GError *e = NULL; \
		TYPE val = g_key_file_get_ ## TYPE (w->config_file, "tg", #NAME, &e); \
		if(e) { \
			debug("Config: error loading field " #NAME "\n"); \
			g_error_free(e); \
		} else { \
			w -> PLACE = val; \
			w -> conf_data -> PLACE = val; \
		} \
	}

	CONFIG_FIELDS(LOAD);
}

void save_config(struct main_window *w)
{
	debug("Config: saving configuration file\n");

	g_key_file_set_string(w->config_file, "tg", "version", VERSION);

#define SAVE(NAME,PLACE,TYPE) \
	g_key_file_set_ ## TYPE (w->config_file, "tg", #NAME, w -> PLACE); \
	w -> conf_data -> PLACE = w -> PLACE;

	CONFIG_FIELDS(SAVE);

#ifdef DEBUG
	GError *ge = NULL;
	g_key_file_save_to_file(w->config_file, w->config_file_name, &ge);

	if(ge) {
		debug("Config: failed to save config file: %s\n",ge->message);
		g_error_free(ge);
		if(testing) exit(1);
	}
#else
	g_key_file_save_to_file(w->config_file, w->config_file_name, NULL);
#endif
}

void save_on_change(struct main_window *w)
{
#define CHANGED(NAME,PLACE,TYPE) \
	if(w -> PLACE != w -> conf_data -> PLACE) { \
		save_config(w); \
		return; \
	}

	CONFIG_FIELDS(CHANGED);
}

/* Saved watches
 *
 * Each watch is stored as its own group in the config file:
 *
 *   [watch:Omega 2500]
 *   bph=25200
 *   lift_angle=47
 *
 * bph=0 means "guess". Group order in the file is the order shown in the UI.
 */

#define WATCH_GROUP_PREFIX "watch:"

static gchar *watch_group(const char *name)
{
	return g_strconcat(WATCH_GROUP_PREFIX, name, NULL);
}

int watch_name_valid(const char *name)
{
	if(!name || !*name) return 0;
	if(!g_utf8_validate(name, -1, NULL)) return 0;
	if(g_utf8_strlen(name, -1) > WATCH_NAME_MAX) return 0;
	if(g_ascii_isspace(name[0]) || g_ascii_isspace(name[strlen(name) - 1])) return 0;
	const char *p;
	for(p = name; *p; p++) {
		/* GKeyFile group names cannot contain brackets or control characters */
		if(*p == '[' || *p == ']' || g_ascii_iscntrl(*p)) return 0;
	}
	return 1;
}

int get_watch(struct main_window *w, const char *name, int *bph, double *la)
{
	if(!watch_name_valid(name)) return 1;

	gchar *group = watch_group(name);
	GError *e = NULL;
	int b = g_key_file_get_integer(w->config_file, group, "bph", &e);
	double l = 0;
	if(!e) l = g_key_file_get_double(w->config_file, group, "lift_angle", &e);
	g_free(group);

	if(e) {
		g_error_free(e);
		return 1;
	}
	if(b != 0 && (b < MIN_BPH || b > MAX_BPH)) return 1;
	if(l < MIN_LA || l > MAX_LA) return 1;

	*bph = b;
	*la = l;
	return 0;
}

int watch_exists(struct main_window *w, const char *name)
{
	if(!watch_name_valid(name)) return 0;
	gchar *group = watch_group(name);
	gboolean ret = g_key_file_has_group(w->config_file, group);
	g_free(group);
	return ret;
}

gchar **list_watches(struct main_window *w)
{
	gsize n = 0, i, j = 0;
	gchar **groups = g_key_file_get_groups(w->config_file, &n);
	gchar **names = g_new0(gchar *, n + 1);
	size_t prefix_len = strlen(WATCH_GROUP_PREFIX);

	for(i = 0; i < n; i++) {
		if(strncmp(groups[i], WATCH_GROUP_PREFIX, prefix_len)) continue;
		const char *name = groups[i] + prefix_len;
		int bph;
		double la;
		if(get_watch(w, name, &bph, &la)) {
			debug("Config: ignoring invalid watch entry [%s]\n", groups[i]);
			continue;
		}
		names[j++] = g_strdup(name);
	}

	g_strfreev(groups);
	return names;
}

void set_watch(struct main_window *w, const char *name, int bph, double la)
{
	if(!watch_name_valid(name)) return;
	gchar *group = watch_group(name);
	g_key_file_set_integer(w->config_file, group, "bph", bph);
	g_key_file_set_double(w->config_file, group, "lift_angle", la);
	g_free(group);
	save_config(w);
}

void delete_watch(struct main_window *w, const char *name)
{
	if(!watch_name_valid(name)) return;
	gchar *group = watch_group(name);
	g_key_file_remove_group(w->config_file, group, NULL);
	g_free(group);
	save_config(w);
}

void close_config(struct main_window *w)
{
	g_key_file_free(w->config_file);
	g_free(w->config_file_name);
	free(w->conf_data);
}
