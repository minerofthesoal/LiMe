/*
 * LiMe Input Settings
 * Input device configuration and keyboard layout management
 * Handles keyboard layouts, mouse/touchpad settings, and input method editing
 */

#include "lime-input-settings.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>

#define INPUT_CONFIG_DIR ".config/lime/input"
#define INPUT_CONFIG_FILE "input.conf"
#define SHORTCUTS_CONFIG_FILE "shortcuts.conf"
#define LAYOUTS_CONFIG_FILE "layouts.conf"

typedef struct {
  GObject parent;

  GList *input_devices;
  GList *keyboard_layouts;
  GHashTable *keyboard_shortcuts;

  GSettings *mouse_settings;
  GSettings *keyboard_settings;

  /* Mouse settings */
  gdouble mouse_sensitivity;
  gdouble mouse_acceleration;
  gboolean button_swap;

  /* Touchpad settings */
  gboolean touchpad_enabled;
  gdouble touchpad_sensitivity;
  gboolean tap_to_click;
  gboolean two_finger_scroll;
  gboolean natural_scroll;
  gboolean edge_scrolling;

  /* Keyboard settings */
  gint key_repeat_delay;
  gint key_repeat_interval;
  gboolean volume_keys_enabled;
  gchar *caps_lock_behavior;

  /* Input method */
  gchar *active_input_method;
  GList *available_input_methods;
} LiMeInputSettingsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeInputSettings, lime_input_settings, G_TYPE_OBJECT);

enum {
  SIGNAL_DEVICE_ADDED,
  SIGNAL_DEVICE_REMOVED,
  SIGNAL_LAYOUT_CHANGED,
  SIGNAL_DEVICE_SETTINGS_CHANGED,
  LAST_SIGNAL
};

static guint input_signals[LAST_SIGNAL] = { 0 };

static void
lime_input_settings_class_init(LiMeInputSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  input_signals[SIGNAL_DEVICE_ADDED] =
    g_signal_new("device-added",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  input_signals[SIGNAL_DEVICE_REMOVED] =
    g_signal_new("device-removed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  input_signals[SIGNAL_LAYOUT_CHANGED] =
    g_signal_new("layout-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  input_signals[SIGNAL_DEVICE_SETTINGS_CHANGED] =
    g_signal_new("device-settings-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
lime_input_settings_init(LiMeInputSettings *settings)
{
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->input_devices = NULL;
  priv->keyboard_layouts = NULL;
  priv->keyboard_shortcuts = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                     g_free, g_free);

  priv->mouse_settings = g_settings_new("org.cinnamon.desktop.peripherals.mouse");
  priv->keyboard_settings = g_settings_new("org.cinnamon.desktop.peripherals.keyboard");

  /* Default mouse settings */
  priv->mouse_sensitivity = 1.0;
  priv->mouse_acceleration = 1.0;
  priv->button_swap = FALSE;

  /* Default touchpad settings */
  priv->touchpad_enabled = TRUE;
  priv->touchpad_sensitivity = 1.0;
  priv->tap_to_click = TRUE;
  priv->two_finger_scroll = TRUE;
  priv->natural_scroll = FALSE;
  priv->edge_scrolling = FALSE;

  /* Default keyboard settings */
  priv->key_repeat_delay = 400;
  priv->key_repeat_interval = 30;
  priv->volume_keys_enabled = TRUE;
  priv->caps_lock_behavior = g_strdup("caps-lock");

  /* Input methods */
  priv->active_input_method = g_strdup("ibus");
  priv->available_input_methods = NULL;
  priv->available_input_methods = g_list_append(priv->available_input_methods,
                                                 g_strdup("ibus"));
  priv->available_input_methods = g_list_append(priv->available_input_methods,
                                                 g_strdup("fcitx"));

  g_debug("Input settings initialized");
}

/**
 * lime_input_settings_new:
 *
 * Create a new input settings manager
 *
 * Returns: A new #LiMeInputSettings
 */
LiMeInputSettings *
lime_input_settings_new(void)
{
  return g_object_new(LIME_TYPE_INPUT_SETTINGS, NULL);
}

/**
 * lime_input_settings_get_devices:
 * @settings: A #LiMeInputSettings
 *
 * Get list of input devices
 *
 * Returns: (transfer none): List of input devices
 */
GList *
lime_input_settings_get_devices(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), NULL);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  return priv->input_devices;
}

/**
 * lime_input_settings_get_device:
 * @settings: A #LiMeInputSettings
 * @device_id: Device ID
 *
 * Get input device by ID
 *
 * Returns: (transfer none): Input device
 */
LiMeInputDevice *
lime_input_settings_get_device(LiMeInputSettings *settings,
                               const gchar *device_id)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), NULL);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  for (GList *link = priv->input_devices; link; link = link->next) {
    LiMeInputDevice *device = (LiMeInputDevice *)link->data;
    if (g_strcmp0(device->device_id, device_id) == 0) {
      return device;
    }
  }

  return NULL;
}

