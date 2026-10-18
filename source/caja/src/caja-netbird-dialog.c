/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include "caja-netbird-dialog.h"
#include "caja-window-manage-views.h"

enum { NAME, ADDRESS, STATE, N_COLUMNS };
enum { RESPONSE_REFRESH = 1 };
typedef struct {
    GtkWidget *dialog, *view, *search, *message, *user, *path, *protocol, *remember;
    GtkListStore *store;
    GtkTreeModel *filter;
    CajaWindow *window;
    GSubprocess *process;
    GCancellable *cancel;
    GKeyFile *profiles;
    gchar *profile_path;
    gboolean closed;
} NetbirdDialog;

static gchar *
selected_value (NetbirdDialog *d, gint column)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *value = NULL;
    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (d->view)), &model, &iter))
        gtk_tree_model_get (model, &iter, column, &value, -1);
    return value;
}

static gboolean
visible_peer (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
    NetbirdDialog *d = data;
    gchar *name, *address, *folded, *needle;
    gboolean visible;
    gtk_tree_model_get (model, iter, NAME, &name, ADDRESS, &address, -1);
    folded = g_utf8_casefold (name, -1);
    needle = g_utf8_casefold (gtk_entry_get_text (GTK_ENTRY (d->search)), -1);
    visible = strstr (folded, needle) != NULL || strstr (address, needle) != NULL;
    g_free (name); g_free (address); g_free (folded); g_free (needle);
    return visible;
}

static void
search_changed (GtkEntry *entry, gpointer data)
{
    NetbirdDialog *d = data;
    gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (d->filter));
}

static void
selection_changed (GtkTreeSelection *selection, gpointer data)
{
    NetbirdDialog *d = data;
    gchar *name = selected_value (d, NAME);
    gchar *group, *user, *path, *protocol;
    gtk_dialog_set_response_sensitive (GTK_DIALOG (d->dialog), GTK_RESPONSE_ACCEPT, name != NULL);
    if (!name) return;
    group = g_compute_checksum_for_string (G_CHECKSUM_SHA256, name, -1);
    user = g_key_file_get_string (d->profiles, group, "user", NULL);
    path = g_key_file_get_string (d->profiles, group, "path", NULL);
    protocol = g_key_file_get_string (d->profiles, group, "protocol", NULL);
    gtk_entry_set_text (GTK_ENTRY (d->user), user ? user : g_get_user_name ());
    gtk_entry_set_text (GTK_ENTRY (d->path), path ? path : "/");
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (d->protocol), protocol ? protocol : "sftp");
    g_free (name); g_free (group); g_free (user); g_free (path); g_free (protocol);
}

static void
peers_loaded (GObject *source, GAsyncResult *result, gpointer data)
{
    GtkWidget *dialog = data;
    NetbirdDialog *d = g_object_get_data (G_OBJECT (dialog), "netbird");
    gchar *out = NULL, *err = NULL;
    GError *error = NULL;
    GKeyFile *peers = g_key_file_new ();
    gboolean ok = g_subprocess_communicate_utf8_finish (G_SUBPROCESS (source), result, &out, &err, &error);
    if (d->closed) goto done;
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), RESPONSE_REFRESH, TRUE);
    if (!ok || !g_subprocess_get_successful (G_SUBPROCESS (source)) ||
        !g_key_file_load_from_data (peers, out ? out : "", -1, G_KEY_FILE_NONE, &error)) {
        gtk_label_set_text (GTK_LABEL (d->message), error ? error->message :
                            (err && *err ? err : "Nie udało się odczytać NetBird."));
        goto done;
    }
    gint count = g_key_file_get_integer (peers, "netbird", "count", NULL);
    gboolean connected = g_key_file_get_boolean (peers, "netbird", "connected", NULL);
    for (gint i = 0; i < count && i < 10000; i++) {
        gchar *group = g_strdup_printf ("peer%d", i);
        gchar *name = g_key_file_get_string (peers, group, "name", NULL);
        gchar *address = g_key_file_get_string (peers, group, "address", NULL);
        gchar *state = g_key_file_get_string (peers, group, "status", NULL);
        if (name && address && g_hostname_is_ip_address (address)) {
            GtkTreeIter iter;
            gtk_list_store_append (d->store, &iter);
            gtk_list_store_set (d->store, &iter, NAME, name, ADDRESS, address,
                                STATE, g_strcmp0 (state, "Connected") == 0 ? "Połączony" : "Brak połączenia", -1);
        }
        g_free (group); g_free (name); g_free (address); g_free (state);
    }
    gtk_label_set_text (GTK_LABEL (d->message), connected ?
        "Wybierz komputer i podaj konto na nim. Połączenie NetBird nie oznacza dostępności SFTP/SMB." :
        "NetBird nie jest połączony z serwerem zarządzającym. Lista może być nieaktualna.");
