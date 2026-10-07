/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-container.h - the container widget for file manager icons

   Copyright (C) 2002 Sun Microsystems, Inc.

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

   Author: Michael Meeks <michael@ximian.com>
*/
#include <config.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <eel/eel-glib-extensions.h>

#include <libcaja-private/caja-global-preferences.h>
#include <libcaja-private/caja-file-attributes.h>
#include <libcaja-private/caja-thumbnails.h>
#include <libcaja-private/caja-desktop-icon-file.h>
#include <libcaja-private/caja-folder-summary.h>

#include "fm-icon-container.h"

G_DEFINE_TYPE (FMIconContainer, fm_icon_container, CAJA_TYPE_ICON_CONTAINER);

static GQuark attribute_none_q;

static FMIconView *
get_icon_view (CajaIconContainer *container)
{
    /* Type unsafe comparison for performance */
    return ((FMIconContainer *)container)->view;
}

static CajaIconInfo *
folder_summary_icon (CajaFile *file, int size, int scale, gboolean accepting)
{
    guint directories = 0, files = 0, unreadable = 0;
    CajaRequestStatus status = caja_file_get_deep_counts (file, &directories, &files,
                                                        &unreadable, NULL, NULL, TRUE);
    guint kinds = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (file), CAJA_FOLDER_KINDS_KEY));
    static const char *labels[] = { "PDF", "IMG", "VID", "MUS", "TXT", "DOC", "ZIP", "FILE" };
    const char *label = "DIR";
    char *count;
    cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size * scale, size * scale);
    cairo_t *cr = cairo_create (surface);
    cairo_scale (cr, size * scale / 48.0, size * scale / 48.0);

    /* A tabbed amber silhouette is deliberately different from document sheets. */
    cairo_move_to (cr, 3, 10); cairo_line_to (cr, 19, 10);
    cairo_line_to (cr, 24, 15); cairo_line_to (cr, 45, 15);
    cairo_line_to (cr, 45, 42); cairo_line_to (cr, 3, 42); cairo_close_path (cr);
    cairo_set_source_rgb (cr, .69, .43, .12); cairo_fill (cr);
    cairo_rectangle (cr, 3, 19, 42, 23);
    cairo_set_source_rgb (cr, accepting ? 1.0 : .94, accepting ? .83 : .67, .25);
    cairo_fill (cr);

    if (status == CAJA_REQUEST_DONE && unreadable == 0 && directories == 0)
    {
        /* No descendant directories, not necessarily an empty folder. */
        cairo_set_source_rgba (cr, .31, .21, .08, .65);
        cairo_set_line_width (cr, 3);
        cairo_move_to (cr, 7, 39); cairo_line_to (cr, 40, 18); cairo_stroke (cr);
    }
    if (size >= 32)
    {
        if (kinds != 0)
        {
            label = (kinds & 1) ? "PDF+" : "MIX";
            for (guint i = 0; i < G_N_ELEMENTS (labels); i++)
                if (kinds == (1u << i)) label = labels[i];
        }
        else if (status != CAJA_REQUEST_DONE) label = "...";
        else if (unreadable != 0) label = "?";

        cairo_rectangle (cr, 5, 21, 29, 12);
        cairo_set_source_rgb (cr, .16, .20, .25); cairo_fill (cr);
        cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size (cr, 9);
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_move_to (cr, 7, 30); cairo_show_text (cr, label);

        count = status == CAJA_REQUEST_NOT_STARTED ? g_strdup ("...") :
            g_strdup_printf ("%u%s", directories + files,
                             status != CAJA_REQUEST_DONE ? "+" : unreadable ? "?" : "");
        cairo_text_extents_t extents;
        cairo_set_font_size (cr, 9);
        cairo_text_extents (cr, count, &extents);
        if (extents.width > 38) cairo_set_font_size (cr, 9 * 38 / extents.width);
        cairo_text_extents (cr, count, &extents);
        cairo_rectangle (cr, 43 - extents.width - 4, 34, extents.width + 6, 12);
        cairo_set_source_rgb (cr, .10, .13, .18); cairo_fill (cr);
        cairo_set_source_rgb (cr, 1, 1, 1);
        cairo_move_to (cr, 43 - extents.width - 1, 43); cairo_show_text (cr, count);
        g_free (count);
    }
    cairo_destroy (cr);
    GdkPixbuf *pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, size * scale, size * scale);
    CajaIconInfo *icon = caja_icon_info_new_for_pixbuf (pixbuf, scale);
    g_object_unref (pixbuf);
    cairo_surface_destroy (surface);
    return icon;
}

