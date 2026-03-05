/*
 * LiMe Settings Manager
 * System-wide settings and preferences
 * Complete settings management system for the desktop environment
 */

#include "lime-settings.h"
#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <json-glib/json-glib.h>

#define SETTINGS_DIR ".config/lime"
#define SETTINGS_FILE "desktop.conf"

struct _LiMeSettings {
  GObject parent;

  GSettings *gsettings;
  GKeyFile *keyfile;
  gchar *config_path;

  /* Display settings */
  gint monitor_count;
  gint *monitor_resolutions;
  gint primary_monitor;

  /* Theme settings */
  gchar *theme_name;
  gchar *icon_theme;
  gchar *cursor_theme;
  gint cursor_size;
  gdouble ui_scale;

  /* Window manager settings */
  gboolean focus_follows_mouse;
  gint button_layout;
  gint workspace_count;
  gboolean workspace_wraparound;

  /* Panel settings */
  gboolean panel_autohide;
  gint panel_position;
  gint panel_size;

  /* Keyboard shortcuts */
  GHashTable *keybindings;

  /* Sound settings */
  gfloat master_volume;
  gboolean mute;
  gboolean sound_effects;

  /* Power settings */
  gint screen_timeout;
  gint sleep_timeout;
  gboolean lid_action;

  /* Accessibility */
  gboolean high_contrast;
  gint text_scaling;
  gboolean visual_alerts;

  /* AI settings */
  gchar *ai_model;
  gchar *ai_provider;
  gboolean ai_enabled;

  /* Privacy settings */
  gboolean enable_tracking;
  gboolean collect_stats;

  /* Performance */
  gboolean enable_animations;
  gboolean enable_compositing;
  gint animation_speed;

  /* File watchers */
  GFileMonitor *config_monitor;

  /* Signal handlers */
  gulong changed_handler;
};

G_DEFINE_TYPE(LiMeSettings, lime_settings, G_TYPE_OBJECT);

enum {
  SIGNAL_CHANGED,
  SIGNAL_THEME_CHANGED,
  SIGNAL_KEYBINDING_CHANGED,
  LAST_SIGNAL
};

static guint settings_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations */
static void lime_settings_load_from_disk(LiMeSettings *settings);
static void lime_settings_save_to_disk(LiMeSettings *settings);
static void lime_settings_init_defaults(LiMeSettings *settings);
static void on_config_file_changed(GFileMonitor *monitor, GFile *file,
                                    GFile *other_file, GFileMonitorEvent event,
                                    gpointer user_data);

/**
 * lime_settings_class_init:
 * @klass: The class structure
 *
 * Initialize settings class
 */
