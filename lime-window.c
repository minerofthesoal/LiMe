/*
 * LiMe Window
 * Individual window object for the desktop environment
 * Complete implementation of window management for individual windows
 */

#include "lime-window.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gdk/gdkx.h>

#define WINDOW_NAME_SIZE 256
#define DEFAULT_BORDER_WIDTH 1

struct _LiMeWindow {
  GObject parent;

  Display *x11_display;
  Window xid;
  Window parent_window;

  gchar *name;
  gchar *class;
  gchar *instance;

  gint x, y;
  gint width, height;
  gint border_width;

  gboolean mapped;
  gboolean visible;
  gboolean focused;
  gboolean minimized;
  gboolean maximized;
  gboolean maximized_h;
  gboolean maximized_v;
  gboolean fullscreen;
  gboolean skip_taskbar;
  gboolean skip_pager;
  gboolean modal;
  gboolean keep_above;
  gboolean keep_below;

  gint workspace;

  Atom wm_protocols;
  Atom wm_delete_window;
  Atom wm_take_focus;
  Atom net_wm_ping;

  Atom   net_wm_window_type;
  Window net_wm_window_type_normal;
  Window net_wm_window_type_desktop;
  Window net_wm_window_type_dock;
  Window net_wm_window_type_toolbar;
  Window net_wm_window_type_menu;
  Window net_wm_window_type_utility;
  Window net_wm_window_type_splash;
  Window net_wm_window_type_dialog;

  gint window_type;

  XWindowAttributes attrs;
  XSizeHints size_hints;
  XWMHints *wm_hints;

  gint normal_width, normal_height;
  gint normal_x, normal_y;

  GHashTable *properties;
  GSList *decorations;

  GSettings *settings;
  gulong property_handler;
};

G_DEFINE_TYPE(LiMeWindow, lime_window, G_TYPE_OBJECT);

enum {
  SIGNAL_FOCUS_CHANGED,
  SIGNAL_TITLE_CHANGED,
  SIGNAL_GEOMETRY_CHANGED,
  SIGNAL_STATE_CHANGED,
  SIGNAL_WORKSPACE_CHANGED,
  LAST_SIGNAL
};

static guint window_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations */
static void lime_window_read_wmhints(LiMeWindow *window);
static void lime_window_read_normal_hints(LiMeWindow *window);
static void lime_window_read_properties(LiMeWindow *window);
static void lime_window_read_window_type(LiMeWindow *window);
static gchar * lime_window_get_property_string(LiMeWindow *window, Atom atom);

/**
 * lime_window_class_init:
 * @klass: The class structure
 *
 * Initialize the window class
 */