static CajaIconInfo *
fm_icon_container_get_icon_images (CajaIconContainer *container,
                                   CajaIconData      *data,
                                   int                    size,
                                   GList                **emblem_pixbufs,
                                   char                 **embedded_text,
                                   gboolean               for_drag_accept,
                                   gboolean               need_large_embeddded_text,
                                   gboolean              *embedded_text_needs_loading,
                                   gboolean              *has_window_open)
{
    FMIconView *icon_view;
    CajaFile *file;
    gboolean use_embedding;
    CajaFileIconFlags flags;
    guint emblem_size;
    gint scale;

    file = (CajaFile *) data;

    g_assert (CAJA_IS_FILE (file));
    icon_view = get_icon_view (container);
    g_return_val_if_fail (icon_view != NULL, NULL);

    use_embedding = FALSE;
    if (embedded_text)
    {
        *embedded_text = caja_file_peek_top_left_text (file, need_large_embeddded_text, embedded_text_needs_loading);
        use_embedding = *embedded_text != NULL;
    }

    if (emblem_pixbufs != NULL)
    {
        emblem_size = caja_icon_get_emblem_size_for_icon_size (size);
        /* don't return images larger than the actual icon size */
        emblem_size = MIN (emblem_size, size);

        if (emblem_size > 0)
        {
            char **emblems_to_ignore;

            emblems_to_ignore = fm_directory_view_get_emblem_names_to_exclude
                                (FM_DIRECTORY_VIEW (icon_view));
            *emblem_pixbufs = caja_file_get_emblem_pixbufs (file,
                              emblem_size,
                              FALSE,
                              emblems_to_ignore);
            g_strfreev (emblems_to_ignore);
        }
    }

    *has_window_open = caja_file_has_open_window (file);

    flags = CAJA_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM;
    if (!fm_icon_view_is_compact (icon_view) ||
            caja_icon_container_get_zoom_level (container) > CAJA_ZOOM_LEVEL_STANDARD)
    {
        flags |= CAJA_FILE_ICON_FLAGS_USE_THUMBNAILS;
        if (fm_icon_view_is_compact (icon_view))
        {
            flags |= CAJA_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE;
        }
    }

    if (use_embedding)
    {
        flags |= CAJA_FILE_ICON_FLAGS_EMBEDDING_TEXT;
    }
    if (for_drag_accept)
    {
        flags |= CAJA_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT;
    }

    scale = gtk_widget_get_scale_factor (GTK_WIDGET (icon_view));

    if (caja_file_is_directory (file) && !CAJA_IS_DESKTOP_ICON_FILE (file))
        return folder_summary_icon (file, size, scale, for_drag_accept);

    return caja_file_get_icon (file, size, scale, flags);
}

static char *
fm_icon_container_get_icon_description (CajaIconContainer *container,
                                        CajaIconData      *data)
{
    CajaFile *file;
    char *mime_type;
    const char *description;

    file = CAJA_FILE (data);
    g_assert (CAJA_IS_FILE (file));

    if (CAJA_IS_DESKTOP_ICON_FILE (file))
    {
        return NULL;
    }

    if (caja_file_is_directory (file))
    {
        guint directories = 0, files = 0, unreadable = 0;
        CajaRequestStatus status = caja_file_get_deep_counts (file, &directories, &files,
                                                            &unreadable, NULL, NULL, TRUE);
        return g_strdup_printf ("Folder: %u files, %u subfolders (recursive)%s",
                                files, directories,
                                status != CAJA_REQUEST_DONE ? "; counting..." :
                                unreadable ? "; incomplete: unreadable folders" :
                                directories == 0 ? "; crossed out: no subfolders" : "");
    }

    mime_type = caja_file_get_mime_type (file);
    description = g_content_type_get_description (mime_type);
    g_free (mime_type);
    return g_strdup (description);
}

static void
fm_icon_container_start_monitor_top_left (CajaIconContainer *container,
        CajaIconData      *data,
        gconstpointer          client,
        gboolean               large_text)
{
    CajaFile *file;
    CajaFileAttributes attributes;

    file = (CajaFile *) data;

    g_assert (CAJA_IS_FILE (file));

    attributes = CAJA_FILE_ATTRIBUTE_TOP_LEFT_TEXT;
    if (large_text)
    {
        attributes |= CAJA_FILE_ATTRIBUTE_LARGE_TOP_LEFT_TEXT;
    }
    caja_file_monitor_add (file, client, attributes);
}

