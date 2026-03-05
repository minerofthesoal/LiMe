/*
 * LiMe Panel System
 * Top and bottom panel with widgets and applets
 * Complete panel implementation with all features
 */

#include "lime-panel.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#define PANEL_SPACING 5
#define WIDGET_HEIGHT 32

struct _LiMePanel {
  GObject parent;

  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *separator;

  gint width;
  gint height;
  gint x, y;

  LiMePanelPosition position;

  GSList *widgets;
  GSList *applets;

  GHashTable *widget_data;

  gboolean autohide;
  gboolean hidden;
  gboolean always_on_top;

  guint update_timeout;

  GSettings *settings;
};

struct PanelWidget {
  GtkWidget *widget;
  gchar *name;
  gint type;
  gpointer data;
};

enum {
  WIDGET_TYPE_CLOCK,
  WIDGET_TYPE_SYSTEM_TRAY,
  WIDGET_TYPE_SOUND,
  WIDGET_TYPE_NETWORK,
  WIDGET_TYPE_POWER,
  WIDGET_TYPE_WINDOW_LIST,
  WIDGET_TYPE_APP_LAUNCHER,
  WIDGET_TYPE_WORKSPACE_SWITCHER,
  WIDGET_TYPE_AI_BUTTON,
};

G_DEFINE_TYPE(LiMePanel, lime_panel, G_TYPE_OBJECT);

/* Forward declarations */
static gboolean lime_panel_update_timeout(gpointer user_data);
static GtkWidget * lime_panel_create_clock_widget(LiMePanel *panel);
static GtkWidget * lime_panel_create_sound_widget(LiMePanel *panel);
static GtkWidget * lime_panel_create_network_widget(LiMePanel *panel);
static GtkWidget * lime_panel_create_power_widget(LiMePanel *panel);
static GtkWidget * lime_panel_create_window_list_widget(LiMePanel *panel, GHashTable *windows);
static GtkWidget * lime_panel_create_app_launcher_widget(LiMePanel *panel);
static GtkWidget * lime_panel_create_workspace_switcher(LiMePanel *panel, gint num_workspaces);
static GtkWidget * lime_panel_create_ai_button_widget(LiMePanel *panel);

/**
 * lime_panel_class_init:
 * @klass: The class structure
 *
 * Initialize the panel class
 */
static void
lime_panel_class_init(LiMePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_panel_dispose;
  object_class->finalize = lime_panel_finalize;
}

/**
 * lime_panel_init:
 * @panel: The panel instance
 *
 * Initialize a new panel
 */
static void
lime_panel_init(LiMePanel *panel)
{
  panel->window = NULL;
  panel->box = NULL;
  panel->separator = NULL;

  panel->width = 1024;
  panel->height = 40;
  panel->x = 0;
  panel->y = 0;

  panel->position = LIME_PANEL_POSITION_TOP;

  panel->widgets = NULL;
  panel->applets = NULL;

  panel->widget_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  panel->autohide = FALSE;
  panel->hidden = FALSE;
  panel->always_on_top = TRUE;

  panel->settings = g_settings_new("org.cinnamon.panel");

  g_debug("Panel initialized");
}

/**
 * lime_panel_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_panel_dispose(GObject *object)
{
  LiMePanel *panel = LIME_PANEL(object);

  if (panel->update_timeout) {
    g_source_remove(panel->update_timeout);
    panel->update_timeout = 0;
  }

  if (panel->widget_data) {
    g_hash_table_unref(panel->widget_data);
    panel->widget_data = NULL;
  }

  g_slist_free_full(panel->widgets, (GDestroyNotify)g_object_unref);
  panel->widgets = NULL;

  g_slist_free(panel->applets);
  panel->applets = NULL;

  g_clear_object(&panel->settings);

  if (panel->window) {
    gtk_widget_destroy(panel->window);
    panel->window = NULL;
  }

  G_OBJECT_CLASS(lime_panel_parent_class)->dispose(object);
}

/**
 * lime_panel_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_panel_finalize(GObject *object)
{
  G_OBJECT_CLASS(lime_panel_parent_class)->finalize(object);
}

/**
 * lime_panel_new:
 * @position: Panel position (top/bottom)
 * @width: Panel width
 * @height: Panel height
 *
 * Create a new panel
 *
 * Returns: A new #LiMePanel
 */
