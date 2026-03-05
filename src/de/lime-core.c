/*
 * LiMe Core - Main Desktop Environment Core
 * Comprehensive desktop environment implementation
 * Part of LiMe OS - Cinnamon fork with AI integration
 */

#include "lime-core.h"
#include "lime-window-manager.h"
#include "lime-panel.h"
#include "lime-theme-manager.h"
#include "lime-ai-integration.h"
#include "lime-settings.h"
#include "lime-launcher.h"
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Global singleton instance */
static LiMeCore *lime_core_instance = NULL;

struct _LiMeCore {
  GObject parent;

  /* Core components */
  LiMeWindowManager *wm;
  LiMePanel *top_panel;
  LiMePanel *bottom_panel;
  LiMeThemeManager *theme_manager;
  LiMeAIIntegration *ai_integration;
  LiMeSettings *settings;
  LiMeLauncher *launcher;

  /* Display and screen */
  GdkDisplay *display;
  GdkScreen *screen;
  Display *x11_display;
  int screen_width;
  int screen_height;

  /* Main loop and state */
  GMainLoop *main_loop;
  gboolean running;
  gboolean compositor_running;

  /* Window tracking */
  GHashTable *active_windows;
  GQueue *window_stack;

  /* Workspace management */
  gint num_workspaces;
  gint current_workspace;
  LiMeWorkspace **workspaces;

  /* Signals and event handling */
  gulong screen_changed_handler;
  gulong monitors_changed_handler;

  /* Configuration */
  GSettings *gsettings;
  GKeyFile *config;

  /* Timing and performance */
  guint frame_time_source;
  guint cleanup_timer;

  /* System state */
  gboolean locked;
  gboolean fullscreen_mode;
  gboolean show_notifications;

  /* Hooks for extensions */
  GSList *loaded_extensions;
};

G_DEFINE_TYPE(LiMeCore, lime_core, G_TYPE_OBJECT);

/* Forward declarations */
static void lime_core_setup_panels(LiMeCore *core);
static void lime_core_setup_window_manager(LiMeCore *core);
static void lime_core_setup_theme(LiMeCore *core);
static void lime_core_setup_signals(LiMeCore *core);
static void lime_core_create_workspaces(LiMeCore *core);
static gboolean lime_core_process_events(gpointer user_data);
static void lime_core_handle_window_open(LiMeCore *core, LiMeWindow *window);
static void lime_core_handle_window_close(LiMeCore *core, LiMeWindow *window);

/* Signal handlers */
static void on_screen_size_changed(GdkScreen *screen, LiMeCore *core);
static void on_monitors_changed(GdkScreen *screen, LiMeCore *core);
static void on_focus_window_changed(LiMeWindowManager *wm, LiMeWindow *window, LiMeCore *core);

/* Cleanup and shutdown */
static void lime_core_dispose(GObject *object);
static void lime_core_finalize(GObject *object);

/**
 * lime_core_class_init:
 * @klass: The class structure
 *
 * Initialize the LiMeCore class
 */
