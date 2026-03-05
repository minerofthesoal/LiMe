/*
 * LiMe Window Manager
 * X11 window and window stack management
 * Full implementation of window management functions
 */

#include "lime-window-manager.h"
#include "lime-window.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>
#include <gdk/gdkx.h>

#define WINDOW_MANAGER_MASK (SubstructureNotifyMask | SubstructureRedirectMask)
#define MAX_WINDOWS 256

struct _LiMeWindowManager {
  GObject parent;

  Display *x11_display;
  Window root;
  Screen *x11_screen;
  GdkScreen *gdk_screen;

  GHashTable *windows;
  GQueue *window_stack;

  Window active_window;
  Window last_active_window;

  GHashTable *window_decorations;

  guint event_source;
  gboolean managing;

  Atom net_active_window;
  Atom net_wm_state;
  Atom net_wm_window_type;
  Atom net_client_list;
  Atom net_current_desktop;

  Cursor normal_cursor;
  Cursor resize_cursor;
  Cursor move_cursor;

  gint screen_width;
  gint screen_height;

  GSettings *settings;
  gboolean focus_follows_mouse;
  gboolean click_to_focus;
  gboolean raise_on_click;

  gint workspaces;
  gint *window_workspace_mapping;

  GHashTable *window_groups;

  guint focus_timer;
  guint update_timer;
};

G_DEFINE_TYPE(LiMeWindowManager, lime_window_manager, G_TYPE_OBJECT);

/* Signal handlers */
enum {
  SIGNAL_WINDOW_OPENED,
  SIGNAL_WINDOW_CLOSED,
  SIGNAL_FOCUS_CHANGED,
  SIGNAL_STACKING_CHANGED,
  LAST_SIGNAL
};

static guint wm_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations */
static gboolean lime_window_manager_process_x11_events(gpointer user_data);
static void lime_window_manager_manage_window(LiMeWindowManager *wm, Window w);
static void lime_window_manager_unmanage_window(LiMeWindowManager *wm, Window w);
static void lime_window_manager_update_client_list(LiMeWindowManager *wm);
static void lime_window_manager_update_active_window(LiMeWindowManager *wm);
static void lime_window_manager_update_window_stacking(LiMeWindowManager *wm);
static void lime_window_manager_focus_window_impl(LiMeWindowManager *wm, Window w);

/**
 * lime_window_manager_class_init:
 * @klass: The class structure
 *
 * Initialize the window manager class
 */