done:
    g_clear_object (&d->process);
    g_clear_object (&d->cancel);
    g_clear_error (&error);
    g_key_file_unref (peers);
    g_free (out); g_free (err);
    g_object_unref (dialog);
}

static void
refresh_peers (NetbirdDialog *d)
{
    GError *error = NULL;
    gchar *python = g_find_program_in_path ("python3");
    gtk_list_store_clear (d->store);
    gtk_dialog_set_response_sensitive (GTK_DIALOG (d->dialog), GTK_RESPONSE_ACCEPT, FALSE);
    if (!python) {
        gtk_label_set_text (GTK_LABEL (d->message), "Brak Python 3 — nie można odczytać listy NetBird.");
        return;
    }
    {
        const char *env_data = g_getenv ("CAJA_DATADIR");
        char *script_path = g_build_filename (env_data && *env_data ? env_data : CAJA_DATADIR, "netbird-peers.py", NULL);
        d->process = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
                                      &error, python, script_path, NULL);
        g_free (script_path);
    }
    g_free (python);
    if (!d->process) {
        gtk_label_set_text (GTK_LABEL (d->message), error->message);
        g_error_free (error);
        return;
    }
    gtk_label_set_text (GTK_LABEL (d->message), "Pobieranie komputerów NetBird…");
    gtk_dialog_set_response_sensitive (GTK_DIALOG (d->dialog), RESPONSE_REFRESH, FALSE);
    d->cancel = g_cancellable_new ();
    g_subprocess_communicate_utf8_async (d->process, NULL, d->cancel, peers_loaded, g_object_ref (d->dialog));
}

static void
dialog_response (GtkDialog *dialog, gint response, gpointer data)
{
    NetbirdDialog *d = data;
    if (response == RESPONSE_REFRESH) { refresh_peers (d); return; }
    if (response == GTK_RESPONSE_ACCEPT) {
        gchar *address = selected_value (d, ADDRESS);
        gchar *name = selected_value (d, NAME);
        const gchar *protocol = gtk_combo_box_get_active_id (GTK_COMBO_BOX (d->protocol));
        const gchar *user = gtk_entry_get_text (GTK_ENTRY (d->user));
        const gchar *path = gtk_entry_get_text (GTK_ENTRY (d->path));
        if (!address || !protocol) { g_free (address); g_free (name); return; }
        gchar *escaped_user = g_uri_escape_string (user, NULL, FALSE);
        gchar *absolute_path = g_str_has_prefix (path, "/") ? g_strdup (path) : g_strconcat ("/", path, NULL);
        gchar *escaped_path = g_uri_escape_string (absolute_path, "/", FALSE);
        gchar *host = strchr (address, ':') ? g_strdup_printf ("[%s]", address) : g_strdup (address);
        gchar *uri = g_strdup_printf ("%s://%s%s%s%s", protocol, escaped_user, *user ? "@" : "", host, escaped_path);
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d->remember))) {
            gchar *group = g_compute_checksum_for_string (G_CHECKSUM_SHA256, name, -1);
            gchar *directory = g_path_get_dirname (d->profile_path);
            GError *error = NULL;
            g_key_file_set_string (d->profiles, group, "user", user);
            g_key_file_set_string (d->profiles, group, "path", path);
            g_key_file_set_string (d->profiles, group, "protocol", protocol);
            if (g_mkdir_with_parents (directory, 0700) != 0 ||
                !g_key_file_save_to_file (d->profiles, d->profile_path, &error)) {
                gtk_label_set_text (GTK_LABEL (d->message), error ? error->message : "Nie można zapisać profilu.");
                g_clear_error (&error);
                g_free (group); g_free (directory); g_free (address); g_free (name);
                g_free (escaped_user); g_free (absolute_path); g_free (escaped_path); g_free (host); g_free (uri);
                return;
            }
            g_chmod (d->profile_path, 0600);
            g_free (group); g_free (directory);
        }
        GFile *location = g_file_new_for_uri (uri);
        caja_window_go_to (d->window, location);
        g_object_unref (location);
        g_free (address); g_free (name); g_free (escaped_user); g_free (absolute_path);
        g_free (escaped_path); g_free (host); g_free (uri);
    }
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
dialog_destroyed (GtkWidget *widget, gpointer data)
{
    NetbirdDialog *d = data;
    d->closed = TRUE;
    if (d->cancel) g_cancellable_cancel (d->cancel);
    if (d->process) g_subprocess_force_exit (d->process);
}

