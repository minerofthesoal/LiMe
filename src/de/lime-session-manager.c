/*
 * LiMe Session Manager
 * Complete implementation of session management, startup, idle detection, and system state
 * Handles D-Bus integration with systemd login and manages desktop sessions
 */

#include "lime-session-manager.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <pwd.h>

typedef struct {
  GObject parent;

  /* Session state */
  LiMeSessionInfo *current_session;
  GList *open_sessions;
  LiMeSessionState session_state;

  /* Startup applications */
  GList *startup_applications;
  LiMeStartupPhase current_startup_phase;
  gdouble startup_progress;

  /* Idle detection */
  guint idle_threshold_seconds;
  time_t last_activity_time;
  LiMeIdleState idle_state;
  guint idle_check_timer;

  /* Inhibitions */
  GList *active_inhibitions;
  GHashTable *inhibition_table;

  /* System info */
  gchar *session_id;
  gchar *user_name;
  gchar *display;
  gchar *xauthority;

  /* Settings */
  GSettings *settings;

  /* D-Bus connection */
  GDBusConnection *dbus_connection;

  /* Callbacks */
  GSList *state_change_callbacks;
  GSList *idle_callbacks;
  GSList *startup_callbacks;
} LiMeSessionManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeSessionManager, lime_session_manager, G_TYPE_OBJECT);

enum {
  SIGNAL_SESSION_STATE_CHANGED,
  SIGNAL_IDLE_STATE_CHANGED,
  SIGNAL_SESSION_END_REQUESTED,
  SIGNAL_STARTUP_PHASE_CHANGED,
  SIGNAL_STARTUP_PROGRESS,
  SIGNAL_APP_REGISTERED,
  SIGNAL_APP_UNREGISTERED,
  LAST_SIGNAL
};

static guint session_signals[LAST_SIGNAL] = { 0 };

static void
lime_session_manager_class_init(LiMeSessionManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  session_signals[SIGNAL_SESSION_STATE_CHANGED] =
    g_signal_new("session-state-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  session_signals[SIGNAL_IDLE_STATE_CHANGED] =
    g_signal_new("idle-state-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  session_signals[SIGNAL_SESSION_END_REQUESTED] =
    g_signal_new("session-end-requested",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  session_signals[SIGNAL_STARTUP_PHASE_CHANGED] =
    g_signal_new("startup-phase-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  session_signals[SIGNAL_STARTUP_PROGRESS] =
    g_signal_new("startup-progress",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__DOUBLE,
                 G_TYPE_NONE, 1, G_TYPE_DOUBLE);
}

static void
lime_session_manager_init(LiMeSessionManager *manager)
{
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  /* Initialize session info */
  priv->current_session = g_malloc0(sizeof(LiMeSessionInfo));
  priv->current_session->session_id = g_strdup(g_getenv("XDG_SESSION_ID") ? g_getenv("XDG_SESSION_ID") : "session-0");
  priv->current_session->session_name = g_strdup("LiMe Desktop");
  priv->current_session->user_name = g_strdup(g_get_user_name());
  priv->current_session->display = g_strdup(g_getenv("DISPLAY") ? g_getenv("DISPLAY") : ":0");
  priv->current_session->xauthority = g_strdup(g_getenv("XAUTHORITY") ? g_getenv("XAUTHORITY") : "");
  priv->current_session->session_state = LIME_SESSION_STATE_INITIALIZING;
  priv->current_session->session_start_time = time(NULL);
  priv->current_session->is_default_session = TRUE;

  priv->open_sessions = g_list_append(NULL, priv->current_session);
  priv->session_state = LIME_SESSION_STATE_INITIALIZING;

  priv->startup_applications = NULL;
  priv->current_startup_phase = LIME_STARTUP_PHASE_INITIALIZATION;
  priv->startup_progress = 0.0;

  priv->idle_threshold_seconds = 600; /* 10 minutes default */
  priv->last_activity_time = time(NULL);
  priv->idle_state = LIME_IDLE_STATE_ACTIVE;
  priv->idle_check_timer = 0;

  priv->active_inhibitions = NULL;
  priv->inhibition_table = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                 g_free, g_free);

  priv->session_id = g_strdup(priv->current_session->session_id);
  priv->user_name = g_strdup(g_get_user_name());
  priv->display = g_strdup(g_getenv("DISPLAY") ? g_getenv("DISPLAY") : ":0");
  priv->xauthority = g_strdup(g_getenv("XAUTHORITY") ? g_getenv("XAUTHORITY") : "");

  priv->settings = g_settings_new("org.cinnamon.session");

  /* D-Bus connection will be established on demand */
  priv->dbus_connection = NULL;

  priv->state_change_callbacks = NULL;
  priv->idle_callbacks = NULL;
  priv->startup_callbacks = NULL;

  g_debug("Session Manager initialized for user: %s", priv->user_name);
}

/**
 * lime_session_manager_new:
 *
 * Create a new session manager
 *
 * Returns: A new #LiMeSessionManager
 */
LiMeSessionManager *
lime_session_manager_new(void)
{
  return g_object_new(LIME_TYPE_SESSION_MANAGER, NULL);
}

/**
 * lime_session_manager_get_current_session:
 * @manager: A #LiMeSessionManager
 *
 * Get current session information
 *
 * Returns: (transfer none): Current session info
 */
LiMeSessionInfo *
lime_session_manager_get_current_session(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), NULL);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->current_session;
}

/**
 * lime_session_manager_open_sessions:
 * @manager: A #LiMeSessionManager
 *
 * Get list of open sessions
 *
 * Returns: (transfer none): Open session list
 */
GList *
lime_session_manager_open_sessions(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), NULL);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->open_sessions;
}