static void
lime_window_class_init(LiMeWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_window_dispose;
  object_class->finalize = lime_window_finalize;

  window_signals[SIGNAL_FOCUS_CHANGED] =
    g_signal_new("focus-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__BOOLEAN,
                 G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

  window_signals[SIGNAL_TITLE_CHANGED] =
    g_signal_new("title-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  window_signals[SIGNAL_GEOMETRY_CHANGED] =
    g_signal_new("geometry-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  window_signals[SIGNAL_STATE_CHANGED] =
    g_signal_new("state-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  window_signals[SIGNAL_WORKSPACE_CHANGED] =
    g_signal_new("workspace-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);
}

/**
 * lime_window_init:
 * @window: The window instance
 *
 * Initialize a new window
 */
static void
lime_window_init(LiMeWindow *window)
{
  window->x11_display = NULL;
  window->xid = None;
  window->parent_window = None;

  window->name = NULL;
  window->class = NULL;
  window->instance = NULL;

  window->x = 0;
  window->y = 0;
  window->width = 800;
  window->height = 600;
  window->border_width = DEFAULT_BORDER_WIDTH;

  window->mapped = FALSE;
  window->visible = FALSE;
  window->focused = FALSE;
  window->minimized = FALSE;
  window->maximized = FALSE;
  window->maximized_h = FALSE;
  window->maximized_v = FALSE;
  window->fullscreen = FALSE;
  window->skip_taskbar = FALSE;
  window->skip_pager = FALSE;
  window->modal = FALSE;
  window->keep_above = FALSE;
  window->keep_below = FALSE;

  window->workspace = 0;
  window->window_type = LIME_WINDOW_TYPE_NORMAL;

  window->normal_width = window->width;
  window->normal_height = window->height;
  window->normal_x = window->x;
  window->normal_y = window->y;

  window->properties = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                              NULL, g_free);
  window->decorations = NULL;
  window->settings = g_settings_new("org.cinnamon.windows");

  g_debug("Window initialized");
}

/**
 * lime_window_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_window_dispose(GObject *object)
{
  LiMeWindow *window = LIME_WINDOW(object);

  if (window->property_handler) {
    g_signal_handler_disconnect(window->settings, window->property_handler);
    window->property_handler = 0;
  }

  if (window->properties) {
    g_hash_table_unref(window->properties);
    window->properties = NULL;
  }

  g_slist_free_full(window->decorations, g_object_unref);
  window->decorations = NULL;

  g_clear_object(&window->settings);

  if (window->wm_hints) {
    XFree(window->wm_hints);
    window->wm_hints = NULL;
  }

  G_OBJECT_CLASS(lime_window_parent_class)->dispose(object);
}

/**
 * lime_window_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_window_finalize(GObject *object)
{
  LiMeWindow *window = LIME_WINDOW(object);

  g_free(window->name);
  g_free(window->class);
  g_free(window->instance);

  G_OBJECT_CLASS(lime_window_parent_class)->finalize(object);
}

/**
 * lime_window_new:
 * @x11_display: The X11 display
 * @xid: The X11 window ID
 *
 * Create a new window object
 *
 * Returns: A new #LiMeWindow
 */
LiMeWindow *
lime_window_new(Display *x11_display, Window xid)
{
  LiMeWindow *window;

  g_return_val_if_fail(x11_display != NULL, NULL);
  g_return_val_if_fail(xid != None, NULL);

  window = g_object_new(LIME_TYPE_WINDOW, NULL);

  window->x11_display = x11_display;
  window->xid = xid;

  /* Get window attributes */
  if (!XGetWindowAttributes(x11_display, xid, &window->attrs)) {
    g_object_unref(window);
    return NULL;
  }

  window->x = window->attrs.x;
  window->y = window->attrs.y;
  window->width = window->attrs.width;
  window->height = window->attrs.height;
  window->border_width = window->attrs.border_width;
  window->mapped = (window->attrs.map_state == IsViewable);

  window->normal_x = window->x;
  window->normal_y = window->y;
  window->normal_width = window->width;
  window->normal_height = window->height;

  /* Read window properties */
  lime_window_read_wmhints(window);
  lime_window_read_normal_hints(window);
  lime_window_read_window_type(window);
  lime_window_read_properties(window);

  g_debug("Created window object for 0x%lx - %s", xid, window->name);

  return window;
}

/**
 * lime_window_read_wmhints:
 * @window: A #LiMeWindow
 *
 * Read WM_HINTS property
 */
static void
lime_window_read_wmhints(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  window->wm_hints = XGetWMHints(window->x11_display, window->xid);

  if (window->wm_hints) {
    if (window->wm_hints->flags & InputHint) {
      /* Window accepts input */
    }
    if (window->wm_hints->flags & WindowGroupHint) {
      /* Window is part of a group */
    }
  }
}

/**
 * lime_window_read_normal_hints:
 * @window: A #LiMeWindow
 *
 * Read WM_NORMAL_HINTS property
 */
static void
lime_window_read_normal_hints(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  long dummy;
  XGetWMNormalHints(window->x11_display, window->xid, &window->size_hints, &dummy);
}

/**
 * lime_window_read_window_type:
 * @window: A #LiMeWindow
 *
 * Read the window type
 */
static void
lime_window_read_window_type(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  Atom type = XInternAtom(window->x11_display, "_NET_WM_WINDOW_TYPE", False);
  Atom actual;
  int format;
  unsigned long n, extra;
  unsigned char *data = NULL;

  if (XGetWindowProperty(window->x11_display, window->xid, type, 0, 32,
                         False, XA_ATOM, &actual, &format, &n, &extra, &data) == Success) {
    if (n > 0) {
      Atom *atoms = (Atom *)data;
      window->net_wm_window_type = atoms[0];
    }
    if (data) XFree(data);
  }

  /* Determine window type */
  Atom dock = XInternAtom(window->x11_display, "_NET_WM_WINDOW_TYPE_DOCK", False);
  Atom desktop = XInternAtom(window->x11_display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
  Atom dialog = XInternAtom(window->x11_display, "_NET_WM_WINDOW_TYPE_DIALOG", False);

  if (window->net_wm_window_type == dock) {
    window->window_type = LIME_WINDOW_TYPE_DOCK;
  } else if (window->net_wm_window_type == desktop) {
    window->window_type = LIME_WINDOW_TYPE_DESKTOP;
  } else if (window->net_wm_window_type == dialog) {
    window->window_type = LIME_WINDOW_TYPE_DIALOG;
  } else {
    window->window_type = LIME_WINDOW_TYPE_NORMAL;
  }
}

/**
 * lime_window_read_properties:
 * @window: A #LiMeWindow
 *
 * Read window properties like title, class, instance
 */
static void
lime_window_read_properties(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  /* Get window name */
  window->name = lime_window_get_property_string(window, XA_WM_NAME);
  if (!window->name || strlen(window->name) == 0) {
    Atom net_wm_name = XInternAtom(window->x11_display, "_NET_WM_NAME", False);
    window->name = lime_window_get_property_string(window, net_wm_name);
  }

  if (!window->name) {
    window->name = g_strdup("Unnamed Window");
  }

  /* Get window class */
  XClassHint class_hint;
  if (XGetClassHint(window->x11_display, window->xid, &class_hint)) {
    window->class = g_strdup(class_hint.res_class);
    window->instance = g_strdup(class_hint.res_name);
    XFree(class_hint.res_class);
    XFree(class_hint.res_name);
  }

  if (!window->class) {
    window->class = g_strdup("Unknown");
  }
  if (!window->instance) {
    window->instance = g_strdup("unknown");
  }
}

/**
 * lime_window_get_property_string:
 * @window: A #LiMeWindow
 * @atom: The atom to read
 *
 * Get a string property from the window
 *
 * Returns: (transfer full): The property value
 */
static gchar *
lime_window_get_property_string(LiMeWindow *window, Atom atom)
{
  g_return_val_if_fail(LIME_IS_WINDOW(window), NULL);

  Atom actual;
  int format;
  unsigned long n, extra;
  unsigned char *data = NULL;
  gchar *result = NULL;

  if (XGetWindowProperty(window->x11_display, window->xid, atom,
                         0, 512, False, AnyPropertyType,
                         &actual, &format, &n, &extra, &data) == Success) {
    if (n > 0 && data) {
      result = g_strndup((gchar *)data, n);
    }
    if (data) XFree(data);
  }

  return result;
}

/**
 * lime_window_map:
 * @window: A #LiMeWindow
 *
 * Mark window as mapped
 */
void
lime_window_map(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  window->mapped = TRUE;
  window->visible = TRUE;

  g_signal_emit(window, window_signals[SIGNAL_STATE_CHANGED], 0);
}

/**
 * lime_window_unmap:
 * @window: A #LiMeWindow
 *
 * Mark window as unmapped
 */
void
lime_window_unmap(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  window->mapped = FALSE;
  window->visible = FALSE;

  g_signal_emit(window, window_signals[SIGNAL_STATE_CHANGED], 0);
}

/**
 * lime_window_focus:
 * @window: A #LiMeWindow
 *
 * Focus the window
 */
void
lime_window_focus(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  window->focused = TRUE;

  g_signal_emit(window, window_signals[SIGNAL_FOCUS_CHANGED], 0, TRUE);
}

/**
 * lime_window_unfocus:
 * @window: A #LiMeWindow
 *
 * Remove focus from the window
 */
void
lime_window_unfocus(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  window->focused = FALSE;

  g_signal_emit(window, window_signals[SIGNAL_FOCUS_CHANGED], 0, FALSE);
}

/**
 * lime_window_iconify:
 * @window: A #LiMeWindow
 *
 * Minimize (iconify) the window
 */
void
lime_window_iconify(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  XIconifyWindow(window->x11_display, window->xid, DefaultScreen(window->x11_display));
  window->minimized = TRUE;

  g_signal_emit(window, window_signals[SIGNAL_STATE_CHANGED], 0);
}

/**
 * lime_window_maximize:
 * @window: A #LiMeWindow
 *
 * Maximize the window
 */
void
lime_window_maximize(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  if (window->maximized) {
    return;
  }

  /* Save normal geometry */
  window->normal_x = window->x;
  window->normal_y = window->y;
  window->normal_width = window->width;
  window->normal_height = window->height;

  /* Get screen dimensions */
  int screen = DefaultScreen(window->x11_display);
  int screen_width = DisplayWidth(window->x11_display, screen);
  int screen_height = DisplayHeight(window->x11_display, screen);

  /* Maximize to screen size */
  XMoveResizeWindow(window->x11_display, window->xid, 0, 0, screen_width, screen_height);

  window->x = 0;
  window->y = 0;
  window->width = screen_width;
  window->height = screen_height;
  window->maximized = TRUE;
  window->maximized_h = TRUE;
  window->maximized_v = TRUE;

  g_signal_emit(window, window_signals[SIGNAL_GEOMETRY_CHANGED], 0);
  g_signal_emit(window, window_signals[SIGNAL_STATE_CHANGED], 0);
}

/**
 * lime_window_restore:
 * @window: A #LiMeWindow
 *
 * Restore window to normal size
 */
void
lime_window_restore(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  if (!window->maximized) {
    return;
  }

  XMoveResizeWindow(window->x11_display, window->xid,
                    window->normal_x, window->normal_y,
                    window->normal_width, window->normal_height);

  window->x = window->normal_x;
  window->y = window->normal_y;
  window->width = window->normal_width;
  window->height = window->normal_height;
  window->maximized = FALSE;
  window->maximized_h = FALSE;
  window->maximized_v = FALSE;
  window->minimized = FALSE;

  g_signal_emit(window, window_signals[SIGNAL_GEOMETRY_CHANGED], 0);
  g_signal_emit(window, window_signals[SIGNAL_STATE_CHANGED], 0);
}

/**
 * lime_window_close:
 * @window: A #LiMeWindow
 *
 * Close the window
 */
void
lime_window_close(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  Atom wm_delete = XInternAtom(window->x11_display, "WM_DELETE_WINDOW", False);
  XClientMessageEvent msg;

  msg.type = ClientMessage;
  msg.window = window->xid;
  msg.message_type = XInternAtom(window->x11_display, "WM_PROTOCOLS", False);
  msg.format = 32;
  msg.data.l[0] = wm_delete;
  msg.data.l[1] = CurrentTime;

  XSendEvent(window->x11_display, window->xid, False, NoEventMask, (XEvent *)&msg);
}

/**
 * lime_window_raise:
 * @window: A #LiMeWindow
 *
 * Raise window to front
 */
void
lime_window_raise(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  XRaiseWindow(window->x11_display, window->xid);
}

/**
 * lime_window_lower:
 * @window: A #LiMeWindow
 *
 * Lower window to back
 */
void
lime_window_lower(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  XLowerWindow(window->x11_display, window->xid);
}

/**
 * lime_window_move:
 * @window: A #LiMeWindow
 * @x: New X coordinate
 * @y: New Y coordinate
 *
 * Move the window
 */
void
lime_window_move(LiMeWindow *window, gint x, gint y)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  XMoveWindow(window->x11_display, window->xid, x, y);
  window->x = x;
  window->y = y;

  g_signal_emit(window, window_signals[SIGNAL_GEOMETRY_CHANGED], 0);
}

/**
 * lime_window_resize:
 * @window: A #LiMeWindow
 * @width: New width
 * @height: New height
 *
 * Resize the window
 */
void
lime_window_resize(LiMeWindow *window, gint width, gint height)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  XResizeWindow(window->x11_display, window->xid, width, height);
  window->width = width;
  window->height = height;

  g_signal_emit(window, window_signals[SIGNAL_GEOMETRY_CHANGED], 0);
}

/**
 * lime_window_update_properties:
 * @window: A #LiMeWindow
 *
 * Update window properties
 */
void
lime_window_update_properties(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  gchar *old_name = window->name;

  /* Get updated name */
  window->name = lime_window_get_property_string(window, XA_WM_NAME);
  if (!window->name || strlen(window->name) == 0) {
    Atom net_wm_name = XInternAtom(window->x11_display, "_NET_WM_NAME", False);
    window->name = lime_window_get_property_string(window, net_wm_name);
  }

  if (!window->name) {
    window->name = g_strdup("Unnamed Window");
  }

  if (g_strcmp0(old_name, window->name) != 0) {
    g_signal_emit(window, window_signals[SIGNAL_TITLE_CHANGED], 0, window->name);
  }

  g_free(old_name);
}

/**
 * lime_window_handle_state_change:
 * @window: A #LiMeWindow
 * @data: The state change data
 *
 * Handle _NET_WM_STATE changes
 */
void
lime_window_handle_state_change(LiMeWindow *window, glong *data)
{
  g_return_if_fail(LIME_IS_WINDOW(window));
  g_return_if_fail(data != NULL);

  g_signal_emit(window, window_signals[SIGNAL_STATE_CHANGED], 0);
}

/**
 * lime_window_theme_changed:
 * @window: A #LiMeWindow
 *
 * Notify window of theme change
 */
void
lime_window_theme_changed(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  /* Redraw window decorations */
}

/**
 * lime_window_screen_resized:
 * @window: A #LiMeWindow
 * @new_width: New screen width
 * @new_height: New screen height
 *
 * Notify window of screen resize
 */
void
lime_window_screen_resized(LiMeWindow *window, gint new_width, gint new_height)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  /* Adjust window if it was maximized */
  if (window->maximized) {
    XResizeWindow(window->x11_display, window->xid, new_width, new_height);
    window->width = new_width;
    window->height = new_height;
  }
}

/**
 * lime_window_ensure_on_screen:
 * @window: A #LiMeWindow
 *
 * Ensure window is visible on screen
 */
void
lime_window_ensure_on_screen(LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  int screen = DefaultScreen(window->x11_display);
  int screen_width = DisplayWidth(window->x11_display, screen);
  int screen_height = DisplayHeight(window->x11_display, screen);

  gint new_x = window->x;
  gint new_y = window->y;

  /* Clamp to screen bounds */
  if (new_x + window->width > screen_width) {
    new_x = screen_width - window->width;
  }
  if (new_y + window->height > screen_height) {
    new_y = screen_height - window->height;
  }
  if (new_x < 0) new_x = 0;
  if (new_y < 0) new_y = 0;

  if (new_x != window->x || new_y != window->y) {
    lime_window_move(window, new_x, new_y);
  }
}

/**
 * lime_window_get_xid:
 * @window: A #LiMeWindow
 *
 * Get the X11 window ID
 *
 * Returns: The X11 window ID
 */
Window
lime_window_get_xid(LiMeWindow *window)
{
  g_return_val_if_fail(LIME_IS_WINDOW(window), None);

  return window->xid;
}

/**
 * lime_window_get_name:
 * @window: A #LiMeWindow
 *
 * Get the window name/title
 *
 * Returns: (transfer none): The window title
 */
const gchar *
lime_window_get_name(LiMeWindow *window)
{
  g_return_val_if_fail(LIME_IS_WINDOW(window), NULL);

  return window->name;
}

/**
 * lime_window_get_class:
 * @window: A #LiMeWindow
 *
 * Get the window class
 *
 * Returns: (transfer none): The window class
 */
const gchar *
lime_window_get_class(LiMeWindow *window)
{
  g_return_val_if_fail(LIME_IS_WINDOW(window), NULL);

  return window->class;
}

/**
 * lime_window_get_geometry:
 * @window: A #LiMeWindow
 * @x: (out): X coordinate
 * @y: (out): Y coordinate
 * @width: (out): Width
 * @height: (out): Height
 *
 * Get window geometry
 */
void
lime_window_get_geometry(LiMeWindow *window,
                         gint *x, gint *y,
                         gint *width, gint *height)
{
  g_return_if_fail(LIME_IS_WINDOW(window));

  if (x) *x = window->x;
  if (y) *y = window->y;
  if (width) *width = window->width;
  if (height) *height = window->height;
}

/**
 * lime_window_is_focused:
 * @window: A #LiMeWindow
 *
 * Check if window has focus
 *
 * Returns: %TRUE if focused
 */
gboolean
lime_window_is_focused(LiMeWindow *window)
{
  g_return_val_if_fail(LIME_IS_WINDOW(window), FALSE);

  return window->focused;
}

/**
 * lime_window_is_maximized:
 * @window: A #LiMeWindow
 *
 * Check if window is maximized
 *
 * Returns: %TRUE if maximized
 */
gboolean
lime_window_is_maximized(LiMeWindow *window)
{
  g_return_val_if_fail(LIME_IS_WINDOW(window), FALSE);

  return window->maximized;
}

/**
 * lime_window_is_minimized:
 * @window: A #LiMeWindow
 *
 * Check if window is minimized
 *
 * Returns: %TRUE if minimized
 */
gboolean
lime_window_is_minimized(LiMeWindow *window)
{
  g_return_val_if_fail(LIME_IS_WINDOW(window), FALSE);

  return window->minimized;
}

/**
 * lime_window_get_window_type:
 * @window: A #LiMeWindow
 *
 * Get the window type
 *
 * Returns: The window type
 */
gint
lime_window_get_window_type(LiMeWindow *window)
{
  g_return_val_if_fail(LIME_IS_WINDOW(window), LIME_WINDOW_TYPE_NORMAL);

  return window->window_type;
}