/**
 * lime_input_settings_enable_device:
 * @settings: A #LiMeInputSettings
 * @device_id: Device ID
 * @enabled: Whether to enable the device
 *
 * Enable or disable an input device
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_enable_device(LiMeInputSettings *settings,
                                  const gchar *device_id,
                                  gboolean enabled)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);

  LiMeInputDevice *device = lime_input_settings_get_device(settings, device_id);
  if (!device) return FALSE;

  device->enabled = enabled;
  g_debug("Device %s %s", device_id, enabled ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_input_settings_set_device_sensitivity:
 * @settings: A #LiMeInputSettings
 * @device_id: Device ID
 * @sensitivity: Sensitivity value (0.0 - 2.0)
 *
 * Set device sensitivity
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_device_sensitivity(LiMeInputSettings *settings,
                                           const gchar *device_id,
                                           gdouble sensitivity)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);

  LiMeInputDevice *device = lime_input_settings_get_device(settings, device_id);
  if (!device) return FALSE;

  device->sensitivity = CLAMP(sensitivity, 0.0, 2.0);
  g_debug("Set sensitivity for device %s to %.2f", device_id, device->sensitivity);

  return TRUE;
}

/**
 * lime_input_settings_set_mouse_sensitivity:
 * @settings: A #LiMeInputSettings
 * @sensitivity: Mouse sensitivity (0.0 - 2.0)
 *
 * Set mouse pointer sensitivity
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_mouse_sensitivity(LiMeInputSettings *settings, gdouble sensitivity)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->mouse_sensitivity = CLAMP(sensitivity, 0.0, 2.0);
  g_debug("Mouse sensitivity set to %.2f", priv->mouse_sensitivity);

  return TRUE;
}

/**
 * lime_input_settings_get_mouse_sensitivity:
 * @settings: A #LiMeInputSettings
 *
 * Get mouse pointer sensitivity
 *
 * Returns: Sensitivity value
 */
gdouble
lime_input_settings_get_mouse_sensitivity(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), 1.0);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  return priv->mouse_sensitivity;
}

/**
 * lime_input_settings_set_mouse_acceleration:
 * @settings: A #LiMeInputSettings
 * @accel: Acceleration factor
 *
 * Set mouse acceleration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_mouse_acceleration(LiMeInputSettings *settings, gdouble accel)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->mouse_acceleration = CLAMP(accel, 0.0, 3.0);
  g_debug("Mouse acceleration set to %.2f", priv->mouse_acceleration);

  return TRUE;
}

/**
 * lime_input_settings_set_mouse_button_swap:
 * @settings: A #LiMeInputSettings
 * @swap: Whether to swap buttons
 *
 * Enable/disable mouse button swap (for left-handed users)
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_mouse_button_swap(LiMeInputSettings *settings, gboolean swap)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->button_swap = swap;
  g_debug("Mouse button swap %s", swap ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_input_settings_get_mouse_button_swap:
 * @settings: A #LiMeInputSettings
 *
 * Get mouse button swap status
 *
 * Returns: %TRUE if button swap is enabled
 */
gboolean
lime_input_settings_get_mouse_button_swap(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  return priv->button_swap;
}

