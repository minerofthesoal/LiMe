/*
 * LiMe Panel Header
 * Panel system interface
 */

#ifndef __LIME_PANEL_H__
#define __LIME_PANEL_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _LiMePanel LiMePanel;
typedef struct _LiMePanelClass LiMePanelClass;

#define LIME_TYPE_PANEL (lime_panel_get_type())
#define LIME_PANEL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_PANEL, LiMePanel))
#define LIME_IS_PANEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_PANEL))

typedef enum {
  LIME_PANEL_POSITION_TOP,
  LIME_PANEL_POSITION_BOTTOM,
  LIME_PANEL_POSITION_LEFT,
  LIME_PANEL_POSITION_RIGHT
} LiMePanelPosition;

typedef struct {
  GObjectClass parent;
} LiMePanelClass;

GType lime_panel_get_type(void);

LiMePanel * lime_panel_new(LiMePanelPosition position, gint width, gint height);

void lime_panel_add_clock(LiMePanel *panel);
void lime_panel_add_system_tray(LiMePanel *panel);
void lime_panel_add_sound_control(LiMePanel *panel);
void lime_panel_add_network_widget(LiMePanel *panel);
void lime_panel_add_power_button(LiMePanel *panel);
void lime_panel_add_window_list(LiMePanel *panel, GHashTable *windows);
void lime_panel_add_app_launcher(LiMePanel *panel);
void lime_panel_add_workspace_switcher(LiMePanel *panel, gint num_workspaces);
void lime_panel_add_ai_button(LiMePanel *panel);

void lime_panel_update(LiMePanel *panel);
void lime_panel_update_active_window(LiMePanel *panel, gpointer window);

void lime_panel_set_width(LiMePanel *panel, gint width);
void lime_panel_show(LiMePanel *panel);
void lime_panel_hide(LiMePanel *panel);

G_END_DECLS

#endif /* __LIME_PANEL_H__ */
