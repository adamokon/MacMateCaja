/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* caja-program-choosing.c - functions for selecting and activating
 				 programs for opening/viewing particular files.

   Copyright (C) 2000 Eazel, Inc.

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
   Boston, MA 02110-1301, USA.

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#ifdef __APPLE__
#include <mate-submodules/libegg/eggdesktopfile.h>
#else
#include <gio/gdesktopappinfo.h>
#endif
#include <gdk/gdk.h>
#ifdef HAVE_X11
#include <gdk/gdkx.h>
#endif

#include <eel/eel-mate-extensions.h>
#include <eel/eel-stock-dialogs.h>

#include "caja-program-choosing.h"
#include "caja-mime-actions.h"
#include "caja-global-preferences.h"
#include "caja-icon-info.h"
#include "caja-recent.h"
#include "caja-desktop-icon-file.h"

/**
 * application_cannot_open_location
 *
 * Handle the case where an application has been selected to be launched,
 * and it cannot handle the current uri scheme.  This can happen
 * because the default application for a file type may not be able
 * to handle some kinds of locations.   We want to tell users that their
 * default application doesn't work here, rather than switching off to
 * a different one without them noticing.
 *
 * @application: The application that was to be launched.
 * @file: The file whose location was passed as a parameter to the application
 * @parent_window: A window to use as the parent for any error dialogs.
 *  */
static void
application_cannot_open_location (GAppInfo *application,
                                  CajaFile *file,
                                  const char *uri_scheme,
                                  GtkWindow *parent_window)
{
#ifdef NEW_MIME_COMPLETE
    char *prompt;
    char *message;
    char *file_name;

    file_name = caja_file_get_display_name (file);

    if (caja_mime_has_any_applications_for_file (file))
    {
        GtkDialog *message_dialog;
        int response;

        if (application != NULL)
        {
            prompt = _("Open Failed, would you like to choose another application?");
            message = g_strdup_printf (_("\"%s\" cannot open \"%s\" because \"%s\" cannot access files at \"%s\" "
                                         "locations."),
                                       g_app_info_get_display_name (application), file_name,
                                       g_app_info_get_display_name (application), uri_scheme);
        }
        else
        {
            prompt = _("Open Failed, would you like to choose another action?");
            message = g_strdup_printf (_("The default action cannot open \"%s\" because it cannot access files at \"%s\" "
                                         "locations."),
                                       file_name, uri_scheme);
        }

        message_dialog = eel_show_yes_no_dialog (prompt,
                                                 message,
                                                 "gtk-ok",
                                                 "process-stop",
                                                 parent_window);

        response = gtk_dialog_run (message_dialog);
        gtk_widget_destroy (GTK_WIDGET (message_dialog));

        if (response == GTK_RESPONSE_YES)
        {
            LaunchParameters *launch_parameters;

            launch_parameters = launch_parameters_new (file, parent_window);
            caja_choose_application_for_file
            (file,
             parent_window,
             launch_application_callback,
             launch_parameters);

        }
        g_free (message);
    }
    else
    {
        if (application != NULL)
        {
            prompt = g_strdup_printf (_("\"%s\" cannot open \"%s\" because \"%s\" cannot access files at \"%s\" "
                                        "locations."), g_app_info_get_display_name (application), file_name,
                                      g_app_info_get_display_name (application), uri_scheme);
            message = _("No other applications are available to view this file. "
                        "If you copy this file onto your computer, you may be able to open "
                        "it.");
        }
        else
        {
            prompt = g_strdup_printf (_("The default action cannot open \"%s\" because it cannot access files at \"%s\" "
                                        "locations."), file_name, uri_scheme);
            message = _("No other actions are available to view this file. "
                        "If you copy this file onto your computer, you may be able to open "
                        "it.");
        }

        eel_show_info_dialog (prompt, message, parent_window);
        g_free (prompt);
    }

    g_free (file_name);
#endif
}

/**
 * caja_launch_application:
 *
 * Fork off a process to launch an application with a given file as a
 * parameter. Provide a parent window for error dialogs.
 *
 * @application: The application to be launched.
 * @uris: The files whose locations should be passed as a parameter to the application.
 * @parent_window: A window to use as the parent for any error dialogs.
 */
