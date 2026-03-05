/*
 * LiMe Window Header
 * Individual window object interface
 */

#ifndef __LIME_WINDOW_H__
#define __LIME_WINDOW_H__

#include <glib-object.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

typedef struct _LiMeWindow LiMeWindow;
typedef struct _LiMeWindowClass LiMeWindowClass;

#define LIME_TYPE_WINDOW (lime_window_get_type())
#define LIME_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_WINDOW, LiMeWindow))
#define LIME_IS_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_WINDOW))

typedef enum {
  LIME_WINDOW_TYPE_NORMAL,
  LIME_WINDOW_TYPE_DESKTOP,
  LIME_WINDOW_TYPE_DOCK,
  LIME_WINDOW_TYPE_TOOLBAR,
  LIME_WINDOW_TYPE_MENU,
  LIME_WINDOW_TYPE_UTILITY,
  LIME_WINDOW_TYPE_SPLASH,
  LIME_WINDOW_TYPE_DIALOG
} LiMeWindowType;

typedef struct {
  GObjectClass parent;
} LiMeWindowClass;

GType lime_window_get_type(void);

LiMeWindow * lime_window_new(Display *x11_display, Window xid);

void lime_window_map(LiMeWindow *window);
void lime_window_unmap(LiMeWindow *window);
void lime_window_focus(LiMeWindow *window);
void lime_window_unfocus(LiMeWindow *window);

void lime_window_iconify(LiMeWindow *window);
void lime_window_maximize(LiMeWindow *window);
void lime_window_restore(LiMeWindow *window);
void lime_window_close(LiMeWindow *window);

void lime_window_raise(LiMeWindow *window);
void lime_window_lower(LiMeWindow *window);

void lime_window_move(LiMeWindow *window, gint x, gint y);
void lime_window_resize(LiMeWindow *window, gint width, gint height);

void lime_window_update_properties(LiMeWindow *window);
void lime_window_handle_state_change(LiMeWindow *window, glong *data);
void lime_window_theme_changed(LiMeWindow *window);
void lime_window_screen_resized(LiMeWindow *window, gint new_width, gint new_height);
void lime_window_ensure_on_screen(LiMeWindow *window);

Window lime_window_get_xid(LiMeWindow *window);
const gchar * lime_window_get_name(LiMeWindow *window);
const gchar * lime_window_get_class(LiMeWindow *window);

void lime_window_get_geometry(LiMeWindow *window,
                               gint *x, gint *y,
                               gint *width, gint *height);

gboolean lime_window_is_focused(LiMeWindow *window);
gboolean lime_window_is_maximized(LiMeWindow *window);
gboolean lime_window_is_minimized(LiMeWindow *window);

gint lime_window_get_window_type(LiMeWindow *window);

G_END_DECLS

#endif /* __LIME_WINDOW_H__ */
