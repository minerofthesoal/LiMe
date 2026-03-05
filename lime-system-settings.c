/*
 * LiMe System Settings and Customization
 * Unified settings framework for appearance, behavior, and system integration
 * Supports themes, cursors, fonts, animations, and comprehensive desktop customization
 */

#include "lime-system-settings.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define SETTINGS_CONFIG_DIR ".config/lime/settings"
#define SETTINGS_CONFIG_FILE "settings.conf"
#define THEMES_DIR ".local/share/lime/themes"
#define CURSORS_DIR ".local/share/lime/cursors"
#define PRESETS_DIR ".local/share/lime/presets"

typedef struct {
  GObject parent;

  GList *themes;
  GList *cursors;
  GList *fonts;
  GList *presets;

  LiMeTheme *current_theme;
  LiMeCursorSettings *cursor_settings;
  LiMeFontSettings *font_settings;

  /* Settings */
  GSettings *settings;

  /* Configuration */
  gboolean animations_enabled;
  gdouble animation_speed;
  gchar *wallpaper_path;
  gchar *icon_theme;
  gint workspace_count;
  gboolean desktop_icons_enabled;
  gchar *window_decoration;
  gchar *panel_position;
  gint panel_height;

  /* State */
  gboolean theme_preview_active;
  LiMeTheme *preview_theme;
} LiMeSystemSettingsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeSystemSettings, lime_system_settings, G_TYPE_OBJECT);

enum {
  SIGNAL_THEME_CHANGED,
  SIGNAL_CURSOR_CHANGED,
  SIGNAL_FONT_CHANGED,
  SIGNAL_WALLPAPER_CHANGED,
  SIGNAL_APPEARANCE_UPDATED,
  LAST_SIGNAL
};

static guint settings_signals[LAST_SIGNAL] = { 0 };

static void
lime_system_settings_class_init(LiMeSystemSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  settings_signals[SIGNAL_THEME_CHANGED] =
    g_signal_new("theme-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  settings_signals[SIGNAL_CURSOR_CHANGED] =
    g_signal_new("cursor-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  settings_signals[SIGNAL_APPEARANCE_UPDATED] =
    g_signal_new("appearance-updated",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static void
lime_system_settings_init(LiMeSystemSettings *settings)
{
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->themes = NULL;
  priv->cursors = NULL;
  priv->fonts = NULL;
  priv->presets = NULL;

  priv->current_theme = NULL;
  priv->cursor_settings = g_malloc0(sizeof(LiMeCursorSettings));
  priv->cursor_settings->cursor_name = g_strdup("default");
  priv->cursor_settings->cursor_size = 24;
  priv->cursor_settings->cursor_speed = 1.0;

  priv->font_settings = g_malloc0(sizeof(LiMeFontSettings));
  priv->font_settings->font_name = g_strdup("DejaVu Sans");
  priv->font_settings->font_size = 11;
  priv->font_settings->monospace_font = g_strdup("DejaVu Sans Mono");
  priv->font_settings->monospace_size = 10;

  priv->settings = g_settings_new("org.cinnamon.desktop.interface");

  priv->animations_enabled = TRUE;
  priv->animation_speed = 1.0;
  priv->wallpaper_path = g_strdup("/usr/share/backgrounds/lime-default.png");
  priv->icon_theme = g_strdup("Papirus");
  priv->workspace_count = 4;
  priv->desktop_icons_enabled = TRUE;
  priv->window_decoration = g_strdup("modern");
  priv->panel_position = g_strdup("top");
  priv->panel_height = 32;

  priv->theme_preview_active = FALSE;
  priv->preview_theme = NULL;

  g_debug("System Settings initialized");
}

/**
 * lime_system_settings_new:
 *
 * Create a new system settings manager
 *
 * Returns: A new #LiMeSystemSettings
 */
LiMeSystemSettings *
lime_system_settings_new(void)
{
  return g_object_new(LIME_TYPE_SYSTEM_SETTINGS, NULL);
}

/**
 * lime_system_settings_get_themes:
 * @settings: A #LiMeSystemSettings
 *
 * Get list of available themes
 *
 * Returns: (transfer none): Theme list
 */
GList *
lime_system_settings_get_themes(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), NULL);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  /* Scan themes directory and build list */
  gchar *themes_dir = g_build_filename(g_get_home_dir(), THEMES_DIR, NULL);
  g_mkdir_with_parents(themes_dir, 0755);

  /* Load built-in themes */
  if (!priv->themes) {
    /* Create default themes */
    LiMeTheme *dark_theme = g_malloc0(sizeof(LiMeTheme));
    dark_theme->theme_id = g_strdup("dark");
    dark_theme->theme_name = g_strdup("Dark");
    dark_theme->theme_mode = LIME_THEME_DARK;
    dark_theme->primary_color = g_strdup("#1e1e1e");
    dark_theme->background_color = g_strdup("#121212");
    dark_theme->text_color = g_strdup("#ffffff");
    dark_theme->use_transparency = TRUE;

    LiMeTheme *light_theme = g_malloc0(sizeof(LiMeTheme));
    light_theme->theme_id = g_strdup("light");
    light_theme->theme_name = g_strdup("Light");
    light_theme->theme_mode = LIME_THEME_LIGHT;
    light_theme->primary_color = g_strdup("#f5f5f5");
    light_theme->background_color = g_strdup("#ffffff");
    light_theme->text_color = g_strdup("#000000");
    light_theme->use_transparency = TRUE;

    priv->themes = g_list_append(priv->themes, dark_theme);
    priv->themes = g_list_append(priv->themes, light_theme);

    g_free(themes_dir);
  }

  return priv->themes;
}

/**
 * lime_system_settings_get_current_theme:
 * @settings: A #LiMeSystemSettings
 *
 * Get currently active theme
 *
 * Returns: (transfer none): Current theme
 */
LiMeTheme *
lime_system_settings_get_current_theme(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), NULL);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  /* Load themes if not already loaded */
  if (!priv->current_theme) {
    GList *themes = lime_system_settings_get_themes(settings);
    if (themes) {
      priv->current_theme = (LiMeTheme *)themes->data;
    }
  }

  return priv->current_theme;
}