/**
 * lime_input_settings_enable_touchpad:
 * @settings: A #LiMeInputSettings
 * @enabled: Whether to enable touchpad
 *
 * Enable or disable touchpad
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_enable_touchpad(LiMeInputSettings *settings, gboolean enabled)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->touchpad_enabled = enabled;
  g_debug("Touchpad %s", enabled ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_input_settings_set_touchpad_sensitivity:
 * @settings: A #LiMeInputSettings
 * @sensitivity: Touchpad sensitivity (0.0 - 2.0)
 *
 * Set touchpad sensitivity
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_touchpad_sensitivity(LiMeInputSettings *settings,
                                             gdouble sensitivity)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->touchpad_sensitivity = CLAMP(sensitivity, 0.0, 2.0);
  g_debug("Touchpad sensitivity set to %.2f", priv->touchpad_sensitivity);

  return TRUE;
}

/**
 * lime_input_settings_set_tap_to_click:
 * @settings: A #LiMeInputSettings
 * @enabled: Whether to enable tap-to-click
 *
 * Enable/disable touchpad tap-to-click
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_tap_to_click(LiMeInputSettings *settings, gboolean enabled)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->tap_to_click = enabled;
  g_debug("Tap-to-click %s", enabled ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_input_settings_set_two_finger_scroll:
 * @settings: A #LiMeInputSettings
 * @enabled: Whether to enable two-finger scrolling
 *
 * Enable/disable two-finger scrolling on touchpad
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_two_finger_scroll(LiMeInputSettings *settings, gboolean enabled)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->two_finger_scroll = enabled;
  g_debug("Two-finger scroll %s", enabled ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_input_settings_set_natural_scroll:
 * @settings: A #LiMeInputSettings
 * @enabled: Whether to enable natural scrolling
 *
 * Enable/disable natural scrolling direction
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_natural_scroll(LiMeInputSettings *settings, gboolean enabled)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->natural_scroll = enabled;
  g_debug("Natural scroll %s", enabled ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_input_settings_set_edge_scrolling:
 * @settings: A #LiMeInputSettings
 * @enabled: Whether to enable edge scrolling
 *
 * Enable/disable edge scrolling on touchpad
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_edge_scrolling(LiMeInputSettings *settings, gboolean enabled)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->edge_scrolling = enabled;
  g_debug("Edge scrolling %s", enabled ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_input_settings_get_keyboard_layouts:
 * @settings: A #LiMeInputSettings
 *
 * Get available keyboard layouts
 *
 * Returns: (transfer none): List of keyboard layouts
 */
GList *
lime_input_settings_get_keyboard_layouts(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), NULL);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  return priv->keyboard_layouts;
}

/**
 * lime_input_settings_add_keyboard_layout:
 * @settings: A #LiMeInputSettings
 * @layout_code: Keyboard layout code (e.g., "us", "fr")
 * @variant: Layout variant (optional)
 *
 * Add a keyboard layout
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_add_keyboard_layout(LiMeInputSettings *settings,
                                        const gchar *layout_code,
                                        const gchar *variant)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  g_return_val_if_fail(layout_code != NULL, FALSE);

  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  LiMeKeyboardLayout *layout = g_malloc0(sizeof(LiMeKeyboardLayout));
  layout->layout_code = g_strdup(layout_code);
  layout->layout_name = g_strdup_printf("%s%s%s", layout_code,
                                        variant ? "-" : "",
                                        variant ? variant : "");
  layout->variant = variant ? g_strdup(variant) : NULL;
  layout->is_active = (priv->keyboard_layouts == NULL);

  priv->keyboard_layouts = g_list_append(priv->keyboard_layouts, layout);
  g_debug("Added keyboard layout: %s", layout->layout_name);

  return TRUE;
}

/**
 * lime_input_settings_remove_keyboard_layout:
 * @settings: A #LiMeInputSettings
 * @layout_code: Keyboard layout code
 *
 * Remove a keyboard layout
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_remove_keyboard_layout(LiMeInputSettings *settings,
                                           const gchar *layout_code)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  for (GList *link = priv->keyboard_layouts; link; link = link->next) {
    LiMeKeyboardLayout *layout = (LiMeKeyboardLayout *)link->data;
    if (g_strcmp0(layout->layout_code, layout_code) == 0) {
      g_free(layout->layout_code);
      g_free(layout->layout_name);
      g_free(layout->variant);
      g_free(layout);

      priv->keyboard_layouts = g_list_remove_link(priv->keyboard_layouts, link);
      g_debug("Removed keyboard layout: %s", layout_code);
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * lime_input_settings_set_active_layout:
 * @settings: A #LiMeInputSettings
 * @layout_code: Keyboard layout code
 *
 * Set active keyboard layout
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_active_layout(LiMeInputSettings *settings,
                                      const gchar *layout_code)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  for (GList *link = priv->keyboard_layouts; link; link = link->next) {
    LiMeKeyboardLayout *layout = (LiMeKeyboardLayout *)link->data;
    layout->is_active = (g_strcmp0(layout->layout_code, layout_code) == 0);
  }

  g_debug("Set active keyboard layout to: %s", layout_code);
  g_signal_emit(settings, input_signals[SIGNAL_LAYOUT_CHANGED], 0, layout_code);

  return TRUE;
}

/**
 * lime_input_settings_get_active_layout:
 * @settings: A #LiMeInputSettings
 *
 * Get active keyboard layout code
 *
 * Returns: (transfer full): Layout code
 */
