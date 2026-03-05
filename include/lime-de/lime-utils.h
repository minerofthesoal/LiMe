/*
 * LiMe Utilities Header
 * Helper functions and utilities
 */

#ifndef __LIME_UTILS_H__
#define __LIME_UTILS_H__

#include <glib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

/* Color utilities */
gboolean lime_color_hex_to_rgb(const gchar *hex_color,
                                guint *r, guint *g, guint *b);
gchar * lime_color_rgb_to_hex(guint r, guint g, guint b);
gchar * lime_color_darken(const gchar *hex_color, gdouble amount);
gchar * lime_color_lighten(const gchar *hex_color, gdouble amount);

/* File utilities */
gchar * lime_get_config_dir(void);
gchar * lime_get_data_dir(void);
gboolean lime_ensure_dir(const gchar *path);

/* String utilities */
gboolean lime_string_array_contains(const gchar * const *array, const gchar *item);
gint lime_string_array_find(const gchar * const *array, const gchar *item);

/* Window utilities */
gint lime_window_get_monitor(gint x, gint y);

/* Geometry utilities */
gboolean lime_rect_contains_point(gint x, gint y, gint width, gint height,
                                   gint px, gint py);
gboolean lime_rect_intersects(gint x1, gint y1, gint w1, gint h1,
                               gint x2, gint y2, gint w2, gint h2);

/* Time utilities */
gchar * lime_timestamp_get_human_readable(gint64 timestamp);

/* Process utilities */
gboolean lime_spawn_async(const gchar *command);

G_END_DECLS

#endif /* __LIME_UTILS_H__ */
