#include "config.h"

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-bg.h>

struct _MateBG
{
  GObject parent_instance;
  char *filename;
  MateBGPlacement placement;
  MateBGColorType color_type;
  GdkRGBA primary;
  GdkRGBA secondary;
  gboolean draw_background;
};

struct _MateBGClass
{
  GObjectClass parent_class;
};

enum
{
  CHANGED,
  TRANSITIONED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (MateBG, mate_bg, G_TYPE_OBJECT)

static void
mate_bg_finalize (GObject *object)
{
  MateBG *bg = MATE_BG (object);

  g_clear_pointer (&bg->filename, g_free);
  G_OBJECT_CLASS (mate_bg_parent_class)->finalize (object);
}

static void
mate_bg_class_init (MateBGClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mate_bg_finalize;

  signals[CHANGED] = g_signal_new ("changed",
                                   G_OBJECT_CLASS_TYPE (object_class),
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL, NULL,
                                   g_cclosure_marshal_VOID__VOID,
                                   G_TYPE_NONE, 0);

  signals[TRANSITIONED] = g_signal_new ("transitioned",
                                        G_OBJECT_CLASS_TYPE (object_class),
                                        G_SIGNAL_RUN_LAST,
                                        0,
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE, 0);
}

static void
mate_bg_init (MateBG *bg)
{
  bg->placement = MATE_BG_PLACEMENT_CENTERED;
  bg->color_type = MATE_BG_COLOR_SOLID;
  bg->draw_background = FALSE;
  gdk_rgba_parse (&bg->primary, "#000000");
  gdk_rgba_parse (&bg->secondary, "#000000");
}

MateBG *
mate_bg_new (void)
{
  return g_object_new (MATE_TYPE_BG, NULL);
}

void
mate_bg_load_from_preferences (MateBG *bg)
{
}

void
mate_bg_load_from_system_preferences (MateBG *bg)
{
}

void
mate_bg_load_from_system_gsettings (MateBG    *bg,
                                    GSettings *settings,
                                    gboolean   reset_apply)
{
}

void
mate_bg_load_from_gsettings (MateBG    *bg,
                             GSettings *settings)
{
}

void
mate_bg_save_to_preferences (MateBG *bg)
{
}

void
mate_bg_save_to_gsettings (MateBG    *bg,
                           GSettings *settings)
{
}

void
mate_bg_set_filename (MateBG     *bg,
                      const char *filename)
{
  g_return_if_fail (MATE_IS_BG (bg));

  g_free (bg->filename);
  bg->filename = g_strdup (filename);
}

void
mate_bg_set_placement (MateBG          *bg,
                       MateBGPlacement  placement)
{
  g_return_if_fail (MATE_IS_BG (bg));

  bg->placement = placement;
}

void
mate_bg_set_color (MateBG          *bg,
                   MateBGColorType  type,
                   GdkRGBA         *primary,
                   GdkRGBA         *secondary)
{
  g_return_if_fail (MATE_IS_BG (bg));

  bg->color_type = type;
  if (primary != NULL)
    bg->primary = *primary;
  if (secondary != NULL)
    bg->secondary = *secondary;
}

void
mate_bg_set_draw_background (MateBG   *bg,
                             gboolean  draw_background)
{
  g_return_if_fail (MATE_IS_BG (bg));

  bg->draw_background = draw_background;
}

gboolean
mate_bg_get_draw_background (MateBG *bg)
{
  g_return_val_if_fail (MATE_IS_BG (bg), FALSE);

  return bg->draw_background;
}

MateBGPlacement
mate_bg_get_placement (MateBG *bg)
{
  g_return_val_if_fail (MATE_IS_BG (bg), MATE_BG_PLACEMENT_CENTERED);

  return bg->placement;
}

void
mate_bg_get_color (MateBG          *bg,
                   MateBGColorType *type,
                   GdkRGBA         *primary,
                   GdkRGBA         *secondary)
{
  g_return_if_fail (MATE_IS_BG (bg));

  if (type != NULL)
    *type = bg->color_type;
  if (primary != NULL)
    *primary = bg->primary;
  if (secondary != NULL)
    *secondary = bg->secondary;
}

const gchar *
mate_bg_get_filename (MateBG *bg)
{
  g_return_val_if_fail (MATE_IS_BG (bg), NULL);

  return bg->filename;
}

void
mate_bg_draw (MateBG    *bg,
              GdkPixbuf *dest,
              GdkScreen *screen,
              gboolean   is_root)
{
}

cairo_surface_t *
mate_bg_create_surface (MateBG    *bg,
                        GdkWindow *window,
                        int        width,
                        int        height,
                        gboolean   root)
{
  return cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                     MAX (width, 1),
                                     MAX (height, 1));
}

cairo_surface_t *
mate_bg_create_surface_scale (MateBG    *bg,
                              GdkWindow *window,
                              int        width,
                              int        height,
                              int        scale,
                              gboolean   root)
{
  return mate_bg_create_surface (bg, window, width, height, root);
}

gboolean
mate_bg_get_image_size (MateBG                       *bg,
                        MateDesktopThumbnailFactory *factory,
                        int                           best_width,
                        int                           best_height,
                        int                          *width,
                        int                          *height)
{
  if (width != NULL)
    *width = 0;
  if (height != NULL)
    *height = 0;

  return FALSE;
}

GdkPixbuf *
mate_bg_create_thumbnail (MateBG                       *bg,
                          MateDesktopThumbnailFactory *factory,
                          GdkScreen                    *screen,
                          int                           dest_width,
                          int                           dest_height)
{
  return NULL;
}

gboolean
mate_bg_is_dark (MateBG *bg,
                 int     dest_width,
                 int     dest_height)
{
  return FALSE;
}

gboolean
mate_bg_has_multiple_sizes (MateBG *bg)
{
  return FALSE;
}

gboolean
mate_bg_changes_with_time (MateBG *bg)
{
  return FALSE;
}

GdkPixbuf *
mate_bg_create_frame_thumbnail (MateBG                       *bg,
                                MateDesktopThumbnailFactory *factory,
                                GdkScreen                    *screen,
                                int                           dest_width,
                                int                           dest_height,
                                int                           frame_num)
{
  return NULL;
}

void
mate_bg_set_surface_as_root (GdkScreen       *screen,
                             cairo_surface_t *surface)
{
}

MateBGCrossfade *
mate_bg_set_surface_as_root_with_crossfade (GdkScreen       *screen,
                                            cairo_surface_t *surface)
{
  return NULL;
}

cairo_surface_t *
mate_bg_get_surface_from_root (GdkScreen *screen)
{
  return NULL;
}