static void
lime_core_class_init(LiMeCoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_core_dispose;
  object_class->finalize = lime_core_finalize;

  /* Signals */
  signals[SIGNAL_STARTUP] =
    g_signal_new("startup",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  signals[SIGNAL_READY] =
    g_signal_new("ready",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  signals[SIGNAL_WINDOW_OPENED] =
    g_signal_new("window-opened",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE, 1, LIME_TYPE_WINDOW);

  signals[SIGNAL_WINDOW_CLOSED] =
    g_signal_new("window-closed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE, 1, LIME_TYPE_WINDOW);

  signals[SIGNAL_WORKSPACE_SWITCHED] =
    g_signal_new("workspace-switched",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_THEME_CHANGED] =
    g_signal_new("theme-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);
}

/**
 * lime_core_init:
 * @core: The LiMeCore instance
 *
 * Initialize a new LiMeCore instance
 */
static void
lime_core_init(LiMeCore *core)
{
  core->running = FALSE;
  core->compositor_running = FALSE;
  core->locked = FALSE;
  core->fullscreen_mode = FALSE;
  core->show_notifications = TRUE;
  core->num_workspaces = 4;
  core->current_workspace = 0;

  core->active_windows = g_hash_table_new_full(
    g_direct_hash, g_direct_equal,
    NULL, g_object_unref);

  core->window_stack = g_queue_new();
  core->loaded_extensions = NULL;

  /* Initialize display */
  core->display = gdk_display_get_default();
  core->screen = gdk_display_get_default_screen(core->display);
  core->x11_display = GDK_DISPLAY_XDISPLAY(core->display);

  core->screen_width = gdk_screen_get_width(core->screen);
  core->screen_height = gdk_screen_get_height(core->screen);

  g_debug("LiMe Core initialized - Screen: %dx%d",
          core->screen_width, core->screen_height);
}

/**
 * lime_core_dispose:
 * @object: The GObject instance
 *
 * Clean up allocated resources
 */
static void
lime_core_dispose(GObject *object)
{
  LiMeCore *core = LIME_CORE(object);

  if (core->screen_changed_handler) {
    g_signal_handler_disconnect(core->screen, core->screen_changed_handler);
    core->screen_changed_handler = 0;
  }

  if (core->monitors_changed_handler) {
    g_signal_handler_disconnect(core->screen, core->monitors_changed_handler);
    core->monitors_changed_handler = 0;
  }

  if (core->frame_time_source) {
    g_source_remove(core->frame_time_source);
    core->frame_time_source = 0;
  }

  if (core->cleanup_timer) {
    g_source_remove(core->cleanup_timer);
    core->cleanup_timer = 0;
  }

  g_clear_object(&core->wm);
  g_clear_object(&core->top_panel);
  g_clear_object(&core->bottom_panel);
  g_clear_object(&core->theme_manager);
  g_clear_object(&core->ai_integration);
  g_clear_object(&core->settings);
  g_clear_object(&core->launcher);
  g_clear_object(&core->gsettings);

  if (core->active_windows) {
    g_hash_table_unref(core->active_windows);
    core->active_windows = NULL;
  }

  if (core->window_stack) {
    g_queue_free(core->window_stack);
    core->window_stack = NULL;
  }

  if (core->workspaces) {
    for (int i = 0; i < core->num_workspaces; i++) {
      g_clear_object(&core->workspaces[i]);
    }
    g_free(core->workspaces);
    core->workspaces = NULL;
  }

  if (core->config) {
    g_key_file_unref(core->config);
    core->config = NULL;
  }

  g_slist_free_full(core->loaded_extensions, g_object_unref);
  core->loaded_extensions = NULL;

  G_OBJECT_CLASS(lime_core_parent_class)->dispose(object);
}

/**
 * lime_core_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_core_finalize(GObject *object)
{
  G_OBJECT_CLASS(lime_core_parent_class)->finalize(object);
}

/**
 * lime_core_get_default:
 *
 * Get the singleton instance of LiMeCore
 *
 * Returns: (transfer none): The global LiMeCore instance
 */
LiMeCore *
lime_core_get_default(void)
{
  if (!lime_core_instance) {
    lime_core_instance = g_object_new(LIME_TYPE_CORE, NULL);
  }

  return lime_core_instance;
}

/**
 * lime_core_setup_window_manager:
 * @core: A #LiMeCore
 *
 * Initialize the window manager
 */
static void
lime_core_setup_window_manager(LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  g_debug("Setting up window manager");

  core->wm = lime_window_manager_new(core->x11_display, core->screen);

  g_signal_connect(core->wm, "focus-window-changed",
                   G_CALLBACK(on_focus_window_changed), core);

  lime_window_manager_manage_existing_windows(core->wm);
}

/**
 * lime_core_setup_panels:
 * @core: A #LiMeCore
 *
 * Create and display the top and bottom panels
 */
static void
lime_core_setup_panels(LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  g_debug("Setting up panels");

  /* Create top panel */
  core->top_panel = lime_panel_new(LIME_PANEL_POSITION_TOP,
                                    core->screen_width,
                                    40);
  lime_panel_add_clock(core->top_panel);
  lime_panel_add_system_tray(core->top_panel);
  lime_panel_add_sound_control(core->top_panel);
  lime_panel_add_network_widget(core->top_panel);
  lime_panel_add_power_button(core->top_panel);

  /* Add window list to top panel */
  lime_panel_add_window_list(core->top_panel, core->active_windows);

  /* Create bottom panel (taskbar) */
  core->bottom_panel = lime_panel_new(LIME_PANEL_POSITION_BOTTOM,
                                       core->screen_width,
                                       50);
  lime_panel_add_app_launcher(core->bottom_panel);
  lime_panel_add_workspace_switcher(core->bottom_panel, core->num_workspaces);

  /* Add AI assistant button to bottom panel */
  lime_panel_add_ai_button(core->bottom_panel);

  g_debug("Panels created successfully");
}

/**
 * lime_core_setup_theme:
 * @core: A #LiMeCore
 *
 * Initialize theming system
 */
static void
lime_core_setup_theme(LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  g_debug("Setting up theme system");

  core->theme_manager = lime_theme_manager_new();

  /* Load default theme */
  lime_theme_manager_load_default(core->theme_manager);

  /* Apply theme to GTK */
  GtkSettings *gtk_settings = gtk_settings_get_default();

  gchar *theme_name = lime_theme_manager_get_current_theme(core->theme_manager);
  g_object_set(gtk_settings, "gtk-theme-name", theme_name, NULL);

  g_free(theme_name);
}

/**
 * lime_core_create_workspaces:
 * @core: A #LiMeCore
 *
 * Initialize workspace objects
 */
static void
lime_core_create_workspaces(LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  g_debug("Creating %d workspaces", core->num_workspaces);

  core->workspaces = g_malloc0(sizeof(LiMeWorkspace *) * core->num_workspaces);

  for (int i = 0; i < core->num_workspaces; i++) {
    core->workspaces[i] = lime_workspace_new(i, "Workspace %d", i + 1);
    g_debug("Created workspace %d", i);
  }
}

/**
 * lime_core_setup_signals:
 * @core: A #LiMeCore
 *
 * Set up X11 event handling
 */
static void
lime_core_setup_signals(LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  g_debug("Setting up signal handlers");

  core->screen_changed_handler = g_signal_connect(
    core->screen, "size-changed",
    G_CALLBACK(on_screen_size_changed), core);

  core->monitors_changed_handler = g_signal_connect(
    core->screen, "monitors-changed",
    G_CALLBACK(on_monitors_changed), core);

  /* Set up frame rate timer */
  core->frame_time_source = g_timeout_add(16, lime_core_process_events, core);
}

/**
 * on_screen_size_changed:
 * @screen: The GdkScreen
 * @core: The LiMeCore instance
 *
 * Handle screen size changes
 */
static void
on_screen_size_changed(GdkScreen *screen, LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  gint new_width = gdk_screen_get_width(screen);
  gint new_height = gdk_screen_get_height(screen);

  g_debug("Screen size changed: %dx%d -> %dx%d",
          core->screen_width, core->screen_height,
          new_width, new_height);

  core->screen_width = new_width;
  core->screen_height = new_height;

  /* Resize panels */
  if (core->top_panel) {
    lime_panel_set_width(core->top_panel, new_width);
  }
  if (core->bottom_panel) {
    lime_panel_set_width(core->bottom_panel, new_width);
  }

  /* Notify windows */
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, core->active_windows);

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    LiMeWindow *window = LIME_WINDOW(value);
    lime_window_screen_resized(window, new_width, new_height);
  }
}

/**
 * on_monitors_changed:
 * @screen: The GdkScreen
 * @core: The LiMeCore instance
 *
 * Handle monitor configuration changes
 */
static void
on_monitors_changed(GdkScreen *screen, LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  gint num_monitors = gdk_screen_get_n_monitors(screen);

  g_debug("Monitor configuration changed, now %d monitors", num_monitors);

  /* Reorganize windows across monitors */
  if (core->wm) {
    lime_window_manager_reorganize_monitors(core->wm, num_monitors);
  }
}

/**
 * on_focus_window_changed:
 * @wm: The window manager
 * @window: The newly focused window
 * @core: The LiMeCore instance
 *
 * Handle focus changes
 */
static void
on_focus_window_changed(LiMeWindowManager *wm,
                        LiMeWindow *window,
                        LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  if (window) {
    g_debug("Focus changed to window: %s",
            lime_window_get_name(window));

    /* Update taskbar */
    if (core->bottom_panel) {
      lime_panel_update_active_window(core->bottom_panel, window);
    }
  }
}

/**
 * lime_core_handle_window_open:
 * @core: A #LiMeCore
 * @window: The newly opened window
 *
 * Handle a window opening
 */
static void
lime_core_handle_window_open(LiMeCore *core, LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_CORE(core));
  g_return_if_fail(LIME_IS_WINDOW(window));

  g_debug("Window opened: %s", lime_window_get_name(window));

  /* Add to tracking */
  guint32 xid = lime_window_get_xid(window);
  g_hash_table_insert(core->active_windows, GUINT_TO_POINTER(xid),
                      g_object_ref(window));

  /* Add to window stack */
  g_queue_push_head(core->window_stack, g_object_ref(window));

  /* Emit signal */
  g_signal_emit(core, signals[SIGNAL_WINDOW_OPENED], 0, window);
}

/**
 * lime_core_handle_window_close:
 * @core: A #LiMeCore
 * @window: The closed window
 *
 * Handle a window closing
 */
static void
lime_core_handle_window_close(LiMeCore *core, LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_CORE(core));
  g_return_if_fail(LIME_IS_WINDOW(window));

  g_debug("Window closed: %s", lime_window_get_name(window));

  /* Remove from tracking */
  guint32 xid = lime_window_get_xid(window);
  g_hash_table_remove(core->active_windows, GUINT_TO_POINTER(xid));

  /* Remove from stack */
  g_queue_remove(core->window_stack, window);
  g_object_unref(window);

  /* Emit signal */
  g_signal_emit(core, signals[SIGNAL_WINDOW_CLOSED], 0, window);
}