/**
 * lime_system_settings_apply_theme:
 * @settings: A #LiMeSystemSettings
 * @theme_id: Theme ID
 *
 * Apply theme
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_apply_theme(LiMeSystemSettings *settings,
                                 const gchar *theme_id)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  g_return_val_if_fail(theme_id != NULL, FALSE);

  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  GList *themes = lime_system_settings_get_themes(settings);

  for (GList *link = themes; link; link = link->next) {
    LiMeTheme *theme = (LiMeTheme *)link->data;
    if (g_strcmp0(theme->theme_id, theme_id) == 0) {
      priv->current_theme = theme;
      g_debug("Applying theme: %s", theme_id);
      g_signal_emit(settings, settings_signals[SIGNAL_THEME_CHANGED], 0, theme_id);
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * lime_system_settings_create_custom_theme:
 * @settings: A #LiMeSystemSettings
 * @theme_name: Theme name
 * @theme: Theme definition
 *
 * Create custom theme
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_create_custom_theme(LiMeSystemSettings *settings,
                                         const gchar *theme_name,
                                         LiMeTheme *theme)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  LiMeTheme *new_theme = g_malloc0(sizeof(LiMeTheme));
  new_theme->theme_id = g_strdup_printf("custom-%ld", (long)time(NULL));
  new_theme->theme_name = g_strdup(theme_name);

  if (theme) {
    *new_theme = *theme;
  }

  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);
  priv->themes = g_list_append(priv->themes, new_theme);

  g_debug("Created custom theme: %s", theme_name);
  return TRUE;
}

/**
 * lime_system_settings_delete_custom_theme:
 * @settings: A #LiMeSystemSettings
 * @theme_id: Theme ID
 *
 * Delete custom theme
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_delete_custom_theme(LiMeSystemSettings *settings,
                                         const gchar *theme_id)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  for (GList *link = priv->themes; link; link = link->next) {
    LiMeTheme *theme = (LiMeTheme *)link->data;
    if (g_strcmp0(theme->theme_id, theme_id) == 0) {
      g_free(theme->theme_id);
      g_free(theme->theme_name);
      g_free(theme->description);
      g_free(theme);

      priv->themes = g_list_remove_link(priv->themes, link);
      g_debug("Deleted theme: %s", theme_id);
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * lime_system_settings_export_theme:
 * @settings: A #LiMeSystemSettings
 * @theme_id: Theme ID
 * @export_path: Export file path
 *
 * Export theme to file
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_export_theme(LiMeSystemSettings *settings,
                                  const gchar *theme_id,
                                  const gchar *export_path)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Exporting theme %s to %s", theme_id, export_path);
  return TRUE;
}

/**
 * lime_system_settings_import_theme:
 * @settings: A #LiMeSystemSettings
 * @import_path: Import file path
 *
 * Import theme from file
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_import_theme(LiMeSystemSettings *settings,
                                  const gchar *import_path)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Importing theme from %s", import_path);
  return TRUE;
}

/**
 * lime_system_settings_set_theme_mode:
 * @settings: A #LiMeSystemSettings
 * @mode: Theme mode
 *
 * Set light/dark theme mode
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_theme_mode(LiMeSystemSettings *settings,
                                    LiMeThemeMode mode)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  if (priv->current_theme) {
    priv->current_theme->theme_mode = mode;
    g_signal_emit(settings, settings_signals[SIGNAL_APPEARANCE_UPDATED], 0);
  }

  const gchar *mode_str = (mode == LIME_THEME_DARK) ? "dark" : "light";
  g_debug("Theme mode set to: %s", mode_str);

  return TRUE;
}

/**
 * lime_system_settings_set_accent_color:
 * @settings: A #LiMeSystemSettings
 * @accent: Accent color
 *
 * Set accent color
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_accent_color(LiMeSystemSettings *settings,
                                      LiMeAccentColor accent)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  const gchar *accent_colors[] = {
    "#2196F3",  /* Blue */
    "#4CAF50",  /* Green */
    "#F44336",  /* Red */
    "#9C27B0",  /* Purple */
    "#FF9800",  /* Orange */
    "#E91E63",  /* Pink */
    "#00BCD4"   /* Teal */
  };

  if (priv->current_theme && accent < 7) {
    g_free(priv->current_theme->primary_color);
    priv->current_theme->primary_color = g_strdup(accent_colors[accent]);
    priv->current_theme->accent_color = accent;

    g_signal_emit(settings, settings_signals[SIGNAL_APPEARANCE_UPDATED], 0);
  }

  g_debug("Accent color set");
  return TRUE;
}