static void
lime_settings_class_init(LiMeSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_settings_dispose;
  object_class->finalize = lime_settings_finalize;

  /* Signals */
  settings_signals[SIGNAL_CHANGED] =
    g_signal_new("changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  settings_signals[SIGNAL_THEME_CHANGED] =
    g_signal_new("theme-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  settings_signals[SIGNAL_KEYBINDING_CHANGED] =
    g_signal_new("keybinding-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);
}

/**
 * lime_settings_init:
 * @settings: The settings instance
 *
 * Initialize a new settings object
 */
static void
lime_settings_init(LiMeSettings *settings)
{
  settings->gsettings = NULL;
  settings->keyfile = NULL;
  settings->config_path = NULL;

  settings->monitor_count = 1;
  settings->monitor_resolutions = NULL;
  settings->primary_monitor = 0;

  settings->theme_name = g_strdup("LiMe-Default");
  settings->icon_theme = g_strdup("Papirus");
  settings->cursor_theme = g_strdup("Adwaita");
  settings->cursor_size = 24;
  settings->ui_scale = 1.0;

  settings->focus_follows_mouse = FALSE;
  settings->button_layout = 0;
  settings->workspace_count = 4;
  settings->workspace_wraparound = FALSE;

  settings->panel_autohide = FALSE;
  settings->panel_position = 0; /* Top */
  settings->panel_size = 40;

  settings->keybindings = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, g_free);

  settings->master_volume = 0.7;
  settings->mute = FALSE;
  settings->sound_effects = TRUE;

  settings->screen_timeout = 300;
  settings->sleep_timeout = 600;
  settings->lid_action = TRUE;

  settings->high_contrast = FALSE;
  settings->text_scaling = 100;
  settings->visual_alerts = TRUE;

  settings->ai_model = g_strdup("qwen:2b");
  settings->ai_provider = g_strdup("qwen");
  settings->ai_enabled = TRUE;

  settings->enable_tracking = FALSE;
  settings->collect_stats = FALSE;

  settings->enable_animations = TRUE;
  settings->enable_compositing = TRUE;
  settings->animation_speed = 1.0;

  lime_settings_init_defaults(settings);

  g_debug("Settings initialized");
}

/**
 * lime_settings_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_settings_dispose(GObject *object)
{
  LiMeSettings *settings = LIME_SETTINGS(object);

  if (settings->config_monitor) {
    g_file_monitor_cancel(settings->config_monitor);
    g_clear_object(&settings->config_monitor);
  }

  if (settings->keybindings) {
    g_hash_table_unref(settings->keybindings);
    settings->keybindings = NULL;
  }

  g_clear_object(&settings->gsettings);

  if (settings->keyfile) {
    g_key_file_unref(settings->keyfile);
    settings->keyfile = NULL;
  }

  G_OBJECT_CLASS(lime_settings_parent_class)->dispose(object);
}

/**
 * lime_settings_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_settings_finalize(GObject *object)
{
  LiMeSettings *settings = LIME_SETTINGS(object);

  g_free(settings->config_path);
  g_free(settings->theme_name);
  g_free(settings->icon_theme);
  g_free(settings->cursor_theme);
  g_free(settings->ai_model);
  g_free(settings->ai_provider);
  g_free(settings->monitor_resolutions);

  G_OBJECT_CLASS(lime_settings_parent_class)->finalize(object);
}

/**
 * lime_settings_init_defaults:
 * @settings: A #LiMeSettings
 *
 * Initialize default keybindings and settings
 */
static void
lime_settings_init_defaults(LiMeSettings *settings)
{
  g_return_if_fail(LIME_IS_SETTINGS(settings));

  /* Default keybindings */
  g_hash_table_insert(settings->keybindings,
                      g_strdup("switch-workspace-1"), g_strdup("Super+1"));
  g_hash_table_insert(settings->keybindings,
                      g_strdup("switch-workspace-2"), g_strdup("Super+2"));
  g_hash_table_insert(settings->keybindings,
                      g_strdup("switch-workspace-3"), g_strdup("Super+3"));
  g_hash_table_insert(settings->keybindings,
                      g_strdup("switch-workspace-4"), g_strdup("Super+4"));

  g_hash_table_insert(settings->keybindings,
                      g_strdup("launch-app-menu"), g_strdup("Super"));
  g_hash_table_insert(settings->keybindings,
                      g_strdup("launch-ai"), g_strdup("Super+A"));
  g_hash_table_insert(settings->keybindings,
                      g_strdup("window-close"), g_strdup("Alt+F4"));
  g_hash_table_insert(settings->keybindings,
                      g_strdup("window-maximize"), g_strdup("Super+Up"));
  g_hash_table_insert(settings->keybindings,
                      g_strdup("window-minimize"), g_strdup("Super+H"));

  g_hash_table_insert(settings->keybindings,
                      g_strdup("screenshot"), g_strdup("Print"));
  g_hash_table_insert(settings->keybindings,
                      g_strdup("screenshot-area"), g_strdup("Shift+Print"));

  g_hash_table_insert(settings->keybindings,
                      g_strdup("terminal"), g_strdup("Ctrl+Alt+T"));

  g_hash_table_insert(settings->keybindings,
                      g_strdup("lock-screen"), g_strdup("Super+L"));
}

/**
 * lime_settings_new:
 *
 * Create a new settings object
 *
 * Returns: A new #LiMeSettings
 */
LiMeSettings *
lime_settings_new(void)
{
  LiMeSettings *settings;

  settings = g_object_new(LIME_TYPE_SETTINGS, NULL);

  settings->config_path = g_build_filename(g_get_home_dir(), SETTINGS_DIR,
                                           SETTINGS_FILE, NULL);

  /* Create config directory if needed */
  gchar *config_dir = g_build_filename(g_get_home_dir(), SETTINGS_DIR, NULL);
  g_mkdir_with_parents(config_dir, 0755);
  g_free(config_dir);

  settings->gsettings = g_settings_new("org.cinnamon");
  settings->keyfile = g_key_file_new();

  /* Load existing configuration */
  lime_settings_load_from_disk(settings);

  /* Watch for external changes */
  GFile *config_file = g_file_new_for_path(settings->config_path);
  settings->config_monitor = g_file_monitor_file(config_file, G_FILE_MONITOR_NONE,
                                                  NULL, NULL);
  g_signal_connect(settings->config_monitor, "changed",
                   G_CALLBACK(on_config_file_changed), settings);
  g_object_unref(config_file);

  return settings;
}

/**
 * lime_settings_load_from_disk:
 * @settings: A #LiMeSettings
 *
 * Load settings from disk
 */
static void
lime_settings_load_from_disk(LiMeSettings *settings)
{
  g_return_if_fail(LIME_IS_SETTINGS(settings));

  if (!g_file_test(settings->config_path, G_FILE_TEST_EXISTS)) {
    lime_settings_save_to_disk(settings);
    return;
  }

  GError *error = NULL;
  g_key_file_load_from_file(settings->keyfile, settings->config_path,
                            G_KEY_FILE_KEEP_COMMENTS, &error);

  if (error) {
    g_warning("Failed to load settings: %s", error->message);
    g_error_free(error);
    return;
  }

  /* Load theme settings */
  if (g_key_file_has_key(settings->keyfile, "theme", "name", NULL)) {
    gchar *name = g_key_file_get_string(settings->keyfile, "theme", "name", NULL);
    if (name) {
      g_free(settings->theme_name);
      settings->theme_name = name;
    }
  }

  /* Load window manager settings */
  if (g_key_file_has_key(settings->keyfile, "wm", "focus-follows-mouse", NULL)) {
    settings->focus_follows_mouse =
      g_key_file_get_boolean(settings->keyfile, "wm", "focus-follows-mouse", NULL);
  }

  /* Load panel settings */
  if (g_key_file_has_key(settings->keyfile, "panel", "autohide", NULL)) {
    settings->panel_autohide =
      g_key_file_get_boolean(settings->keyfile, "panel", "autohide", NULL);
  }

  g_debug("Settings loaded from %s", settings->config_path);
}

/**
 * lime_settings_save_to_disk:
 * @settings: A #LiMeSettings
 *
 * Save settings to disk
 */
static void
lime_settings_save_to_disk(LiMeSettings *settings)
{
  g_return_if_fail(LIME_IS_SETTINGS(settings));

  GError *error = NULL;

  /* Clear existing keyfile */
  JsonBuilder *builder = json_builder_new();

  /* Build JSON structure */
  json_builder_begin_object(builder);

  /* Theme section */
  json_builder_set_member_name(builder, "theme");
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "name");
  json_builder_add_string_value(builder, settings->theme_name);
  json_builder_end_object(builder);

  /* WM section */
  json_builder_set_member_name(builder, "wm");
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "focus-follows-mouse");
  json_builder_add_boolean_value(builder, settings->focus_follows_mouse);
  json_builder_set_member_name(builder, "workspace-count");
  json_builder_add_int_value(builder, settings->workspace_count);
  json_builder_end_object(builder);

  /* Panel section */
  json_builder_set_member_name(builder, "panel");
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "autohide");
  json_builder_add_boolean_value(builder, settings->panel_autohide);
  json_builder_set_member_name(builder, "position");
  json_builder_add_int_value(builder, settings->panel_position);
  json_builder_end_object(builder);

  /* AI section */
  json_builder_set_member_name(builder, "ai");
  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "enabled");
  json_builder_add_boolean_value(builder, settings->ai_enabled);
  json_builder_set_member_name(builder, "model");
  json_builder_add_string_value(builder, settings->ai_model);
  json_builder_end_object(builder);

  json_builder_end_object(builder);

  JsonGenerator *generator = json_generator_new();
  JsonNode *root = json_builder_get_root(builder);
  json_generator_set_root(generator, root);

  gchar *json_str = json_generator_to_data(generator, NULL);

  /* Write to file */
  g_file_set_contents(settings->config_path, json_str, -1, &error);

  if (error) {
    g_warning("Failed to save settings: %s", error->message);
    g_error_free(error);
  } else {
    g_debug("Settings saved to %s", settings->config_path);
  }

  g_free(json_str);
  g_object_unref(generator);
  g_object_unref(builder);
}

