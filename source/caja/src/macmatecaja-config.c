#include "macmatecaja-config.h"
#include <gio/gio.h>

static gchar *left_path;
static gchar *right_path;
static GList *places;

static gboolean
valid_text (const gchar *text)
{
    const gchar *p;
    if (text == NULL || *text == '\0' || !g_utf8_validate (text, -1, NULL))
        return FALSE;
    for (p = text; *p != '\0'; p++)
        if ((guchar) *p < 32 || (guchar) *p == 127)
            return FALSE;
    return TRUE;
}

/* No shell evaluation, tilde expansion, or credential logging. */
static gboolean
valid_location (const gchar *text)
{
    GUri *uri;
    const gchar *scheme;
    gboolean valid = FALSE;
    if (!valid_text (text))
        return FALSE;
    if (g_path_is_absolute (text))
        return TRUE;
    uri = g_uri_parse (text, G_URI_FLAGS_NONE, NULL);
    if (uri == NULL)
        return FALSE;
    scheme = g_uri_get_scheme (uri);
    if (g_strcmp0 (scheme, "file") == 0) {
        gchar *filename = g_filename_from_uri (text, NULL, NULL);
        valid = filename != NULL && g_path_is_absolute (filename);
        g_free (filename);
    } else if (g_strcmp0 (scheme, "sftp") == 0 ||
               g_strcmp0 (scheme, "smb") == 0 ||
               g_strcmp0 (scheme, "ftp") == 0 ||
               g_strcmp0 (scheme, "afp") == 0 ||
               g_strcmp0 (scheme, "dav") == 0 ||
               g_strcmp0 (scheme, "davs") == 0) {
        valid = g_uri_get_host (uri) != NULL && *g_uri_get_host (uri) != '\0';
    }
    g_uri_unref (uri);
    return valid;
}

void
macmatecaja_config_free_places (GList *list)
{
    GList *entry;
    for (entry = list; entry != NULL; entry = entry->next) {
        MacMateCajaPlace *place = entry->data;
        g_free (place->label);
        g_free (place->uri);
        g_free (place->path);
        g_free (place);
    }
    g_list_free (list);
}

static gchar *
read_location (GKeyFile *file, const gchar *group, const gchar *key)
{
    gchar *value = g_key_file_get_string (file, group, key, NULL);
    if (!valid_location (value))
        g_clear_pointer (&value, g_free);
    return value;
}

void
macmatecaja_config_init (void)
{
    const gchar *override = g_getenv ("MACMATECAJA_CONFIG_FILE");
    const gchar *xdg = g_getenv ("XDG_CONFIG_HOME");
    gchar *filename;
    GKeyFile *file;
    gchar **keys;
    gsize i;

    g_clear_pointer (&left_path, g_free);
    g_clear_pointer (&right_path, g_free);
    macmatecaja_config_free_places (places);
    places = NULL;

    if (override != NULL && *override != '\0') {
        /* An invalid explicit override fails closed, never reads the cwd. */
        if (!valid_text (override) || !g_path_is_absolute (override))
            return;
        filename = g_strdup (override);
    } else if (xdg != NULL && g_path_is_absolute (xdg)) {
        filename = g_build_filename (xdg, "macmatecaja", "config.ini", NULL);
    } else {
        filename = g_build_filename (g_get_home_dir (), ".config",
                                     "macmatecaja", "config.ini", NULL);
    }
    file = g_key_file_new ();
    if (!g_key_file_load_from_file (file, filename, G_KEY_FILE_NONE, NULL))
        goto out;
    left_path = read_location (file, "ui", "left_path");
    right_path = read_location (file, "ui", "right_path");
    keys = g_key_file_get_keys (file, "places", NULL, NULL);
    for (i = 0; keys != NULL && keys[i] != NULL; i++) {
        gchar *value = read_location (file, "places", keys[i]);
        MacMateCajaPlace *place;
        GFile *location;
        if (value == NULL || !valid_text (keys[i])) {
            g_free (value);
            continue;
        }
        location = g_file_new_for_commandline_arg (value);
        place = g_new0 (MacMateCajaPlace, 1);
        place->label = g_strdup (keys[i]);
        place->uri = g_file_get_uri (location);
        place->path = g_file_get_path (location);
        places = g_list_prepend (places, place);
        g_object_unref (location);
        g_free (value);
    }
    places = g_list_reverse (places);
    g_strfreev (keys);
out:
    g_key_file_unref (file);
    g_free (filename);
}

gchar *
macmatecaja_config_get_left_path (void)
{
    gchar *desktop;
    if (left_path != NULL)
        return g_strdup (left_path);
    desktop = g_build_filename (g_get_home_dir (), "Desktop", NULL);
    if (g_file_test (desktop, G_FILE_TEST_IS_DIR))
        return desktop;
    g_free (desktop);
    return g_strdup (g_get_home_dir ());
}

gchar *
macmatecaja_config_get_right_path (void)
{
    return g_strdup (right_path != NULL ? right_path : g_get_home_dir ());
}

GList *
macmatecaja_config_get_places (void)
{
    GList *entry, *copy = NULL;
    for (entry = places; entry != NULL; entry = entry->next) {
        const MacMateCajaPlace *place = entry->data;
        MacMateCajaPlace *cloned = g_new0 (MacMateCajaPlace, 1);
        cloned->label = g_strdup (place->label);
        cloned->uri = g_strdup (place->uri);
        cloned->path = g_strdup (place->path);
        copy = g_list_prepend (copy, cloned);
    }
    return g_list_reverse (copy);
}
