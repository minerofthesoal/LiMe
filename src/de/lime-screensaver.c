/*
 * LiMe Screensaver and Lock Screen
 * Screensaver management and security lock screen
 * Provides blankscreen, pattern displays, and lock screen functionality
 */

#include "lime-screensaver.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>

#define SCREENSAVER_CONFIG_DIR ".config/lime/screensaver"
#define SCREENSAVER_CONFIG_FILE "screensaver.conf"

typedef struct {
  GObject parent;

  /* Screensaver settings */
  gboolean enabled;
  gint timeout_seconds;
  LiMeScreensaverType current_type;
  gboolean lock_on_screensaver;

  /* Lock screen settings */
  gint lock_timeout_seconds;
  gboolean show_clock;
  gboolean show_notifications;
  gboolean show_user_image;

  /* Idle tracking */
  gint idle_timeout_seconds;
  guint idle_timer;
  guint screensaver_timer;

  /* State */
  gboolean screensaver_active;
  gboolean is_locked;

  /* Configuration */
  GSettings *settings;

  /* Callbacks */
  GSList *screensaver_callbacks;
} LiMeScreensaverPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeScreensaver, lime_screensaver, G_TYPE_OBJECT);

enum {
  SIGNAL_SCREENSAVER_ACTIVATED,
  SIGNAL_SCREENSAVER_DEACTIVATED,
  SIGNAL_LOCKED,
  SIGNAL_UNLOCKED,
  SIGNAL_IDLE,
  LAST_SIGNAL
};

static guint screensaver_signals[LAST_SIGNAL] = { 0 };