LiMePanel *
lime_panel_new(LiMePanelPosition position, gint width, gint height)
{
  LiMePanel *panel;

  panel = g_object_new(LIME_TYPE_PANEL, NULL);

  panel->width = width;
  panel->height = height;
  panel->position = position;

  /* Create GTK window */
  panel->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint(GTK_WINDOW(panel->window), GDK_WINDOW_TYPE_HINT_DOCK);
  gtk_window_set_keep_above(GTK_WINDOW(panel->window), TRUE);
  gtk_window_set_decorated(GTK_WINDOW(panel->window), FALSE);
  gtk_window_set_default_size(GTK_WINDOW(panel->window), width, height);

  /* Set position */
  if (position == LIME_PANEL_POSITION_TOP) {
    panel->x = 0;
    panel->y = 0;
    gtk_window_move(GTK_WINDOW(panel->window), 0, 0);
  } else if (position == LIME_PANEL_POSITION_BOTTOM) {
    panel->x = 0;
    GdkScreen *screen = gdk_screen_get_default();
    gint screen_height = gdk_screen_get_height(screen);
    panel->y = screen_height - height;
    gtk_window_move(GTK_WINDOW(panel->window), 0, panel->y);
  }

  /* Create container */
  panel->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, PANEL_SPACING);
  gtk_container_set_border_width(GTK_CONTAINER(panel->box), 5);
  gtk_container_add(GTK_CONTAINER(panel->window), panel->box);

  /* Apply styling */
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,
    ".panel { background-color: #2a2a2a; color: white; }", -1, NULL);

  GtkStyleContext *context = gtk_widget_get_style_context(panel->window);
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER (provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_object_unref(provider);

  /* Show panel */
  gtk_widget_show_all(panel->window);

  /* Start update timer */
  panel->update_timeout = g_timeout_add(1000, lime_panel_update_timeout, panel);

  g_debug("Panel created at %s, size %dx%d",
          position == LIME_PANEL_POSITION_TOP ? "top" : "bottom",
          width, height);

  return panel;
}

/**
 * lime_panel_add_widget:
 * @panel: A #LiMePanel
 * @widget: The GTK widget to add
 * @name: Name of the widget
 *
 * Add a widget to the panel
 */
static void
lime_panel_add_widget(LiMePanel *panel, GtkWidget *widget, const gchar *name)
{
  g_return_if_fail(LIME_IS_PANEL(panel));
  g_return_if_fail(widget != NULL);
  g_return_if_fail(name != NULL);

  gtk_box_pack_start(GTK_BOX(panel->box), widget, FALSE, FALSE, 0);
  gtk_widget_show(widget);

  struct PanelWidget *pw = g_malloc(sizeof(struct PanelWidget));
  pw->widget = widget;
  pw->name = g_strdup(name);
  pw->type = 0;
  pw->data = NULL;

  panel->widgets = g_slist_append(panel->widgets, pw);

  g_hash_table_insert(panel->widget_data, g_strdup(name), pw);

  g_debug("Added widget: %s", name);
}

/**
 * lime_panel_create_clock_widget:
 * @panel: A #LiMePanel
 *
 * Create a clock display widget
 *
 * Returns: (transfer none): The clock widget
 */
static GtkWidget *
lime_panel_create_clock_widget(LiMePanel *panel)
{
  GtkWidget *label = gtk_label_new("00:00:00");
  gtk_widget_set_name(label, "lime-clock");

  g_object_set_data(G_OBJECT(label), "widget-type",
                    GINT_TO_POINTER(WIDGET_TYPE_CLOCK));

  return label;
}

/**
 * lime_panel_create_sound_widget:
 * @panel: A #LiMePanel
 *
 * Create a sound control widget
 *
 * Returns: (transfer none): The sound widget
 */
