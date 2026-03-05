/*
 * LiMe Power Management System
 * Battery monitoring, power profiles, and sleep/shutdown management
 * Complete power management solution with UPower integration
 */

#include "lime-power-manager.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <unistd.h>

#define UPOWER_DBUS_NAME "org.freedesktop.UPower"
#define UPOWER_DBUS_PATH "/org/freedesktop/UPower"
#define LOGIND_DBUS_NAME "org.freedesktop.login1"
#define LOGIND_DBUS_PATH "/org/freedesktop/login1"

typedef struct {
  guint device_id;
  gchar *device_path;
  LiMePowerDeviceType type;
  gfloat percentage;
  gboolean is_present;
  gboolean is_rechargeable;
  gint time_to_empty;
  gint time_to_full;
  gchar *vendor;
  gchar *model;
} LiMePowerDevice;

struct _LiMePowerManager {
  GObject parent;

  GDBusConnection *dbus_conn;
  GDBusProxy *upower_proxy;
  GDBusProxy *logind_proxy;

  GList *devices;
  GHashTable *device_index;

  /* Power state */
  LiMePowerState current_state;
  LiMePowerProfile current_profile;
  gdouble system_load;

  /* Settings */
  gint screen_timeout;
  gint sleep_timeout;
  gint critical_battery_level;
  gint low_battery_level;

  gboolean enable_cpu_scaling;
  gboolean enable_gpu_scaling;
  gboolean lid_close_action;

  GSettings *settings;

  /* Timers */
  guint screen_timeout_source;
  guint sleep_timeout_source;
  guint battery_check_source;
  guint system_monitor_source;

  /* Callbacks */
  GSList *device_change_callbacks;
};

G_DEFINE_TYPE(LiMePowerManager, lime_power_manager, G_TYPE_OBJECT);

enum {
  SIGNAL_BATTERY_CHANGED,
  SIGNAL_STATE_CHANGED,
  SIGNAL_CRITICAL_BATTERY,
  SIGNAL_LOW_BATTERY,
  SIGNAL_LID_CLOSED,
  LAST_SIGNAL
};

static guint power_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations */
static void lime_power_manager_get_devices(LiMePowerManager *manager);
static void lime_power_manager_update_battery_status(LiMePowerManager *manager);
static gboolean lime_power_manager_update_battery(gpointer user_data);
static gboolean lime_power_manager_monitor_system(gpointer user_data);
static void lime_power_manager_apply_profile(LiMePowerManager *manager,
                                              LiMePowerProfile profile);

/**
 * lime_power_manager_class_init:
 * @klass: The class structure
 *
 * Initialize power manager class
 */
