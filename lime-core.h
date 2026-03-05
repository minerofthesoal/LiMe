/*
 * LiMe Core Header
 * Main desktop environment core interface
 */

#ifndef __LIME_CORE_H__
#define __LIME_CORE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _LiMeCore LiMeCore;
typedef struct _LiMeCoreClass LiMeCoreClass;
typedef struct _LiMeWindow LiMeWindow;
typedef struct _LiMeWindowManager LiMeWindowManager;
typedef struct _LiMePanel LiMePanel;
typedef struct _LiMeThemeManager LiMeThemeManager;
typedef struct _LiMeAIIntegration LiMeAIIntegration;
typedef struct _LiMeSettings LiMeSettings;
typedef struct _LiMeLauncher LiMeLauncher;
typedef struct _LiMeWorkspace LiMeWorkspace;

#define LIME_TYPE_CORE (lime_core_get_type())
#define LIME_CORE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_CORE, LiMeCore))
#define LIME_IS_CORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_CORE))
#define LIME_CORE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), LIME_TYPE_CORE, LiMeCoreClass))

typedef enum {
  SIGNAL_STARTUP,
  SIGNAL_READY,
  SIGNAL_WINDOW_OPENED,
  SIGNAL_WINDOW_CLOSED,
  SIGNAL_WORKSPACE_SWITCHED,
  SIGNAL_THEME_CHANGED,
  LAST_SIGNAL
} LiMeCoreSignal;

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
  GObjectClass parent;
} LiMeCoreClass;

GType lime_core_get_type(void);

LiMeCore * lime_core_get_default(void);

gboolean lime_core_startup(LiMeCore *core);
void lime_core_shutdown(LiMeCore *core);
void lime_core_run(LiMeCore *core);

gboolean lime_core_switch_workspace(LiMeCore *core, gint workspace_index);
void lime_core_add_window_to_workspace(LiMeCore *core,
                                        LiMeWindow *window,
                                        gint workspace_index);

LiMeWindow * lime_core_get_active_window(LiMeCore *core);
GList * lime_core_get_window_list(LiMeCore *core);

LiMeThemeManager * lime_core_get_theme_manager(LiMeCore *core);
LiMeAIIntegration * lime_core_get_ai_integration(LiMeCore *core);

guint32 lime_core_show_notification(LiMeCore *core,
                                     const gchar *title,
                                     const gchar *body,
                                     guint timeout);

void lime_core_set_desktop_locked(LiMeCore *core, gboolean locked);
void lime_core_reload_theme(LiMeCore *core);
void lime_core_get_screen_geometry(LiMeCore *core, gint *width, gint *height);

gint lime_core_get_num_workspaces(LiMeCore *core);
gint lime_core_get_current_workspace(LiMeCore *core);
gboolean lime_core_has_compositor(LiMeCore *core);

void lime_core_enable_fullscreen(LiMeCore *core, gboolean enable);

G_END_DECLS

#endif /* __LIME_CORE_H__ */
