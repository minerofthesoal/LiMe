/*
 * LiMe Session Manager
 * Session management, desktop startup, user sessions, and idle detection
 * Handles session initialization, application startup, and system state
 */

#ifndef __LIME_SESSION_MANAGER_H__
#define __LIME_SESSION_MANAGER_H__

#include <glib-object.h>
#include <time.h>

G_BEGIN_DECLS

#define LIME_TYPE_SESSION_MANAGER (lime_session_manager_get_type())
#define LIME_SESSION_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_SESSION_MANAGER, LiMeSessionManager))
#define LIME_IS_SESSION_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_SESSION_MANAGER))

typedef struct _LiMeSessionManager LiMeSessionManager;
typedef struct _LiMeSessionManagerClass LiMeSessionManagerClass;

typedef enum {
  LIME_SESSION_STATE_INITIALIZING,
  LIME_SESSION_STATE_RUNNING,
  LIME_SESSION_STATE_QUERY_END,
  LIME_SESSION_STATE_ENDING,
  LIME_SESSION_STATE_ENDED
} LiMeSessionState;

typedef enum {
  LIME_IDLE_STATE_ACTIVE,
  LIME_IDLE_STATE_IDLE,
  LIME_IDLE_STATE_VERY_IDLE
} LiMeIdleState;

typedef enum {
  LIME_STARTUP_PHASE_INITIALIZATION,
  LIME_STARTUP_PHASE_WINDOW_MANAGER,
  LIME_STARTUP_PHASE_PANEL,
  LIME_STARTUP_PHASE_NOTIFICATION,
  LIME_STARTUP_PHASE_APPLICATIONS,
  LIME_STARTUP_PHASE_COMPLETE
} LiMeStartupPhase;

typedef struct {
  gchar *session_name;
  gchar *session_id;
  gchar *user_name;
  gchar *display;
  gchar *xauthority;
  LiMeSessionState session_state;
  time_t session_start_time;
  time_t session_end_time;
  gboolean is_default_session;
} LiMeSessionInfo;

typedef struct {
  gchar *app_id;
  gchar *app_name;
  gchar *app_path;
  gboolean should_start;
  gboolean is_running;
  GPid process_id;
  gchar *autostart_phase;
} LiMeStartupApplication;

typedef struct {
  gchar *inhibition_id;
  gchar *app_name;
  gchar *reason;
  guint flags;
  time_t inhibition_time;
} LiMeInhibition;

GType lime_session_manager_get_type(void);

LiMeSessionManager * lime_session_manager_new(void);

/* Session management */
LiMeSessionInfo * lime_session_manager_get_current_session(LiMeSessionManager *manager);
GList * lime_session_manager_open_sessions(LiMeSessionManager *manager);
gboolean lime_session_manager_is_session_active(LiMeSessionManager *manager);

/* Startup applications */
GList * lime_session_manager_get_startup_applications(LiMeSessionManager *manager);
gboolean lime_session_manager_add_startup_application(LiMeSessionManager *manager,
                                                      const gchar *app_id,
                                                      const gchar *app_path);
gboolean lime_session_manager_remove_startup_application(LiMeSessionManager *manager,
                                                         const gchar *app_id);
gboolean lime_session_manager_enable_startup_application(LiMeSessionManager *manager,
                                                         const gchar *app_id);
gboolean lime_session_manager_disable_startup_application(LiMeSessionManager *manager,
                                                          const gchar *app_id);

/* Idle detection */
LiMeIdleState lime_session_manager_get_idle_state(LiMeSessionManager *manager);
guint lime_session_manager_get_idle_time(LiMeSessionManager *manager);
gboolean lime_session_manager_set_idle_threshold(LiMeSessionManager *manager,
                                                 guint idle_threshold_seconds);

/* Session state */
LiMeSessionState lime_session_manager_get_session_state(LiMeSessionManager *manager);
gchar * lime_session_manager_get_session_name(LiMeSessionManager *manager);
gchar * lime_session_manager_get_session_id(LiMeSessionManager *manager);
gchar * lime_session_manager_get_user_name(LiMeSessionManager *manager);

/* Session save/restore */
gboolean lime_session_manager_save_session(LiMeSessionManager *manager);
gboolean lime_session_manager_restore_session(LiMeSessionManager *manager);

/* System state */
gboolean lime_session_manager_lock_session(LiMeSessionManager *manager);
gboolean lime_session_manager_unlock_session(LiMeSessionManager *manager);
gboolean lime_session_manager_end_session(LiMeSessionManager *manager);
gboolean lime_session_manager_shutdown_system(LiMeSessionManager *manager);
gboolean lime_session_manager_reboot_system(LiMeSessionManager *manager);
gboolean lime_session_manager_suspend_system(LiMeSessionManager *manager);
gboolean lime_session_manager_hibernate_system(LiMeSessionManager *manager);
gboolean lime_session_manager_logout(LiMeSessionManager *manager);

/* Inhibition (for preventing logout/shutdown) */
gchar * lime_session_manager_inhibit(LiMeSessionManager *manager,
                                     const gchar *app_name,
                                     guint flags,
                                     const gchar *reason);
gboolean lime_session_manager_uninhibit(LiMeSessionManager *manager,
                                        const gchar *inhibition_id);
GList * lime_session_manager_get_active_inhibitions(LiMeSessionManager *manager);

/* Startup control */
gboolean lime_session_manager_start_startup_sequence(LiMeSessionManager *manager);
gboolean lime_session_manager_start_phase(LiMeSessionManager *manager,
                                          LiMeStartupPhase phase);
LiMeStartupPhase lime_session_manager_get_current_phase(LiMeSessionManager *manager);
gdouble lime_session_manager_get_startup_progress(LiMeSessionManager *manager);

/* Activity tracking */
gboolean lime_session_manager_user_active(LiMeSessionManager *manager);
time_t lime_session_manager_get_last_activity_time(LiMeSessionManager *manager);

/* Configuration */
gboolean lime_session_manager_save_config(LiMeSessionManager *manager);
gboolean lime_session_manager_load_config(LiMeSessionManager *manager);

G_END_DECLS

#endif /* __LIME_SESSION_MANAGER_H__ */