static void
fm_icon_container_stop_monitor_top_left (CajaIconContainer *container,
        CajaIconData      *data,
        gconstpointer          client)
{
    CajaFile *file;

    file = (CajaFile *) data;

    g_assert (CAJA_IS_FILE (file));

    caja_file_monitor_remove (file, client);
}

static void
fm_icon_container_prioritize_thumbnailing (CajaIconContainer *container,
        CajaIconData      *data)
{
    CajaFile *file;

    file = (CajaFile *) data;

    g_assert (CAJA_IS_FILE (file));

    if (caja_file_is_thumbnailing (file))
    {
        char *uri;

        uri = caja_file_get_uri (file);
        caja_thumbnail_prioritize (uri);
        g_free (uri);
    }
}

/*
 * Get the preference for which caption text should appear
 * beneath icons.
 */
static GQuark *
fm_icon_container_get_icon_text_attributes_from_preferences (void)
{
    static GQuark *attributes = NULL;

    if (attributes == NULL)
    {
        eel_g_settings_add_auto_strv_as_quarks (caja_icon_view_preferences,
                                                CAJA_PREFERENCES_ICON_VIEW_CAPTIONS,
                                                &attributes);
    }

    /* We don't need to sanity check the attributes list even though it came
     * from preferences.
     *
     * There are 2 ways that the values in the list could be bad.
     *
     * 1) The user picks "bad" values.  "bad" values are those that result in
     *    there being duplicate attributes in the list.
     *
     * 2) Value stored in MateConf are tampered with.  Its possible physically do
     *    this by pulling the rug underneath MateConf and manually editing its
     *    config files.  Its also possible to use a third party MateConf key
     *    editor and store garbage for the keys in question.
     *
     * Thankfully, the Caja preferences machinery deals with both of
     * these cases.
     *
     * In the first case, the preferences dialog widgetry prevents
     * duplicate attributes by making "bad" choices insensitive.
     *
     * In the second case, the preferences getter (and also the auto storage) for
     * string_array values are always valid members of the enumeration associated
     * with the preference.
     *
     * So, no more error checking on attributes is needed here and we can return
     * a the auto stored value.
     */
    return attributes;
}

static int
quarkv_length (GQuark *attributes)
{
    int i;
    i = 0;
    while (attributes[i] != 0)
    {
        i++;
    }
    return i;
}

/**
 * fm_icon_view_get_icon_text_attribute_names:
 *
 * Get a list representing which text attributes should be displayed
 * beneath an icon. The result is dependent on zoom level and possibly
 * user configuration. Don't free the result.
 * @view: FMIconView to query.
 *
 **/
static GQuark *
fm_icon_container_get_icon_text_attribute_names (CajaIconContainer *container,
        int *len)
{
    GQuark *attributes;
    int piece_count;

    const int pieces_by_level[] =
    {
        0,	/* CAJA_ZOOM_LEVEL_SMALLEST */
        0,	/* CAJA_ZOOM_LEVEL_SMALLER */
        0,	/* CAJA_ZOOM_LEVEL_SMALL */
        1,	/* CAJA_ZOOM_LEVEL_STANDARD */
        2,	/* CAJA_ZOOM_LEVEL_LARGE */
        2,	/* CAJA_ZOOM_LEVEL_LARGER */
        3	/* CAJA_ZOOM_LEVEL_LARGEST */
    };

    piece_count = pieces_by_level[caja_icon_container_get_zoom_level (container)];

    attributes = fm_icon_container_get_icon_text_attributes_from_preferences ();

    *len = MIN (piece_count, quarkv_length (attributes));

    return attributes;
}

/* This callback returns the text, both the editable part, and the
 * part below that is not editable.
 */