void
caja_launch_application (GAppInfo *application,
                         GList *files,
                         GtkWindow *parent_window)
{
    GList *uris, *l;

    uris = NULL;
    for (l = files; l != NULL; l = l->next)
    {
        uris = g_list_prepend (uris, caja_file_get_activation_uri (l->data));
    }
    uris = g_list_reverse (uris);
    caja_launch_application_by_uri (application, uris,
                                    parent_window);
    g_list_free_full (uris, g_free);
}

#ifndef __APPLE__
static void
dummy_child_watch (GPid     pid,
                   gint     status,
                   gpointer user_data)
{
  /* Nothing, this is just to ensure we don't double fork
   * and break pkexec:
   * https://bugzilla.gnome.org/show_bug.cgi?id=675789
   */
}

static void
gather_pid_callback (GDesktopAppInfo *appinfo,
                     GPid            pid,
                     gpointer        data)
{
    g_child_watch_add(pid, dummy_child_watch, NULL);
}
#endif

static void
launch_application_uris_now (GAppInfo *application,
                             GList *uris,
                             GList *recent_uris,
                             GtkWindow *parent_window)
{
    CajaFile *file;
    gboolean result;
    GError *error;
    GdkDisplay *display;
    GdkAppLaunchContext *launch_context;
    CajaIconInfo *icon;
    int icon_scale;
    GList *l;

    g_assert (uris != NULL);

    if (parent_window != NULL) {
            display = gtk_widget_get_display (GTK_WIDGET (parent_window));
    } else {
            display = gdk_display_get_default ();
    }

    launch_context = gdk_display_get_app_launch_context (display);

    if (parent_window != NULL) {
        gdk_app_launch_context_set_screen (launch_context,
                                           gtk_window_get_screen (parent_window));
    }

    file = caja_file_get_by_uri (uris->data);
    icon_scale = parent_window != NULL ? gtk_widget_get_scale_factor (GTK_WIDGET (parent_window)) : 1;
    icon = caja_file_get_icon (file,
                               48, icon_scale,
                               0);
    caja_file_unref (file);
    if (icon)
    {
        gdk_app_launch_context_set_icon_name (launch_context,
                                              caja_icon_info_get_used_name (icon));
        g_object_unref (icon);
    }

    error = NULL;

#ifdef __APPLE__
    {
        GPtrArray *open_argv;

        /* GLib's desktop-file application launcher does not integrate with
         * macOS Launch Services reliably. All URIs reaching this point have
         * already been made local (ordinary files or editable rclone mounts),
         * so hand their paths to the native `open` command. This honors the
         * user's default macOS application for PDF, images, text, and other
         * document types. */
        open_argv = g_ptr_array_new_with_free_func (g_free);
        g_ptr_array_add (open_argv, g_strdup ("/usr/bin/open"));
        g_ptr_array_add (open_argv, g_strdup ("--"));

        for (l = uris; l != NULL; l = l->next)
        {
            char *path;

            path = g_filename_from_uri (l->data, NULL, &error);
            if (path == NULL)
            {
                break;
            }
            g_ptr_array_add (open_argv, path);
        }

        if (error == NULL)
        {
            g_ptr_array_add (open_argv, NULL);
            result = g_spawn_async (NULL,
                                    (char **) open_argv->pdata,
                                    NULL,
                                    0,
                                    NULL, NULL, NULL,
                                    &error);
        }
        else
        {
            result = FALSE;
        }

        g_ptr_array_free (open_argv, TRUE);
    }
#else
    result = g_desktop_app_info_launch_uris_as_manager (G_DESKTOP_APP_INFO (application),
                                                        uris,
                                                        G_APP_LAUNCH_CONTEXT (launch_context),
                                                        G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                                        NULL, NULL,
                                                        gather_pid_callback, application,
                                                        &error);
#endif

    g_object_unref (launch_context);

    if (!result)
    {
        if (error->domain == G_IO_ERROR &&
                error->code == G_IO_ERROR_NOT_SUPPORTED)
        {
            char *uri_scheme;

            uri_scheme = g_uri_parse_scheme (uris->data);
            application_cannot_open_location (application,
                                              file,
                                              uri_scheme,
                                              parent_window);
            g_free (uri_scheme);
        }
        else
        {
#ifdef NEW_MIME_COMPLETE
            caja_program_chooser_show_invalid_message
            (MATE_VFS_MIME_ACTION_TYPE_APPLICATION, file, parent_window);
#else
            g_warning ("Cannot open app: %s\n", error->message);
#endif
        }
        g_error_free (error);
    }
    else
    {
        for (l = recent_uris; l != NULL; l = l->next)
        {
            file = caja_file_get_by_uri (l->data);
            caja_recent_add_file (file, application);
            caja_file_unref (file);
        }
    }
}