gchar *
lime_input_settings_get_active_layout(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), NULL);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  for (GList *link = priv->keyboard_layouts; link; link = link->next) {
    LiMeKeyboardLayout *layout = (LiMeKeyboardLayout *)link->data;
    if (layout->is_active) {
      return g_strdup(layout->layout_code);
    }
  }

  return g_strdup("us");
}

/**
 * lime_input_settings_set_key_repeat_delay:
 * @settings: A #LiMeInputSettings
 * @delay: Delay in milliseconds
 *
 * Set key repeat delay
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_key_repeat_delay(LiMeInputSettings *settings, gint delay)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->key_repeat_delay = CLAMP(delay, 100, 1000);
  g_debug("Key repeat delay set to %d ms", priv->key_repeat_delay);

  return TRUE;
}

/**
 * lime_input_settings_set_key_repeat_interval:
 * @settings: A #LiMeInputSettings
 * @interval: Interval in milliseconds
 *
 * Set key repeat interval
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_key_repeat_interval(LiMeInputSettings *settings, gint interval)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->key_repeat_interval = CLAMP(interval, 10, 200);
  g_debug("Key repeat interval set to %d ms", priv->key_repeat_interval);

  return TRUE;
}

/**
 * lime_input_settings_set_keyboard_volume_keys:
 * @settings: A #LiMeInputSettings
 * @enabled: Whether to enable volume keys
 *
 * Enable/disable volume control via keyboard keys
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_keyboard_volume_keys(LiMeInputSettings *settings,
                                             gboolean enabled)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  priv->volume_keys_enabled = enabled;
  g_debug("Keyboard volume keys %s", enabled ? "enabled" : "disabled");

  return TRUE;
}

/**
 * lime_input_settings_set_caps_lock_behavior:
 * @settings: A #LiMeInputSettings
 * @behavior: Behavior mode (e.g., "caps-lock", "ctrl", "super")
 *
 * Set Caps Lock key behavior
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_caps_lock_behavior(LiMeInputSettings *settings,
                                           const gchar *behavior)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  g_return_val_if_fail(behavior != NULL, FALSE);

  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  g_free(priv->caps_lock_behavior);
  priv->caps_lock_behavior = g_strdup(behavior);

  g_debug("Caps Lock behavior set to: %s", behavior);
  return TRUE;
}

/**
 * lime_input_settings_bind_shortcut:
 * @settings: A #LiMeInputSettings
 * @action: Action name
 * @shortcut: Shortcut key combination
 *
 * Bind keyboard shortcut to action
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_bind_shortcut(LiMeInputSettings *settings,
                                  const gchar *action,
                                  const gchar *shortcut)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  g_return_val_if_fail(action != NULL && shortcut != NULL, FALSE);

  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  g_hash_table_insert(priv->keyboard_shortcuts, g_strdup(action), g_strdup(shortcut));
  g_debug("Bound shortcut: %s -> %s", action, shortcut);

  return TRUE;
}

/**
 * lime_input_settings_get_shortcut:
 * @settings: A #LiMeInputSettings
 * @action: Action name
 *
 * Get keyboard shortcut for action
 *
 * Returns: (transfer full): Shortcut binding or NULL
 */
gchar *
lime_input_settings_get_shortcut(LiMeInputSettings *settings, const gchar *action)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), NULL);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  const gchar *shortcut = g_hash_table_lookup(priv->keyboard_shortcuts, action);
  return shortcut ? g_strdup(shortcut) : NULL;
}

/**
 * lime_input_settings_unbind_shortcut:
 * @settings: A #LiMeInputSettings
 * @action: Action name
 *
 * Remove keyboard shortcut binding
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_unbind_shortcut(LiMeInputSettings *settings, const gchar *action)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  gboolean removed = g_hash_table_remove(priv->keyboard_shortcuts, action);
  if (removed) {
    g_debug("Unbound shortcut: %s", action);
  }

  return removed;
}

/**
 * lime_input_settings_set_input_method:
 * @settings: A #LiMeInputSettings
 * @input_method: Input method name
 *
 * Set active input method editor
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_set_input_method(LiMeInputSettings *settings,
                                     const gchar *input_method)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  g_return_val_if_fail(input_method != NULL, FALSE);

  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  g_free(priv->active_input_method);
  priv->active_input_method = g_strdup(input_method);

  g_debug("Input method set to: %s", input_method);
  return TRUE;
}

/**
 * lime_input_settings_get_input_method:
 * @settings: A #LiMeInputSettings
 *
 * Get active input method editor
 *
 * Returns: (transfer full): Input method name
 */