static void
lime_window_manager_class_init(LiMeWindowManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_window_manager_dispose;
  object_class->finalize = lime_window_manager_finalize;

  /* Signals */
  wm_signals[SIGNAL_WINDOW_OPENED] =
    g_signal_new("window-opened",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE, 1, LIME_TYPE_WINDOW);

  wm_signals[SIGNAL_WINDOW_CLOSED] =
    g_signal_new("window-closed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE, 1, LIME_TYPE_WINDOW);

  wm_signals[SIGNAL_FOCUS_CHANGED] =
    g_signal_new("focus-window-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__OBJECT,
                 G_TYPE_NONE, 1, LIME_TYPE_WINDOW);

  wm_signals[SIGNAL_STACKING_CHANGED] =
    g_signal_new("stacking-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

/**
 * lime_window_manager_init:
 * @wm: The window manager instance
 *
 * Initialize a new window manager
 */
static void
lime_window_manager_init(LiMeWindowManager *wm)
{
  wm->windows = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                      NULL, g_object_unref);
  wm->window_stack = g_queue_new();
  wm->window_decorations = g_hash_table_new(g_direct_hash, g_direct_equal);
  wm->window_groups = g_hash_table_new(g_direct_hash, g_direct_equal);

  wm->active_window = None;
  wm->last_active_window = None;
  wm->managing = FALSE;

  wm->focus_follows_mouse = FALSE;
  wm->click_to_focus = TRUE;
  wm->raise_on_click = TRUE;
  wm->workspaces = 4;

  g_debug("Window manager initialized");
}

/**
 * lime_window_manager_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_window_manager_dispose(GObject *object)
{
  LiMeWindowManager *wm = LIME_WINDOW_MANAGER(object);

  if (wm->event_source) {
    g_source_remove(wm->event_source);
    wm->event_source = 0;
  }

  if (wm->focus_timer) {
    g_source_remove(wm->focus_timer);
    wm->focus_timer = 0;
  }

  if (wm->update_timer) {
    g_source_remove(wm->update_timer);
    wm->update_timer = 0;
  }

  if (wm->windows) {
    g_hash_table_unref(wm->windows);
    wm->windows = NULL;
  }

  if (wm->window_stack) {
    g_queue_free(wm->window_stack);
    wm->window_stack = NULL;
  }

  if (wm->window_decorations) {
    g_hash_table_unref(wm->window_decorations);
    wm->window_decorations = NULL;
  }

  if (wm->window_groups) {
    g_hash_table_unref(wm->window_groups);
    wm->window_groups = NULL;
  }

  if (wm->window_workspace_mapping) {
    g_free(wm->window_workspace_mapping);
    wm->window_workspace_mapping = NULL;
  }

  g_clear_object(&wm->settings);

  if (wm->normal_cursor) {
    XFreeCursor(wm->x11_display, wm->normal_cursor);
    wm->normal_cursor = None;
  }

  if (wm->resize_cursor) {
    XFreeCursor(wm->x11_display, wm->resize_cursor);
    wm->resize_cursor = None;
  }

  if (wm->move_cursor) {
    XFreeCursor(wm->x11_display, wm->move_cursor);
    wm->move_cursor = None;
  }

  G_OBJECT_CLASS(lime_window_manager_parent_class)->dispose(object);
}

/**
 * lime_window_manager_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_window_manager_finalize(GObject *object)
{
  G_OBJECT_CLASS(lime_window_manager_parent_class)->finalize(object);
}

/**
 * lime_window_manager_new:
 * @x11_display: The X11 display
 * @gdk_screen: The GdkScreen
 *
 * Create a new window manager
 *
 * Returns: A new #LiMeWindowManager
 */
LiMeWindowManager *
lime_window_manager_new(Display *x11_display, GdkScreen *gdk_screen)
{
  LiMeWindowManager *wm;

  g_return_val_if_fail(x11_display != NULL, NULL);
  g_return_val_if_fail(gdk_screen != NULL, NULL);

  wm = g_object_new(LIME_TYPE_WINDOW_MANAGER, NULL);

  wm->x11_display = x11_display;
  wm->gdk_screen = gdk_screen;
  wm->x11_screen = DefaultScreenOfDisplay(x11_display);
  wm->root = RootWindowOfScreen(wm->x11_screen);

  wm->screen_width = WidthOfScreen(wm->x11_screen);
  wm->screen_height = HeightOfScreen(wm->x11_screen);

  /* Initialize atoms */
  wm->net_active_window = XInternAtom(x11_display, "_NET_ACTIVE_WINDOW", False);
  wm->net_wm_state = XInternAtom(x11_display, "_NET_WM_STATE", False);
  wm->net_wm_window_type = XInternAtom(x11_display, "_NET_WM_WINDOW_TYPE", False);
  wm->net_client_list = XInternAtom(x11_display, "_NET_CLIENT_LIST", False);
  wm->net_current_desktop = XInternAtom(x11_display, "_NET_CURRENT_DESKTOP", False);

  /* Initialize cursors */
  wm->normal_cursor = XCreateFontCursor(x11_display, XC_left_ptr);
  wm->resize_cursor = XCreateFontCursor(x11_display, XC_fleur);
  wm->move_cursor = XCreateFontCursor(x11_display, XC_move);

  /* Load settings */
  wm->settings = g_settings_new("org.cinnamon.wm");

  /* Allocate window workspace mapping */
  wm->window_workspace_mapping = g_malloc0(sizeof(gint) * MAX_WINDOWS);

  return wm;
}

/**
 * lime_window_manager_manage_existing_windows:
 * @wm: A #LiMeWindowManager
 *
 * Scan and manage existing windows
 */
void
lime_window_manager_manage_existing_windows(LiMeWindowManager *wm)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  g_debug("Managing existing windows");

  Window root_return, parent_return;
  Window *children;
  unsigned int nchildren, i;

  XQueryTree(wm->x11_display, wm->root,
             &root_return, &parent_return,
             &children, &nchildren);

  for (i = 0; i < nchildren; i++) {
    XWindowAttributes attrs;

    if (XGetWindowAttributes(wm->x11_display, children[i], &attrs)) {
      if (attrs.map_state != IsUnmapped &&
          attrs.override_redirect == False) {
        lime_window_manager_manage_window(wm, children[i]);
      }
    }
  }

  if (children) {
    XFree(children);
  }

  /* Start event processing */
  wm->managing = TRUE;
  wm->event_source = g_idle_add(lime_window_manager_process_x11_events, wm);

  g_debug("Now managing %d windows", g_hash_table_size(wm->windows));
}

/**
 * lime_window_manager_manage_window:
 * @wm: A #LiMeWindowManager
 * @w: The window to manage
 *
 * Start managing a window
 */
static void
lime_window_manager_manage_window(LiMeWindowManager *wm, Window w)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  if (g_hash_table_contains(wm->windows, GUINT_TO_POINTER(w))) {
    return; /* Already managing */
  }

  LiMeWindow *window = lime_window_new(wm->x11_display, w);

  if (window) {
    g_hash_table_insert(wm->windows, GUINT_TO_POINTER(w), window);
    g_queue_push_tail(wm->window_stack, g_object_ref(window));

    g_signal_emit(LIME_WINDOW_MANAGER(wm), wm_signals[SIGNAL_WINDOW_OPENED], 0, window);

    g_debug("Opened window: %s (%lu)", lime_window_get_name(window), w);
  }
}