#ifdef __APPLE__
static char *
get_rclone_ssh_identity_file (const char *user,
                              const char *host,
                              int port)
{
    char *destination;
    char *port_string;
    char *standard_output;
    char **lines;
    char *identity_file;
    char *argv[7];
    int status;
    guint i;

    destination = g_strdup_printf ("%s@%s", user, host);
    port_string = g_strdup_printf ("%d", port);
    standard_output = NULL;
    identity_file = NULL;

    argv[0] = "/usr/bin/ssh";
    argv[1] = "-G";
    argv[2] = "-p";
    argv[3] = port_string;
    argv[4] = destination;
    argv[5] = NULL;
    argv[6] = NULL;

    if (g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL,
                      &standard_output, NULL, &status, NULL) &&
            g_spawn_check_wait_status (status, NULL))
    {
        lines = g_strsplit (standard_output, "\n", -1);
        for (i = 0; lines[i] != NULL; i++)
        {
            if (g_str_has_prefix (lines[i], "identityfile "))
            {
                const char *configured_path;
                char *expanded_path;

                configured_path = lines[i] + strlen ("identityfile ");
                if (g_str_has_prefix (configured_path, "~/"))
                {
                    expanded_path = g_build_filename (g_get_home_dir (),
                                                      configured_path + 2,
                                                      NULL);
                }
                else
                {
                    expanded_path = g_strdup (configured_path);
                }

                if (g_file_test (expanded_path, G_FILE_TEST_IS_REGULAR))
                {
                    identity_file = expanded_path;
                    break;
                }
                g_free (expanded_path);
            }
        }
        g_strfreev (lines);
    }

    g_free (standard_output);
    g_free (port_string);
    g_free (destination);
    return identity_file;
}

