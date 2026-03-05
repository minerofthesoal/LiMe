/*
 * LiMe Bluetooth Manager
 * Bluetooth device discovery, pairing, connection, and management
 * D-Bus integration with BlueZ for system Bluetooth control
 */

#ifndef __LIME_BLUETOOTH_MANAGER_H__
#define __LIME_BLUETOOTH_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_BLUETOOTH_MANAGER (lime_bluetooth_manager_get_type())
#define LIME_BLUETOOTH_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_BLUETOOTH_MANAGER, LiMeBluetoothManager))
#define LIME_IS_BLUETOOTH_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_BLUETOOTH_MANAGER))

typedef struct _LiMeBluetoothManager LiMeBluetoothManager;
typedef struct _LiMeBluetoothManagerClass LiMeBluetoothManagerClass;

typedef enum {
  LIME_BT_DEVICE_TYPE_UNKNOWN,
  LIME_BT_DEVICE_TYPE_AUDIO,
  LIME_BT_DEVICE_TYPE_HEADPHONES,
  LIME_BT_DEVICE_TYPE_HEADSET,
  LIME_BT_DEVICE_TYPE_KEYBOARD,
  LIME_BT_DEVICE_TYPE_MOUSE,
  LIME_BT_DEVICE_TYPE_TABLET,
  LIME_BT_DEVICE_TYPE_PHONE,
  LIME_BT_DEVICE_TYPE_GAMEPAD,
  LIME_BT_DEVICE_TYPE_SPEAKER
} LiMeBluetoothDeviceType;

typedef enum {
  LIME_BT_ADAPTER_STATE_OFF,
  LIME_BT_ADAPTER_STATE_ON,
  LIME_BT_ADAPTER_STATE_DISCOVERING,
  LIME_BT_ADAPTER_STATE_ERROR
} LiMeBluetoothAdapterState;

typedef enum {
  LIME_BT_DEVICE_STATE_DISCONNECTED,
  LIME_BT_DEVICE_STATE_CONNECTING,
  LIME_BT_DEVICE_STATE_CONNECTED,
  LIME_BT_DEVICE_STATE_PAIRING,
  LIME_BT_DEVICE_STATE_PAIRED,
  LIME_BT_DEVICE_STATE_ERROR
} LiMeBluetoothDeviceState;

typedef struct {
  gchar *device_address;
  gchar *device_name;
  gchar *device_alias;
  LiMeBluetoothDeviceType device_type;
  LiMeBluetoothDeviceState connection_state;
  gint signal_strength;
  gboolean is_trusted;
  gboolean is_paired;
  gboolean is_connected;
  guint battery_level;
  gchar *icon_name;
  gchar *adapter_address;
} LiMeBluetoothDevice;

typedef struct {
  gchar *adapter_address;
  gchar *adapter_name;
  gboolean powered;
  gboolean discovering;
  LiMeBluetoothAdapterState state;
  GList *devices;
  GList *paired_devices;
} LiMeBluetoothAdapter;

GType lime_bluetooth_manager_get_type(void);

LiMeBluetoothManager * lime_bluetooth_manager_new(void);

/* Adapter management */
GList * lime_bluetooth_manager_get_adapters(LiMeBluetoothManager *manager);
LiMeBluetoothAdapter * lime_bluetooth_manager_get_default_adapter(LiMeBluetoothManager *manager);
gboolean lime_bluetooth_manager_set_adapter_powered(LiMeBluetoothManager *manager,
                                                     const gchar *adapter_address,
                                                     gboolean powered);
gboolean lime_bluetooth_manager_is_adapter_powered(LiMeBluetoothManager *manager,
                                                    const gchar *adapter_address);

/* Discovery */
gboolean lime_bluetooth_manager_start_discovery(LiMeBluetoothManager *manager);
gboolean lime_bluetooth_manager_stop_discovery(LiMeBluetoothManager *manager);
gboolean lime_bluetooth_manager_is_discovering(LiMeBluetoothManager *manager);

