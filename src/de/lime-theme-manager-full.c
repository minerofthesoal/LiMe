/*
 * LiMe Expanded Theme Manager
 * Complete theme engine and customization system
 * Handles custom themes, colors, fonts, and effcts
 */

#include "lime-theme-manager.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

#define THEMES_DIR "/usr/share/lime/themes"
#define USER_THEMES_DIR ".local/share/lime/themes"
#define THEME_CONFIG_FILE "theme.conf"

struct _LiMeThemeManager {
  GObject parent;

  gchar *current_theme;
  gchar *current_accent_color;
  gchar *current_accent_color_alt;
  gdouble current_transparency;

  GHashTable *available_themes;
  GHashTable *theme_configs;

  GtkCssProvider *css_provider;
  GtkSettings *gtk_settings;

  GSettings *settings;
  gulong settings_handler;

  GSList *theme_search_paths;

  gboolean enable_custom_colors;
  gboolean enable_effects;
  gdouble animation_speed;

  /* Custom font settings */
  gchar *ui_font;
  gchar *document_font;
  gchar *monospace_font;

  gint font_scale;

  /* Color scheme */
  gchar *background_color;
  gchar *foreground_color;
  gchar *text_color;
  gchar *selected_bg_color;
  gchar *selected_text_color;

  /* Effects */
  gboolean enable_shadows;
  gboolean enable_blur;
  gboolean enable_transparency;

  GFileMonitor *themes_monitor;
  guint reload_timeout;
};

G_DEFINE_TYPE(LiMeThemeManager, lime_theme_manager, G_TYPE_OBJECT);

enum {
  SIGNAL_THEME_CHANGED,
  SIGNAL_COLOR_CHANGED,
  SIGNAL_FONT_CHANGED,
  LAST_SIGNAL
};

static guint theme_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations */
static void lime_theme_manager_load_themes(LiMeThemeManager *manager);
static void lime_theme_manager_load_theme_file(LiMeThemeManager *manager,
                                               const gchar *theme_path);
static void lime_theme_manager_apply_css(LiMeThemeManager *manager);
static gboolean lime_theme_manager_reload_timeout(gpointer user_data);
static void on_themes_dir_changed(GFileMonitor *monitor, GFile *file,
                                  GFile *other_file, GFileMonitorEvent event,
                                  gpointer user_data);

/**
 * lime_theme_manager_class_init:
 * @klass: The class structure
 *
 * Initialize theme manager class
 */