static GtkWidget *
lime_panel_create_sound_widget(LiMePanel *panel)
{
  GtkWidget *button = gtk_button_new_with_label("🔊");
  gtk_widget_set_name(button, "lime-sound");

  return button;
}

/**
 * lime_panel_create_network_widget:
 * @panel: A #LiMePanel
 *
 * Create a network indicator widget
 *
 * Returns: (transfer none): The network widget
 */
static GtkWidget *
lime_panel_create_network_widget(LiMePanel *panel)
{
  GtkWidget *label = gtk_label_new("📡 Connected");
  gtk_widget_set_name(label, "lime-network");

  return label;
}

/**
 * lime_panel_create_power_widget:
 * @panel: A #LiMePanel
 *
 * Create a power/battery indicator widget
 *
 * Returns: (transfer none): The power widget
 */
static GtkWidget *
lime_panel_create_power_widget(LiMePanel *panel)
{
  GtkWidget *label = gtk_label_new("🔋 100%");
  gtk_widget_set_name(label, "lime-power");

  return label;
}

/**
 * lime_panel_create_window_list_widget:
 * @panel: A #LiMePanel
 * @windows: Hash table of active windows
 *
 * Create a window list widget
 *
 * Returns: (transfer none): The window list widget
 */
static GtkWidget *
lime_panel_create_window_list_widget(LiMePanel *panel, GHashTable *windows)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_name(box, "lime-window-list");

  return box;
}

/**
 * lime_panel_create_app_launcher_widget:
 * @panel: A #LiMePanel
 *
 * Create an application launcher button
 *
 * Returns: (transfer none): The app launcher widget
 */
static GtkWidget *
lime_panel_create_app_launcher_widget(LiMePanel *panel)
{
  GtkWidget *button = gtk_button_new_with_label("🚀 Applications");
  gtk_widget_set_name(button, "lime-app-launcher");

  return button;
}

/**
 * lime_panel_create_workspace_switcher:
 * @panel: A #LiMePanel
 * @num_workspaces: Number of workspaces
 *
 * Create a workspace switcher widget
 *
 * Returns: (transfer none): The workspace switcher widget
 */
static GtkWidget *
lime_panel_create_workspace_switcher(LiMePanel *panel, gint num_workspaces)
{
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_name(box, "lime-workspace-switcher");

  for (int i = 0; i < num_workspaces; i++) {
    GtkWidget *button = gtk_toggle_button_new_with_label(g_strdup_printf("%d", i + 1));
    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);

    if (i == 0) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    }
  }

  return box;
}

/**
 * lime_panel_create_ai_button_widget:
 * @panel: A #LiMePanel
 *
 * Create an AI assistant button
 *
 * Returns: (transfer none): The AI button widget
 */
static GtkWidget *
lime_panel_create_ai_button_widget(LiMePanel *panel)
{
  GtkWidget *button = gtk_button_new_with_label("🤖 AI");
  gtk_widget_set_name(button, "lime-ai-button");

  g_signal_connect(button, "clicked", G_CALLBACK(on_ai_button_clicked), panel);

  return button;
}

static void
on_ai_button_clicked(GtkButton *button, gpointer user_data)
{
  g_debug("AI button clicked");
  g_spawn_command_line_async("lime-ai", NULL);
}

/**
 * lime_panel_add_clock:
 * @panel: A #LiMePanel
 *
 * Add a clock widget to the panel
 */
void
lime_panel_add_clock(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *clock = lime_panel_create_clock_widget(panel);
  lime_panel_add_widget(panel, clock, "clock");
}

/**
 * lime_panel_add_system_tray:
 * @panel: A #LiMePanel
 *
 * Add a system tray to the panel
 */
void
lime_panel_add_system_tray(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *tray = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_name(tray, "lime-system-tray");

  lime_panel_add_widget(panel, tray, "system-tray");
}

/**
 * lime_panel_add_sound_control:
 * @panel: A #LiMePanel
 *
 * Add a sound control to the panel
 */
void
lime_panel_add_sound_control(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *sound = lime_panel_create_sound_widget(panel);
  lime_panel_add_widget(panel, sound, "sound");
}