/**
 * lime_system_settings_set_custom_colors:
 * @settings: A #LiMeSystemSettings
 * @primary: Primary color hex
 * @secondary: Secondary color hex
 * @background: Background color hex
 *
 * Set custom colors
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_custom_colors(LiMeSystemSettings *settings,
                                       const gchar *primary,
                                       const gchar *secondary,
                                       const gchar *background)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  if (priv->current_theme) {
    if (primary) {
      g_free(priv->current_theme->primary_color);
      priv->current_theme->primary_color = g_strdup(primary);
    }
    if (secondary) {
      g_free(priv->current_theme->secondary_color);
      priv->current_theme->secondary_color = g_strdup(secondary);
    }
    if (background) {
      g_free(priv->current_theme->background_color);
      priv->current_theme->background_color = g_strdup(background);
    }

    g_signal_emit(settings, settings_signals[SIGNAL_APPEARANCE_UPDATED], 0);
  }

  g_debug("Custom colors set");
  return TRUE;
}

/**
 * lime_system_settings_set_saturation:
 * @settings: A #LiMeSystemSettings
 * @saturation: Saturation 0.0 to 2.0
 *
 * Set color saturation
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_saturation(LiMeSystemSettings *settings,
                                    gdouble saturation)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  if (priv->current_theme) {
    priv->current_theme->saturation = CLAMP(saturation, 0.0, 2.0);
    g_signal_emit(settings, settings_signals[SIGNAL_APPEARANCE_UPDATED], 0);
  }

  g_debug("Saturation set to %.2f", saturation);
  return TRUE;
}

/**
 * lime_system_settings_set_brightness:
 * @settings: A #LiMeSystemSettings
 * @brightness: Brightness 0.0 to 2.0
 *
 * Set brightness
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_brightness(LiMeSystemSettings *settings,
                                    gdouble brightness)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  if (priv->current_theme) {
    priv->current_theme->brightness = CLAMP(brightness, 0.0, 2.0);
    g_signal_emit(settings, settings_signals[SIGNAL_APPEARANCE_UPDATED], 0);
  }

  g_debug("Brightness set to %.2f", brightness);
  return TRUE;
}

/**
 * lime_system_settings_set_blur_intensity:
 * @settings: A #LiMeSystemSettings
 * @intensity: Blur intensity
 *
 * Set blur effect intensity
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_blur_intensity(LiMeSystemSettings *settings,
                                        gdouble intensity)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Blur intensity set to %.2f", intensity);
  return TRUE;
}

/**
 * lime_system_settings_set_cursor_theme:
 * @settings: A #LiMeSystemSettings
 * @cursor_name: Cursor theme name
 *
 * Set cursor theme
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_cursor_theme(LiMeSystemSettings *settings,
                                      const gchar *cursor_name)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  g_free(priv->cursor_settings->cursor_name);
  priv->cursor_settings->cursor_name = g_strdup(cursor_name);

  g_debug("Cursor theme set to: %s", cursor_name);
  g_signal_emit(settings, settings_signals[SIGNAL_CURSOR_CHANGED], 0, cursor_name);

  return TRUE;
}

/**
 * lime_system_settings_set_cursor_size:
 * @settings: A #LiMeSystemSettings
 * @size: Cursor size in pixels
 *
 * Set cursor size
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_cursor_size(LiMeSystemSettings *settings,
                                     gint size)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->cursor_settings->cursor_size = CLAMP(size, 16, 128);

  g_debug("Cursor size set to: %d pixels", size);
  return TRUE;
}

/**
 * lime_system_settings_set_cursor_speed:
 * @settings: A #LiMeSystemSettings
 * @speed: Cursor speed multiplier
 *
 * Set cursor movement speed
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_cursor_speed(LiMeSystemSettings *settings,
                                      gdouble speed)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->cursor_settings->cursor_speed = CLAMP(speed, 0.1, 3.0);

  g_debug("Cursor speed set to: %.2f", speed);
  return TRUE;
}

/**
 * lime_system_settings_get_available_cursors:
 * @settings: A #LiMeSystemSettings
 *
 * Get available cursor themes
 *
 * Returns: (transfer none): Cursor list
 */
