/*
 * LiMe Display Settings
 * Display configuration, resolution, and monitor management
 * Handles multi-monitor setups, resolution changes, scaling, and DPI
 */

#include "lime-display-settings.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define DISPLAY_CONFIG_DIR ".config/lime/display"
#define DISPLAY_CONFIG_FILE "display.conf"

typedef struct {
  GObject parent;

  GdkScreen *screen;
  GList *monitors;
  gint primary_monitor;

  GSettings *settings;

  /* Display configuration */
  gboolean mirror_mode;
  gboolean extend_mode;

  /* Color management */
  gboolean night_light_enabled;
  gint color_temperature;

  /* Cursor settings */
  LiMeCursorTheme cursor_theme;
  gint cursor_size;

  /* Configuration cache */
  GHashTable *monitor_configs;
} LiMeDisplaySettingsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeDisplaySettings, lime_display_settings, G_TYPE_OBJECT);

enum {
  SIGNAL_MONITOR_ADDED,
  SIGNAL_MONITOR_REMOVED,
  SIGNAL_MONITOR_CHANGED,
  SIGNAL_RESOLUTION_CHANGED,
  LAST_SIGNAL
};

static guint display_signals[LAST_SIGNAL] = { 0 };

static void
lime_display_settings_class_init(LiMeDisplaySettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  display_signals[SIGNAL_MONITOR_ADDED] =
    g_signal_new("monitor-added",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  display_signals[SIGNAL_MONITOR_REMOVED] =
    g_signal_new("monitor-removed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  display_signals[SIGNAL_MONITOR_CHANGED] =
    g_signal_new("monitor-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  display_signals[SIGNAL_RESOLUTION_CHANGED] =
    g_signal_new("resolution-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static void
lime_display_settings_init(LiMeDisplaySettings *settings)
{
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  priv->screen = gdk_screen_get_default();
  priv->monitors = NULL;
  priv->primary_monitor = 0;

  priv->mirror_mode = FALSE;
  priv->extend_mode = TRUE;

  priv->night_light_enabled = FALSE;
  priv->color_temperature = 6500;

  priv->cursor_theme = LIME_CURSOR_THEME_DEFAULT;
  priv->cursor_size = 24;

  priv->monitor_configs = g_hash_table_new(g_direct_hash, g_direct_equal);
  priv->settings = g_settings_new("org.cinnamon.desktop.peripherals.mouse");

  g_debug("Display settings initialized");
}

/**
 * lime_display_settings_new:
 *
 * Create a new display settings manager
 *
 * Returns: A new #LiMeDisplaySettings
 */
LiMeDisplaySettings *
lime_display_settings_new(void)
{
  return g_object_new(LIME_TYPE_DISPLAY_SETTINGS, NULL);
}

/**
 * lime_display_settings_get_monitors:
 * @settings: A #LiMeDisplaySettings
 *
 * Get list of connected monitors
 *
 * Returns: (transfer none): List of monitors
 */
GList *
lime_display_settings_get_monitors(LiMeDisplaySettings *settings)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), NULL);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  return priv->monitors;
}

/**
 * lime_display_settings_get_monitor:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 *
 * Get configuration for a specific monitor
 *
 * Returns: (transfer none): Monitor configuration
 */
LiMeMonitorConfig *
lime_display_settings_get_monitor(LiMeDisplaySettings *settings, gint monitor_id)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), NULL);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  return g_hash_table_lookup(priv->monitor_configs, GINT_TO_POINTER(monitor_id));
}

/**
 * lime_display_settings_apply_monitor_config:
 * @settings: A #LiMeDisplaySettings
 * @config: Monitor configuration
 *
 * Apply monitor configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_apply_monitor_config(LiMeDisplaySettings *settings,
                                           LiMeMonitorConfig *config)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  g_return_val_if_fail(config != NULL, FALSE);

  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  g_debug("Applying monitor config: %s (%dx%d@%dHz)",
          config->name, config->width, config->height, config->refresh_rate);

  g_hash_table_insert(priv->monitor_configs, GINT_TO_POINTER(config->monitor_id), config);
  g_signal_emit(settings, display_signals[SIGNAL_MONITOR_CHANGED], 0, config->monitor_id);

  return TRUE;
}

/**
 * lime_display_settings_set_primary_monitor:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 *
 * Set primary monitor
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_set_primary_monitor(LiMeDisplaySettings *settings, gint monitor_id)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  priv->primary_monitor = monitor_id;
  g_debug("Set primary monitor to %d", monitor_id);

  return TRUE;
}

/**
 * lime_display_settings_get_primary_monitor:
 * @settings: A #LiMeDisplaySettings
 *
 * Get primary monitor ID
 *
 * Returns: Primary monitor ID
 */
