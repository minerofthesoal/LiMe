/*
 * LiMe Display Settings Header
 * Display configuration, resolution, and monitor management
 */

#ifndef __LIME_DISPLAY_SETTINGS_H__
#define __LIME_DISPLAY_SETTINGS_H__

#include <glib-object.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

#define LIME_TYPE_DISPLAY_SETTINGS (lime_display_settings_get_type())
#define LIME_DISPLAY_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_DISPLAY_SETTINGS, LiMeDisplaySettings))
#define LIME_IS_DISPLAY_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_DISPLAY_SETTINGS))

typedef struct _LiMeDisplaySettings LiMeDisplaySettings;
typedef struct _LiMeDisplaySettingsClass LiMeDisplaySettingsClass;

typedef enum {
  LIME_SCALE_100,
  LIME_SCALE_125,
  LIME_SCALE_150,
  LIME_SCALE_175,
  LIME_SCALE_200
} LiMeScaleFactor;

typedef enum {
  LIME_ROTATION_NORMAL,
  LIME_ROTATION_90,
  LIME_ROTATION_180,
  LIME_ROTATION_270
} LiMeRotation;

typedef enum {
  LIME_CURSOR_THEME_DEFAULT,
  LIME_CURSOR_THEME_LIGHT,
  LIME_CURSOR_THEME_DARK,
  LIME_CURSOR_THEME_LARGE
} LiMeCursorTheme;

typedef struct {
  gchar *name;
  gint monitor_id;
  gint width;
  gint height;
  gint refresh_rate;
  gint scale;
  LiMeRotation rotation;
  gint x_offset;
  gint y_offset;
  gboolean is_primary;
  gboolean is_enabled;
} LiMeMonitorConfig;

GType lime_display_settings_get_type(void);

LiMeDisplaySettings * lime_display_settings_new(void);

/* Monitor management */
GList * lime_display_settings_get_monitors(LiMeDisplaySettings *settings);
LiMeMonitorConfig * lime_display_settings_get_monitor(LiMeDisplaySettings *settings, gint monitor_id);
gboolean lime_display_settings_apply_monitor_config(LiMeDisplaySettings *settings,
                                                     LiMeMonitorConfig *config);
gboolean lime_display_settings_set_primary_monitor(LiMeDisplaySettings *settings, gint monitor_id);
gint lime_display_settings_get_primary_monitor(LiMeDisplaySettings *settings);

/* Resolution management */
gboolean lime_display_settings_set_resolution(LiMeDisplaySettings *settings,
                                               gint monitor_id,
                                               gint width, gint height);
gboolean lime_display_settings_set_refresh_rate(LiMeDisplaySettings *settings,
                                                 gint monitor_id,
                                                 gint refresh_rate);
gint * lime_display_settings_get_available_resolutions(LiMeDisplaySettings *settings,
                                                        gint monitor_id,
                                                        gint *count);

/* Scaling and DPI */
gboolean lime_display_settings_set_scale_factor(LiMeDisplaySettings *settings,
                                                 gint monitor_id,
                                                 LiMeScaleFactor scale);
gdouble lime_display_settings_get_scale_factor(LiMeDisplaySettings *settings, gint monitor_id);
gint lime_display_settings_get_dpi(LiMeDisplaySettings *settings, gint monitor_id);

/* Rotation */
gboolean lime_display_settings_set_rotation(LiMeDisplaySettings *settings,
                                             gint monitor_id,
                                             LiMeRotation rotation);
LiMeRotation lime_display_settings_get_rotation(LiMeDisplaySettings *settings, gint monitor_id);

/* Monitor arrangement */
gboolean lime_display_settings_arrange_monitors(LiMeDisplaySettings *settings);
gboolean lime_display_settings_enable_mirror_mode(LiMeDisplaySettings *settings);
gboolean lime_display_settings_enable_extend_mode(LiMeDisplaySettings *settings);

/* Cursor settings */
gboolean lime_display_settings_set_cursor_theme(LiMeDisplaySettings *settings,
                                                 LiMeCursorTheme theme);
gboolean lime_display_settings_set_cursor_size(LiMeDisplaySettings *settings, gint size);

/* Configuration save/load */
gboolean lime_display_settings_save_config(LiMeDisplaySettings *settings);
gboolean lime_display_settings_load_config(LiMeDisplaySettings *settings);

/* Night light/color management */
gboolean lime_display_settings_enable_night_light(LiMeDisplaySettings *settings);
gboolean lime_display_settings_set_color_temperature(LiMeDisplaySettings *settings, gint temperature);

G_END_DECLS

#endif /* __LIME_DISPLAY_SETTINGS_H__ */