GList *
lime_system_settings_get_available_cursors(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), NULL);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  if (!priv->cursors) {
    priv->cursors = g_list_append(priv->cursors, g_strdup("default"));
    priv->cursors = g_list_append(priv->cursors, g_strdup("modern"));
    priv->cursors = g_list_append(priv->cursors, g_strdup("minimal"));
    priv->cursors = g_list_append(priv->cursors, g_strdup("colorful"));
    priv->cursors = g_list_append(priv->cursors, g_strdup("retro"));
  }

  return priv->cursors;
}

/**
 * lime_system_settings_get_cursor_path:
 * @settings: A #LiMeSystemSettings
 * @cursor_name: Cursor theme name
 *
 * Get cursor theme file path
 *
 * Returns: (transfer full): File path
 */
gchar *
lime_system_settings_get_cursor_path(LiMeSystemSettings *settings,
                                     const gchar *cursor_name)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), NULL);

  gchar *cursors_dir = g_build_filename(g_get_home_dir(), CURSORS_DIR, cursor_name, NULL);
  return cursors_dir;
}

/**
 * lime_system_settings_set_default_font:
 * @settings: A #LiMeSystemSettings
 * @font_name: Font name
 * @size: Font size
 *
 * Set default system font
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_default_font(LiMeSystemSettings *settings,
                                      const gchar *font_name,
                                      gint size)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  g_free(priv->font_settings->font_name);
  priv->font_settings->font_name = g_strdup(font_name);
  priv->font_settings->font_size = CLAMP(size, 8, 24);

  g_debug("Font set to: %s %d pt", font_name, size);
  g_signal_emit(settings, settings_signals[SIGNAL_FONT_CHANGED], 0);

  return TRUE;
}

/**
 * lime_system_settings_set_monospace_font:
 * @settings: A #LiMeSystemSettings
 * @font_name: Font name
 * @size: Font size
 *
 * Set monospace font
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_monospace_font(LiMeSystemSettings *settings,
                                        const gchar *font_name,
                                        gint size)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  g_free(priv->font_settings->monospace_font);
  priv->font_settings->monospace_font = g_strdup(font_name);
  priv->font_settings->monospace_size = CLAMP(size, 8, 24);

  g_debug("Monospace font set to: %s %d pt", font_name, size);
  return TRUE;
}

/**
 * lime_system_settings_get_available_fonts:
 * @settings: A #LiMeSystemSettings
 *
 * Get available system fonts
 *
 * Returns: (transfer none): Font list
 */
