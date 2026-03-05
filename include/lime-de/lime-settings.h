/*
 * LiMe Settings Header
 * System settings interface
 */

#ifndef __LIME_SETTINGS_H__
#define __LIME_SETTINGS_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _LiMeSettings LiMeSettings;
typedef struct _LiMeSettingsClass LiMeSettingsClass;

#define LIME_TYPE_SETTINGS (lime_settings_get_type())
#define LIME_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_SETTINGS, LiMeSettings))
#define LIME_IS_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_SETTINGS))

typedef struct {
  GObjectClass parent;
} LiMeSettingsClass;

GType lime_settings_get_type(void);

LiMeSettings * lime_settings_new(void);

void lime_settings_set_theme(LiMeSettings *settings, const gchar *theme_name);
const gchar * lime_settings_get_theme(LiMeSettings *settings);

void lime_settings_set_keybinding(LiMeSettings *settings,
                                   const gchar *action,
                                   const gchar *binding);
const gchar * lime_settings_get_keybinding(LiMeSettings *settings,
                                            const gchar *action);

gint lime_settings_get_workspace_count(LiMeSettings *settings);
void lime_settings_set_workspace_count(LiMeSettings *settings, gint count);

gboolean lime_settings_is_animations_enabled(LiMeSettings *settings);
gboolean lime_settings_is_compositing_enabled(LiMeSettings *settings);

const gchar * lime_settings_get_ai_model(LiMeSettings *settings);
void lime_settings_set_ai_model(LiMeSettings *settings, const gchar *model);

G_END_DECLS

#endif /* __LIME_SETTINGS_H__ */