/* Device management */
GList * lime_bluetooth_manager_get_all_devices(LiMeBluetoothManager *manager);
GList * lime_bluetooth_manager_get_paired_devices(LiMeBluetoothManager *manager);
GList * lime_bluetooth_manager_get_connected_devices(LiMeBluetoothManager *manager);
LiMeBluetoothDevice * lime_bluetooth_manager_get_device(LiMeBluetoothManager *manager,
                                                        const gchar *device_address);

/* Pairing */
gboolean lime_bluetooth_manager_pair_device(LiMeBluetoothManager *manager,
                                            const gchar *device_address);
gboolean lime_bluetooth_manager_unpair_device(LiMeBluetoothManager *manager,
                                              const gchar *device_address);
gboolean lime_bluetooth_manager_is_device_paired(LiMeBluetoothManager *manager,
                                                 const gchar *device_address);
gboolean lime_bluetooth_manager_set_device_trusted(LiMeBluetoothManager *manager,
                                                    const gchar *device_address,
                                                    gboolean trusted);

/* Connection */
gboolean lime_bluetooth_manager_connect_device(LiMeBluetoothManager *manager,
                                               const gchar *device_address);
gboolean lime_bluetooth_manager_disconnect_device(LiMeBluetoothManager *manager,
                                                  const gchar *device_address);
gboolean lime_bluetooth_manager_is_device_connected(LiMeBluetoothManager *manager,
                                                    const gchar *device_address);

/* Device information */
gchar * lime_bluetooth_manager_get_device_name(LiMeBluetoothManager *manager,
                                               const gchar *device_address);
gboolean lime_bluetooth_manager_set_device_alias(LiMeBluetoothManager *manager,
                                                 const gchar *device_address,
                                                 const gchar *alias);
LiMeBluetoothDeviceType lime_bluetooth_manager_get_device_type(LiMeBluetoothManager *manager,
                                                               const gchar *device_address);
gint lime_bluetooth_manager_get_device_signal_strength(LiMeBluetoothManager *manager,
                                                       const gchar *device_address);
guint lime_bluetooth_manager_get_device_battery_level(LiMeBluetoothManager *manager,
                                                      const gchar *device_address);

/* Connection profiles */
gboolean lime_bluetooth_manager_connect_profile(LiMeBluetoothManager *manager,
                                                const gchar *device_address,
                                                const gchar *profile_uuid);
gboolean lime_bluetooth_manager_disconnect_profile(LiMeBluetoothManager *manager,
                                                   const gchar *device_address,
                                                   const gchar *profile_uuid);

/* OBEX file transfer */
gboolean lime_bluetooth_manager_send_file(LiMeBluetoothManager *manager,
                                          const gchar *device_address,
                                          const gchar *file_path);
gboolean lime_bluetooth_manager_receive_files(LiMeBluetoothManager *manager,
                                              const gchar *destination_directory);

/* Visibility and settings */
gboolean lime_bluetooth_manager_set_discoverable(LiMeBluetoothManager *manager,
                                                 gboolean discoverable);
gboolean lime_bluetooth_manager_is_discoverable(LiMeBluetoothManager *manager);
gboolean lime_bluetooth_manager_set_discoverable_timeout(LiMeBluetoothManager *manager,
                                                         guint timeout_seconds);

/* Device removal */
gboolean lime_bluetooth_manager_remove_device(LiMeBluetoothManager *manager,
                                              const gchar *device_address);
gboolean lime_bluetooth_manager_forget_device(LiMeBluetoothManager *manager,
                                              const gchar *device_address);

/* Configuration */
gboolean lime_bluetooth_manager_save_config(LiMeBluetoothManager *manager);
gboolean lime_bluetooth_manager_load_config(LiMeBluetoothManager *manager);

G_END_DECLS

#endif /* __LIME_BLUETOOTH_MANAGER_H__ */
