/*
 * LiMe Application Launcher
 * Application menu and launcher system
 * Comprehensive application discovery and launching
 */

#include "lime-launcher.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>

#define APPLICATIONS_DIR "/usr/share/applications"

struct _LiMeLauncher {
  GObject parent;

  GAppInfoMonitor *app_monitor;
  GList *apps;
  GHashTable *app_index;

  gboolean favorites_changed;
  GList *favorites;

  GSettings *settings;

  gulong app_changed_handler;
};

G_DEFINE_TYPE(LiMeLauncher, lime_launcher, G_TYPE_OBJECT);

enum {
  SIGNAL_APP_ADDED,
  SIGNAL_APP_REMOVED,
  SIGNAL_APPS_CHANGED,
  SIGNAL_FAVORITES_CHANGED,
  LAST_SIGNAL
};

static guint launcher_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations */
static void lime_launcher_load_applications(LiMeLauncher *launcher);
static void on_app_info_changed(GAppInfoMonitor *monitor, gpointer user_data);

/**
 * lime_launcher_class_init:
 * @klass: The class structure
 *
 * Initialize launcher class
 */
static void
lime_launcher_class_init(LiMeLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_launcher_dispose;
  object_class->finalize = lime_launcher_finalize;

  /* Signals */
  launcher_signals[SIGNAL_APP_ADDED] =
    g_signal_new("app-added",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE, 1, G_TYPE_APP_INFO);

  launcher_signals[SIGNAL_APP_REMOVED] =
    g_signal_new("app-removed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  launcher_signals[SIGNAL_APPS_CHANGED] =
    g_signal_new("apps-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  launcher_signals[SIGNAL_FAVORITES_CHANGED] =
    g_signal_new("favorites-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

/**
 * lime_launcher_init:
 * @launcher: The launcher instance
 *
 * Initialize a new launcher
 */
static void
lime_launcher_init(LiMeLauncher *launcher)
{
  launcher->app_monitor = g_app_info_monitor_get();
  launcher->apps = NULL;
  launcher->app_index = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                g_free, g_object_unref);

  launcher->favorites_changed = FALSE;
  launcher->favorites = NULL;

  launcher->settings = g_settings_new("org.cinnamon.launcher");

  g_debug("Launcher initialized");
}

/**
 * lime_launcher_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_launcher_dispose(GObject *object)
{
  LiMeLauncher *launcher = LIME_LAUNCHER(object);

  if (launcher->app_changed_handler) {
    g_signal_handler_disconnect(launcher->app_monitor, launcher->app_changed_handler);
    launcher->app_changed_handler = 0;
  }

  if (launcher->app_index) {
    g_hash_table_unref(launcher->app_index);
    launcher->app_index = NULL;
  }

  g_list_free_full(launcher->apps, g_object_unref);
  launcher->apps = NULL;

  g_list_free(launcher->favorites);
  launcher->favorites = NULL;

  g_clear_object(&launcher->settings);

  G_OBJECT_CLASS(lime_launcher_parent_class)->dispose(object);
}

/**
 * lime_launcher_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_launcher_finalize(GObject *object)
{
  G_OBJECT_CLASS(lime_launcher_parent_class)->finalize(object);
}

/**
 * lime_launcher_new:
 *
 * Create a new launcher
 *
 * Returns: A new #LiMeLauncher
 */
LiMeLauncher *
lime_launcher_new(void)
{
  LiMeLauncher *launcher = g_object_new(LIME_TYPE_LAUNCHER, NULL);

  /* Load applications */
  lime_launcher_load_applications(launcher);

  /* Watch for application changes */
  launcher->app_changed_handler =
    g_signal_connect(launcher->app_monitor, "changed",
                     G_CALLBACK(on_app_info_changed), launcher);

  return launcher;
}

/**
 * lime_launcher_load_applications:
 * @launcher: A #LiMeLauncher
 *
 * Load available applications
 */
static void
lime_launcher_load_applications(LiMeLauncher *launcher)
{
  g_return_if_fail(LIME_IS_LAUNCHER(launcher));

  g_debug("Loading applications...");

  GList *apps = g_app_info_get_all();

  for (GList *link = apps; link; link = link->next) {
    GAppInfo *app = G_APP_INFO(link->data);

    if (g_app_info_should_show(app)) {
      const gchar *id = g_app_info_get_id(app);
      launcher->apps = g_list_prepend(launcher->apps, g_object_ref(app));
      g_hash_table_insert(launcher->app_index, g_strdup(id), g_object_ref(app));
    }
  }

  launcher->apps = g_list_reverse(launcher->apps);
  g_list_free(apps);

  g_debug("Loaded %u applications", g_list_length(launcher->apps));
}

/**
 * on_app_info_changed:
 * @monitor: The app info monitor
 * @user_data: The launcher
 *
 * Handle application list changes
 */
static void
on_app_info_changed(GAppInfoMonitor *monitor, gpointer user_data)
{
  LiMeLauncher *launcher = LIME_LAUNCHER(user_data);

  g_debug("Application list changed");

  /* Clear and reload */
  g_list_free_full(launcher->apps, g_object_unref);
  g_hash_table_remove_all(launcher->app_index);

  lime_launcher_load_applications(launcher);

  g_signal_emit(launcher, launcher_signals[SIGNAL_APPS_CHANGED], 0);
}

/**
 * lime_launcher_get_applications:
 * @launcher: A #LiMeLauncher
 *
 * Get list of all applications
 *
 * Returns: (element-type GAppInfo) (transfer none): List of applications
 */
GList *
lime_launcher_get_applications(LiMeLauncher *launcher)
{
  g_return_val_if_fail(LIME_IS_LAUNCHER(launcher), NULL);

  return launcher->apps;
}

/**
 * lime_launcher_search:
 * @launcher: A #LiMeLauncher
 * @query: The search query
 *
 * Search for applications
 *
 * Returns: (element-type GAppInfo) (transfer container): Matching applications
 */
GList *
lime_launcher_search(LiMeLauncher *launcher, const gchar *query)
{
  g_return_val_if_fail(LIME_IS_LAUNCHER(launcher), NULL);
  g_return_val_if_fail(query != NULL, NULL);

  GList *results = NULL;
  gchar *query_lower = g_utf8_strdown(query, -1);

  for (GList *link = launcher->apps; link; link = link->next) {
    GAppInfo *app = G_APP_INFO(link->data);

    const gchar *name = g_app_info_get_name(app);
    const gchar *desc = g_app_info_get_description(app);

    gchar *name_lower = g_utf8_strdown(name, -1);
    gchar *desc_lower = desc ? g_utf8_strdown(desc, -1) : NULL;

    gboolean matches = FALSE;

    if (g_strstr_len(name_lower, -1, query_lower)) {
      matches = TRUE;
    } else if (desc_lower && g_strstr_len(desc_lower, -1, query_lower)) {
      matches = TRUE;
    }

    if (matches) {
      results = g_list_prepend(results, app);
    }

    g_free(name_lower);
    g_free(desc_lower);
  }

  g_free(query_lower);

  return g_list_reverse(results);
}

/**
 * lime_launcher_launch_app:
 * @launcher: A #LiMeLauncher
 * @app: The application to launch
 *
 * Launch an application
 *
 * Returns: %TRUE on success
 */
gboolean
lime_launcher_launch_app(LiMeLauncher *launcher, GAppInfo *app)
{
  g_return_val_if_fail(LIME_IS_LAUNCHER(launcher), FALSE);
  g_return_val_if_fail(G_IS_APP_INFO(app), FALSE);

  GError *error = NULL;

  gboolean success = g_app_info_launch(app, NULL, NULL, &error);

  if (!success) {
    g_warning("Failed to launch application: %s", error->message);
    g_error_free(error);
  } else {
    g_debug("Launched: %s", g_app_info_get_name(app));
  }

  return success;
}

/**
 * lime_launcher_add_favorite:
 * @launcher: A #LiMeLauncher
 * @app: The application to add to favorites
 *
 * Add an application to favorites
 */
void
lime_launcher_add_favorite(LiMeLauncher *launcher, GAppInfo *app)
{
  g_return_if_fail(LIME_IS_LAUNCHER(launcher));
  g_return_if_fail(G_IS_APP_INFO(app));

  if (!g_list_find(launcher->favorites, app)) {
    launcher->favorites = g_list_append(launcher->favorites, app);
    launcher->favorites_changed = TRUE;

    g_signal_emit(launcher, launcher_signals[SIGNAL_FAVORITES_CHANGED], 0);
  }
}

/**
 * lime_launcher_remove_favorite:
 * @launcher: A #LiMeLauncher
 * @app: The application to remove from favorites
 *
 * Remove an application from favorites
 */
void
lime_launcher_remove_favorite(LiMeLauncher *launcher, GAppInfo *app)
{
  g_return_if_fail(LIME_IS_LAUNCHER(launcher));
  g_return_if_fail(G_IS_APP_INFO(app));

  if (g_list_remove(launcher->favorites, app)) {
    launcher->favorites_changed = TRUE;

    g_signal_emit(launcher, launcher_signals[SIGNAL_FAVORITES_CHANGED], 0);
  }
}

/**
 * lime_launcher_get_favorites:
 * @launcher: A #LiMeLauncher
 *
 * Get favorite applications
 *
 * Returns: (element-type GAppInfo) (transfer none): List of favorite applications
 */
GList *
lime_launcher_get_favorites(LiMeLauncher *launcher)
{
  g_return_val_if_fail(LIME_IS_LAUNCHER(launcher), NULL);

  return launcher->favorites;
}

/**
 * lime_launcher_is_favorite:
 * @launcher: A #LiMeLauncher
 * @app: The application to check
 *
 * Check if an application is a favorite
 *
 * Returns: %TRUE if favorite
 */
gboolean
lime_launcher_is_favorite(LiMeLauncher *launcher, GAppInfo *app)
{
  g_return_val_if_fail(LIME_IS_LAUNCHER(launcher), FALSE);
  g_return_val_if_fail(G_IS_APP_INFO(app), FALSE);

  return g_list_find(launcher->favorites, app) != NULL;
}

/**
 * lime_launcher_get_application:
 * @launcher: A #LiMeLauncher
 * @app_id: The application ID
 *
 * Get an application by ID
 *
 * Returns: (transfer none): The application or %NULL
 */
GAppInfo *
lime_launcher_get_application(LiMeLauncher *launcher, const gchar *app_id)
{
  g_return_val_if_fail(LIME_IS_LAUNCHER(launcher), NULL);
  g_return_val_if_fail(app_id != NULL, NULL);

  return g_hash_table_lookup(launcher->app_index, app_id);
}

/**
 * lime_launcher_get_num_applications:
 * @launcher: A #LiMeLauncher
 *
 * Get the number of available applications
 *
 * Returns: Number of applications
 */
guint
lime_launcher_get_num_applications(LiMeLauncher *launcher)
{
  g_return_val_if_fail(LIME_IS_LAUNCHER(launcher), 0);

  return g_list_length(launcher->apps);
}
