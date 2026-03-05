/*
 * LiMe System Settings and Customization Header
 * Unified settings framework for appearance, behavior, and system integration
 */

#ifndef __LIME_SYSTEM_SETTINGS_H__
#define __LIME_SYSTEM_SETTINGS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_SYSTEM_SETTINGS (lime_system_settings_get_type())
#define LIME_SYSTEM_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_SYSTEM_SETTINGS, LiMeSystemSettings))
#define LIME_IS_SYSTEM_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_SYSTEM_SETTINGS))

typedef struct _LiMeSystemSettings LiMeSystemSettings;
typedef struct _LiMeSystemSettingsClass LiMeSystemSettingsClass;

typedef enum {
  LIME_THEME_LIGHT,
  LIME_THEME_DARK,
  LIME_THEME_CUSTOM
} LiMeThemeMode;

typedef enum {
  LIME_ACCENT_BLUE,
  LIME_ACCENT_GREEN,
  LIME_ACCENT_RED,
  LIME_ACCENT_PURPLE,
  LIME_ACCENT_ORANGE,
  LIME_ACCENT_PINK,
  LIME_ACCENT_TEAL,
  LIME_ACCENT_CUSTOM
} LiMeAccentColor;

typedef struct {
  gchar *theme_id;
  gchar *theme_name;
  gchar *description;
  LiMeThemeMode theme_mode;
  LiMeAccentColor accent_color;
  gchar *primary_color;
  gchar *secondary_color;
  gchar *background_color;
  gchar *text_color;
  gdouble saturation;
  gdouble brightness;
  gint border_radius;
  gint shadow_size;
  gboolean use_transparency;
} LiMeTheme;

typedef struct {
  gchar *cursor_name;
  gint cursor_size;
  gdouble cursor_speed;
} LiMeCursorSettings;

typedef struct {
  gchar *font_name;
  gint font_size;
  gchar *monospace_font;
  gint monospace_size;
} LiMeFontSettings;

GType lime_system_settings_get_type(void);

LiMeSystemSettings * lime_system_settings_new(void);

/* Theme management */
GList * lime_system_settings_get_themes(LiMeSystemSettings *settings);
LiMeTheme * lime_system_settings_get_current_theme(LiMeSystemSettings *settings);
gboolean lime_system_settings_apply_theme(LiMeSystemSettings *settings,
                                          const gchar *theme_id);
gboolean lime_system_settings_create_custom_theme(LiMeSystemSettings *settings,
                                                   const gchar *theme_name,
                                                   LiMeTheme *theme);
gboolean lime_system_settings_delete_custom_theme(LiMeSystemSettings *settings,
                                                   const gchar *theme_id);
gboolean lime_system_settings_export_theme(LiMeSystemSettings *settings,
                                            const gchar *theme_id,
                                            const gchar *export_path);
gboolean lime_system_settings_import_theme(LiMeSystemSettings *settings,
                                            const gchar *import_path);

/* Theme customization */
gboolean lime_system_settings_set_theme_mode(LiMeSystemSettings *settings,
                                              LiMeThemeMode mode);
gboolean lime_system_settings_set_accent_color(LiMeSystemSettings *settings,
                                                LiMeAccentColor accent);
gboolean lime_system_settings_set_custom_colors(LiMeSystemSettings *settings,
                                                 const gchar *primary,
                                                 const gchar *secondary,
                                                 const gchar *background);
gboolean lime_system_settings_set_saturation(LiMeSystemSettings *settings,
                                              gdouble saturation);
gboolean lime_system_settings_set_brightness(LiMeSystemSettings *settings,
                                              gdouble brightness);
gboolean lime_system_settings_set_blur_intensity(LiMeSystemSettings *settings,
                                                  gdouble intensity);

/* Cursor customization */
gboolean lime_system_settings_set_cursor_theme(LiMeSystemSettings *settings,
                                                const gchar *cursor_name);
gboolean lime_system_settings_set_cursor_size(LiMeSystemSettings *settings,
                                               gint size);
gboolean lime_system_settings_set_cursor_speed(LiMeSystemSettings *settings,
                                                gdouble speed);
GList * lime_system_settings_get_available_cursors(LiMeSystemSettings *settings);
gchar * lime_system_settings_get_cursor_path(LiMeSystemSettings *settings,
                                              const gchar *cursor_name);

/* Font settings */
gboolean lime_system_settings_set_default_font(LiMeSystemSettings *settings,
                                                const gchar *font_name,
                                                gint size);
gboolean lime_system_settings_set_monospace_font(LiMeSystemSettings *settings,
                                                  const gchar *font_name,
                                                  gint size);
GList * lime_system_settings_get_available_fonts(LiMeSystemSettings *settings);

/* Animation settings */
gboolean lime_system_settings_enable_animations(LiMeSystemSettings *settings);
gboolean lime_system_settings_disable_animations(LiMeSystemSettings *settings);
gboolean lime_system_settings_set_animation_speed(LiMeSystemSettings *settings,
                                                   gdouble speed);

/* Desktop appearance */
gboolean lime_system_settings_set_wallpaper(LiMeSystemSettings *settings,
                                             const gchar *image_path);
gchar * lime_system_settings_get_wallpaper(LiMeSystemSettings *settings);
gboolean lime_system_settings_enable_desktop_icons(LiMeSystemSettings *settings);
gboolean lime_system_settings_disable_desktop_icons(LiMeSystemSettings *settings);

/* Advanced customization */
gboolean lime_system_settings_set_window_decoration(LiMeSystemSettings *settings,
                                                     const gchar *style);
gboolean lime_system_settings_set_panel_position(LiMeSystemSettings *settings,
                                                  const gchar *position);
gboolean lime_system_settings_set_panel_height(LiMeSystemSettings *settings,
                                                gint height);
gboolean lime_system_settings_set_icon_theme(LiMeSystemSettings *settings,
                                              const gchar *icon_theme);
gboolean lime_system_settings_set_workspace_count(LiMeSystemSettings *settings,
                                                   gint count);

/* Presets */
GList * lime_system_settings_get_presets(LiMeSystemSettings *settings);
gboolean lime_system_settings_apply_preset(LiMeSystemSettings *settings,
                                            const gchar *preset_id);
gboolean lime_system_settings_create_preset(LiMeSystemSettings *settings,
                                             const gchar *preset_name);
gboolean lime_system_settings_delete_preset(LiMeSystemSettings *settings,
                                             const gchar *preset_id);

/* Live preview */
gboolean lime_system_settings_preview_theme(LiMeSystemSettings *settings,
                                             const gchar *theme_id);
gboolean lime_system_settings_apply_theme_permanent(LiMeSystemSettings *settings);

/* Configuration */
gboolean lime_system_settings_save_config(LiMeSystemSettings *settings);
gboolean lime_system_settings_load_config(LiMeSystemSettings *settings);
gboolean lime_system_settings_reset_to_defaults(LiMeSystemSettings *settings);

G_END_DECLS

#endif /* __LIME_SYSTEM_SETTINGS_H__ */