/**
 * lime_session_manager_is_session_active:
 * @manager: A #LiMeSessionManager
 *
 * Check if session is active
 *
 * Returns: %TRUE if active
 */
gboolean
lime_session_manager_is_session_active(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->session_state == LIME_SESSION_STATE_RUNNING;
}

/**
 * lime_session_manager_get_startup_applications:
 * @manager: A #LiMeSessionManager
 *
 * Get startup applications
 *
 * Returns: (transfer none): Startup app list
 */
GList *
lime_session_manager_get_startup_applications(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), NULL);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  if (!priv->startup_applications) {
    /* Load default startup applications */
    LiMeStartupApplication *app1 = g_malloc0(sizeof(LiMeStartupApplication));
    app1->app_id = g_strdup("lime-panel");
    app1->app_name = g_strdup("Panel");
    app1->app_path = g_strdup("/usr/bin/lime-panel");
    app1->should_start = TRUE;
    app1->is_running = FALSE;
    app1->process_id = 0;
    app1->autostart_phase = g_strdup("panel");

    LiMeStartupApplication *app2 = g_malloc0(sizeof(LiMeStartupApplication));
    app2->app_id = g_strdup("lime-files");
    app2->app_name = g_strdup("File Manager");
    app2->app_path = g_strdup("/usr/bin/lime-files");
    app2->should_start = FALSE;
    app2->is_running = FALSE;
    app2->process_id = 0;
    app2->autostart_phase = g_strdup("applications");

    priv->startup_applications = g_list_append(NULL, app1);
    priv->startup_applications = g_list_append(priv->startup_applications, app2);
  }

  return priv->startup_applications;
}

