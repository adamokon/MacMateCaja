/* MacMateCaja: bounded-cost bookkeeping for asynchronous directory counts.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef CAJA_DEEP_COUNT_TRACKER_H
#define CAJA_DEEP_COUNT_TRACKER_H

#include <glib.h>

static inline GHashTable *
caja_deep_count_inode_set_new (void)
{
    return g_hash_table_new_full (g_int64_hash, g_int64_equal, g_free, NULL);
}

/* TRUE means this inode was already counted. Missing/zero inodes cannot
 * identify hardlinks, so each such entry must contribute its own size. */
static inline gboolean
caja_deep_count_record_inode (GHashTable *inodes, guint64 inode)
{
    guint64 *key;
    if (inode == 0)
        return FALSE;
    if (g_hash_table_contains (inodes, &inode))
        return TRUE;
    key = g_new (guint64, 1);
    *key = inode;
    g_hash_table_add (inodes, key);
    return FALSE;
}

static inline gboolean
caja_deep_count_progress_due (gint64 *last_update, gint64 now)
{
    if (now - *last_update < 100 * G_TIME_SPAN_MILLISECOND)
        return FALSE;
    *last_update = now;
    return TRUE;
}

#endif