/**
 * lime_core_process_events:
 * @user_data: The LiMeCore instance
 *
 * Main event processing loop
 *
 * Returns: %TRUE to continue processing
 */
static gboolean
lime_core_process_events(gpointer user_data)
{
  LiMeCore *core = LIME_CORE(user_data);

  /* Process X11 events */
  if (core->wm) {
    lime_window_manager_process_events(core->wm);
  }

  /* Update panels */
  if (core->top_panel) {
    lime_panel_update(core->top_panel);
  }
  if (core->bottom_panel) {
    lime_panel_update(core->bottom_panel);
  }

  return G_SOURCE_CONTINUE;
}

/**
 * lime_core_startup:
 * @core: A #LiMeCore
 *
 * Start the desktop environment
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
lime_core_startup(LiMeCore *core)
{
  g_return_val_if_fail(LIME_IS_CORE(core), FALSE);

  if (core->running) {
    g_warning("LiMe Core is already running");
    return FALSE;
  }

  g_debug("Starting LiMe Desktop Environment");

  /* Initialize components */
  lime_core_create_workspaces(core);
  lime_core_setup_theme(core);
  lime_core_setup_window_manager(core);
  lime_core_setup_panels(core);
  lime_core_setup_signals(core);

  /* Initialize AI integration */
  core->ai_integration = lime_ai_integration_new();
  lime_ai_integration_init(core->ai_integration);

  /* Load settings */
  core->gsettings = g_settings_new("org.cinnamon");

  core->running = TRUE;
  core->compositor_running = TRUE;

  g_signal_emit(core, signals[SIGNAL_STARTUP], 0);

  g_debug("LiMe Core startup complete");

  return TRUE;
}