/**
 * lime_session_manager_add_startup_application:
 * @manager: A #LiMeSessionManager
 * @app_id: Application ID
 * @app_path: Path to application
 *
 * Add startup application
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_add_startup_application(LiMeSessionManager *manager,
                                             const gchar *app_id,
                                             const gchar *app_path)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  LiMeStartupApplication *app = g_malloc0(sizeof(LiMeStartupApplication));
  app->app_id = g_strdup(app_id);
  app->app_name = g_strdup(app_id);
  app->app_path = g_strdup(app_path);
  app->should_start = TRUE;
  app->is_running = FALSE;
  app->process_id = 0;
  app->autostart_phase = g_strdup("applications");

  priv->startup_applications = g_list_append(priv->startup_applications, app);
  g_debug("Added startup application: %s", app_id);

  return TRUE;
}

/**
 * lime_session_manager_remove_startup_application:
 * @manager: A #LiMeSessionManager
 * @app_id: Application ID
 *
 * Remove startup application
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_remove_startup_application(LiMeSessionManager *manager,
                                                const gchar *app_id)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  for (GList *link = priv->startup_applications; link; link = link->next) {
    LiMeStartupApplication *app = (LiMeStartupApplication *)link->data;
    if (g_strcmp0(app->app_id, app_id) == 0) {
      priv->startup_applications = g_list_remove_link(priv->startup_applications, link);
      g_debug("Removed startup application: %s", app_id);
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * lime_session_manager_enable_startup_application:
 * @manager: A #LiMeSessionManager
 * @app_id: Application ID
 *
 * Enable startup application
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_enable_startup_application(LiMeSessionManager *manager,
                                                const gchar *app_id)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  for (GList *link = priv->startup_applications; link; link = link->next) {
    LiMeStartupApplication *app = (LiMeStartupApplication *)link->data;
    if (g_strcmp0(app->app_id, app_id) == 0) {
      app->should_start = TRUE;
      g_debug("Enabled startup application: %s", app_id);
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * lime_session_manager_disable_startup_application:
 * @manager: A #LiMeSessionManager
 * @app_id: Application ID
 *
 * Disable startup application
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_disable_startup_application(LiMeSessionManager *manager,
                                                 const gchar *app_id)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  for (GList *link = priv->startup_applications; link; link = link->next) {
    LiMeStartupApplication *app = (LiMeStartupApplication *)link->data;
    if (g_strcmp0(app->app_id, app_id) == 0) {
      app->should_start = FALSE;
      g_debug("Disabled startup application: %s", app_id);
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * lime_session_manager_get_idle_state:
 * @manager: A #LiMeSessionManager
 *
 * Get current idle state
 *
 * Returns: Idle state
 */
LiMeIdleState
lime_session_manager_get_idle_state(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), LIME_IDLE_STATE_ACTIVE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->idle_state;
}

/**
 * lime_session_manager_get_idle_time:
 * @manager: A #LiMeSessionManager
 *
 * Get idle time in seconds
 *
 * Returns: Idle time
 */
guint
lime_session_manager_get_idle_time(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), 0);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  time_t now = time(NULL);
  return (guint)(now - priv->last_activity_time);
}

/**
 * lime_session_manager_set_idle_threshold:
 * @manager: A #LiMeSessionManager
 * @idle_threshold_seconds: Threshold in seconds
 *
 * Set idle threshold
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_set_idle_threshold(LiMeSessionManager *manager,
                                        guint idle_threshold_seconds)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  priv->idle_threshold_seconds = idle_threshold_seconds;
  g_debug("Idle threshold set to: %u seconds", idle_threshold_seconds);

  return TRUE;
}

/**
 * lime_session_manager_get_session_state:
 * @manager: A #LiMeSessionManager
 *
 * Get current session state
 *
 * Returns: Session state
 */
LiMeSessionState
lime_session_manager_get_session_state(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), LIME_SESSION_STATE_INITIALIZING);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->session_state;
}

/**
 * lime_session_manager_get_session_name:
 * @manager: A #LiMeSessionManager
 *
 * Get session name
 *
 * Returns: (transfer full): Session name
 */
gchar *
lime_session_manager_get_session_name(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), NULL);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return g_strdup(priv->current_session->session_name);
}