gint
lime_display_settings_get_primary_monitor(LiMeDisplaySettings *settings)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), 0);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  return priv->primary_monitor;
}

/**
 * lime_display_settings_set_resolution:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 * @width: Resolution width
 * @height: Resolution height
 *
 * Set monitor resolution
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_set_resolution(LiMeDisplaySettings *settings,
                                     gint monitor_id,
                                     gint width, gint height)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  LiMeMonitorConfig *config = g_hash_table_lookup(priv->monitor_configs,
                                                   GINT_TO_POINTER(monitor_id));
  if (!config) return FALSE;

  config->width = width;
  config->height = height;

  g_debug("Set resolution for monitor %d to %dx%d", monitor_id, width, height);
  g_signal_emit(settings, display_signals[SIGNAL_RESOLUTION_CHANGED], 0);

  return TRUE;
}

/**
 * lime_display_settings_set_refresh_rate:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 * @refresh_rate: Refresh rate in Hz
 *
 * Set monitor refresh rate
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_set_refresh_rate(LiMeDisplaySettings *settings,
                                       gint monitor_id,
                                       gint refresh_rate)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  LiMeMonitorConfig *config = g_hash_table_lookup(priv->monitor_configs,
                                                   GINT_TO_POINTER(monitor_id));
  if (!config) return FALSE;

  config->refresh_rate = refresh_rate;
  g_debug("Set refresh rate for monitor %d to %dHz", monitor_id, refresh_rate);

  return TRUE;
}

/**
 * lime_display_settings_get_available_resolutions:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 * @count: (out): Number of resolutions
 *
 * Get available resolutions for monitor
 *
 * Returns: (array length=count): Array of resolutions
 */
gint *
lime_display_settings_get_available_resolutions(LiMeDisplaySettings *settings,
                                                gint monitor_id,
                                                gint *count)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), NULL);

  /* Standard resolution list */
  static gint resolutions[] = {
    1024, 768,    /* 4:3 */
    1280, 720,    /* 16:9 */
    1280, 1024,   /* 5:4 */
    1366, 768,    /* 16:9 */
    1440, 900,    /* 16:10 */
    1600, 1200,   /* 4:3 */
    1680, 1050,   /* 16:10 */
    1920, 1080,   /* 16:9 */
    1920, 1200,   /* 16:10 */
    2560, 1440,   /* 16:9 */
    2560, 1600,   /* 16:10 */
    3840, 2160    /* 4K */
  };

  if (count) *count = sizeof(resolutions) / sizeof(resolutions[0]);
  return resolutions;
}

/**
 * lime_display_settings_set_scale_factor:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 * @scale: Scale factor
 *
 * Set DPI scaling for monitor
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_set_scale_factor(LiMeDisplaySettings *settings,
                                       gint monitor_id,
                                       LiMeScaleFactor scale)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  LiMeMonitorConfig *config = g_hash_table_lookup(priv->monitor_configs,
                                                   GINT_TO_POINTER(monitor_id));
  if (!config) return FALSE;

  const gchar *scale_names[] = { "100%", "125%", "150%", "175%", "200%" };
  config->scale = scale * 25 + 100;

  g_debug("Set scale factor for monitor %d to %s", monitor_id, scale_names[scale]);

  return TRUE;
}

/**
 * lime_display_settings_get_scale_factor:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 *
 * Get DPI scaling for monitor
 *
 * Returns: Scale factor as double
 */
gdouble
lime_display_settings_get_scale_factor(LiMeDisplaySettings *settings, gint monitor_id)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), 1.0);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  LiMeMonitorConfig *config = g_hash_table_lookup(priv->monitor_configs,
                                                   GINT_TO_POINTER(monitor_id));
  if (!config) return 1.0;

  return config->scale / 100.0;
}

