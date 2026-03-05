/*
 * LiMe Workspace Header
 */

#ifndef __LIME_WORKSPACE_H__
#define __LIME_WORKSPACE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _LiMeWorkspace LiMeWorkspace;
typedef struct _LiMeWorkspaceClass LiMeWorkspaceClass;

#define LIME_TYPE_WORKSPACE (lime_workspace_get_type())
#define LIME_WORKSPACE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_WORKSPACE, LiMeWorkspace))
#define LIME_IS_WORKSPACE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_WORKSPACE))

typedef struct {
  GObjectClass parent;
} LiMeWorkspaceClass;

GType lime_workspace_get_type(void);

LiMeWorkspace * lime_workspace_new(gint index, const gchar *name_fmt, int name_arg);

void lime_workspace_add_window(LiMeWorkspace *ws, gpointer window);
void lime_workspace_remove_window(LiMeWorkspace *ws, gpointer window);

void lime_workspace_show_windows(LiMeWorkspace *ws);
void lime_workspace_hide_windows(LiMeWorkspace *ws);

GList * lime_workspace_get_windows(LiMeWorkspace *ws);
gint lime_workspace_get_num_windows(LiMeWorkspace *ws);

const gchar * lime_workspace_get_name(LiMeWorkspace *ws);
gint lime_workspace_get_index(LiMeWorkspace *ws);

G_END_DECLS

#endif /* __LIME_WORKSPACE_H__ */