static void
lime_screensaver_class_init(LiMeScreensaverClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  screensaver_signals[SIGNAL_SCREENSAVER_ACTIVATED] =
    g_signal_new("screensaver-activated",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  screensaver_signals[SIGNAL_SCREENSAVER_DEACTIVATED] =
    g_signal_new("screensaver-deactivated",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  screensaver_signals[SIGNAL_LOCKED] =
    g_signal_new("locked",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  screensaver_signals[SIGNAL_UNLOCKED] =
    g_signal_new("unlocked",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  screensaver_signals[SIGNAL_IDLE] =
    g_signal_new("idle",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static gboolean
lime_screensaver_idle_timeout_cb(gpointer user_data)
{
  LiMeScreensaver *saver = LIME_SCREENSAVER(user_data);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  if (priv->enabled && !priv->screensaver_active) {
    g_debug("Idle timeout reached, activating screensaver");
    lime_screensaver_activate(saver);
  }

  return G_SOURCE_CONTINUE;
}

static void
lime_screensaver_init(LiMeScreensaver *saver)
{
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->enabled = TRUE;
  priv->timeout_seconds = 600;  /* 10 minutes */
  priv->current_type = LIME_SCREENSAVER_BLANK;
  priv->lock_on_screensaver = FALSE;

  priv->lock_timeout_seconds = 60;  /* 1 minute */
  priv->show_clock = TRUE;
  priv->show_notifications = FALSE;
  priv->show_user_image = TRUE;

  priv->idle_timeout_seconds = 600;
  priv->screensaver_active = FALSE;
  priv->is_locked = FALSE;

  priv->settings = g_settings_new("org.cinnamon.screensaver");

  priv->idle_timer = g_timeout_add_seconds(30, lime_screensaver_idle_timeout_cb, saver);
  priv->screensaver_callbacks = NULL;

  g_debug("Screensaver initialized");
}

/**
 * lime_screensaver_new:
 *
 * Create a new screensaver manager
 *
 * Returns: A new #LiMeScreensaver
 */
LiMeScreensaver *
lime_screensaver_new(void)
{
  return g_object_new(LIME_TYPE_SCREENSAVER, NULL);
}

/**
 * lime_screensaver_enable:
 * @saver: A #LiMeScreensaver
 *
 * Enable the screensaver
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_enable(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->enabled = TRUE;
  g_debug("Screensaver enabled");

  return TRUE;
}

/**
 * lime_screensaver_disable:
 * @saver: A #LiMeScreensaver
 *
 * Disable the screensaver
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_disable(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->enabled = FALSE;
  if (priv->screensaver_active) {
    lime_screensaver_deactivate(saver);
  }

  g_debug("Screensaver disabled");
  return TRUE;
}

/**
 * lime_screensaver_is_enabled:
 * @saver: A #LiMeScreensaver
 *
 * Check if screensaver is enabled
 *
 * Returns: %TRUE if enabled
 */
gboolean
lime_screensaver_is_enabled(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  return priv->enabled;
}

/**
 * lime_screensaver_set_timeout:
 * @saver: A #LiMeScreensaver
 * @timeout_seconds: Timeout in seconds
 *
 * Set screensaver timeout
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_set_timeout(LiMeScreensaver *saver, gint timeout_seconds)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->timeout_seconds = CLAMP(timeout_seconds, 30, 3600);
  g_debug("Screensaver timeout set to %d seconds", priv->timeout_seconds);

  return TRUE;
}

/**
 * lime_screensaver_get_timeout:
 * @saver: A #LiMeScreensaver
 *
 * Get screensaver timeout
 *
 * Returns: Timeout in seconds
 */
gint
lime_screensaver_get_timeout(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), 600);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  return priv->timeout_seconds;
}

/**
 * lime_screensaver_set_type:
 * @saver: A #LiMeScreensaver
 * @type: Screensaver type
 *
 * Set screensaver type
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_set_type(LiMeScreensaver *saver, LiMeScreensaverType type)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  const gchar *type_names[] = { "blank", "patterns", "slideshow", "custom" };
  priv->current_type = type;

  g_debug("Screensaver type set to: %s", type_names[type]);
  return TRUE;
}

/**
 * lime_screensaver_get_type_active:
 * @saver: A #LiMeScreensaver
 *
 * Get active screensaver type
 *
 * Returns: Screensaver type
 */
LiMeScreensaverType
lime_screensaver_get_type_active(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), LIME_SCREENSAVER_BLANK);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  return priv->current_type;
}

/**
 * lime_screensaver_set_lock_on_screensaver:
 * @saver: A #LiMeScreensaver
 * @enabled: Whether to lock on screensaver
 *
 * Enable/disable automatic lock when screensaver activates
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_set_lock_on_screensaver(LiMeScreensaver *saver, gboolean enabled)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->lock_on_screensaver = enabled;
  g_debug("Lock on screensaver %s", enabled ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_screensaver_get_lock_on_screensaver:
 * @saver: A #LiMeScreensaver
 *
 * Get automatic lock on screensaver status
 *
 * Returns: %TRUE if enabled
 */
gboolean
lime_screensaver_get_lock_on_screensaver(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  return priv->lock_on_screensaver;
}

/**
 * lime_screensaver_activate:
 * @saver: A #LiMeScreensaver
 *
 * Activate the screensaver
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_activate(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  if (priv->screensaver_active) return TRUE;

  priv->screensaver_active = TRUE;
  g_debug("Screensaver activated");

  if (priv->lock_on_screensaver) {
    lime_screensaver_lock(saver);
  }

  g_signal_emit(saver, screensaver_signals[SIGNAL_SCREENSAVER_ACTIVATED], 0);
  return TRUE;
}

/**
 * lime_screensaver_deactivate:
 * @saver: A #LiMeScreensaver
 *
 * Deactivate the screensaver
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_deactivate(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  if (!priv->screensaver_active) return TRUE;

  priv->screensaver_active = FALSE;
  g_debug("Screensaver deactivated");

  if (priv->is_locked) {
    lime_screensaver_unlock(saver);
  }

  g_signal_emit(saver, screensaver_signals[SIGNAL_SCREENSAVER_DEACTIVATED], 0);
  return TRUE;
}

/**
 * lime_screensaver_is_active:
 * @saver: A #LiMeScreensaver
 *
 * Check if screensaver is active
 *
 * Returns: %TRUE if active
 */
gboolean
lime_screensaver_is_active(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  return priv->screensaver_active;
}

/**
 * lime_screensaver_lock:
 * @saver: A #LiMeScreensaver
 *
 * Lock the session
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_lock(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  if (priv->is_locked) return TRUE;

  priv->is_locked = TRUE;
  g_debug("Session locked");

  g_signal_emit(saver, screensaver_signals[SIGNAL_LOCKED], 0);
  return TRUE;
}

/**
 * lime_screensaver_unlock:
 * @saver: A #LiMeScreensaver
 *
 * Unlock the session
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_unlock(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  if (!priv->is_locked) return TRUE;

  priv->is_locked = FALSE;
  g_debug("Session unlocked");

  g_signal_emit(saver, screensaver_signals[SIGNAL_UNLOCKED], 0);
  return TRUE;
}

/**
 * lime_screensaver_is_locked:
 * @saver: A #LiMeScreensaver
 *
 * Check if session is locked
 *
 * Returns: %TRUE if locked
 */
gboolean
lime_screensaver_is_locked(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  return priv->is_locked;
}

/**
 * lime_screensaver_set_lock_timeout:
 * @saver: A #LiMeScreensaver
 * @timeout_seconds: Timeout in seconds
 *
 * Set lock screen timeout
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_set_lock_timeout(LiMeScreensaver *saver, gint timeout_seconds)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->lock_timeout_seconds = CLAMP(timeout_seconds, 10, 600);
  g_debug("Lock timeout set to %d seconds", priv->lock_timeout_seconds);

  return TRUE;
}

/**
 * lime_screensaver_get_lock_timeout:
 * @saver: A #LiMeScreensaver
 *
 * Get lock screen timeout
 *
 * Returns: Timeout in seconds
 */
gint
lime_screensaver_get_lock_timeout(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), 60);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  return priv->lock_timeout_seconds;
}