static void
lime_theme_manager_class_init(LiMeThemeManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_theme_manager_dispose;
  object_class->finalize = lime_theme_manager_finalize;

  /* Signals */
  theme_signals[SIGNAL_THEME_CHANGED] =
    g_signal_new("theme-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  theme_signals[SIGNAL_COLOR_CHANGED] =
    g_signal_new("color-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  theme_signals[SIGNAL_FONT_CHANGED] =
    g_signal_new("font-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);
}

/**
 * lime_theme_manager_init:
 * @manager: The theme manager instance
 *
 * Initialize a new theme manager
 */
static void
lime_theme_manager_init(LiMeThemeManager *manager)
{
  manager->current_theme = g_strdup("LiMe-Default");
  manager->current_accent_color = g_strdup("#5294E2");
  manager->current_accent_color_alt = g_strdup("#2E8B57");
  manager->current_transparency = 0.95;

  manager->available_themes = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                     g_free, g_free);
  manager->theme_configs = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, (GDestroyNotify)g_key_file_unref);

  manager->gtk_settings = gtk_settings_get_default();
  manager->css_provider = gtk_css_provider_new();

  manager->settings = g_settings_new("org.cinnamon.theme");

  manager->theme_search_paths = NULL;
  manager->theme_search_paths = g_slist_prepend(manager->theme_search_paths,
                                                 g_strdup(THEMES_DIR));
  manager->theme_search_paths = g_slist_prepend(manager->theme_search_paths,
    g_build_filename(g_get_home_dir(), USER_THEMES_DIR, NULL));

  manager->enable_custom_colors = TRUE;
  manager->enable_effects = TRUE;
  manager->animation_speed = 1.0;

  manager->ui_font = g_strdup("Noto Sans,  10");
  manager->document_font = g_strdup("Noto Serif, 11");
  manager->monospace_font = g_strdup("Monospace, 10");

  manager->font_scale = 100;

  manager->background_color = g_strdup("#FFFFFF");
  manager->foreground_color = g_strdup("#F5F5F5");
  manager->text_color = g_strdup("#333333");
  manager->selected_bg_color = g_strdup("#5294E2");
  manager->selected_text_color = g_strdup("#FFFFFF");

  manager->enable_shadows = TRUE;
  manager->enable_blur = TRUE;
  manager->enable_transparency = TRUE;

  g_debug("Theme manager initialized");
}

/**
 * lime_theme_manager_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_theme_manager_dispose(GObject *object)
{
  LiMeThemeManager *manager = LIME_THEME_MANAGER(object);

  if (manager->settings_handler) {
    g_signal_handler_disconnect(manager->settings, manager->settings_handler);
    manager->settings_handler = 0;
  }

  if (manager->reload_timeout) {
    g_source_remove(manager->reload_timeout);
    manager->reload_timeout = 0;
  }

  if (manager->themes_monitor) {
    g_file_monitor_cancel(manager->themes_monitor);
    g_clear_object(&manager->themes_monitor);
  }

  if (manager->available_themes) {
    g_hash_table_unref(manager->available_themes);
    manager->available_themes = NULL;
  }

  if (manager->theme_configs) {
    g_hash_table_unref(manager->theme_configs);
    manager->theme_configs = NULL;
  }

  g_slist_free_full(manager->theme_search_paths, g_free);
  manager->theme_search_paths = NULL;

  g_clear_object(&manager->settings);
  g_clear_object(&manager->css_provider);

  G_OBJECT_CLASS(lime_theme_manager_parent_class)->dispose(object);
}

/**
 * lime_theme_manager_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_theme_manager_finalize(GObject *object)
{
  LiMeThemeManager *manager = LIME_THEME_MANAGER(object);

  g_free(manager->current_theme);
  g_free(manager->current_accent_color);
  g_free(manager->current_accent_color_alt);
  g_free(manager->ui_font);
  g_free(manager->document_font);
  g_free(manager->monospace_font);
  g_free(manager->background_color);
  g_free(manager->foreground_color);
  g_free(manager->text_color);
  g_free(manager->selected_bg_color);
  g_free(manager->selected_text_color);

  G_OBJECT_CLASS(lime_theme_manager_parent_class)->finalize(object);
}

/**
 * lime_theme_manager_new:
 *
 * Create a new theme manager
 *
 * Returns: A new #LiMeThemeManager
 */
LiMeThemeManager *
lime_theme_manager_new(void)
{
  LiMeThemeManager *manager = g_object_new(LIME_TYPE_THEME_MANAGER, NULL);

  lime_theme_manager_load_themes(manager);

  return manager;
}

/**
 * lime_theme_manager_load_themes:
 * @manager: A #LiMeThemeManager
 *
 * Load available themes
 */
static void
lime_theme_manager_load_themes(LiMeThemeManager *manager)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));

  g_debug("Loading themes...");

  for (GSList *link = manager->theme_search_paths; link; link = link->next) {
    gchar *search_path = (gchar *)link->data;

    if (!g_file_test(search_path, G_FILE_TEST_IS_DIR)) {
      continue;
    }

    GDir *dir = g_dir_open(search_path, 0, NULL);
    if (!dir) continue;

    const gchar *entry;
    while ((entry = g_dir_read_name(dir))) {
      gchar *theme_path = g_build_filename(search_path, entry, NULL);

      if (g_file_test(theme_path, G_FILE_TEST_IS_DIR)) {
        gchar *config_file = g_build_filename(theme_path, THEME_CONFIG_FILE, NULL);

        if (g_file_test(config_file, G_FILE_TEST_EXISTS)) {
          lime_theme_manager_load_theme_file(manager, theme_path);
          g_hash_table_insert(manager->available_themes,
                              g_strdup(entry),
                              g_strdup(theme_path));
        }

        g_free(config_file);
      }

      g_free(theme_path);
    }

    g_dir_close(dir);
  }

  g_debug("Loaded %u themes", g_hash_table_size(manager->available_themes));
}