/**
 * lime_display_settings_get_dpi:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 *
 * Get DPI for monitor
 *
 * Returns: DPI value
 */
gint
lime_display_settings_get_dpi(LiMeDisplaySettings *settings, gint monitor_id)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), 96);

  /* Standard DPI: 96 for standard, multiply by scale factor */
  return (gint)(96.0 * lime_display_settings_get_scale_factor(settings, monitor_id));
}

/**
 * lime_display_settings_set_rotation:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 * @rotation: Rotation angle
 *
 * Set monitor rotation
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_set_rotation(LiMeDisplaySettings *settings,
                                   gint monitor_id,
                                   LiMeRotation rotation)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  LiMeMonitorConfig *config = g_hash_table_lookup(priv->monitor_configs,
                                                   GINT_TO_POINTER(monitor_id));
  if (!config) return FALSE;

  const gchar *rotation_names[] = { "Normal", "90°", "180°", "270°" };
  config->rotation = rotation;

  g_debug("Set rotation for monitor %d to %s", monitor_id, rotation_names[rotation]);

  return TRUE;
}

/**
 * lime_display_settings_get_rotation:
 * @settings: A #LiMeDisplaySettings
 * @monitor_id: Monitor ID
 *
 * Get monitor rotation
 *
 * Returns: Rotation angle
 */
LiMeRotation
lime_display_settings_get_rotation(LiMeDisplaySettings *settings, gint monitor_id)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), LIME_ROTATION_NORMAL);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  LiMeMonitorConfig *config = g_hash_table_lookup(priv->monitor_configs,
                                                   GINT_TO_POINTER(monitor_id));
  if (!config) return LIME_ROTATION_NORMAL;

  return config->rotation;
}

