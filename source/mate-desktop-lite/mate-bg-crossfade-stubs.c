#include "config.h"

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-bg-crossfade.h>

struct _MateBGCrossfadePrivate
{
  gboolean started;
};

enum
{
  FINISHED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MateBGCrossfade, mate_bg_crossfade, G_TYPE_OBJECT)

static void
mate_bg_crossfade_class_init (MateBGCrossfadeClass *klass)
{
  signals[FINISHED] = g_signal_new ("finished",
                                    G_OBJECT_CLASS_TYPE (klass),
                                    G_SIGNAL_RUN_LAST,
                                    0,
                                    NULL, NULL,
                                    g_cclosure_marshal_VOID__OBJECT,
                                    G_TYPE_NONE, 1, G_TYPE_OBJECT);
}

static void
mate_bg_crossfade_init (MateBGCrossfade *fade)
{
  fade->priv = mate_bg_crossfade_get_instance_private (fade);
}

MateBGCrossfade *
mate_bg_crossfade_new (int width,
                       int height)
{
  return g_object_new (MATE_TYPE_BG_CROSSFADE, NULL);
}

gboolean
mate_bg_crossfade_set_start_surface (MateBGCrossfade *fade,
                                     cairo_surface_t *surface)
{
  return TRUE;
}

gboolean
mate_bg_crossfade_set_end_surface (MateBGCrossfade *fade,
                                   cairo_surface_t *surface)
{
  return TRUE;
}

void
mate_bg_crossfade_start (MateBGCrossfade *fade,
                         GdkWindow       *window)
{
  g_return_if_fail (MATE_IS_BG_CROSSFADE (fade));

  fade->priv->started = TRUE;
}

void
mate_bg_crossfade_start_widget (MateBGCrossfade *fade,
                                GtkWidget       *widget)
{
  g_return_if_fail (MATE_IS_BG_CROSSFADE (fade));

  fade->priv->started = TRUE;
}

gboolean
mate_bg_crossfade_is_started (MateBGCrossfade *fade)
{
  g_return_val_if_fail (MATE_IS_BG_CROSSFADE (fade), FALSE);

  return fade->priv->started;
}

void
mate_bg_crossfade_stop (MateBGCrossfade *fade)
{
  g_return_if_fail (MATE_IS_BG_CROSSFADE (fade));

  fade->priv->started = FALSE;
}