/**
 * lime_core_shutdown:
 * @core: A #LiMeCore
 *
 * Shut down the desktop environment gracefully
 */
void
lime_core_shutdown(LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  if (!core->running) {
    return;
  }

  g_debug("Shutting down LiMe Core");

  core->running = FALSE;
  core->compositor_running = FALSE;

  /* Close all windows */
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, core->active_windows);

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    LiMeWindow *window = LIME_WINDOW(value);
    lime_window_close(window);
  }

  if (core->main_loop && g_main_loop_is_running(core->main_loop)) {
    g_main_loop_quit(core->main_loop);
  }

  g_debug("LiMe Core shutdown complete");
}

/**
 * lime_core_run:
 * @core: A #LiMeCore
 *
 * Run the main event loop
 */
void
lime_core_run(LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  if (!core->running) {
    g_warning("LiMe Core not started, call lime_core_startup() first");
    return;
  }

  g_debug("Entering main event loop");

  core->main_loop = g_main_loop_new(NULL, FALSE);

  g_signal_emit(core, signals[SIGNAL_READY], 0);

  g_main_loop_run(core->main_loop);

  g_main_loop_unref(core->main_loop);
  core->main_loop = NULL;
}

/**
 * lime_core_switch_workspace:
 * @core: A #LiMeCore
 * @workspace_index: The workspace to switch to
 *
 * Switch to a different workspace
 *
 * Returns: %TRUE on success
 */