/**
 * lime_window_manager_unmanage_window:
 * @wm: A #LiMeWindowManager
 * @w: The window to stop managing
 *
 * Stop managing a window
 */
static void
lime_window_manager_unmanage_window(LiMeWindowManager *wm, Window w)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  LiMeWindow *window = g_hash_table_lookup(wm->windows, GUINT_TO_POINTER(w));

  if (window) {
    g_signal_emit(LIME_WINDOW_MANAGER(wm), wm_signals[SIGNAL_WINDOW_CLOSED], 0, window);

    g_queue_remove(wm->window_stack, window);
    g_hash_table_remove(wm->windows, GUINT_TO_POINTER(w));

    g_debug("Closed window: %s (%lu)", lime_window_get_name(window), w);
  }
}

/**
 * lime_window_manager_process_x11_events:
 * @user_data: The window manager
 *
 * Process X11 events
 *
 * Returns: %TRUE to continue processing
 */
static gboolean
lime_window_manager_process_x11_events(gpointer user_data)
{
  LiMeWindowManager *wm = LIME_WINDOW_MANAGER(user_data);
  XEvent event;

  if (!wm->managing) {
    return G_SOURCE_REMOVE;
  }

  while (XPending(wm->x11_display)) {
    XNextEvent(wm->x11_display, &event);

    switch (event.type) {
      case CreateNotify:
        lime_window_manager_manage_window(wm, event.xcreatewindow.window);
        break;

      case DestroyNotify:
        lime_window_manager_unmanage_window(wm, event.xdestroywindow.window);
        break;

      case MapNotify:
        {
          LiMeWindow *window = g_hash_table_lookup(wm->windows,
                                                     GUINT_TO_POINTER(event.xmap.window));
          if (window) {
            lime_window_map(window);
          }
        }
        break;

      case UnmapNotify:
        {
          LiMeWindow *window = g_hash_table_lookup(wm->windows,
                                                     GUINT_TO_POINTER(event.xunmap.window));
          if (window) {
            lime_window_unmap(window);
          }
        }
        break;

      case FocusIn:
        {
          LiMeWindow *window = g_hash_table_lookup(wm->windows,
                                                     GUINT_TO_POINTER(event.xfocus.window));
          if (window) {
            wm->active_window = event.xfocus.window;
            g_signal_emit(LIME_WINDOW_MANAGER(wm), wm_signals[SIGNAL_FOCUS_CHANGED], 0, window);
          }
        }
        break;

      case ConfigureRequest:
        {
          XConfigureRequestEvent *cr = &event.xconfigurerequest;
          XWindowChanges wc;
          unsigned int mask = cr->value_mask;

          if (mask & CWX) wc.x = cr->x;
          if (mask & CWY) wc.y = cr->y;
          if (mask & CWWidth) wc.width = cr->width;
          if (mask & CWHeight) wc.height = cr->height;
          if (mask & CWBorderWidth) wc.border_width = cr->border_width;
          if (mask & CWSibling) wc.sibling = cr->above;
          if (mask & CWStackMode) wc.stack_mode = cr->detail;

          XConfigureWindow(wm->x11_display, cr->window, mask, &wc);
        }
        break;

      case PropertyNotify:
        {
          LiMeWindow *window = g_hash_table_lookup(wm->windows,
                                                     GUINT_TO_POINTER(event.xproperty.window));
          if (window && event.xproperty.atom == XA_WM_NAME) {
            lime_window_update_properties(window);
          }
        }
        break;

      case ClientMessage:
        if (event.xclient.message_type == wm->net_wm_state) {
          /* Handle WM state changes */
          LiMeWindow *window = g_hash_table_lookup(wm->windows,
                                                     GUINT_TO_POINTER(event.xclient.window));
          if (window) {
            lime_window_handle_state_change(window, event.xclient.data.l);
          }
        }
        break;

      default:
        break;
    }
  }

  lime_window_manager_update_active_window(wm);
  lime_window_manager_update_window_stacking(wm);

  return G_SOURCE_CONTINUE;
}

