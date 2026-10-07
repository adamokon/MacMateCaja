#include "macmatecaja-config.h"
#include <glib/gstdio.h>

static gchar *test_root;
static gchar *config_file;

static void load (const gchar *text)
{
    g_assert_true (g_file_set_contents (config_file, text, -1, NULL));
    g_setenv ("MACMATECAJA_CONFIG_FILE", config_file, TRUE);
    macmatecaja_config_init ();
}

static void defaults (void)
{
    gchar *right;
    g_setenv ("MACMATECAJA_CONFIG_FILE", "/nonexistent/macmatecaja/config.ini", TRUE);
    macmatecaja_config_init ();
    right = macmatecaja_config_get_right_path ();
    g_assert_cmpstr (right, ==, g_get_home_dir ());
    g_assert_null (macmatecaja_config_get_places ());
    g_free (right);
}

static void valid_and_owned (void)
{
    gchar *left;
    GList *first, *second;
    MacMateCajaPlace *place;
    load ("[ui]\nleft_path=/tmp/Zażółć gęślą\nright_path=sftp://user@example.invalid/a%20b\n"
          "[places]\nZdjęcia=/tmp/Obrazy żółte\nServer=sftp://user@example.invalid/docs\n");
    left = macmatecaja_config_get_left_path ();
    g_assert_cmpstr (left, ==, "/tmp/Zażółć gęślą");
    left[0] = 'X';
    g_free (left);
    left = macmatecaja_config_get_left_path ();
    g_assert_cmpstr (left, ==, "/tmp/Zażółć gęślą");
    g_free (left);
    first = macmatecaja_config_get_places ();
    second = macmatecaja_config_get_places ();
    g_assert_cmpuint (g_list_length (first), ==, 2);
    place = first->data;
    g_assert_cmpstr (place->path, ==, "/tmp/Obrazy żółte");
    g_assert_true (g_str_has_prefix (place->uri, "file:///tmp/Obrazy%20"));
    place->label[0] = 'X';
    g_assert_cmpstr (((MacMateCajaPlace *) second->data)->label, ==, "Zdjęcia");
    g_assert_null (((MacMateCajaPlace *) first->next->data)->path);
    macmatecaja_config_free_places (first);
    macmatecaja_config_free_places (second);
}

static void invalid_and_reinit (void)
{
    gchar *right;
    GList *list;
    load ("[ui]\nright_path=relative/path\n[places]\nRelative=relative\n"
          "Command=$(touch /tmp/never-run-this)\nNoHost=sftp:///docs\n"
          "BadEscape=sftp://example.invalid/%zz\nBadScheme=https://example.invalid\n"
          "Newline=/tmp/hello\\nworld\nGood=file:///tmp/good%20file\n");
    right = macmatecaja_config_get_right_path ();
    g_assert_cmpstr (right, ==, g_get_home_dir ());
    g_free (right);
    list = macmatecaja_config_get_places ();
    g_assert_cmpuint (g_list_length (list), ==, 1);
    g_assert_cmpstr (((MacMateCajaPlace *) list->data)->path, ==, "/tmp/good file");
    macmatecaja_config_free_places (list);
    load ("this is not an ini file");
    g_assert_null (macmatecaja_config_get_places ());
    g_setenv ("MACMATECAJA_CONFIG_FILE", "relative.ini", TRUE);
    macmatecaja_config_init ();
    g_assert_null (macmatecaja_config_get_places ());
}

static void default_location (void)
{
    gchar *dir = g_build_filename (test_root, "xdg", "macmatecaja", NULL);
    gchar *file = g_build_filename (dir, "config.ini", NULL);
    gchar *right;
    gchar *xdg;
    g_assert_cmpint (g_mkdir_with_parents (dir, 0700), ==, 0);
    g_assert_true (g_file_set_contents (file, "[ui]\nright_path=/tmp/configured\n", -1, NULL));
    g_unsetenv ("MACMATECAJA_CONFIG_FILE");
    xdg = g_path_get_dirname (dir);
    g_setenv ("XDG_CONFIG_HOME", xdg, TRUE);
    g_free (xdg);
    macmatecaja_config_init ();
    right = macmatecaja_config_get_right_path ();
    g_assert_cmpstr (right, ==, "/tmp/configured");
    g_free (right);
    g_remove (file);
    g_rmdir (dir);
    g_free (file);
    g_free (dir);
}

int main (int argc, char **argv)
{
    gint result;
    gchar *xdg;
    g_test_init (&argc, &argv, NULL);
    test_root = g_dir_make_tmp ("macmatecaja-config-test-XXXXXX", NULL);
    g_assert_nonnull (test_root);
    config_file = g_build_filename (test_root, "config.ini", NULL);
    g_test_add_func ("/config/defaults", defaults);
    g_test_add_func ("/config/valid-owned", valid_and_owned);
    g_test_add_func ("/config/invalid-reinit", invalid_and_reinit);
    g_test_add_func ("/config/xdg", default_location);
    result = g_test_run ();
    g_remove (config_file);
    xdg = g_build_filename (test_root, "xdg", NULL);
    g_rmdir (xdg);
    g_free (xdg);
    g_rmdir (test_root);
    g_free (test_root);
    g_free (config_file);
    return result;
}