/**
 * lime_theme_manager_load_theme_file:
 * @manager: A #LiMeThemeManager
 * @theme_path: Path to the theme
 *
 * Load a single theme configuration
 */
static void
lime_theme_manager_load_theme_file(LiMeThemeManager *manager,
                                   const gchar *theme_path)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));
  g_return_if_fail(theme_path != NULL);

  gchar *config_file = g_build_filename(theme_path, THEME_CONFIG_FILE, NULL);
  GKeyFile *keyfile = g_key_file_new();
  GError *error = NULL;

  if (!g_key_file_load_from_file(keyfile, config_file, 0, &error)) {
    g_warning("Failed to load theme config: %s", error->message);
    g_error_free(error);
    g_key_file_unref(keyfile);
    g_free(config_file);
    return;
  }

  /* Extract theme name */
  gchar *theme_name = g_path_get_basename(theme_path);
  g_hash_table_insert(manager->theme_configs, g_strdup(theme_name), keyfile);

  g_free(theme_name);
  g_free(config_file);
}

/**
 * lime_theme_manager_load_default:
 * @manager: A #LiMeThemeManager
 *
 * Load the default theme
 */
void
lime_theme_manager_load_default(LiMeThemeManager *manager)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));

  lime_theme_manager_load_theme(manager, "LiMe-Default");
}

/**
 * lime_theme_manager_load_theme:
 * @manager: A #LiMeThemeManager
 * @theme_name: Name of the theme to load
 *
 * Load a specific theme
 */
void
lime_theme_manager_load_theme(LiMeThemeManager *manager, const gchar *theme_name)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));
  g_return_if_fail(theme_name != NULL);

  gchar *theme_path = g_hash_table_lookup(manager->available_themes, theme_name);

  if (!theme_path) {
    g_warning("Theme not found: %s", theme_name);
    return;
  }

  g_free(manager->current_theme);
  manager->current_theme = g_strdup(theme_name);

  lime_theme_manager_apply_css(manager);

  g_signal_emit(manager, theme_signals[SIGNAL_THEME_CHANGED], 0, theme_name);

  g_debug("Loaded theme: %s", theme_name);
}

/**
 * lime_theme_manager_apply_css:
 * @manager: A #LiMeThemeManager
 *
 * Apply CSS to GTK
 */
static void
lime_theme_manager_apply_css(LiMeThemeManager *manager)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));

  GtkCssProvider *provider = gtk_css_provider_new();

  gchar *css_data = g_strdup_printf(
    ".lime-window { background-color: %s; color: %s; }\n"
    ".lime-button { background-color: %s; color: white; }\n"
    ".lime-button:hover { opacity: 0.9; }\n"
    ".lime-panel { background-color: #2a2a2a; color: white; }\n"
    "selection { background-color: %s; color: %s; }",
    manager->background_color,
    manager->text_color,
    manager->current_accent_color,
    manager->selected_bg_color,
    manager->selected_text_color
  );

  gtk_css_provider_load_from_data(provider, css_data, -1, NULL);

  GtkStyleContext *context = gtk_style_context_new();
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_object_unref(provider);
  g_free(css_data);
}

/**
 * lime_theme_manager_set_accent_color:
 * @manager: A #LiMeThemeManager
 * @color: The accent color code
 *
 * Set the accent color
 */