/**
 * lime_screensaver_set_show_clock:
 * @saver: A #LiMeScreensaver
 * @show: Whether to show clock
 *
 * Show/hide clock on lock screen
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_set_show_clock(LiMeScreensaver *saver, gboolean show)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->show_clock = show;
  g_debug("Lock screen clock %s", show ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_screensaver_set_show_notifications:
 * @saver: A #LiMeScreensaver
 * @show: Whether to show notifications
 *
 * Show/hide notifications on lock screen
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_set_show_notifications(LiMeScreensaver *saver, gboolean show)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->show_notifications = show;
  g_debug("Lock screen notifications %s", show ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_screensaver_set_show_user_image:
 * @saver: A #LiMeScreensaver
 * @show: Whether to show user image
 *
 * Show/hide user image on lock screen
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_set_show_user_image(LiMeScreensaver *saver, gboolean show)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->show_user_image = show;
  g_debug("Lock screen user image %s", show ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_screensaver_set_idle_timeout:
 * @saver: A #LiMeScreensaver
 * @timeout_seconds: Timeout in seconds
 *
 * Set idle timeout before screensaver activates
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_set_idle_timeout(LiMeScreensaver *saver, gint timeout_seconds)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  priv->idle_timeout_seconds = CLAMP(timeout_seconds, 30, 3600);
  g_debug("Idle timeout set to %d seconds", priv->idle_timeout_seconds);

  return TRUE;
}

/**
 * lime_screensaver_get_idle_timeout:
 * @saver: A #LiMeScreensaver
 *
 * Get idle timeout
 *
 * Returns: Timeout in seconds
 */
gint
lime_screensaver_get_idle_timeout(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), 600);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  return priv->idle_timeout_seconds;
}