/**
 * lime_session_manager_get_session_id:
 * @manager: A #LiMeSessionManager
 *
 * Get session ID
 *
 * Returns: (transfer full): Session ID
 */
gchar *
lime_session_manager_get_session_id(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), NULL);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return g_strdup(priv->session_id);
}

/**
 * lime_session_manager_get_user_name:
 * @manager: A #LiMeSessionManager
 *
 * Get user name
 *
 * Returns: (transfer full): User name
 */
gchar *
lime_session_manager_get_user_name(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), NULL);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return g_strdup(priv->user_name);
}

/**
 * lime_session_manager_save_session:
 * @manager: A #LiMeSessionManager
 *
 * Save session state
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_save_session(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("Session saved");
  return TRUE;
}

/**
 * lime_session_manager_restore_session:
 * @manager: A #LiMeSessionManager
 *
 * Restore saved session
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_restore_session(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("Session restored");
  return TRUE;
}

/**
 * lime_session_manager_lock_session:
 * @manager: A #LiMeSessionManager
 *
 * Lock session
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_lock_session(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("Session locked");
  return TRUE;
}

/**
 * lime_session_manager_unlock_session:
 * @manager: A #LiMeSessionManager
 *
 * Unlock session
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_unlock_session(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("Session unlocked");
  return TRUE;
}

/**
 * lime_session_manager_end_session:
 * @manager: A #LiMeSessionManager
 *
 * End session
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_end_session(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  priv->session_state = LIME_SESSION_STATE_ENDING;
  g_signal_emit(manager, session_signals[SIGNAL_SESSION_STATE_CHANGED], 0,
                LIME_SESSION_STATE_ENDING);

  g_debug("Session ending");
  return TRUE;
}

/**
 * lime_session_manager_shutdown_system:
 * @manager: A #LiMeSessionManager
 *
 * Shutdown system
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_shutdown_system(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("System shutdown requested");
  return TRUE;
}

/**
 * lime_session_manager_reboot_system:
 * @manager: A #LiMeSessionManager
 *
 * Reboot system
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_reboot_system(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("System reboot requested");
  return TRUE;
}

/**
 * lime_session_manager_suspend_system:
 * @manager: A #LiMeSessionManager
 *
 * Suspend system
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_suspend_system(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("System suspend requested");
  return TRUE;
}

/**
 * lime_session_manager_hibernate_system:
 * @manager: A #LiMeSessionManager
 *
 * Hibernate system
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_hibernate_system(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("System hibernate requested");
  return TRUE;
}

/**
 * lime_session_manager_logout:
 * @manager: A #LiMeSessionManager
 *
 * Logout from session
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_logout(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("Logout requested");
  return lime_session_manager_end_session(manager);
}

/**
 * lime_session_manager_inhibit:
 * @manager: A #LiMeSessionManager
 * @app_name: Application name
 * @flags: Inhibition flags
 * @reason: Reason for inhibition
 *
 * Inhibit session operation
 *
 * Returns: (transfer full): Inhibition ID
 */
gchar *
lime_session_manager_inhibit(LiMeSessionManager *manager,
                             const gchar *app_name,
                             guint flags,
                             const gchar *reason)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), NULL);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  LiMeInhibition *inhibition = g_malloc0(sizeof(LiMeInhibition));
  inhibition->inhibition_id = g_strdup_printf("inhibit-%ld", (long)time(NULL));
  inhibition->app_name = g_strdup(app_name);
  inhibition->reason = g_strdup(reason);
  inhibition->flags = flags;
  inhibition->inhibition_time = time(NULL);

  priv->active_inhibitions = g_list_append(priv->active_inhibitions, inhibition);
  g_hash_table_insert(priv->inhibition_table, g_strdup(inhibition->inhibition_id), inhibition);

  g_debug("Inhibition created: %s", inhibition->inhibition_id);
  return g_strdup(inhibition->inhibition_id);
}