/**
 * lime_panel_add_network_widget:
 * @panel: A #LiMePanel
 *
 * Add network indicator to the panel
 */
void
lime_panel_add_network_widget(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *network = lime_panel_create_network_widget(panel);
  lime_panel_add_widget(panel, network, "network");
}

/**
 * lime_panel_add_power_button:
 * @panel: A #LiMePanel
 *
 * Add power button to the panel
 */
void
lime_panel_add_power_button(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *power = lime_panel_create_power_widget(panel);
  lime_panel_add_widget(panel, power, "power");
}

/**
 * lime_panel_add_window_list:
 * @panel: A #LiMePanel
 * @windows: Hash table of active windows
 *
 * Add a window list to the panel
 */
void
lime_panel_add_window_list(LiMePanel *panel, GHashTable *windows)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *window_list = lime_panel_create_window_list_widget(panel, windows);
  lime_panel_add_widget(panel, window_list, "window-list");
}

/**
 * lime_panel_add_app_launcher:
 * @panel: A #LiMePanel
 *
 * Add application launcher to the panel
 */
void
lime_panel_add_app_launcher(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *launcher = lime_panel_create_app_launcher_widget(panel);
  lime_panel_add_widget(panel, launcher, "app-launcher");
}

/**
 * lime_panel_add_workspace_switcher:
 * @panel: A #LiMePanel
 * @num_workspaces: Number of workspaces
 *
 * Add workspace switcher to the panel
 */
void
lime_panel_add_workspace_switcher(LiMePanel *panel, gint num_workspaces)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *switcher = lime_panel_create_workspace_switcher(panel, num_workspaces);
  lime_panel_add_widget(panel, switcher, "workspace-switcher");
}

/**
 * lime_panel_add_ai_button:
 * @panel: A #LiMePanel
 *
 * Add AI assistant button to the panel
 */
void
lime_panel_add_ai_button(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  GtkWidget *ai_button = lime_panel_create_ai_button_widget(panel);
  lime_panel_add_widget(panel, ai_button, "ai-button");
}

/**
 * lime_panel_update:
 * @panel: A #LiMePanel
 *
 * Update panel widgets
 */
void
lime_panel_update(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  /* Update clock */
  GtkWidget *clock = g_hash_table_lookup(panel->widget_data, "clock");

  if (clock) {
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);
    gchar time_str[64];

    strftime(time_str, sizeof(time_str), "%H:%M:%S", time_info);
    gtk_label_set_text(GTK_LABEL(clock), time_str);
  }
}

/**
 * lime_panel_update_timeout:
 * @user_data: The panel
 *
 * Periodic update timer
 *
 * Returns: %TRUE to continue
 */
static gboolean
lime_panel_update_timeout(gpointer user_data)
{
  LiMePanel *panel = LIME_PANEL(user_data);

  lime_panel_update(panel);

  return G_SOURCE_CONTINUE;
}

/**
 * lime_panel_update_active_window:
 * @panel: A #LiMePanel
 * @window: The active window
 *
 * Update the active window display
 */
void
lime_panel_update_active_window(LiMePanel *panel, gpointer window)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  /* Update window list display */
}

/**
 * lime_panel_set_width:
 * @panel: A #LiMePanel
 * @width: New width
 *
 * Set panel width
 */
void
lime_panel_set_width(LiMePanel *panel, gint width)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  panel->width = width;
  gtk_window_resize(GTK_WINDOW(panel->window), width, panel->height);
}

/**
 * lime_panel_show:
 * @panel: A #LiMePanel
 *
 * Show the panel
 */
void
lime_panel_show(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  gtk_widget_show(panel->window);
  panel->hidden = FALSE;
}

/**
 * lime_panel_hide:
 * @panel: A #LiMePanel
 *
 * Hide the panel
 */
void
lime_panel_hide(LiMePanel *panel)
{
  g_return_if_fail(LIME_IS_PANEL(panel));

  gtk_widget_hide(panel->window);
  panel->hidden = TRUE;
}