/**
 * lime_screensaver_save_config:
 * @saver: A #LiMeScreensaver
 *
 * Save screensaver configuration to disk
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_save_config(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  gchar *config_dir = g_build_filename(g_get_home_dir(), SCREENSAVER_CONFIG_DIR, NULL);
  g_mkdir_with_parents(config_dir, 0755);

  gchar *config_path = g_build_filename(config_dir, SCREENSAVER_CONFIG_FILE, NULL);

  GString *config_content = g_string_new("");
  g_string_append(config_content, "[Screensaver]\n");
  g_string_append_printf(config_content, "enabled=%d\n", priv->enabled);
  g_string_append_printf(config_content, "timeout=%d\n", priv->timeout_seconds);
  g_string_append_printf(config_content, "type=%d\n", priv->current_type);
  g_string_append_printf(config_content, "lock_on_screensaver=%d\n", priv->lock_on_screensaver);

  g_string_append(config_content, "\n[LockScreen]\n");
  g_string_append_printf(config_content, "lock_timeout=%d\n", priv->lock_timeout_seconds);
  g_string_append_printf(config_content, "show_clock=%d\n", priv->show_clock);
  g_string_append_printf(config_content, "show_notifications=%d\n", priv->show_notifications);
  g_string_append_printf(config_content, "show_user_image=%d\n", priv->show_user_image);

  g_string_append(config_content, "\n[Idle]\n");
  g_string_append_printf(config_content, "idle_timeout=%d\n", priv->idle_timeout_seconds);

  GError *error = NULL;
  g_file_set_contents(config_path, config_content->str, -1, &error);

  if (error) {
    g_warning("Failed to save screensaver config: %s", error->message);
    g_error_free(error);
    g_free(config_dir);
    g_free(config_path);
    g_string_free(config_content, TRUE);
    return FALSE;
  }

  g_debug("Screensaver configuration saved to %s", config_path);
  g_free(config_dir);
  g_free(config_path);
  g_string_free(config_content, TRUE);

  return TRUE;
}

/**
 * lime_screensaver_load_config:
 * @saver: A #LiMeScreensaver
 *
 * Load screensaver configuration from disk
 *
 * Returns: %TRUE on success
 */
gboolean
lime_screensaver_load_config(LiMeScreensaver *saver)
{
  g_return_val_if_fail(LIME_IS_SCREENSAVER(saver), FALSE);
  LiMeScreensaverPrivate *priv = lime_screensaver_get_instance_private(saver);

  gchar *config_dir = g_build_filename(g_get_home_dir(), SCREENSAVER_CONFIG_DIR, NULL);
  gchar *config_path = g_build_filename(config_dir, SCREENSAVER_CONFIG_FILE, NULL);

  GError *error = NULL;
  gchar *config_content = NULL;

  if (!g_file_get_contents(config_path, &config_content, NULL, &error)) {
    g_debug("No screensaver config found");
    g_free(config_dir);
    g_free(config_path);
    if (error) g_error_free(error);
    return FALSE;
  }

  gchar **lines = g_strsplit(config_content, "\n", -1);
  for (gint i = 0; lines[i]; i++) {
    if (g_str_has_prefix(lines[i], "enabled=")) {
      priv->enabled = atoi(lines[i] + 8);
    } else if (g_str_has_prefix(lines[i], "timeout=")) {
      priv->timeout_seconds = atoi(lines[i] + 8);
    } else if (g_str_has_prefix(lines[i], "lock_on_screensaver=")) {
      priv->lock_on_screensaver = atoi(lines[i] + 20);
    } else if (g_str_has_prefix(lines[i], "show_clock=")) {
      priv->show_clock = atoi(lines[i] + 11);
    } else if (g_str_has_prefix(lines[i], "show_notifications=")) {
      priv->show_notifications = atoi(lines[i] + 19);
    }
  }

  g_strfreev(lines);
  g_free(config_content);
  g_free(config_dir);
  g_free(config_path);

  g_debug("Screensaver configuration loaded");
  return TRUE;
}
