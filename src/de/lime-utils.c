/*
 * LiMe Utilities and Helpers
 * Various utility functions and helpers for the desktop environment
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* Color utilities */

/**
 * lime_color_hex_to_rgb:
 * @hex_color: Hex color string (#RRGGBB)
 * @r: (out): Red component (0-255)
 * @g: (out): Green component (0-255)
 * @b: (out): Blue component (0-255)
 *
 * Convert hex color to RGB components
 *
 * Returns: %TRUE if successful
 */
gboolean
lime_color_hex_to_rgb(const gchar *hex_color,
                      guint *r, guint *g, guint *b)
{
  g_return_val_if_fail(hex_color != NULL, FALSE);

  if (hex_color[0] != '#' || strlen(hex_color) != 7) {
    return FALSE;
  }

  gchar *endptr = NULL;
  gulong value = g_ascii_strtoll(hex_color + 1, &endptr, 16);

  if (endptr != hex_color + 7) {
    return FALSE;
  }

  if (r) *r = (value >> 16) & 0xFF;
  if (g) *g = (value >> 8) & 0xFF;
  if (b) *b = value & 0xFF;

  return TRUE;
}

/**
 * lime_color_rgb_to_hex:
 * @r: Red component (0-255)
 * @g: Green component (0-255)
 * @b: Blue component (0-255)
 *
 * Convert RGB components to hex color string
 *
 * Returns: (transfer full): Hex color string
 */
gchar *
lime_color_rgb_to_hex(guint r, guint g, guint b)
{
  r = CLAMP(r, 0, 255);
  g = CLAMP(g, 0, 255);
  b = CLAMP(b, 0, 255);

  return g_strdup_printf("#%02X%02X%02X", r, g, b);
}

/**
 * lime_color_darken:
 * @hex_color: Hex color string
 * @amount: Darkening amount (0.0 - 1.0)
 *
 * Darken a color
 *
 * Returns: (transfer full): Darkened color
 */
gchar *
lime_color_darken(const gchar *hex_color, gdouble amount)
{
  guint r, g, b;

  if (!lime_color_hex_to_rgb(hex_color, &r, &g, &b)) {
    return g_strdup(hex_color);
  }

  amount = CLAMP(amount, 0.0, 1.0);

  r = (guint)(r * (1.0 - amount));
  g = (guint)(g * (1.0 - amount));
  b = (guint)(b * (1.0 - amount));

  return lime_color_rgb_to_hex(r, g, b);
}

/**
 * lime_color_lighten:
 * @hex_color: Hex color string
 * @amount: Lightening amount (0.0 - 1.0)
 *
 * Lighten a color
 *
 * Returns: (transfer full): Lightened color
 */
gchar *
lime_color_lighten(const gchar *hex_color, gdouble amount)
{
  guint r, g, b;

  if (!lime_color_hex_to_rgb(hex_color, &r, &g, &b)) {
    return g_strdup(hex_color);
  }

  amount = CLAMP(amount, 0.0, 1.0);

  r = (guint)CLAMP(r + (255 - r) * amount, 0, 255);
  g = (guint)CLAMP(g + (255 - g) * amount, 0, 255);
  b = (guint)CLAMP(b + (255 - b) * amount, 0, 255);

  return lime_color_rgb_to_hex(r, g, b);
}

/* File utilities */

/**
 * lime_get_config_dir:
 *
 * Get the LiMe configuration directory
 *
 * Returns: (transfer full): Config directory path
 */
gchar *
lime_get_config_dir(void)
{
  return g_build_filename(g_get_home_dir(), ".config/lime", NULL);
}

/**
 * lime_get_data_dir:
 *
 * Get the LiMe data directory
 *
 * Returns: (transfer full): Data directory path
 */
gchar *
lime_get_data_dir(void)
{
  return g_build_filename(g_get_home_dir(), ".local/share/lime", NULL);
}