static char *
get_rclone_local_uri (const char *uri)
{
    GUri *parsed_uri;
    const char *scheme;
    const char *host;
    const char *uri_user;
    const char *encoded_path;
    const char *relative_path;
    const char *user;
    char *decoded_path;
    char *authority;
    char *authority_checksum;
    char *runtime_base;
    char *mount_base;
    char *mount_path;
    char *cache_path;
    char *local_path;
    char *local_uri;
    char *identity_file;
    char *known_hosts_file;
    char *remote;
    char *standard_error;
    char *argv[20];
    int port;
    int status;
    guint arg_index;
    gboolean mounted;

    parsed_uri = g_uri_parse (uri,
                              G_URI_FLAGS_PARSE_RELAXED | G_URI_FLAGS_ENCODED_PATH,
                              NULL);
    if (parsed_uri == NULL)
    {
        return NULL;
    }

    scheme = g_uri_get_scheme (parsed_uri);
    host = g_uri_get_host (parsed_uri);
    if (g_strcmp0 (scheme, "sftp") != 0 || host == NULL)
    {
        g_uri_unref (parsed_uri);
        return NULL;
    }

    uri_user = g_uri_get_user (parsed_uri);
    user = uri_user != NULL && uri_user[0] != '\0' ? uri_user : g_get_user_name ();
    port = g_uri_get_port (parsed_uri);
    if (port < 0)
    {
        port = 22;
    }

    encoded_path = g_uri_get_path (parsed_uri);
    decoded_path = g_uri_unescape_string (encoded_path != NULL ? encoded_path : "/", NULL);
    if (decoded_path == NULL)
    {
        g_uri_unref (parsed_uri);
        return NULL;
    }
    relative_path = decoded_path[0] == '/' ? decoded_path + 1 : decoded_path;

    authority = g_strdup_printf ("%s@%s:%d", user, host, port);
    authority_checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, authority, -1);
    runtime_base = g_build_filename (g_get_tmp_dir (), "MacMateCaja", NULL);
    mount_base = g_build_filename (runtime_base, "rclone-mounts", NULL);
    cache_path = g_build_filename (runtime_base, "rclone-vfs", NULL);
    mount_path = g_build_filename (mount_base, authority_checksum, NULL);
    local_path = g_build_filename (mount_path, relative_path, NULL);
    local_uri = NULL;

    mounted = g_file_test (local_path, G_FILE_TEST_EXISTS);
    if (!mounted && g_mkdir_with_parents (mount_path, 0700) == 0 &&
            g_mkdir_with_parents (cache_path, 0700) == 0)
    {
        identity_file = get_rclone_ssh_identity_file (user, host, port);
        known_hosts_file = g_build_filename (g_get_home_dir (), ".ssh", "known_hosts", NULL);
        if (!g_file_test (known_hosts_file, G_FILE_TEST_IS_REGULAR))
        {
            g_clear_pointer (&known_hosts_file, g_free);
        }

        if (identity_file != NULL && known_hosts_file != NULL)
        {
            remote = g_strdup_printf (":sftp,host=%s,user=%s,port=%d,key_file=%s,known_hosts_file=%s:/",
                                      host, user, port, identity_file, known_hosts_file);
        }
        else if (identity_file != NULL)
        {
            remote = g_strdup_printf (":sftp,host=%s,user=%s,port=%d,key_file=%s:/",
                                      host, user, port, identity_file);
        }
        else
        {
            remote = g_strdup_printf (":sftp,host=%s,user=%s,port=%d:/",
                                      host, user, port);
        }

        standard_error = NULL;
        arg_index = 0;
        argv[arg_index++] = "rclone";
        argv[arg_index++] = "nfsmount";
        argv[arg_index++] = remote;
        argv[arg_index++] = mount_path;
        argv[arg_index++] = "--daemon";
        argv[arg_index++] = "--daemon-wait";
        argv[arg_index++] = "3s";
        argv[arg_index++] = "--vfs-cache-mode";
        argv[arg_index++] = "full";
        argv[arg_index++] = "--vfs-write-back";
        argv[arg_index++] = "2s";
        argv[arg_index++] = "--vfs-cache-max-age";
        argv[arg_index++] = "10m";
        argv[arg_index++] = "--cache-dir";
        argv[arg_index++] = cache_path;
        argv[arg_index++] = "--log-level";
        argv[arg_index++] = "ERROR";
        argv[arg_index] = NULL;

        if (g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                          NULL, NULL, NULL, &standard_error, &status, NULL) &&
                g_spawn_check_wait_status (status, NULL))
        {
            mounted = g_file_test (local_path, G_FILE_TEST_EXISTS);
        }

        if (!mounted && standard_error != NULL && standard_error[0] != '\0')
        {
            g_warning ("Could not create editable rclone mount: %s", standard_error);
        }

        g_free (standard_error);
        g_free (remote);
        g_free (known_hosts_file);
        g_free (identity_file);
    }

    if (mounted)
    {
        local_uri = g_filename_to_uri (local_path, NULL, NULL);
    }

    g_free (local_path);
    g_free (mount_path);
    g_free (cache_path);
    g_free (mount_base);
    g_free (runtime_base);
    g_free (authority_checksum);
    g_free (authority);
    g_free (decoded_path);
    g_uri_unref (parsed_uri);
    return local_uri;
}

#endif

void
caja_launch_application_by_uri (GAppInfo *application,
                                GList *uris,
                                GtkWindow *parent_window)
{
#ifdef __APPLE__
    GList *launch_uris;
    GList *l;
    gboolean mount_failed;

    g_assert (uris != NULL);

    launch_uris = NULL;
    mount_failed = FALSE;

    for (l = uris; l != NULL; l = l->next)
    {
        const char *uri;
        GFile *source;
        char *local_path;
        char *rclone_local_uri;

        uri = l->data;
        source = g_file_new_for_uri (uri);
        local_path = g_file_get_path (source);
        rclone_local_uri = local_path == NULL ? get_rclone_local_uri (uri) : NULL;

        if (rclone_local_uri != NULL)
        {
            launch_uris = g_list_prepend (launch_uris, rclone_local_uri);
        }
        else if (local_path != NULL)
        {
            launch_uris = g_list_prepend (launch_uris,
                                          g_filename_to_uri (local_path, NULL, NULL));
        }
        else
        {
            mount_failed = TRUE;
        }

        g_object_unref (source);
        g_free (local_path);

        if (mount_failed)
        {
            break;
        }
    }

    if (mount_failed)
    {
        eel_show_error_dialog (_("The remote file could not be opened."),
                               _("MacMateCaja could not create an editable rclone mount. No temporary copy was created."),
                               parent_window);
    }
    else
    {
        launch_uris = g_list_reverse (launch_uris);
        launch_application_uris_now (application,
                                     launch_uris,
                                     uris,
                                     parent_window);
    }
    g_list_free_full (launch_uris, g_free);
#else
    launch_application_uris_now (application, uris, uris, parent_window);
#endif
}

