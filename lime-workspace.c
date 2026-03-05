/*
 * LiMe Workspace Management
 * Virtual workspace implementation
 */

#include "lime-workspace.h"
#include <string.h>
#include <glib.h>

struct _LiMeWorkspace {
  GObject parent;

  gint index;
  gchar *name;

  GQueue *windows;
  GQueue *hidden_windows;

  GSettings *settings;
  gboolean modified;
};

G_DEFINE_TYPE(LiMeWorkspace, lime_workspace, G_TYPE_OBJECT);

static void
lime_workspace_class_init(LiMeWorkspaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = lime_workspace_dispose;
  object_class->finalize = lime_workspace_finalize;
}

static void
lime_workspace_init(LiMeWorkspace *ws)
{
  ws->index = 0;
  ws->name = NULL;
  ws->windows = g_queue_new();
  ws->hidden_windows = g_queue_new();
  ws->settings = g_settings_new("org.cinnamon.workspaces");
  ws->modified = FALSE;
}

static void
lime_workspace_dispose(GObject *object)
{
  LiMeWorkspace *ws = LIME_WORKSPACE(object);

  if (ws->windows) {
    g_queue_free(ws->windows);
    ws->windows = NULL;
  }

  if (ws->hidden_windows) {
    g_queue_free(ws->hidden_windows);
    ws->hidden_windows = NULL;
  }

  g_clear_object(&ws->settings);

  G_OBJECT_CLASS(lime_workspace_parent_class)->dispose(object);
}

static void
lime_workspace_finalize(GObject *object)
{
  LiMeWorkspace *ws = LIME_WORKSPACE(object);
  g_free(ws->name);
  G_OBJECT_CLASS(lime_workspace_parent_class)->finalize(object);
}

LiMeWorkspace *
lime_workspace_new(gint index, const gchar *name_fmt, int name_arg)
{
  LiMeWorkspace *ws = g_object_new(LIME_TYPE_WORKSPACE, NULL);

  ws->index = index;
  if (name_fmt) {
    ws->name = g_strdup_printf(name_fmt, name_arg);
  } else {
    ws->name = g_strdup_printf("Workspace %d", index + 1);
  }

  g_debug("Workspace created: %s", ws->name);

  return ws;
}

void
lime_workspace_add_window(LiMeWorkspace *ws, gpointer window)
{
  g_return_if_fail(LIME_IS_WORKSPACE(ws));
  g_return_if_fail(window != NULL);

  if (!g_queue_find(ws->windows, window)) {
    g_queue_push_tail(ws->windows, window);
    ws->modified = TRUE;
  }
}

void
lime_workspace_remove_window(LiMeWorkspace *ws, gpointer window)
{
  g_return_if_fail(LIME_IS_WORKSPACE(ws));
  g_return_if_fail(window != NULL);

  if (g_queue_remove(ws->windows, window)) {
    ws->modified = TRUE;
  }
}

void
lime_workspace_show_windows(LiMeWorkspace *ws)
{
  g_return_if_fail(LIME_IS_WORKSPACE(ws));

  GList *link = g_queue_peek_head_link(ws->windows);
  while (link) {
    /* Show window */
    link = link->next;
  }
}

void
lime_workspace_hide_windows(LiMeWorkspace *ws)
{
  g_return_if_fail(LIME_IS_WORKSPACE(ws));

  GList *link = g_queue_peek_head_link(ws->windows);
  while (link) {
    /* Hide window */
    link = link->next;
  }
}

GList *
lime_workspace_get_windows(LiMeWorkspace *ws)
{
  g_return_val_if_fail(LIME_IS_WORKSPACE(ws), NULL);

  return g_queue_peek_head_link(ws->windows);
}

gint
lime_workspace_get_num_windows(LiMeWorkspace *ws)
{
  g_return_val_if_fail(LIME_IS_WORKSPACE(ws), 0);

  return g_queue_get_length(ws->windows);
}

const gchar *
lime_workspace_get_name(LiMeWorkspace *ws)
{
  g_return_val_if_fail(LIME_IS_WORKSPACE(ws), NULL);

  return ws->name;
}

gint
lime_workspace_get_index(LiMeWorkspace *ws)
{
  g_return_val_if_fail(LIME_IS_WORKSPACE(ws), -1);

  return ws->index;
}