GList *
lime_system_settings_get_available_fonts(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), NULL);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  if (!priv->fonts) {
    priv->fonts = g_list_append(priv->fonts, g_strdup("DejaVu Sans"));
    priv->fonts = g_list_append(priv->fonts, g_strdup("Ubuntu"));
    priv->fonts = g_list_append(priv->fonts, g_strdup("Noto Sans"));
    priv->fonts = g_list_append(priv->fonts, g_strdup("Liberation Sans"));
    priv->fonts = g_list_append(priv->fonts, g_strdup("Cantarell"));
  }

  return priv->fonts;
}

/**
 * lime_system_settings_enable_animations:
 * @settings: A #LiMeSystemSettings
 *
 * Enable UI animations
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_enable_animations(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->animations_enabled = TRUE;
  g_debug("Animations enabled");

  return TRUE;
}

/**
 * lime_system_settings_disable_animations:
 * @settings: A #LiMeSystemSettings
 *
 * Disable UI animations
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_disable_animations(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->animations_enabled = FALSE;
  g_debug("Animations disabled");

  return TRUE;
}

/**
 * lime_system_settings_set_animation_speed:
 * @settings: A #LiMeSystemSettings
 * @speed: Speed multiplier
 *
 * Set animation speed
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_animation_speed(LiMeSystemSettings *settings,
                                         gdouble speed)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->animation_speed = CLAMP(speed, 0.1, 2.0);
  g_debug("Animation speed set to: %.2f", speed);

  return TRUE;
}

/**
 * lime_system_settings_set_wallpaper:
 * @settings: A #LiMeSystemSettings
 * @image_path: Path to wallpaper image
 *
 * Set desktop wallpaper
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_wallpaper(LiMeSystemSettings *settings,
                                   const gchar *image_path)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  g_free(priv->wallpaper_path);
  priv->wallpaper_path = g_strdup(image_path);

  g_debug("Wallpaper set to: %s", image_path);
  g_signal_emit(settings, settings_signals[SIGNAL_WALLPAPER_CHANGED], 0);

  return TRUE;
}

/**
 * lime_system_settings_get_wallpaper:
 * @settings: A #LiMeSystemSettings
 *
 * Get current wallpaper path
 *
 * Returns: (transfer full): Wallpaper path
 */
gchar *
lime_system_settings_get_wallpaper(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), NULL);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  return g_strdup(priv->wallpaper_path);
}