/**
 * caja_launch_application_from_command:
 *
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 *
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @...: Passed as parameters to the application after quoting each of them.
 */
void
caja_launch_application_from_command (GdkScreen  *screen,
                                      const char *name,
                                      const char *command_string,
                                      gboolean use_terminal,
                                      ...)
{
    char *full_command, *tmp;
    char *parameter;
    va_list ap;

    full_command = g_strdup (command_string);

    va_start (ap, use_terminal);

    while ((parameter = va_arg (ap, char *)) != NULL)
    {
        char *quoted_parameter;

        quoted_parameter = g_shell_quote (parameter);
        tmp = g_strconcat (full_command, " ", quoted_parameter, NULL);
        g_free (quoted_parameter);

        g_free (full_command);
        full_command = tmp;

    }

    va_end (ap);

    if (use_terminal)
    {
        eel_mate_open_terminal_on_screen (full_command, screen);
    }
    else
    {
        GAppInfo *app_info = NULL;
        app_info = g_app_info_create_from_commandline (full_command,
                                                       NULL,
                                                       G_APP_INFO_CREATE_NONE,
                                                       NULL);
        if (app_info != NULL)
        {
            GdkAppLaunchContext *launch_context;
            GdkDisplay *display;

            display = gdk_screen_get_display (screen);
            launch_context = gdk_display_get_app_launch_context (display);
            gdk_app_launch_context_set_screen (launch_context, screen);
            g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (launch_context), NULL);
            g_object_unref (launch_context);
            g_object_unref (app_info);
        }
    }

    g_free (full_command);
}

/**
 * caja_launch_application_from_command:
 *
 * Fork off a process to launch an application with a given uri as
 * a parameter.
 *
 * @command_string: The application to be launched, with any desired
 * command-line options.
 * @parameters: Passed as parameters to the application after quoting each of them.
 */
void
caja_launch_application_from_command_array (GdkScreen  *screen,
        const char *name,
        const char *command_string,
        gboolean use_terminal,
        const char * const * parameters)
{
    char *full_command, *tmp;

    full_command = g_strdup (command_string);

    if (parameters != NULL)
    {
        const char * const *p;

        for (p = parameters; *p != NULL; p++)
        {
            char *quoted_parameter;

            quoted_parameter = g_shell_quote (*p);
            tmp = g_strconcat (full_command, " ", quoted_parameter, NULL);
            g_free (quoted_parameter);

            g_free (full_command);
            full_command = tmp;
        }
    }

    if (use_terminal)
    {
        eel_mate_open_terminal_on_screen (full_command, screen);
    }
    else
    {
        GAppInfo *app_info = NULL;
        app_info = g_app_info_create_from_commandline (full_command,
                                                       NULL,
                                                       G_APP_INFO_CREATE_NONE,
                                                       NULL);
        if (app_info != NULL)
        {
            GdkAppLaunchContext *launch_context;
            GdkDisplay *display;

            display = gdk_screen_get_display (screen);
            launch_context = gdk_display_get_app_launch_context (display);
            gdk_app_launch_context_set_screen (launch_context, screen);
            g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (launch_context), NULL);
            g_object_unref (launch_context);
            g_object_unref (app_info);
        }
    }

    g_free (full_command);
}

