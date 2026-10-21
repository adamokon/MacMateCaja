/* Synthetic regression tests; no access to real files or remote locations. */
#include "../source/caja/libcaja-private/caja-deep-count-tracker.h"
#include "../source/caja/libcaja-private/caja-file-attributes.h"
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <unistd.h>

static void test_move_permissions (void)
{
    GError *error = NULL;
    char *parent, *child;
    GFile *file;
    GFileInfo *info;
    gboolean writable, removable;
    if (geteuid () == 0) {
        g_test_skip ("Root bypasses the synthetic parent permission restriction");
        return;
    }
    parent = g_dir_make_tmp ("macmatecaja-permission-test-XXXXXX", &error);
    g_assert_no_error (error);
    child = g_build_filename (parent, "owned-child", NULL);
    g_assert_cmpint (g_mkdir (child, 0700), ==, 0);
    g_assert_cmpint (g_chmod (parent, 0500), ==, 0);
    file = g_file_new_for_path (child);
    info = g_file_query_info (file, "access::can-write,access::can-delete",
                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
    writable = info && g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
    removable = info && g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);
    /* Restore only our fixture, including on a failed query, before asserting. */
    g_assert_cmpint (g_chmod (parent, 0700), ==, 0);
    g_assert_cmpint (g_rmdir (child), ==, 0);
    g_assert_cmpint (g_rmdir (parent), ==, 0);
    g_clear_object (&info);
    g_object_unref (file);
    g_free (child);
    g_free (parent);
    g_assert_no_error (error);
    g_assert_true (writable);
    g_assert_false (removable);
}

static void test_display_readiness (void)
{
    g_assert_cmpuint (CAJA_FILE_ATTRIBUTES_FOR_DISPLAY, ==,
                     CAJA_FILE_ATTRIBUTE_INFO | CAJA_FILE_ATTRIBUTE_LINK_INFO);
    g_assert_cmpuint (CAJA_FILE_ATTRIBUTES_FOR_DISPLAY &
                     (CAJA_FILE_ATTRIBUTE_THUMBNAIL | CAJA_FILE_ATTRIBUTE_DEEP_COUNTS |
                      CAJA_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT), ==, 0);
}

static void test_identity (void)
{
    GHashTable *set = caja_deep_count_inode_set_new ();
    guint64 high = ((guint64) 1 << 60) + 42;
    g_assert_false (caja_deep_count_record_inode (set, 0));
    g_assert_false (caja_deep_count_record_inode (set, 0));
    g_assert_false (caja_deep_count_record_inode (set, 42));
    g_assert_false (caja_deep_count_record_inode (set, high));
    g_assert_true (caja_deep_count_record_inode (set, high));
    g_assert_true (caja_deep_count_record_inode (set, 42));
    g_assert_cmpuint (g_hash_table_size (set), ==, 2);
    g_hash_table_destroy (set);
}

static void test_progress (void)
{
    gint64 last = 1000000;
    g_assert_false (caja_deep_count_progress_due (&last, 1000001));
    g_assert_false (caja_deep_count_progress_due (&last, 1099999));
    g_assert_true (caja_deep_count_progress_due (&last, 1100000));
    g_assert_false (caja_deep_count_progress_due (&last, 1100001));
    g_assert_true (caja_deep_count_progress_due (&last, 1200000));
}

typedef struct {
    GHashTable *set;
    GMainLoop *loop;
    guint count, beats, batches;
} Work;

static gboolean heartbeat (gpointer data)
{
    ((Work *) data)->beats++;
    return G_SOURCE_CONTINUE;
}

static gboolean batch (gpointer data)
{
    Work *work = data;
    for (guint n = 0; n < 100; n++)
        g_assert_false (caja_deep_count_record_inode (work->set, ++work->count));
    work->batches++;
    if (work->count == 500000) {
        g_main_loop_quit (work->loop);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void test_large_async_stream (void)
{
    Work work = { caja_deep_count_inode_set_new (), g_main_loop_new (NULL, FALSE), 0, 0, 0 };
    gint64 start = g_get_monotonic_time ();
    guint timer = g_timeout_add (1, heartbeat, &work);
    g_idle_add_full (G_PRIORITY_LOW, batch, &work, NULL);
    g_main_loop_run (work.loop);
    g_source_remove (timer);
    g_assert_cmpuint (work.batches, ==, 5000);
    g_assert_cmpuint (work.beats, >, 0);
    g_assert_cmpuint (g_hash_table_size (work.set), ==, 500000);
    g_test_message ("500000 entries: %.3fs, %u main-loop heartbeats",
                    (g_get_monotonic_time () - start) / 1000000.0, work.beats);
    g_main_loop_unref (work.loop);
    g_hash_table_destroy (work.set);
}

int main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/deep-count/identity", test_identity);
    g_test_add_func ("/display/readiness-without-thumbnails", test_display_readiness);
    g_test_add_func ("/move/parent-permission-required", test_move_permissions);
    g_test_add_func ("/deep-count/progress", test_progress);
    g_test_add_func ("/deep-count/large-async-stream", test_large_async_stream);
    return g_test_run ();
}
