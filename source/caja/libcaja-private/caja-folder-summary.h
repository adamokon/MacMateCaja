/* Content hints collected by the existing asynchronous deep-count job.
 * Extension hints only: never open file bodies or launch thumbnailers here. */
#ifndef CAJA_FOLDER_SUMMARY_H
#define CAJA_FOLDER_SUMMARY_H

#include <glib.h>
#include <string.h>

#define CAJA_FOLDER_KINDS_KEY "macmatecaja-folder-content-kinds"

static inline guint
caja_folder_content_kind (const char *name)
{
    const char *extension = name ? strrchr (name, '.') : NULL;
    static const char *groups[] = {
        "|pdf|", "|jpg|jpeg|png|gif|webp|heic|tif|tiff|svg|bmp|",
        "|mp4|mov|mkv|avi|webm|", "|mp3|wav|flac|m4a|ogg|",
        "|txt|md|csv|json|xml|yaml|yml|c|h|py|js|sh|log|",
        "|doc|docx|odt|xls|xlsx|ods|ppt|pptx|odp|",
        "|zip|7z|rar|gz|tar|bz2|xz|"
    };
    guint kind = 1u << 7;
    if (extension != NULL)
    {
        char *lower = g_ascii_strdown (extension + 1, -1);
        char *needle = g_strdup_printf ("|%s|", lower);
        for (guint i = 0; i < G_N_ELEMENTS (groups); i++)
            if (strstr (groups[i], needle) != NULL)
            {
                kind = 1u << i;
                break;
            }
        g_free (needle);
        g_free (lower);
    }
    return kind;
}

#endif
