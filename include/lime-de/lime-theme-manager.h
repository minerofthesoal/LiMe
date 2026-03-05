/*
 * LiMe Theme Manager
 * Handles theming, customization, and UI styling
 */

#ifndef __LIME_THEME_MANAGER_H__
#define __LIME_THEME_MANAGER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

typedef struct _LiMeThemeManager LiMeThemeManager;
typedef struct _LiMeThemeManagerClass LiMeThemeManagerClass;

#define LIME_TYPE_THEME_MANAGER (lime_theme_manager_get_type ())
#define LIME_THEME_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIME_TYPE_THEME_MANAGER, LiMeThemeManager))
#define LIME_IS_THEME_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIME_TYPE_THEME_MANAGER))

GType lime_theme_manager_get_type (void);

LiMeThemeManager * lime_theme_manager_new (void);

void lime_theme_manager_load_default (LiMeThemeManager *manager);
void lime_theme_manager_load_theme (LiMeThemeManager *manager, const char *theme_name);
void lime_theme_manager_set_accent_color (LiMeThemeManager *manager, const char *color);
void lime_theme_manager_enable_custom_fonts (LiMeThemeManager *manager, gboolean enable);
void lime_theme_manager_set_transparency (LiMeThemeManager *manager, gdouble alpha);

gchar * lime_theme_manager_get_current_theme (LiMeThemeManager *manager);

#endif /* __LIME_THEME_MANAGER_H__ */