/**
 * lime_display_settings_arrange_monitors:
 * @settings: A #LiMeDisplaySettings
 *
 * Arrange all monitors automatically
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_arrange_monitors(LiMeDisplaySettings *settings)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  g_debug("Arranging monitors automatically");

  gint offset_x = 0;
  GList *link;

  for (link = priv->monitors; link; link = link->next) {
    LiMeMonitorConfig *config = (LiMeMonitorConfig *)link->data;
    config->x_offset = offset_x;
    config->y_offset = 0;
    offset_x += config->width;
  }

  g_signal_emit(settings, display_signals[SIGNAL_RESOLUTION_CHANGED], 0);
  return TRUE;
}

/**
 * lime_display_settings_enable_mirror_mode:
 * @settings: A #LiMeDisplaySettings
 *
 * Enable mirror mode (clone displays)
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_enable_mirror_mode(LiMeDisplaySettings *settings)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  g_debug("Enabling mirror mode");
  priv->mirror_mode = TRUE;
  priv->extend_mode = FALSE;

  return TRUE;
}

/**
 * lime_display_settings_enable_extend_mode:
 * @settings: A #LiMeDisplaySettings
 *
 * Enable extend mode (multiple workspaces)
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_enable_extend_mode(LiMeDisplaySettings *settings)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  g_debug("Enabling extend mode");
  priv->mirror_mode = FALSE;
  priv->extend_mode = TRUE;

  return lime_display_settings_arrange_monitors(settings);
}

/**
 * lime_display_settings_set_cursor_theme:
 * @settings: A #LiMeDisplaySettings
 * @theme: Cursor theme
 *
 * Set cursor theme
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_set_cursor_theme(LiMeDisplaySettings *settings,
                                       LiMeCursorTheme theme)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  const gchar *theme_names[] = { "default", "light", "dark", "large" };
  priv->cursor_theme = theme;

  g_debug("Set cursor theme to %s", theme_names[theme]);
  return TRUE;
}

/**
 * lime_display_settings_set_cursor_size:
 * @settings: A #LiMeDisplaySettings
 * @size: Cursor size in pixels
 *
 * Set cursor size
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_set_cursor_size(LiMeDisplaySettings *settings, gint size)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  priv->cursor_size = CLAMP(size, 16, 64);
  g_debug("Set cursor size to %d", priv->cursor_size);

  return TRUE;
}

/**
 * lime_display_settings_save_config:
 * @settings: A #LiMeDisplaySettings
 *
 * Save display configuration to disk
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_save_config(LiMeDisplaySettings *settings)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  gchar *config_dir = g_build_filename(g_get_home_dir(), DISPLAY_CONFIG_DIR, NULL);
  g_mkdir_with_parents(config_dir, 0755);

  gchar *config_path = g_build_filename(config_dir, DISPLAY_CONFIG_FILE, NULL);

  GString *config_content = g_string_new("");
  g_string_append(config_content, "[Display]\n");
  g_string_append_printf(config_content, "primary_monitor=%d\n", priv->primary_monitor);
  g_string_append_printf(config_content, "mirror_mode=%d\n", priv->mirror_mode);
  g_string_append_printf(config_content, "extend_mode=%d\n", priv->extend_mode);
  g_string_append_printf(config_content, "night_light=%d\n", priv->night_light_enabled);
  g_string_append_printf(config_content, "color_temp=%d\n", priv->color_temperature);
  g_string_append_printf(config_content, "cursor_theme=%d\n", priv->cursor_theme);
  g_string_append_printf(config_content, "cursor_size=%d\n", priv->cursor_size);

  GError *error = NULL;
  g_file_set_contents(config_path, config_content->str, -1, &error);

  if (error) {
    g_warning("Failed to save display config: %s", error->message);
    g_error_free(error);
    g_free(config_dir);
    g_free(config_path);
    g_string_free(config_content, TRUE);
    return FALSE;
  }

  g_debug("Display configuration saved to %s", config_path);
  g_free(config_dir);
  g_free(config_path);
  g_string_free(config_content, TRUE);

  return TRUE;
}

/**
 * lime_display_settings_load_config:
 * @settings: A #LiMeDisplaySettings
 *
 * Load display configuration from disk
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_load_config(LiMeDisplaySettings *settings)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  gchar *config_dir = g_build_filename(g_get_home_dir(), DISPLAY_CONFIG_DIR, NULL);
  gchar *config_path = g_build_filename(config_dir, DISPLAY_CONFIG_FILE, NULL);

  GError *error = NULL;
  gchar *config_content = NULL;

  if (!g_file_get_contents(config_path, &config_content, NULL, &error)) {
    g_debug("No display config found");
    g_free(config_dir);
    g_free(config_path);
    if (error) g_error_free(error);
    return FALSE;
  }

  /* Parse basic config values */
  gchar **lines = g_strsplit(config_content, "\n", -1);
  for (gint i = 0; lines[i]; i++) {
    if (g_str_has_prefix(lines[i], "primary_monitor=")) {
      priv->primary_monitor = atoi(lines[i] + 16);
    } else if (g_str_has_prefix(lines[i], "night_light=")) {
      priv->night_light_enabled = atoi(lines[i] + 12);
    } else if (g_str_has_prefix(lines[i], "color_temp=")) {
      priv->color_temperature = atoi(lines[i] + 11);
    }
  }

  g_strfreev(lines);
  g_free(config_content);
  g_free(config_dir);
  g_free(config_path);

  g_debug("Display configuration loaded");
  return TRUE;
}

/**
 * lime_display_settings_enable_night_light:
 * @settings: A #LiMeDisplaySettings
 *
 * Enable night light mode
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_enable_night_light(LiMeDisplaySettings *settings)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  priv->night_light_enabled = TRUE;
  g_debug("Night light enabled");

  return TRUE;
}

/**
 * lime_display_settings_set_color_temperature:
 * @settings: A #LiMeDisplaySettings
 * @temperature: Color temperature in Kelvin
 *
 * Set color temperature for night light
 *
 * Returns: %TRUE on success
 */
gboolean
lime_display_settings_set_color_temperature(LiMeDisplaySettings *settings, gint temperature)
{
  g_return_val_if_fail(LIME_IS_DISPLAY_SETTINGS(settings), FALSE);
  LiMeDisplaySettingsPrivate *priv = lime_display_settings_get_instance_private(settings);

  priv->color_temperature = CLAMP(temperature, 2700, 6500);
  g_debug("Color temperature set to %dK", priv->color_temperature);

  return TRUE;
}