static void
fm_icon_container_get_icon_text (CajaIconContainer *container,
                                 CajaIconData      *data,
                                 char                 **editable_text,
                                 char                 **additional_text,
                                 gboolean               include_invisible)
{
    GQuark *attributes;
    char *text_array[4];
    int i, j, num_attributes;
    FMIconView *icon_view;
    CajaFile *file;
    gboolean use_additional;

    file = CAJA_FILE (data);

    g_assert (CAJA_IS_FILE (file));
    g_assert (editable_text != NULL);
    icon_view = get_icon_view (container);
    g_return_if_fail (icon_view != NULL);

    use_additional = (additional_text != NULL);

    /* In the smallest zoom mode, no text is drawn. */
    if (caja_icon_container_get_zoom_level (container) == CAJA_ZOOM_LEVEL_SMALLEST &&
            !include_invisible)
    {
        *editable_text = NULL;
    }
    else
    {
        /* Strip the suffix for caja object xml files. */
        *editable_text = caja_file_get_display_name (file);
    }

    if (!use_additional)
    {
        return;
    }

    if (fm_icon_view_is_compact (icon_view))
    {
        *additional_text = NULL;
        return;
    }

    if (CAJA_IS_DESKTOP_ICON_FILE (file))
    {
        /* Don't show the normal extra information for desktop icons, it doesn't
         * make sense. */
        *additional_text = NULL;
        return;
    }

    /* Handle link files specially. */
    if (caja_file_is_caja_link (file))
    {
        /* FIXME bugzilla.gnome.org 42531: Does sync. I/O and works only locally. */
        *additional_text = NULL;
        if (caja_file_is_local (file))
        {
            char *actual_uri;
            gchar *description;

            actual_uri = caja_file_get_uri (file);
            description = caja_link_local_get_additional_text (actual_uri);

            if (description)
                *additional_text = g_strdup_printf (" \n%s\n ", description);

            g_free (description);
            g_free (actual_uri);
        }
        /* Don't show the normal extra information for desktop files, it doesn't
         * make sense. */
        return;
    }

    /* Find out what attributes go below each icon. */
    attributes = fm_icon_container_get_icon_text_attribute_names (container,
                 &num_attributes);

    /* Get the attributes. */
    j = 0;
    for (i = 0; i < num_attributes; ++i)
    {
        if (attributes[i] == attribute_none_q)
        {
            continue;
        }

        text_array[j++] =
            caja_file_get_string_attribute_with_default_q (file, attributes[i]);
    }
    text_array[j] = NULL;

    /* Return them. */
    if (j == 0)
    {
        *additional_text = NULL;
    }
    else if (j == 1)
    {
        /* Only one item, avoid the strdup + free */
        *additional_text = text_array[0];
    }
    else
    {
        *additional_text = g_strjoinv ("\n", text_array);

        for (i = 0; i < j; i++)
        {
            g_free (text_array[i]);
        }
    }
}

/* Sort as follows:
 *   0) computer link
 *   1) home link
 *   2) network link
 *   3) mount links
 *   4) other
 *   5) trash link
 */
typedef enum
{
    SORT_COMPUTER_LINK,
    SORT_HOME_LINK,
    SORT_NETWORK_LINK,
    SORT_MOUNT_LINK,
    SORT_OTHER,
    SORT_TRASH_LINK
} SortCategory;

static SortCategory
get_sort_category (CajaFile *file)
{
    SortCategory category;

    category = SORT_OTHER;

    if (CAJA_IS_DESKTOP_ICON_FILE (file))
    {
        CajaDesktopLink *link;

        link = caja_desktop_icon_file_get_link (CAJA_DESKTOP_ICON_FILE (file));

        if (link != NULL)
        {
            switch (caja_desktop_link_get_link_type (link))
            {
            case CAJA_DESKTOP_LINK_COMPUTER:
                category = SORT_COMPUTER_LINK;
                break;
            case CAJA_DESKTOP_LINK_HOME:
                category = SORT_HOME_LINK;
                break;
            case CAJA_DESKTOP_LINK_MOUNT:
                category = SORT_MOUNT_LINK;
                break;
            case CAJA_DESKTOP_LINK_TRASH:
                category = SORT_TRASH_LINK;
                break;
            case CAJA_DESKTOP_LINK_NETWORK:
                category = SORT_NETWORK_LINK;
                break;
            default:
                category = SORT_OTHER;
                break;
            }
            g_object_unref (link);
        }
    }

    return category;
}

static int
fm_desktop_icon_container_icons_compare (CajaIconContainer *container,
        CajaIconData      *data_a,
        CajaIconData      *data_b)
{
    CajaFile *file_a;
    CajaFile *file_b;
    FMDirectoryView *directory_view;
    SortCategory category_a, category_b;

    file_a = (CajaFile *) data_a;
    file_b = (CajaFile *) data_b;

    directory_view = FM_DIRECTORY_VIEW (FM_ICON_CONTAINER (container)->view);
    g_return_val_if_fail (directory_view != NULL, 0);

    category_a = get_sort_category (file_a);
    category_b = get_sort_category (file_b);

    if (category_a == category_b)
    {
        return caja_file_compare_for_sort
               (file_a, file_b, CAJA_FILE_SORT_BY_DISPLAY_NAME,
                fm_directory_view_should_sort_directories_first (directory_view),
                FALSE);
    }

    if (category_a < category_b)
    {
        return -1;
    }
    else
    {
        return +1;
    }
}