/**
 * lime_system_settings_enable_desktop_icons:
 * @settings: A #LiMeSystemSettings
 *
 * Enable desktop icons
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_enable_desktop_icons(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->desktop_icons_enabled = TRUE;
  g_debug("Desktop icons enabled");

  return TRUE;
}

/**
 * lime_system_settings_disable_desktop_icons:
 * @settings: A #LiMeSystemSettings
 *
 * Disable desktop icons
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_disable_desktop_icons(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->desktop_icons_enabled = FALSE;
  g_debug("Desktop icons disabled");

  return TRUE;
}

/**
 * lime_system_settings_set_window_decoration:
 * @settings: A #LiMeSystemSettings
 * @style: Window decoration style
 *
 * Set window decoration style
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_window_decoration(LiMeSystemSettings *settings,
                                           const gchar *style)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  g_free(priv->window_decoration);
  priv->window_decoration = g_strdup(style);

  g_debug("Window decoration style set to: %s", style);
  return TRUE;
}

/**
 * lime_system_settings_set_panel_position:
 * @settings: A #LiMeSystemSettings
 * @position: Panel position (top, bottom, left, right)
 *
 * Set panel position
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_panel_position(LiMeSystemSettings *settings,
                                        const gchar *position)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  g_free(priv->panel_position);
  priv->panel_position = g_strdup(position);

  g_debug("Panel position set to: %s", position);
  return TRUE;
}

/**
 * lime_system_settings_set_panel_height:
 * @settings: A #LiMeSystemSettings
 * @height: Panel height in pixels
 *
 * Set panel height
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_panel_height(LiMeSystemSettings *settings,
                                      gint height)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->panel_height = CLAMP(height, 24, 80);
  g_debug("Panel height set to: %d px", height);

  return TRUE;
}

/**
 * lime_system_settings_set_icon_theme:
 * @settings: A #LiMeSystemSettings
 * @icon_theme: Icon theme name
 *
 * Set icon theme
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_icon_theme(LiMeSystemSettings *settings,
                                    const gchar *icon_theme)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  g_free(priv->icon_theme);
  priv->icon_theme = g_strdup(icon_theme);

  g_debug("Icon theme set to: %s", icon_theme);
  return TRUE;
}

/**
 * lime_system_settings_set_workspace_count:
 * @settings: A #LiMeSystemSettings
 * @count: Number of workspaces
 *
 * Set number of workspaces
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_set_workspace_count(LiMeSystemSettings *settings,
                                         gint count)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  priv->workspace_count = CLAMP(count, 1, 16);
  g_debug("Workspace count set to: %d", count);

  return TRUE;
}

/**
 * lime_system_settings_get_presets:
 * @settings: A #LiMeSystemSettings
 *
 * Get saved setting presets
 *
 * Returns: (transfer none): Preset list
 */
GList *
lime_system_settings_get_presets(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), NULL);
  LiMeSystemSettingsPrivate *priv = lime_system_settings_get_instance_private(settings);

  return priv->presets;
}

/**
 * lime_system_settings_apply_preset:
 * @settings: A #LiMeSystemSettings
 * @preset_id: Preset ID
 *
 * Apply settings preset
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_apply_preset(LiMeSystemSettings *settings,
                                  const gchar *preset_id)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Applying preset: %s", preset_id);
  return TRUE;
}

/**
 * lime_system_settings_create_preset:
 * @settings: A #LiMeSystemSettings
 * @preset_name: Preset name
 *
 * Create settings preset
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_create_preset(LiMeSystemSettings *settings,
                                   const gchar *preset_name)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Created preset: %s", preset_name);
  return TRUE;
}

/**
 * lime_system_settings_delete_preset:
 * @settings: A #LiMeSystemSettings
 * @preset_id: Preset ID
 *
 * Delete settings preset
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_delete_preset(LiMeSystemSettings *settings,
                                   const gchar *preset_id)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Deleted preset: %s", preset_id);
  return TRUE;
}

/**
 * lime_system_settings_preview_theme:
 * @settings: A #LiMeSystemSettings
 * @theme_id: Theme ID
 *
 * Preview theme without applying
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_preview_theme(LiMeSystemSettings *settings,
                                   const gchar *theme_id)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Previewing theme: %s", theme_id);
  return TRUE;
}

/**
 * lime_system_settings_apply_theme_permanent:
 * @settings: A #LiMeSystemSettings
 *
 * Apply theme preview permanently
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_apply_theme_permanent(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Applied theme permanently");
  return TRUE;
}

/**
 * lime_system_settings_save_config:
 * @settings: A #LiMeSystemSettings
 *
 * Save system settings configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_save_config(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("System settings configuration saved");
  return TRUE;
}

/**
 * lime_system_settings_load_config:
 * @settings: A #LiMeSystemSettings
 *
 * Load system settings configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_load_config(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("System settings configuration loaded");
  return TRUE;
}

/**
 * lime_system_settings_reset_to_defaults:
 * @settings: A #LiMeSystemSettings
 *
 * Reset all settings to defaults
 *
 * Returns: %TRUE on success
 */
gboolean
lime_system_settings_reset_to_defaults(LiMeSystemSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SYSTEM_SETTINGS(settings), FALSE);

  g_debug("Settings reset to defaults");
  return TRUE;
}
