/*
 * LiMe Window Manager Header
 * X11 window management interface
 */

#ifndef __LIME_WINDOW_MANAGER_H__
#define __LIME_WINDOW_MANAGER_H__

#include <glib-object.h>
#include <X11/Xlib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _LiMeWindowManager LiMeWindowManager;
typedef struct _LiMeWindowManagerClass LiMeWindowManagerClass;
typedef struct _LiMeWindow LiMeWindow;

#define LIME_TYPE_WINDOW_MANAGER (lime_window_manager_get_type())
#define LIME_WINDOW_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_WINDOW_MANAGER, LiMeWindowManager))
#define LIME_IS_WINDOW_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_WINDOW_MANAGER))

typedef struct {
  GObjectClass parent;
} LiMeWindowManagerClass;

GType lime_window_manager_get_type(void);

LiMeWindowManager * lime_window_manager_new(Display *x11_display, GdkScreen *gdk_screen);

void lime_window_manager_manage_existing_windows(LiMeWindowManager *wm);

void lime_window_manager_focus_window(LiMeWindowManager *wm, LiMeWindow *window);
void lime_window_manager_close_window(LiMeWindowManager *wm, LiMeWindow *window);
void lime_window_manager_minimize_window(LiMeWindowManager *wm, LiMeWindow *window);
void lime_window_manager_maximize_window(LiMeWindowManager *wm, LiMeWindow *window);
void lime_window_manager_restore_window(LiMeWindowManager *wm, LiMeWindow *window);

void lime_window_manager_move_window(LiMeWindowManager *wm,
                                      LiMeWindow *window,
                                      gint x, gint y);
void lime_window_manager_resize_window(LiMeWindowManager *wm,
                                        LiMeWindow *window,
                                        gint width, gint height);

void lime_window_manager_process_events(LiMeWindowManager *wm);
void lime_window_manager_reorganize_monitors(LiMeWindowManager *wm, gint num_monitors);

GList * lime_window_manager_get_windows(LiMeWindowManager *wm);
LiMeWindow * lime_window_manager_get_active_window(LiMeWindowManager *wm);
guint lime_window_manager_get_num_windows(LiMeWindowManager *wm);

G_END_DECLS

#endif /* __LIME_WINDOW_MANAGER_H__ */