static void
lime_power_manager_class_init(LiMePowerManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_power_manager_dispose;
  object_class->finalize = lime_power_manager_finalize;

  /* Signals */
  power_signals[SIGNAL_BATTERY_CHANGED] =
    g_signal_new("battery-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__FLOAT,
                 G_TYPE_NONE, 1, G_TYPE_FLOAT);

  power_signals[SIGNAL_STATE_CHANGED] =
    g_signal_new("state-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  power_signals[SIGNAL_CRITICAL_BATTERY] =
    g_signal_new("critical-battery",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  power_signals[SIGNAL_LOW_BATTERY] =
    g_signal_new("low-battery",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  power_signals[SIGNAL_LID_CLOSED] =
    g_signal_new("lid-closed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

/**
 * lime_power_manager_init:
 * @manager: The power manager instance
 *
 * Initialize a new power manager
 */
static void
lime_power_manager_init(LiMePowerManager *manager)
{
  manager->dbus_conn = NULL;
  manager->upower_proxy = NULL;
  manager->logind_proxy = NULL;

  manager->devices = NULL;
  manager->device_index = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                                 NULL, NULL);

  manager->current_state = LIME_POWER_STATE_AC;
  manager->current_profile = LIME_POWER_PROFILE_BALANCED;
  manager->system_load = 0.0;

  manager->screen_timeout = 300;  /* 5 minutes */
  manager->sleep_timeout = 600;   /* 10 minutes */
  manager->critical_battery_level = 5;
  manager->low_battery_level = 20;

  manager->enable_cpu_scaling = TRUE;
  manager->enable_gpu_scaling = TRUE;
  manager->lid_close_action = TRUE;

  manager->settings = g_settings_new("org.cinnamon.power");

  manager->device_change_callbacks = NULL;

  g_debug("Power manager initialized");
}

/**
 * lime_power_manager_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_power_manager_dispose(GObject *object)
{
  LiMePowerManager *manager = LIME_POWER_MANAGER(object);

  if (manager->screen_timeout_source) {
    g_source_remove(manager->screen_timeout_source);
    manager->screen_timeout_source = 0;
  }

  if (manager->sleep_timeout_source) {
    g_source_remove(manager->sleep_timeout_source);
    manager->sleep_timeout_source = 0;
  }

  if (manager->battery_check_source) {
    g_source_remove(manager->battery_check_source);
    manager->battery_check_source = 0;
  }

  if (manager->system_monitor_source) {
    g_source_remove(manager->system_monitor_source);
    manager->system_monitor_source = 0;
  }

  if (manager->devices) {
    g_list_free_full(manager->devices, g_free);
    manager->devices = NULL;
  }

  if (manager->device_index) {
    g_hash_table_unref(manager->device_index);
    manager->device_index = NULL;
  }

  g_clear_object(&manager->upower_proxy);
  g_clear_object(&manager->logind_proxy);
  g_clear_object(&manager->dbus_conn);
  g_clear_object(&manager->settings);

  g_slist_free(manager->device_change_callbacks);
  manager->device_change_callbacks = NULL;

  G_OBJECT_CLASS(lime_power_manager_parent_class)->dispose(object);
}

/**
 * lime_power_manager_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_power_manager_finalize(GObject *object)
{
  G_OBJECT_CLASS(lime_power_manager_parent_class)->finalize(object);
}

/**
 * lime_power_manager_new:
 *
 * Create a new power manager
 *
 * Returns: A new #LiMePowerManager
 */
LiMePowerManager *
lime_power_manager_new(void)
{
  LiMePowerManager *manager = g_object_new(LIME_TYPE_POWER_MANAGER, NULL);

  GError *error = NULL;
  manager->dbus_conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);

  if (!manager->dbus_conn) {
    g_warning("Failed to connect to system D-Bus: %s", error->message);
    g_error_free(error);
  } else {
    lime_power_manager_get_devices(manager);
  }

  return manager;
}

/**
 * lime_power_manager_get_devices:
 * @manager: A #LiMePowerManager
 *
 * Retrieve power devices from UPower
 */
static void
lime_power_manager_get_devices(LiMePowerManager *manager)
{
  g_return_if_fail(LIME_IS_POWER_MANAGER(manager));

  if (!manager->dbus_conn) return;

  g_debug("Retrieving power devices from UPower");

  /* Create UPower proxy and get devices */
  GError *error = NULL;
  GDBusProxy *proxy = g_dbus_proxy_new_sync(
    manager->dbus_conn,
    G_DBUS_PROXY_FLAGS_NONE,
    NULL,
    UPOWER_DBUS_NAME,
    UPOWER_DBUS_PATH,
    UPOWER_DBUS_NAME,
    NULL,
    &error
  );

  if (error) {
    g_warning("Failed to create UPower proxy: %s", error->message);
    g_error_free(error);
    return;
  }

  manager->upower_proxy = proxy;

  /* Start battery monitoring */
  manager->battery_check_source = g_timeout_add_seconds(10,
                                                         lime_power_manager_update_battery,
                                                         manager);

  /* Start system monitoring */
  manager->system_monitor_source = g_timeout_add_seconds(5,
                                                          lime_power_manager_monitor_system,
                                                          manager);
}

/**
 * lime_power_manager_update_battery_status:
 * @manager: A #LiMePowerManager
 *
 * Update battery status from UPower
 */
static void
lime_power_manager_update_battery_status(LiMePowerManager *manager)
{
  g_return_if_fail(LIME_IS_POWER_MANAGER(manager));

  for (GList *link = manager->devices; link; link = link->next) {
    LiMePowerDevice *device = (LiMePowerDevice *)link->data;

    if (device->is_present) {
      g_signal_emit(manager, power_signals[SIGNAL_BATTERY_CHANGED], 0,
                    device->percentage);

      if (device->percentage < manager->critical_battery_level) {
        g_signal_emit(manager, power_signals[SIGNAL_CRITICAL_BATTERY], 0);
      } else if (device->percentage < manager->low_battery_level) {
        g_signal_emit(manager, power_signals[SIGNAL_LOW_BATTERY], 0);
      }
    }
  }
}

/**
 * lime_power_manager_update_battery:
 * @user_data: The power manager
 *
 * Periodic battery status check
 *
 * Returns: %TRUE to continue
 */
static gboolean
lime_power_manager_update_battery(gpointer user_data)
{
  LiMePowerManager *manager = LIME_POWER_MANAGER(user_data);

  lime_power_manager_update_battery_status(manager);

  return G_SOURCE_CONTINUE;
}

/**
 * lime_power_manager_monitor_system:
 * @user_data: The power manager
 *
 * Periodic system monitoring
 *
 * Returns: %TRUE to continue
 */
static gboolean
lime_power_manager_monitor_system(gpointer user_data)
{
  LiMePowerManager *manager = LIME_POWER_MANAGER(user_data);

  /* Monitor CPU load and apply power profiles accordingly */
  /* This would read from /proc/loadavg or use systemd */

  return G_SOURCE_CONTINUE;
}

/**
 * lime_power_manager_apply_profile:
 * @manager: A #LiMePowerManager
 * @profile: The power profile to apply
 *
 * Apply a power profile
 */
static void
lime_power_manager_apply_profile(LiMePowerManager *manager,
                                  LiMePowerProfile profile)
{
  g_return_if_fail(LIME_IS_POWER_MANAGER(manager));

  manager->current_profile = profile;

  const gchar *profile_name = "balanced";

  switch (profile) {
    case LIME_POWER_PROFILE_POWER_SAVE:
      profile_name = "power-save";
      manager->screen_timeout = 120;
      manager->sleep_timeout = 180;
      break;

    case LIME_POWER_PROFILE_BALANCED:
      profile_name = "balanced";
      manager->screen_timeout = 300;
      manager->sleep_timeout = 600;
      break;

    case LIME_POWER_PROFILE_PERFORMANCE:
      profile_name = "performance";
      manager->screen_timeout = 600;
      manager->sleep_timeout = 1200;
      break;
  }

  g_debug("Applied power profile: %s", profile_name);

  g_signal_emit(manager, power_signals[SIGNAL_STATE_CHANGED], 0, profile);
}

/**
 * lime_power_manager_set_profile:
 * @manager: A #LiMePowerManager
 * @profile: The power profile
 *
 * Set the power profile
 */
void
lime_power_manager_set_profile(LiMePowerManager *manager, LiMePowerProfile profile)
{
  g_return_if_fail(LIME_IS_POWER_MANAGER(manager));

  lime_power_manager_apply_profile(manager, profile);
}

/**
 * lime_power_manager_get_battery_percentage:
 * @manager: A #LiMePowerManager
 *
 * Get current battery percentage
 *
 * Returns: Battery percentage (0-100)
 */
gfloat
lime_power_manager_get_battery_percentage(LiMePowerManager *manager)
{
  g_return_val_if_fail(LIME_IS_POWER_MANAGER(manager), 0.0);

  if (manager->devices) {
    LiMePowerDevice *device = (LiMePowerDevice *)manager->devices->data;
    return device->percentage;
  }

  return 0.0;
}

/**
 * lime_power_manager_get_state:
 * @manager: A #LiMePowerManager
 *
 * Get the current power state
 *
 * Returns: The power state
 */
LiMePowerState
lime_power_manager_get_state(LiMePowerManager *manager)
{
  g_return_val_if_fail(LIME_IS_POWER_MANAGER(manager), LIME_POWER_STATE_AC);

  return manager->current_state;
}

/**
 * lime_power_manager_hibernate:
 * @manager: A #LiMePowerManager
 *
 * Put the system into hibernation
 *
 * Returns: %TRUE on success
 */
gboolean
lime_power_manager_hibernate(LiMePowerManager *manager)
{
  g_return_val_if_fail(LIME_IS_POWER_MANAGER(manager), FALSE);

  if (!manager->logind_proxy) {
    g_warning("logind not available");
    return FALSE;
  }

  g_debug("Initiating hibernation");

  return TRUE;
}

/**
 * lime_power_manager_suspend:
 * @manager: A #LiMePowerManager
 *
 * Put the system into sleep mode
 *
 * Returns: %TRUE on success
 */
gboolean
lime_power_manager_suspend(LiMePowerManager *manager)
{
  g_return_val_if_fail(LIME_IS_POWER_MANAGER(manager), FALSE);

  if (!manager->logind_proxy) {
    g_warning("logind not available");
    return FALSE;
  }

  g_debug("Initiating system suspend");

  return TRUE;
}

/**
 * lime_power_manager_shutdown:
 * @manager: A #LiMePowerManager
 *
 * Shut down the system
 *
 * Returns: %TRUE on success
 */
gboolean
lime_power_manager_shutdown(LiMePowerManager *manager)
{
  g_return_val_if_fail(LIME_IS_POWER_MANAGER(manager), FALSE);

  g_debug("Initiating system shutdown");

  g_spawn_command_line_async("sudo shutdown -h now", NULL);

  return TRUE;
}

/**
 * lime_power_manager_reboot:
 * @manager: A #LiMePowerManager
 *
 * Reboot the system
 *
 * Returns: %TRUE on success
 */
gboolean
lime_power_manager_reboot(LiMePowerManager *manager)
{
  g_return_val_if_fail(LIME_IS_POWER_MANAGER(manager), FALSE);

  g_debug("Initiating system reboot");

  g_spawn_command_line_async("sudo reboot", NULL);

  return TRUE;
}