/**
 * on_config_file_changed:
 * @monitor: The file monitor
 * @file: The changed file
 * @other_file: The other file (if rename)
 * @event: The event type
 * @user_data: The settings object
 *
 * Handle external configuration file changes
 */
static void
on_config_file_changed(GFileMonitor *monitor, GFile *file,
                       GFile *other_file, GFileMonitorEvent event,
                       gpointer user_data)
{
  LiMeSettings *settings = LIME_SETTINGS(user_data);

  if (event == G_FILE_MONITOR_EVENT_CHANGED) {
    g_debug("Configuration file changed, reloading");
    lime_settings_load_from_disk(settings);
    g_signal_emit(settings, settings_signals[SIGNAL_CHANGED], 0, "config");
  }
}

/**
 * lime_settings_set_theme:
 * @settings: A #LiMeSettings
 * @theme_name: The theme name
 *
 * Set the current theme
 */
void
lime_settings_set_theme(LiMeSettings *settings, const gchar *theme_name)
{
  g_return_if_fail(LIME_IS_SETTINGS(settings));
  g_return_if_fail(theme_name != NULL);

  if (g_strcmp0(settings->theme_name, theme_name) != 0) {
    g_free(settings->theme_name);
    settings->theme_name = g_strdup(theme_name);

    lime_settings_save_to_disk(settings);
    g_signal_emit(settings, settings_signals[SIGNAL_THEME_CHANGED], 0, theme_name);
  }
}