void
lime_theme_manager_set_accent_color(LiMeThemeManager *manager, const gchar *color)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));
  g_return_if_fail(color != NULL);

  if (g_strcmp0(manager->current_accent_color, color) != 0) {
    g_free(manager->current_accent_color);
    manager->current_accent_color = g_strdup(color);

    lime_theme_manager_apply_css(manager);
    g_signal_emit(manager, theme_signals[SIGNAL_COLOR_CHANGED], 0, color);
  }
}

/**
 * lime_theme_manager_get_accent_color:
 * @manager: A #LiMeThemeManager
 *
 * Get the current accent color
 *
 * Returns: (transfer none): The accent color
 */
const gchar *
lime_theme_manager_get_accent_color(LiMeThemeManager *manager)
{
  g_return_val_if_fail(LIME_IS_THEME_MANAGER(manager), NULL);

  return manager->current_accent_color;
}

/**
 * lime_theme_manager_reload_current:
 * @manager: A #LiMeThemeManager
 *
 * Reload the current theme
 */
void
lime_theme_manager_reload_current(LiMeThemeManager *manager)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));

  lime_theme_manager_load_theme(manager, manager->current_theme);
}

/**
 * lime_theme_manager_get_current_theme:
 * @manager: A #LiMeThemeManager
 *
 * Get the current theme name
 *
 * Returns: (transfer full): The current theme name
 */
gchar *
lime_theme_manager_get_current_theme(LiMeThemeManager *manager)
{
  g_return_val_if_fail(LIME_IS_THEME_MANAGER(manager), NULL);

  return g_strdup(manager->current_theme);
}

/**
 * lime_theme_manager_get_available_themes:
 * @manager: A #LiMeThemeManager
 *
 * Get list of available themes
 *
 * Returns: (element-type gchar*) (transfer container): Available theme names
 */
GList *
lime_theme_manager_get_available_themes(LiMeThemeManager *manager)
{
  g_return_val_if_fail(LIME_IS_THEME_MANAGER(manager), NULL);

  return g_hash_table_get_keys(manager->available_themes);
}

/**
 * lime_theme_manager_set_transparency:
 * @manager: A #LiMeThemeManager
 * @alpha: Transparency value (0.0 - 1.0)
 *
 * Set window transparency
 */
void
lime_theme_manager_set_transparency(LiMeThemeManager *manager, gdouble alpha)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));

  manager->current_transparency = CLAMP(alpha, 0.0, 1.0);
}

/**
 * lime_theme_manager_set_font:
 * @manager: A #LiMeThemeManager
 * @font_type: The font type (0=ui, 1=document, 2=mono)
 * @font_name: The font name
 *
 * Set a font
 */
void
lime_theme_manager_set_font(LiMeThemeManager *manager, gint font_type,
                            const gchar *font_name)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));
  g_return_if_fail(font_name != NULL);

  switch (font_type) {
    case 0:
      g_free(manager->ui_font);
      manager->ui_font = g_strdup(font_name);
      break;
    case 1:
      g_free(manager->document_font);
      manager->document_font = g_strdup(font_name);
      break;
    case 2:
      g_free(manager->monospace_font);
      manager->monospace_font = g_strdup(font_name);
      break;
  }

  g_signal_emit(manager, theme_signals[SIGNAL_FONT_CHANGED], 0, font_name);
}

/**
 * lime_theme_manager_enable_custom_colors:
 * @manager: A #LiMeThemeManager
 * @enable: Whether to enable custom colors
 *
 * Enable/disable custom color scheme
 */
void
lime_theme_manager_enable_custom_colors(LiMeThemeManager *manager, gboolean enable)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));

  manager->enable_custom_colors = enable;

  if (enable) {
    lime_theme_manager_apply_css(manager);
  }
}

/**
 * lime_theme_manager_set_animation_speed:
 * @manager: A #LiMeThemeManager
 * @speed: Animation speed multiplier
 *
 * Set animation speed
 */
void
lime_theme_manager_set_animation_speed(LiMeThemeManager *manager, gdouble speed)
{
  g_return_if_fail(LIME_IS_THEME_MANAGER(manager));

  manager->animation_speed = CLAMP(speed, 0.1, 3.0);
}
