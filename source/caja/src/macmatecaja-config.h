#ifndef MACMATECAJA_CONFIG_H
#define MACMATECAJA_CONFIG_H

#include <glib.h>

/* Startup-only configuration. All getters return independently owned data. */
typedef struct {
    gchar *label;
    gchar *uri;
    gchar *path; /* NULL for remote locations */
} MacMateCajaPlace;

void macmatecaja_config_init (void);
gchar *macmatecaja_config_get_left_path (void);
gchar *macmatecaja_config_get_right_path (void);
GList *macmatecaja_config_get_places (void);
void macmatecaja_config_free_places (GList *places);

#endif