gchar *
lime_input_settings_get_input_method(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), NULL);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  return g_strdup(priv->active_input_method);
}

/**
 * lime_input_settings_get_available_input_methods:
 * @settings: A #LiMeInputSettings
 *
 * Get list of available input methods
 *
 * Returns: (transfer none): List of input method names
 */
GList *
lime_input_settings_get_available_input_methods(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), NULL);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  return priv->available_input_methods;
}

/**
 * lime_input_settings_save_config:
 * @settings: A #LiMeInputSettings
 *
 * Save input configuration to disk
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_save_config(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  gchar *config_dir = g_build_filename(g_get_home_dir(), INPUT_CONFIG_DIR, NULL);
  g_mkdir_with_parents(config_dir, 0755);

  gchar *config_path = g_build_filename(config_dir, INPUT_CONFIG_FILE, NULL);

  GString *config_content = g_string_new("");
  g_string_append(config_content, "[Mouse]\n");
  g_string_append_printf(config_content, "sensitivity=%.2f\n", priv->mouse_sensitivity);
  g_string_append_printf(config_content, "acceleration=%.2f\n", priv->mouse_acceleration);
  g_string_append_printf(config_content, "button_swap=%d\n", priv->button_swap);

  g_string_append(config_content, "\n[Touchpad]\n");
  g_string_append_printf(config_content, "enabled=%d\n", priv->touchpad_enabled);
  g_string_append_printf(config_content, "sensitivity=%.2f\n", priv->touchpad_sensitivity);
  g_string_append_printf(config_content, "tap_to_click=%d\n", priv->tap_to_click);
  g_string_append_printf(config_content, "two_finger_scroll=%d\n", priv->two_finger_scroll);
  g_string_append_printf(config_content, "natural_scroll=%d\n", priv->natural_scroll);

  g_string_append(config_content, "\n[Keyboard]\n");
  g_string_append_printf(config_content, "repeat_delay=%d\n", priv->key_repeat_delay);
  g_string_append_printf(config_content, "repeat_interval=%d\n", priv->key_repeat_interval);
  g_string_append_printf(config_content, "volume_keys=%d\n", priv->volume_keys_enabled);
  g_string_append_printf(config_content, "caps_lock_behavior=%s\n", priv->caps_lock_behavior);

  g_string_append(config_content, "\n[InputMethod]\n");
  g_string_append_printf(config_content, "active=%s\n", priv->active_input_method);

  GError *error = NULL;
  g_file_set_contents(config_path, config_content->str, -1, &error);

  if (error) {
    g_warning("Failed to save input config: %s", error->message);
    g_error_free(error);
    g_free(config_dir);
    g_free(config_path);
    g_string_free(config_content, TRUE);
    return FALSE;
  }

  g_debug("Input configuration saved to %s", config_path);
  g_free(config_dir);
  g_free(config_path);
  g_string_free(config_content, TRUE);

  return TRUE;
}

/**
 * lime_input_settings_load_config:
 * @settings: A #LiMeInputSettings
 *
 * Load input configuration from disk
 *
 * Returns: %TRUE on success
 */
gboolean
lime_input_settings_load_config(LiMeInputSettings *settings)
{
  g_return_val_if_fail(LIME_IS_INPUT_SETTINGS(settings), FALSE);
  LiMeInputSettingsPrivate *priv = lime_input_settings_get_instance_private(settings);

  gchar *config_dir = g_build_filename(g_get_home_dir(), INPUT_CONFIG_DIR, NULL);
  gchar *config_path = g_build_filename(config_dir, INPUT_CONFIG_FILE, NULL);

  GError *error = NULL;
  gchar *config_content = NULL;

  if (!g_file_get_contents(config_path, &config_content, NULL, &error)) {
    g_debug("No input config found");
    g_free(config_dir);
    g_free(config_path);
    if (error) g_error_free(error);
    return FALSE;
  }

  gchar **lines = g_strsplit(config_content, "\n", -1);
  for (gint i = 0; lines[i]; i++) {
    if (g_str_has_prefix(lines[i], "sensitivity=")) {
      priv->mouse_sensitivity = g_ascii_strtod(lines[i] + 12, NULL);
    } else if (g_str_has_prefix(lines[i], "button_swap=")) {
      priv->button_swap = atoi(lines[i] + 12);
    }
  }

  g_strfreev(lines);
  g_free(config_content);
  g_free(config_dir);
  g_free(config_path);

  g_debug("Input configuration loaded");
  return TRUE;
}