void
caja_launch_desktop_file (GdkScreen   *screen,
                          const char  *desktop_file_uri,
                          const GList *parameter_uris,
                          GtkWindow   *parent_window)
{
    GError *error;
    char *desktop_file_path;
    const GList *p;
    GList *files;
    int total, count;
#ifdef __APPLE__
    EggDesktopFile *desktop_info;
    GSList *documents;
    gboolean launched;
#else
    GDesktopAppInfo *app_info;
#endif
    GdkAppLaunchContext *context;
    GdkDisplay *display;
    GFile *desktop_file;
    GFile *file = NULL;

    /* Don't allow command execution from remote locations
     * to partially mitigate the security
     * risk of executing arbitrary commands.
     */
    desktop_file = g_file_new_for_uri (desktop_file_uri);
    desktop_file_path = g_file_get_path (desktop_file);
    if (!g_file_is_native (desktop_file))
    {
        g_free (desktop_file_path);
        g_object_unref (desktop_file);
        eel_show_error_dialog
        (_("Sorry, but you cannot execute commands from "
           "a remote site."),
         _("This is disabled due to security considerations."),
         parent_window);

        return;
    }
    g_object_unref (desktop_file);

#ifdef __APPLE__
    desktop_info = egg_desktop_file_new (desktop_file_path, NULL);
    g_free (desktop_file_path);
    if (desktop_info == NULL)
    {
        eel_show_error_dialog
        (_("There was an error launching the application."),
         NULL,
         parent_window);
        return;
    }
#else
    app_info = g_desktop_app_info_new_from_filename (desktop_file_path);
    g_free (desktop_file_path);
    if (app_info == NULL)
    {
        eel_show_error_dialog
        (_("There was an error launching the application."),
         NULL,
         parent_window);
        return;
    }
#endif

    /* count the number of uris with local paths */
    count = 0;
    total = g_list_length ((GList *) parameter_uris);
    files = NULL;
#ifdef __APPLE__
    documents = NULL;
#endif
    for (p = parameter_uris; p != NULL; p = p->next)
    {
        file = g_file_new_for_uri ((const char *) p->data);
        if (g_file_is_native (file))
        {
            count++;
        }
        files = g_list_prepend (files, file);
#ifdef __APPLE__
        documents = g_slist_prepend (documents, (gpointer) p->data);
#endif
    }
#ifdef __APPLE__
    documents = g_slist_reverse (documents);
#endif

    /* check if this app only supports local files */
#ifdef __APPLE__
    if (egg_desktop_file_accepts_documents (desktop_info) &&
            !egg_desktop_file_accepts_uris (desktop_info) &&
            parameter_uris != NULL)
#else
    if (g_app_info_supports_files (G_APP_INFO (app_info)) &&
            !g_app_info_supports_uris (G_APP_INFO (app_info)) &&
            parameter_uris != NULL)
#endif
    {
        if (count == 0)
        {
            /* all files are non-local */
            eel_show_error_dialog
            (_("This drop target only supports local files."),
             _("To open non-local files copy them to a local folder and then"
               " drop them again."),
             parent_window);

            g_list_free_full (files, g_object_unref);
#ifdef __APPLE__
            g_slist_free (documents);
            egg_desktop_file_free (desktop_info);
#else
            g_object_unref (app_info);
#endif
            return;
        }
        else if (count != total)
        {
            /* some files are non-local */
            eel_show_warning_dialog
            (_("This drop target only supports local files."),
             _("To open non-local files copy them to a local folder and then"
               " drop them again. The local files you dropped have already been opened."),
             parent_window);
        }
    }

    error = NULL;

    if (parent_window != NULL)
    {
        display = gtk_widget_get_display (GTK_WIDGET (parent_window));
    }
    else if (screen != NULL)
    {
        display = gdk_screen_get_display (screen);
    }
    else
    {
        display = gdk_display_get_default ();
    }

    context = gdk_display_get_app_launch_context (display);

    /* TODO: Ideally we should accept a timestamp here instead of using GDK_CURRENT_TIME */
    gdk_app_launch_context_set_timestamp (context, GDK_CURRENT_TIME);
    if (parent_window != NULL)
    {
        gdk_app_launch_context_set_screen (context,
                                           gtk_window_get_screen (parent_window));
    }
    else if (screen != NULL)
    {
        gdk_app_launch_context_set_screen (context, screen);
    }
#ifdef __APPLE__
    launched = egg_desktop_file_launch (desktop_info,
                                        documents,
                                        &error,
                                        EGG_DESKTOP_FILE_LAUNCH_SCREEN, screen,
                                        EGG_DESKTOP_FILE_LAUNCH_TIME, GDK_CURRENT_TIME,
                                        NULL);
    (void) launched;
#else
    g_desktop_app_info_launch_uris_as_manager (app_info,
                                               (GList *) parameter_uris,
                                               G_APP_LAUNCH_CONTEXT (context),
                                               G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                               NULL, NULL,
                                               gather_pid_callback, app_info,
                                               &error);
#endif
    if (error != NULL)
    {
        char *message;

        message = g_strconcat (_("Details: "), error->message, NULL);
        eel_show_error_dialog
        (_("There was an error launching the application."),
         message,
         parent_window);

        g_error_free (error);
        g_free (message);
    }

    g_list_free_full (files, g_object_unref);
#ifdef __APPLE__
    g_slist_free (documents);
    egg_desktop_file_free (desktop_info);
#else
    g_object_unref (app_info);
#endif
    g_object_unref (context);
}