static int
fm_icon_container_compare_icons (CajaIconContainer *container,
                                 CajaIconData      *icon_a,
                                 CajaIconData      *icon_b)
{
    FMIconView *icon_view;

    icon_view = get_icon_view (container);
    g_return_val_if_fail (icon_view != NULL, 0);

    if (FM_ICON_CONTAINER (container)->sort_for_desktop)
    {
        return fm_desktop_icon_container_icons_compare
               (container, icon_a, icon_b);
    }

    /* Type unsafe comparisons for performance */
    return fm_icon_view_compare_files (icon_view,
                                       (CajaFile *)icon_a,
                                       (CajaFile *)icon_b);
}

static int
fm_icon_container_compare_icons_by_name (CajaIconContainer *container,
        CajaIconData      *icon_a,
        CajaIconData      *icon_b)
{
    return caja_file_compare_for_sort
           (CAJA_FILE (icon_a),
            CAJA_FILE (icon_b),
            CAJA_FILE_SORT_BY_DISPLAY_NAME,
            FALSE, FALSE);
}

static void
fm_icon_container_freeze_updates (CajaIconContainer *container)
{
    FMIconView *icon_view;
    icon_view = get_icon_view (container);
    g_return_if_fail (icon_view != NULL);
    fm_directory_view_freeze_updates (FM_DIRECTORY_VIEW (icon_view));
}

static void
fm_icon_container_unfreeze_updates (CajaIconContainer *container)
{
    FMIconView *icon_view;
    icon_view = get_icon_view (container);
    g_return_if_fail (icon_view != NULL);
    fm_directory_view_unfreeze_updates (FM_DIRECTORY_VIEW (icon_view));
}

static void
fm_icon_container_dispose (GObject *object)
{
    FMIconContainer *icon_container;

    icon_container = FM_ICON_CONTAINER (object);

    icon_container->view = NULL;

    G_OBJECT_CLASS (fm_icon_container_parent_class)->dispose (object);
}

static void
fm_icon_container_class_init (FMIconContainerClass *klass)
{
    CajaIconContainerClass *ic_class;

    ic_class = &klass->parent_class;

    attribute_none_q = g_quark_from_static_string ("none");

    ic_class->get_icon_text = fm_icon_container_get_icon_text;
    ic_class->get_icon_images = fm_icon_container_get_icon_images;
    ic_class->get_icon_description = fm_icon_container_get_icon_description;
    ic_class->start_monitor_top_left = fm_icon_container_start_monitor_top_left;
    ic_class->stop_monitor_top_left = fm_icon_container_stop_monitor_top_left;
    ic_class->prioritize_thumbnailing = fm_icon_container_prioritize_thumbnailing;

    ic_class->compare_icons = fm_icon_container_compare_icons;
    ic_class->compare_icons_by_name = fm_icon_container_compare_icons_by_name;
    ic_class->freeze_updates = fm_icon_container_freeze_updates;
    ic_class->unfreeze_updates = fm_icon_container_unfreeze_updates;

    G_OBJECT_CLASS (klass)->dispose = fm_icon_container_dispose;
}

static void
fm_icon_container_init (FMIconContainer *icon_container)
{
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (icon_container)),
                                 GTK_STYLE_CLASS_VIEW);
}

CajaIconContainer *
fm_icon_container_construct (FMIconContainer *icon_container, FMIconView *view)
{
    AtkObject *atk_obj;

    g_return_val_if_fail (FM_IS_ICON_VIEW (view), NULL);

    icon_container->view = view;
    atk_obj = gtk_widget_get_accessible (GTK_WIDGET (icon_container));
    atk_object_set_name (atk_obj, _("Icon View"));

    return CAJA_ICON_CONTAINER (icon_container);
}

CajaIconContainer *
fm_icon_container_new (FMIconView *view)
{
    return fm_icon_container_construct
           (g_object_new (FM_TYPE_ICON_CONTAINER, NULL),
            view);
}

void
fm_icon_container_set_sort_desktop (FMIconContainer *container,
                                    gboolean         desktop)
{
    container->sort_for_desktop = desktop;
}