/**
 * lime_window_manager_focus_window:
 * @wm: A #LiMeWindowManager
 * @window: The window to focus
 *
 * Focus a window
 */
void
lime_window_manager_focus_window(LiMeWindowManager *wm, LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));
  g_return_if_fail(LIME_IS_WINDOW(window));

  Window xid = lime_window_get_xid(window);
  lime_window_manager_focus_window_impl(wm, xid);
}

/**
 * lime_window_manager_focus_window_impl:
 * @wm: A #LiMeWindowManager
 * @w: The X11 window ID to focus
 *
 * Internal window focus implementation
 */
static void
lime_window_manager_focus_window_impl(LiMeWindowManager *wm, Window w)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  XSetInputFocus(wm->x11_display, w, RevertToPointerRoot, CurrentTime);
  XRaiseWindow(wm->x11_display, w);

  wm->last_active_window = wm->active_window;
  wm->active_window = w;
}

/**
 * lime_window_manager_close_window:
 * @wm: A #LiMeWindowManager
 * @window: The window to close
 *
 * Close a window
 */
void
lime_window_manager_close_window(LiMeWindowManager *wm, LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));
  g_return_if_fail(LIME_IS_WINDOW(window));

  Window xid = lime_window_get_xid(window);

  /* Send WM_DELETE_WINDOW message first */
  Atom wm_delete = XInternAtom(wm->x11_display, "WM_DELETE_WINDOW", False);
  XClientMessageEvent msg;

  msg.type = ClientMessage;
  msg.window = xid;
  msg.message_type = XInternAtom(wm->x11_display, "WM_PROTOCOLS", False);
  msg.format = 32;
  msg.data.l[0] = wm_delete;
  msg.data.l[1] = CurrentTime;

  XSendEvent(wm->x11_display, xid, False, NoEventMask, (XEvent *)&msg);

  /* If that doesn't work, kill the window */
  XKillClient(wm->x11_display, xid);
}

/**
 * lime_window_manager_minimize_window:
 * @wm: A #LiMeWindowManager
 * @window: The window to minimize
 *
 * Minimize a window
 */
void
lime_window_manager_minimize_window(LiMeWindowManager *wm, LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));
  g_return_if_fail(LIME_IS_WINDOW(window));

  lime_window_iconify(window);
}

/**
 * lime_window_manager_maximize_window:
 * @wm: A #LiMeWindowManager
 * @window: The window to maximize
 *
 * Maximize a window
 */
void
lime_window_manager_maximize_window(LiMeWindowManager *wm, LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));
  g_return_if_fail(LIME_IS_WINDOW(window));

  lime_window_maximize(window);
}

/**
 * lime_window_manager_restore_window:
 * @wm: A #LiMeWindowManager
 * @window: The window to restore
 *
 * Restore a window to normal size
 */
void
lime_window_manager_restore_window(LiMeWindowManager *wm, LiMeWindow *window)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));
  g_return_if_fail(LIME_IS_WINDOW(window));

  lime_window_restore(window);
}

/**
 * lime_window_manager_move_window:
 * @wm: A #LiMeWindowManager
 * @window: The window to move
 * @x: New X coordinate
 * @y: New Y coordinate
 *
 * Move a window
 */
void
lime_window_manager_move_window(LiMeWindowManager *wm,
                                 LiMeWindow *window,
                                 gint x, gint y)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));
  g_return_if_fail(LIME_IS_WINDOW(window));

  Window xid = lime_window_get_xid(window);
  XMoveWindow(wm->x11_display, xid, x, y);
}

/**
 * lime_window_manager_resize_window:
 * @wm: A #LiMeWindowManager
 * @window: The window to resize
 * @width: New width
 * @height: New height
 *
 * Resize a window
 */
void
lime_window_manager_resize_window(LiMeWindowManager *wm,
                                   LiMeWindow *window,
                                   gint width, gint height)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));
  g_return_if_fail(LIME_IS_WINDOW(window));

  Window xid = lime_window_get_xid(window);
  XResizeWindow(wm->x11_display, xid, width, height);
}