/**
 * lime_settings_get_theme:
 * @settings: A #LiMeSettings
 *
 * Get the current theme name
 *
 * Returns: (transfer none): The theme name
 */
const gchar *
lime_settings_get_theme(LiMeSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SETTINGS(settings), NULL);

  return settings->theme_name;
}

/**
 * lime_settings_set_keybinding:
 * @settings: A #LiMeSettings
 * @action: The action name
 * @binding: The key binding
 *
 * Set a keybinding
 */
void
lime_settings_set_keybinding(LiMeSettings *settings,
                             const gchar *action,
                             const gchar *binding)
{
  g_return_if_fail(LIME_IS_SETTINGS(settings));
  g_return_if_fail(action != NULL);
  g_return_if_fail(binding != NULL);

  g_hash_table_insert(settings->keybindings,
                      g_strdup(action),
                      g_strdup(binding));

  lime_settings_save_to_disk(settings);
  g_signal_emit(settings, settings_signals[SIGNAL_KEYBINDING_CHANGED], 0, action);
}

/**
 * lime_settings_get_keybinding:
 * @settings: A #LiMeSettings
 * @action: The action name
 *
 * Get a keybinding
 *
 * Returns: (transfer none): The key binding or %NULL
 */
const gchar *
lime_settings_get_keybinding(LiMeSettings *settings, const gchar *action)
{
  g_return_val_if_fail(LIME_IS_SETTINGS(settings), NULL);
  g_return_val_if_fail(action != NULL, NULL);

  return g_hash_table_lookup(settings->keybindings, action);
}

/**
 * lime_settings_get_workspace_count:
 * @settings: A #LiMeSettings
 *
 * Get the number of workspaces
 *
 * Returns: Number of workspaces
 */
gint
lime_settings_get_workspace_count(LiMeSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SETTINGS(settings), 4);

  return settings->workspace_count;
}

/**
 * lime_settings_set_workspace_count:
 * @settings: A #LiMeSettings
 * @count: New workspace count
 *
 * Set the number of workspaces
 */
void
lime_settings_set_workspace_count(LiMeSettings *settings, gint count)
{
  g_return_if_fail(LIME_IS_SETTINGS(settings));
  g_return_if_fail(count > 0);

  if (settings->workspace_count != count) {
    settings->workspace_count = count;
    lime_settings_save_to_disk(settings);
    g_signal_emit(settings, settings_signals[SIGNAL_CHANGED], 0, "workspace-count");
  }
}

/**
 * lime_settings_is_animations_enabled:
 * @settings: A #LiMeSettings
 *
 * Check if animations are enabled
 *
 * Returns: %TRUE if animations are enabled
 */
gboolean
lime_settings_is_animations_enabled(LiMeSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SETTINGS(settings), TRUE);

  return settings->enable_animations;
}

/**
 * lime_settings_is_compositing_enabled:
 * @settings: A #LiMeSettings
 *
 * Check if compositing is enabled
 *
 * Returns: %TRUE if compositing is enabled
 */
gboolean
lime_settings_is_compositing_enabled(LiMeSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SETTINGS(settings), TRUE);

  return settings->enable_compositing;
}

/**
 * lime_settings_get_ai_model:
 * @settings: A #LiMeSettings
 *
 * Get the configured AI model
 *
 * Returns: (transfer none): The AI model ID
 */
const gchar *
lime_settings_get_ai_model(LiMeSettings *settings)
{
  g_return_val_if_fail(LIME_IS_SETTINGS(settings), NULL);

  return settings->ai_model;
}

/**
 * lime_settings_set_ai_model:
 * @settings: A #LiMeSettings
 * @model: The AI model ID
 *
 * Set the AI model
 */
void
lime_settings_set_ai_model(LiMeSettings *settings, const gchar *model)
{
  g_return_if_fail(LIME_IS_SETTINGS(settings));
  g_return_if_fail(model != NULL);

  if (g_strcmp0(settings->ai_model, model) != 0) {
    g_free(settings->ai_model);
    settings->ai_model = g_strdup(model);

    lime_settings_save_to_disk(settings);
    g_signal_emit(settings, settings_signals[SIGNAL_CHANGED], 0, "ai-model");
  }
}