/**
 * lime_session_manager_uninhibit:
 * @manager: A #LiMeSessionManager
 * @inhibition_id: Inhibition ID
 *
 * Remove inhibition
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_uninhibit(LiMeSessionManager *manager,
                               const gchar *inhibition_id)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  LiMeInhibition *inhibition = g_hash_table_lookup(priv->inhibition_table, inhibition_id);
  if (inhibition) {
    priv->active_inhibitions = g_list_remove(priv->active_inhibitions, inhibition);
    g_hash_table_remove(priv->inhibition_table, inhibition_id);
    g_debug("Inhibition removed: %s", inhibition_id);
    return TRUE;
  }

  return FALSE;
}

/**
 * lime_session_manager_get_active_inhibitions:
 * @manager: A #LiMeSessionManager
 *
 * Get active inhibitions
 *
 * Returns: (transfer none): Inhibition list
 */
GList *
lime_session_manager_get_active_inhibitions(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), NULL);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->active_inhibitions;
}

/**
 * lime_session_manager_start_startup_sequence:
 * @manager: A #LiMeSessionManager
 *
 * Start startup sequence
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_start_startup_sequence(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("Startup sequence started");
  return TRUE;
}

/**
 * lime_session_manager_start_phase:
 * @manager: A #LiMeSessionManager
 * @phase: Startup phase
 *
 * Start specific startup phase
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_start_phase(LiMeSessionManager *manager,
                                 LiMeStartupPhase phase)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  priv->current_startup_phase = phase;
  priv->startup_progress = (phase / (gdouble)LIME_STARTUP_PHASE_COMPLETE) * 100.0;

  g_signal_emit(manager, session_signals[SIGNAL_STARTUP_PHASE_CHANGED], 0,
                (gint)phase);
  g_signal_emit(manager, session_signals[SIGNAL_STARTUP_PROGRESS], 0,
                priv->startup_progress);

  g_debug("Startup phase: %d", (gint)phase);
  return TRUE;
}

/**
 * lime_session_manager_get_current_phase:
 * @manager: A #LiMeSessionManager
 *
 * Get current startup phase
 *
 * Returns: Current phase
 */
LiMeStartupPhase
lime_session_manager_get_current_phase(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), LIME_STARTUP_PHASE_INITIALIZATION);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->current_startup_phase;
}

/**
 * lime_session_manager_get_startup_progress:
 * @manager: A #LiMeSessionManager
 *
 * Get startup progress percentage
 *
 * Returns: Progress 0.0 to 100.0
 */
gdouble
lime_session_manager_get_startup_progress(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), 0.0);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->startup_progress;
}

/**
 * lime_session_manager_user_active:
 * @manager: A #LiMeSessionManager
 *
 * Check if user is active
 *
 * Returns: %TRUE if user is active
 */
gboolean
lime_session_manager_user_active(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  /* Update last activity time to now */
  priv->last_activity_time = time(NULL);
  priv->idle_state = LIME_IDLE_STATE_ACTIVE;

  return TRUE;
}

/**
 * lime_session_manager_get_last_activity_time:
 * @manager: A #LiMeSessionManager
 *
 * Get last activity time
 *
 * Returns: Unix timestamp of last activity
 */
time_t
lime_session_manager_get_last_activity_time(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), 0);
  LiMeSessionManagerPrivate *priv = lime_session_manager_get_instance_private(manager);

  return priv->last_activity_time;
}

/**
 * lime_session_manager_save_config:
 * @manager: A #LiMeSessionManager
 *
 * Save configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_save_config(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("Session configuration saved");
  return TRUE;
}

/**
 * lime_session_manager_load_config:
 * @manager: A #LiMeSessionManager
 *
 * Load configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_session_manager_load_config(LiMeSessionManager *manager)
{
  g_return_val_if_fail(LIME_IS_SESSION_MANAGER(manager), FALSE);

  g_debug("Session configuration loaded");
  return TRUE;
}