static void
dialog_free (gpointer data)
{
    NetbirdDialog *d = data;
    g_clear_object (&d->store);
    g_clear_object (&d->filter);
    g_clear_object (&d->process);
    g_clear_object (&d->cancel);
    g_key_file_unref (d->profiles);
    g_free (d->profile_path);
    g_free (d);
}

void
caja_netbird_dialog_show (CajaWindow *window)
{
    NetbirdDialog *d = g_new0 (NetbirdDialog, 1);
    d->window = window;
    d->profiles = g_key_file_new ();
    d->profile_path = g_build_filename (g_get_user_config_dir (), "caja", "netbird-connections.ini", NULL);
    g_key_file_load_from_file (d->profiles, d->profile_path, G_KEY_FILE_NONE, NULL);
    d->dialog = gtk_dialog_new_with_buttons ("Komputery NetBird", GTK_WINDOW (window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "Odśwież", RESPONSE_REFRESH,
        "Zamknij", GTK_RESPONSE_CANCEL, "Połącz", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size (GTK_WINDOW (d->dialog), 760, 520);
    GtkWidget *box = gtk_dialog_get_content_area (GTK_DIALOG (d->dialog));
    gtk_container_set_border_width (GTK_CONTAINER (box), 12);
    gtk_box_set_spacing (GTK_BOX (box), 10);
    d->search = gtk_search_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (d->search), "Szukaj nazwy komputera lub adresu IP");
    gtk_box_pack_start (GTK_BOX (box), d->search, FALSE, FALSE, 0);
    d->store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    d->filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (d->store), NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (d->filter), visible_peer, d, NULL);
    d->view = gtk_tree_view_new_with_model (d->filter);
    const gchar *titles[] = {"Komputer", "Adres NetBird", "Sieć"};
    for (gint i = 0; i < N_COLUMNS; i++) {
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes (titles[i], gtk_cell_renderer_text_new (), "text", i, NULL);
        gtk_tree_view_column_set_resizable (column, TRUE);
        gtk_tree_view_append_column (GTK_TREE_VIEW (d->view), column);
    }
    GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scroll), d->view);
    gtk_box_pack_start (GTK_BOX (box), scroll, TRUE, TRUE, 0);
    GtkWidget *grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (grid), 8);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
    d->protocol = gtk_combo_box_text_new ();
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (d->protocol), "sftp", "SFTP — pliki przez SSH");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (d->protocol), "smb", "SMB — foldery udostępnione");
    gtk_combo_box_set_active (GTK_COMBO_BOX (d->protocol), 0);
    d->user = gtk_entry_new ();
    d->path = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (d->path), "/");
    gtk_entry_set_placeholder_text (GTK_ENTRY (d->user), "Konto na zdalnym komputerze");
    gtk_entry_set_placeholder_text (GTK_ENTRY (d->path), "np. / lub /nazwa-udziału dla SMB");
    GtkWidget *fields[] = {d->protocol, d->user, d->path};
    const gchar *labels[] = {"Połączenie", "Użytkownik", "Folder / udział"};
    for (gint i = 0; i < 3; i++) {
        GtkWidget *label = gtk_label_new (labels[i]);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_grid_attach (GTK_GRID (grid), label, 0, i, 1, 1);
        gtk_widget_set_hexpand (fields[i], TRUE);
        gtk_grid_attach (GTK_GRID (grid), fields[i], 1, i, 1, 1);
    }
    gtk_box_pack_start (GTK_BOX (box), grid, FALSE, FALSE, 0);
    d->remember = gtk_check_button_new_with_label ("Zapamiętaj konto i folder dla tego komputera (bez hasła)");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (d->remember), TRUE);
    gtk_box_pack_start (GTK_BOX (box), d->remember, FALSE, FALSE, 0);
    d->message = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL (d->message), TRUE);
    gtk_label_set_xalign (GTK_LABEL (d->message), 0);
    gtk_box_pack_start (GTK_BOX (box), d->message, FALSE, FALSE, 0);
    g_object_set_data_full (G_OBJECT (d->dialog), "netbird", d, dialog_free);
    g_signal_connect (d->search, "search-changed", G_CALLBACK (search_changed), d);
    g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (d->view)), "changed", G_CALLBACK (selection_changed), d);
    g_signal_connect (d->dialog, "response", G_CALLBACK (dialog_response), d);
    g_signal_connect (d->dialog, "destroy", G_CALLBACK (dialog_destroyed), d);
    gtk_widget_show_all (d->dialog);
    gtk_window_present (GTK_WINDOW (d->dialog));
    gtk_widget_grab_focus (d->search);
    refresh_peers (d);
}
