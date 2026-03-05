/*
 * LiMe Input Settings Header
 * Input device configuration and keyboard layout management
 */

#ifndef __LIME_INPUT_SETTINGS_H__
#define __LIME_INPUT_SETTINGS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_INPUT_SETTINGS (lime_input_settings_get_type())
#define LIME_INPUT_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_INPUT_SETTINGS, LiMeInputSettings))
#define LIME_IS_INPUT_SETTINGS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_INPUT_SETTINGS))

typedef struct _LiMeInputSettings LiMeInputSettings;
typedef struct _LiMeInputSettingsClass LiMeInputSettingsClass;

typedef enum {
  LIME_INPUT_DEVICE_MOUSE,
  LIME_INPUT_DEVICE_TOUCHPAD,
  LIME_INPUT_DEVICE_KEYBOARD,
  LIME_INPUT_DEVICE_JOYSTICK,
  LIME_INPUT_DEVICE_UNKNOWN
} LiMeInputDeviceType;

typedef struct {
  gchar *device_id;
  gchar *device_name;
  LiMeInputDeviceType device_type;
  gboolean enabled;
  gdouble sensitivity;
  gboolean natural_scroll;
  gboolean tap_to_click;
  gboolean two_finger_scroll;
} LiMeInputDevice;

typedef struct {
  gchar *layout_name;
  gchar *layout_code;
  gchar *variant;
  gboolean is_active;
} LiMeKeyboardLayout;

GType lime_input_settings_get_type(void);

LiMeInputSettings * lime_input_settings_new(void);

/* Input device management */
GList * lime_input_settings_get_devices(LiMeInputSettings *settings);
LiMeInputDevice * lime_input_settings_get_device(LiMeInputSettings *settings,
                                                  const gchar *device_id);
gboolean lime_input_settings_enable_device(LiMeInputSettings *settings,
                                            const gchar *device_id,
                                            gboolean enabled);
gboolean lime_input_settings_set_device_sensitivity(LiMeInputSettings *settings,
                                                     const gchar *device_id,
                                                     gdouble sensitivity);

/* Mouse settings */
gboolean lime_input_settings_set_mouse_sensitivity(LiMeInputSettings *settings, gdouble sensitivity);
gdouble lime_input_settings_get_mouse_sensitivity(LiMeInputSettings *settings);
gboolean lime_input_settings_set_mouse_acceleration(LiMeInputSettings *settings, gdouble accel);
gboolean lime_input_settings_set_mouse_button_swap(LiMeInputSettings *settings, gboolean swap);
gboolean lime_input_settings_get_mouse_button_swap(LiMeInputSettings *settings);

/* Touchpad settings */
gboolean lime_input_settings_enable_touchpad(LiMeInputSettings *settings, gboolean enabled);
gboolean lime_input_settings_set_touchpad_sensitivity(LiMeInputSettings *settings,
                                                       gdouble sensitivity);
gboolean lime_input_settings_set_tap_to_click(LiMeInputSettings *settings, gboolean enabled);
gboolean lime_input_settings_set_two_finger_scroll(LiMeInputSettings *settings, gboolean enabled);
gboolean lime_input_settings_set_natural_scroll(LiMeInputSettings *settings, gboolean enabled);
gboolean lime_input_settings_set_edge_scrolling(LiMeInputSettings *settings, gboolean enabled);

/* Keyboard layout management */
GList * lime_input_settings_get_keyboard_layouts(LiMeInputSettings *settings);
gboolean lime_input_settings_add_keyboard_layout(LiMeInputSettings *settings,
                                                  const gchar *layout_code,
                                                  const gchar *variant);
gboolean lime_input_settings_remove_keyboard_layout(LiMeInputSettings *settings,
                                                     const gchar *layout_code);
gboolean lime_input_settings_set_active_layout(LiMeInputSettings *settings,
                                                const gchar *layout_code);
gchar * lime_input_settings_get_active_layout(LiMeInputSettings *settings);

/* Keyboard settings */
gboolean lime_input_settings_set_key_repeat_delay(LiMeInputSettings *settings, gint delay);
gboolean lime_input_settings_set_key_repeat_interval(LiMeInputSettings *settings, gint interval);
gboolean lime_input_settings_set_keyboard_volume_keys(LiMeInputSettings *settings,
                                                       gboolean enabled);
gboolean lime_input_settings_set_caps_lock_behavior(LiMeInputSettings *settings,
                                                     const gchar *behavior);

/* Keyboard shortcuts */
gboolean lime_input_settings_bind_shortcut(LiMeInputSettings *settings,
                                            const gchar *action,
                                            const gchar *shortcut);
gchar * lime_input_settings_get_shortcut(LiMeInputSettings *settings, const gchar *action);
gboolean lime_input_settings_unbind_shortcut(LiMeInputSettings *settings, const gchar *action);

/* Input Method Editor (IME) support */
gboolean lime_input_settings_set_input_method(LiMeInputSettings *settings,
                                               const gchar *input_method);
gchar * lime_input_settings_get_input_method(LiMeInputSettings *settings);
GList * lime_input_settings_get_available_input_methods(LiMeInputSettings *settings);

/* Configuration save/load */
gboolean lime_input_settings_save_config(LiMeInputSettings *settings);
gboolean lime_input_settings_load_config(LiMeInputSettings *settings);

G_END_DECLS

#endif /* __LIME_INPUT_SETTINGS_H__ */