gboolean
lime_core_switch_workspace(LiMeCore *core, gint workspace_index)
{
  g_return_val_if_fail(LIME_IS_CORE(core), FALSE);
  g_return_val_if_fail(workspace_index >= 0 && workspace_index < core->num_workspaces,
                       FALSE);

  if (core->current_workspace == workspace_index) {
    return TRUE;
  }

  g_debug("Switching to workspace %d", workspace_index);

  /* Hide windows from current workspace */
  lime_workspace_hide_windows(core->workspaces[core->current_workspace]);

  /* Switch workspace */
  core->current_workspace = workspace_index;

  /* Show windows from new workspace */
  lime_workspace_show_windows(core->workspaces[core->current_workspace]);

  g_signal_emit(core, signals[SIGNAL_WORKSPACE_SWITCHED], 0, workspace_index);

  return TRUE;
}

/**
 * lime_core_add_window_to_workspace:
 * @core: A #LiMeCore
 * @window: A #LiMeWindow
 * @workspace_index: The target workspace
 *
 * Move a window to a specific workspace
 */
void
lime_core_add_window_to_workspace(LiMeCore *core,
                                   LiMeWindow *window,
                                   gint workspace_index)
{
  g_return_if_fail(LIME_IS_CORE(core));
  g_return_if_fail(LIME_IS_WINDOW(window));
  g_return_if_fail(workspace_index >= 0 && workspace_index < core->num_workspaces);

  lime_workspace_add_window(core->workspaces[workspace_index], window);
}

/**
 * lime_core_get_active_window:
 * @core: A #LiMeCore
 *
 * Get the currently focused window
 *
 * Returns: (transfer none): The active window or %NULL
 */
LiMeWindow *
lime_core_get_active_window(LiMeCore *core)
{
  g_return_val_if_fail(LIME_IS_CORE(core), NULL);

  if (g_queue_get_length(core->window_stack) > 0) {
    return LIME_WINDOW(g_queue_peek_head(core->window_stack));
  }

  return NULL;
}

/**
 * lime_core_get_window_list:
 * @core: A #LiMeCore
 *
 * Get list of all managed windows
 *
 * Returns: (element-type LiMeWindow) (transfer container): List of windows
 */
GList *
lime_core_get_window_list(LiMeCore *core)
{
  g_return_val_if_fail(LIME_IS_CORE(core), NULL);

  return g_hash_table_get_values(core->active_windows);
}

/**
 * lime_core_get_theme_manager:
 * @core: A #LiMeCore
 *
 * Get the theme manager
 *
 * Returns: (transfer none): The theme manager
 */
LiMeThemeManager *
lime_core_get_theme_manager(LiMeCore *core)
{
  g_return_val_if_fail(LIME_IS_CORE(core), NULL);

  return core->theme_manager;
}

/**
 * lime_core_get_ai_integration:
 * @core: A #LiMeCore
 *
 * Get the AI integration module
 *
 * Returns: (transfer none): The AI integration
 */
LiMeAIIntegration *
lime_core_get_ai_integration(LiMeCore *core)
{
  g_return_val_if_fail(LIME_IS_CORE(core), NULL);

  return core->ai_integration;
}

/**
 * lime_core_show_notification:
 * @core: A #LiMeCore
 * @title: Notification title
 * @body: Notification body
 * @timeout: Timeout in milliseconds (0 = no timeout)
 *
 * Show a notification
 *
 * Returns: The notification ID
 */
guint32
lime_core_show_notification(LiMeCore *core,
                             const gchar *title,
                             const gchar *body,
                             guint timeout)
{
  g_return_val_if_fail(LIME_IS_CORE(core), 0);
  g_return_val_if_fail(title != NULL, 0);

  if (!core->show_notifications) {
    return 0;
  }

  /* This would integrate with the notification daemon */
  g_debug("Notification: %s - %s", title, body);

  return 1; /* Placeholder ID */
}