/**
 * lime_ensure_dir:
 * @path: Directory path
 *
 * Ensure a directory exists, creating it if necessary
 *
 * Returns: %TRUE if successful
 */
gboolean
lime_ensure_dir(const gchar *path)
{
  g_return_val_if_fail(path != NULL, FALSE);

  return g_mkdir_with_parents(path, 0755) == 0;
}

/* String utilities */

/**
 * lime_string_array_contains:
 * @array: String array
 * @item: Item to find
 *
 * Check if a string array contains an item
 *
 * Returns: %TRUE if found
 */
gboolean
lime_string_array_contains(const gchar * const *array, const gchar *item)
{
  if (!array) return FALSE;

  for (int i = 0; array[i]; i++) {
    if (g_strcmp0(array[i], item) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * lime_string_array_find:
 * @array: String array
 * @item: Item to find
 *
 * Find index of item in string array
 *
 * Returns: Index or -1 if not found
 */
gint
lime_string_array_find(const gchar * const *array, const gchar *item)
{
  if (!array) return -1;

  for (int i = 0; array[i]; i++) {
    if (g_strcmp0(array[i], item) == 0) {
      return i;
    }
  }

  return -1;
}

/* Window utilities */

/**
 * lime_window_get_monitor:
 * @x: Window X coordinate
 * @y: Window Y coordinate
 *
 * Get monitor index containing the coordinates
 *
 * Returns: Monitor index
 */
gint
lime_window_get_monitor(gint x, gint y)
{
  GdkScreen *screen = gdk_screen_get_default();
  gint n_monitors = gdk_screen_get_n_monitors(screen);

  for (int i = 0; i < n_monitors; i++) {
    GdkRectangle geom;
    gdk_screen_get_monitor_geometry(screen, i, &geom);

    if (x >= geom.x && x < geom.x + geom.width &&
        y >= geom.y && y < geom.y + geom.height) {
      return i;
    }
  }

  return 0;
}

/* Geometry utilities */

/**
 * lime_rect_contains_point:
 * @x: Rectangle x
 * @y: Rectangle y
 * @width: Rectangle width
 * @height: Rectangle height
 * @px: Point x
 * @py: Point y
 *
 * Check if a point is within a rectangle
 *
 * Returns: %TRUE if contained
 */
gboolean
lime_rect_contains_point(gint x, gint y, gint width, gint height,
                         gint px, gint py)
{
  return (px >= x && px < x + width &&
          py >= y && py < y + height);
}

/**
 * lime_rect_intersects:
 * @x1, @y1, @w1, @h1: First rectangle
 * @x2, @y2, @w2, @h2: Second rectangle
 *
 * Check if two rectangles intersect
 *
 * Returns: %TRUE if intersecting
 */
gboolean
lime_rect_intersects(gint x1, gint y1, gint w1, gint h1,
                     gint x2, gint y2, gint w2, gint h2)
{
  return (x1 < x2 + w2 && x1 + w1 > x2 &&
          y1 < y2 + h2 && y1 + h1 > y2);
}

/* Time utilities */

/**
 * lime_timestamp_get_human_readable:
 * @timestamp: Unix timestamp
 *
 * Convert timestamp to human-readable string
 *
 * Returns: (transfer full): Formatted time string
 */
gchar *
lime_timestamp_get_human_readable(gint64 timestamp)
{
  GDateTime *dt = g_date_time_new_from_unix_local(timestamp);
  if (!dt) return g_strdup("Unknown");

  gchar *result = g_strdup(g_date_time_format(dt, "%Y-%m-%d %H:%M:%S"));
  g_date_time_unref(dt);

  return result;
}

/* Process utilities */

/**
 * lime_spawn_async:
 * @command: Command to execute
 *
 * Spawn a process asynchronously
 *
 * Returns: %TRUE on success
 */
gboolean
lime_spawn_async(const gchar *command)
{
  g_return_val_if_fail(command != NULL, FALSE);

  return g_spawn_command_line_async(command, NULL);
}
