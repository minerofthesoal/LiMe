/*
 * LiMe Software Updater
 * System and package update management with automatic update checking and installation
 * Supports pacman (Arch), AUR, Flatpak, and custom package sources
 */

#ifndef __LIME_SOFTWARE_UPDATER_H__
#define __LIME_SOFTWARE_UPDATER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_SOFTWARE_UPDATER (lime_software_updater_get_type())
#define LIME_SOFTWARE_UPDATER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_SOFTWARE_UPDATER, LiMeSoftwareUpdater))
#define LIME_IS_SOFTWARE_UPDATER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_SOFTWARE_UPDATER))

typedef struct _LiMeSoftwareUpdater LiMeSoftwareUpdater;
typedef struct _LiMeSoftwareUpdaterClass LiMeSoftwareUpdaterClass;

typedef enum {
  LIME_UPDATE_SOURCE_OFFICIAL,
  LIME_UPDATE_SOURCE_AUR,
  LIME_UPDATE_SOURCE_FLATPAK,
  LIME_UPDATE_SOURCE_CUSTOM
} LiMeUpdateSource;

typedef enum {
  LIME_UPDATE_LEVEL_SECURITY,
  LIME_UPDATE_LEVEL_CRITICAL,
  LIME_UPDATE_LEVEL_RECOMMENDED,
  LIME_UPDATE_LEVEL_OPTIONAL
} LiMeUpdateLevel;

typedef enum {
  LIME_UPDATE_STATE_AVAILABLE,
  LIME_UPDATE_STATE_DOWNLOADING,
  LIME_UPDATE_STATE_DOWNLOADED,
  LIME_UPDATE_STATE_INSTALLING,
  LIME_UPDATE_STATE_INSTALLED,
  LIME_UPDATE_STATE_FAILED,
  LIME_UPDATE_STATE_CANCELLED
} LiMeUpdateState;

typedef enum {
  LIME_UPDATE_CHECK_MANUAL,
  LIME_UPDATE_CHECK_DAILY,
  LIME_UPDATE_CHECK_WEEKLY,
  LIME_UPDATE_CHECK_MONTHLY,
  LIME_UPDATE_CHECK_DISABLED
} LiMeUpdateCheckFrequency;

typedef struct {
  gchar *package_name;
  gchar *current_version;
  gchar *available_version;
  gchar *description;
  LiMeUpdateSource source;
  LiMeUpdateLevel update_level;
  gchar *changelog;
  guint64 package_size;
  gboolean requires_restart;
  time_t release_date;
} LiMeUpdateInfo;

typedef struct {
  gchar *update_id;
  GList *packages;
  gint total_package_count;
  gint downloaded_package_count;
  guint64 total_size;
  guint64 downloaded_size;
  LiMeUpdateState state;
  gdouble progress;
  gchar *error_message;
} LiMeUpdateGroup;

typedef struct {
  gboolean auto_check_enabled;
  LiMeUpdateCheckFrequency check_frequency;
  gboolean auto_install_security;
  gboolean auto_install_critical;
  gboolean notify_available_updates;
  gint64 last_check_time;
  gint64 next_check_time;
  gboolean update_while_idle;
  gboolean reboot_required;
} LiMeUpdateSettings;

GType lime_software_updater_get_type(void);

LiMeSoftwareUpdater * lime_software_updater_new(void);

/* Update checking */
gboolean lime_software_updater_check_for_updates(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_check_for_updates_async(LiMeSoftwareUpdater *updater);
GList * lime_software_updater_get_available_updates(LiMeSoftwareUpdater *updater);
gint lime_software_updater_get_update_count(LiMeSoftwareUpdater *updater);
gint lime_software_updater_get_security_update_count(LiMeSoftwareUpdater *updater);

/* Update installation */
gboolean lime_software_updater_install_update(LiMeSoftwareUpdater *updater,
                                              LiMeUpdateInfo *update);
gboolean lime_software_updater_install_all_updates(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_install_security_updates(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_cancel_update(LiMeSoftwareUpdater *updater,
                                             const gchar *update_id);

/* Update information */
LiMeUpdateInfo * lime_software_updater_get_update_info(LiMeSoftwareUpdater *updater,
                                                       const gchar *package_name);
GList * lime_software_updater_get_updates_by_level(LiMeSoftwareUpdater *updater,
                                                    LiMeUpdateLevel level);
GList * lime_software_updater_get_updates_by_source(LiMeSoftwareUpdater *updater,
                                                     LiMeUpdateSource source);

/* Installation tracking */
gdouble lime_software_updater_get_installation_progress(LiMeSoftwareUpdater *updater);
LiMeUpdateState lime_software_updater_get_update_state(LiMeSoftwareUpdater *updater,
                                                       const gchar *update_id);
gchar * lime_software_updater_get_update_status_message(LiMeSoftwareUpdater *updater);

/* Settings */
LiMeUpdateSettings * lime_software_updater_get_settings(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_set_auto_check(LiMeSoftwareUpdater *updater,
                                              gboolean enabled);
gboolean lime_software_updater_set_check_frequency(LiMeSoftwareUpdater *updater,
                                                    LiMeUpdateCheckFrequency frequency);
gboolean lime_software_updater_set_auto_install_security(LiMeSoftwareUpdater *updater,
                                                         gboolean enabled);
gboolean lime_software_updater_set_auto_install_critical(LiMeSoftwareUpdater *updater,
                                                          gboolean enabled);
gboolean lime_software_updater_set_notifications(LiMeSoftwareUpdater *updater,
                                                 gboolean enabled);

/* Scheduled updates */
gboolean lime_software_updater_schedule_update(LiMeSoftwareUpdater *updater,
                                               const gchar *update_id,
                                               time_t when);
gboolean lime_software_updater_reschedule_update(LiMeSoftwareUpdater *updater,
                                                 const gchar *update_id,
                                                 time_t new_time);

/* Rollback */
gboolean lime_software_updater_has_rollback_available(LiMeSoftwareUpdater *updater,
                                                      const gchar *package_name);
gboolean lime_software_updater_rollback_update(LiMeSoftwareUpdater *updater,
                                               const gchar *package_name);

/* System information */
gint64 lime_software_updater_get_last_check_time(LiMeSoftwareUpdater *updater);
gint64 lime_software_updater_get_next_check_time(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_is_checking(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_is_installing(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_reboot_required(LiMeSoftwareUpdater *updater);

/* Repository management */
GList * lime_software_updater_get_known_repositories(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_add_custom_repository(LiMeSoftwareUpdater *updater,
                                                     const gchar *repo_url,
                                                     const gchar *repo_name);

/* Cache and cleanup */
gboolean lime_software_updater_clear_cache(LiMeSoftwareUpdater *updater);
guint64 lime_software_updater_get_cache_size(LiMeSoftwareUpdater *updater);

/* Configuration */
gboolean lime_software_updater_save_config(LiMeSoftwareUpdater *updater);
gboolean lime_software_updater_load_config(LiMeSoftwareUpdater *updater);

G_END_DECLS

#endif /* __LIME_SOFTWARE_UPDATER_H__ */
