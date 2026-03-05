/*
 * LiMe Theme Manager - Implementation
 */

#include "lime-theme-manager.h"
#include <string.h>

struct _LiMeThemeManager {
  GObject parent;

  gchar *current_theme;
  gchar *accent_color;
  gdouble transparency;
  gboolean custom_fonts_enabled;

  GSettings *settings;
};

G_DEFINE_TYPE(LiMeThemeManager, lime_theme_manager, G_TYPE_OBJECT);

static void
lime_theme_manager_dispose (GObject *object)
{
  LiMeThemeManager *manager = LIME_THEME_MANAGER (object);

  g_clear_pointer (&manager->current_theme, g_free);
  g_clear_pointer (&manager->accent_color, g_free);
  g_clear_object (&manager->settings);

  G_OBJECT_CLASS (lime_theme_manager_parent_class)->dispose (object);
}

static void
lime_theme_manager_class_init (LiMeThemeManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = lime_theme_manager_dispose;
}

static void
lime_theme_manager_init (LiMeThemeManager *manager)
{
  manager->current_theme = g_strdup ("default");
  manager->accent_color = g_strdup ("#2E8B57");
  manager->transparency = 1.0;
  manager->custom_fonts_enabled = TRUE;
}

LiMeThemeManager *
lime_theme_manager_new (void)
{
  return g_object_new (LIME_TYPE_THEME_MANAGER, NULL);
}

void
lime_theme_manager_load_default (LiMeThemeManager *manager)
{
  g_return_if_fail (LIME_IS_THEME_MANAGER (manager));
  /* Load default theme from /usr/share/lime/themes */
}

void
lime_theme_manager_load_theme (LiMeThemeManager *manager, const char *theme_name)
{
  g_return_if_fail (LIME_IS_THEME_MANAGER (manager));
  g_return_if_fail (theme_name != NULL);

  g_free (manager->current_theme);
  manager->current_theme = g_strdup (theme_name);
}

void
lime_theme_manager_set_accent_color (LiMeThemeManager *manager, const char *color)
{
  g_return_if_fail (LIME_IS_THEME_MANAGER (manager));
  g_return_if_fail (color != NULL);

  g_free (manager->accent_color);
  manager->accent_color = g_strdup (color);
}

void
lime_theme_manager_enable_custom_fonts (LiMeThemeManager *manager, gboolean enable)
{
  g_return_if_fail (LIME_IS_THEME_MANAGER (manager));
  manager->custom_fonts_enabled = enable;
}

void
lime_theme_manager_set_transparency (LiMeThemeManager *manager, gdouble alpha)
{
  g_return_if_fail (LIME_IS_THEME_MANAGER (manager));
  manager->transparency = CLAMP (alpha, 0.0, 1.0);
}

gchar *
lime_theme_manager_get_current_theme (LiMeThemeManager *manager)
{
  g_return_val_if_fail (LIME_IS_THEME_MANAGER (manager), NULL);
  return g_strdup (manager->current_theme);
}