/**
 * lime_window_manager_update_active_window:
 * @wm: A #LiMeWindowManager
 *
 * Update the active window property
 */
static void
lime_window_manager_update_active_window(LiMeWindowManager *wm)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  XChangeProperty(wm->x11_display, wm->root, wm->net_active_window,
                  XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)&wm->active_window, 1);
}

/**
 * lime_window_manager_update_client_list:
 * @wm: A #LiMeWindowManager
 *
 * Update the client list property
 */
static void
lime_window_manager_update_client_list(LiMeWindowManager *wm)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  guint n_windows = g_hash_table_size(wm->windows);
  Window *client_list = g_malloc(sizeof(Window) * n_windows);

  GHashTableIter iter;
  gpointer key;
  guint i = 0;

  g_hash_table_iter_init(&iter, wm->windows);
  while (g_hash_table_iter_next(&iter, &key, NULL)) {
    client_list[i++] = GPOINTER_TO_UINT(key);
  }

  XChangeProperty(wm->x11_display, wm->root, wm->net_client_list,
                  XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)client_list, n_windows);

  g_free(client_list);
}

/**
 * lime_window_manager_update_window_stacking:
 * @wm: A #LiMeWindowManager
 *
 * Update window stacking order
 */
static void
lime_window_manager_update_window_stacking(LiMeWindowManager *wm)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  guint n_windows = g_queue_get_length(wm->window_stack);
  Window *stacking = g_malloc(sizeof(Window) * n_windows);

  GList *link = g_queue_peek_head_link(wm->window_stack);
  guint i = 0;

  while (link) {
    LiMeWindow *window = LIME_WINDOW(link->data);
    stacking[i++] = lime_window_get_xid(window);
    link = link->next;
  }

  Atom net_client_list_stacking = XInternAtom(wm->x11_display,
                                              "_NET_CLIENT_LIST_STACKING", False);
  XChangeProperty(wm->x11_display, wm->root, net_client_list_stacking,
                  XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)stacking, n_windows);

  g_free(stacking);

  g_signal_emit(LIME_WINDOW_MANAGER(wm), wm_signals[SIGNAL_STACKING_CHANGED], 0);
}

/**
 * lime_window_manager_process_events:
 * @wm: A #LiMeWindowManager
 *
 * Process pending events
 */
void
lime_window_manager_process_events(LiMeWindowManager *wm)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  lime_window_manager_process_x11_events(wm);
}

/**
 * lime_window_manager_reorganize_monitors:
 * @wm: A #LiMeWindowManager
 * @num_monitors: New number of monitors
 *
 * Handle monitor configuration changes
 */
void
lime_window_manager_reorganize_monitors(LiMeWindowManager *wm, gint num_monitors)
{
  g_return_if_fail(LIME_IS_WINDOW_MANAGER(wm));

  g_debug("Reorganizing windows across %d monitors", num_monitors);

  /* Reposition windows that are off-screen */
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init(&iter, wm->windows);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    LiMeWindow *window = LIME_WINDOW(value);
    lime_window_ensure_on_screen(window);
  }
}

/**
 * lime_window_manager_get_windows:
 * @wm: A #LiMeWindowManager
 *
 * Get list of managed windows
 *
 * Returns: (element-type LiMeWindow) (transfer container): List of windows
 */
GList *
lime_window_manager_get_windows(LiMeWindowManager *wm)
{
  g_return_val_if_fail(LIME_IS_WINDOW_MANAGER(wm), NULL);

  return g_hash_table_get_values(wm->windows);
}

/**
 * lime_window_manager_get_active_window:
 * @wm: A #LiMeWindowManager
 *
 * Get the currently active window
 *
 * Returns: (transfer none): The active window or %NULL
 */
LiMeWindow *
lime_window_manager_get_active_window(LiMeWindowManager *wm)
{
  g_return_val_if_fail(LIME_IS_WINDOW_MANAGER(wm), NULL);

  return g_hash_table_lookup(wm->windows, GUINT_TO_POINTER(wm->active_window));
}

/**
 * lime_window_manager_get_num_windows:
 * @wm: A #LiMeWindowManager
 *
 * Get the number of managed windows
 *
 * Returns: Number of windows
 */
guint
lime_window_manager_get_num_windows(LiMeWindowManager *wm)
{
  g_return_val_if_fail(LIME_IS_WINDOW_MANAGER(wm), 0);

  return g_hash_table_size(wm->windows);
}