/**
 * lime_core_set_desktop_locked:
 * @core: A #LiMeCore
 * @locked: Whether to lock the desktop
 *
 * Lock/unlock the desktop
 */
void
lime_core_set_desktop_locked(LiMeCore *core, gboolean locked)
{
  g_return_if_fail(LIME_IS_CORE(core));

  if (core->locked == locked) {
    return;
  }

  core->locked = locked;

  if (locked) {
    g_debug("Locking desktop");
    /* Show lock screen */
    g_spawn_command_line_async("lime-lock-screen", NULL);
  } else {
    g_debug("Unlocking desktop");
  }
}

/**
 * lime_core_reload_theme:
 * @core: A #LiMeCore
 *
 * Reload the current theme
 */
void
lime_core_reload_theme(LiMeCore *core)
{
  g_return_if_fail(LIME_IS_CORE(core));

  g_debug("Reloading theme");

  lime_theme_manager_reload_current(core->theme_manager);

  /* Notify windows */
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, core->active_windows);

  while (g_hash_table_iter_next(&iter, &key, &value)) {
    LiMeWindow *window = LIME_WINDOW(value);
    lime_window_theme_changed(window);
  }
}

/**
 * lime_core_get_screen_geometry:
 * @core: A #LiMeCore
 * @width: (out): Screen width
 * @height: (out): Screen height
 *
 * Get screen dimensions
 */
void
lime_core_get_screen_geometry(LiMeCore *core, gint *width, gint *height)
{
  g_return_if_fail(LIME_IS_CORE(core));

  if (width) *width = core->screen_width;
  if (height) *height = core->screen_height;
}

/**
 * lime_core_get_num_workspaces:
 * @core: A #LiMeCore
 *
 * Get the number of workspaces
 *
 * Returns: Number of workspaces
 */
gint
lime_core_get_num_workspaces(LiMeCore *core)
{
  g_return_val_if_fail(LIME_IS_CORE(core), 0);

  return core->num_workspaces;
}

/**
 * lime_core_get_current_workspace:
 * @core: A #LiMeCore
 *
 * Get the current workspace index
 *
 * Returns: Current workspace index
 */
gint
lime_core_get_current_workspace(LiMeCore *core)
{
  g_return_val_if_fail(LIME_IS_CORE(core), 0);

  return core->current_workspace;
}

/**
 * lime_core_has_compositor:
 * @core: A #LiMeCore
 *
 * Check if the compositor is running
 *
 * Returns: %TRUE if compositor is active
 */
gboolean
lime_core_has_compositor(LiMeCore *core)
{
  g_return_val_if_fail(LIME_IS_CORE(core), FALSE);

  return core->compositor_running;
}

/**
 * lime_core_enable_fullscreen:
 * @core: A #LiMeCore
 * @enable: Whether to enable fullscreen mode
 *
 * Toggle fullscreen mode (hides panels)
 */
void
lime_core_enable_fullscreen(LiMeCore *core, gboolean enable)
{
  g_return_if_fail(LIME_IS_CORE(core));

  if (core->fullscreen_mode == enable) {
    return;
  }

  core->fullscreen_mode = enable;

  if (enable) {
    if (core->top_panel) lime_panel_hide(core->top_panel);
    if (core->bottom_panel) lime_panel_hide(core->bottom_panel);
  } else {
    if (core->top_panel) lime_panel_show(core->top_panel);
    if (core->bottom_panel) lime_panel_show(core->bottom_panel);
  }

  g_debug("Fullscreen mode: %s", enable ? "enabled" : "disabled");
}

/* Main entry point */
int
main(int argc, char *argv[])
{
  LiMeCore *core;
  int exit_status = 0;

  gtk_init(&argc, &argv);

  g_print("LiMe Desktop Environment starting...\n");
  g_debug("Using GLib %d.%d.%d",
          glib_major_version,
          glib_minor_version,
          glib_micro_version);

  core = lime_core_get_default();

  if (!lime_core_startup(core)) {
    g_error("Failed to start LiMe Core");
    return 1;
  }

  lime_core_run(core);

  lime_core_shutdown(core);
  g_object_unref(core);

  g_print("LiMe Desktop Environment stopped\n");

  return exit_status;
}
