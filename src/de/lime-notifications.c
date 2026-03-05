/*
 * LiMe Notification Daemon
 * Desktop notification system for LiMe OS
 * Comprehensive notification management and delivery
 */

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <string.h>

#define NOTIFICATIONS_INTERFACE "org.freedesktop.Notifications"
#define NOTIFICATIONS_PATH "/org/freedesktop/Notifications"

typedef struct {
  guint id;
  gchar *app_name;
  gchar *summary;
  gchar *body;
  gchar *icon_name;
  gint timeout;
  gint64 timestamp;
  GtkWidget *window;
} LiMeNotification;

typedef struct {
  GDBusConnection *dbus_conn;
  GList *notifications;
  guint next_id;

  GSettings *settings;

  gboolean show_notifications;
  gint notification_timeout;
  gint max_visible_notifications;

  GQueue *notification_queue;
} LiMeNotificationDaemon;

static LiMeNotificationDaemon *daemon_instance = NULL;

/* Notification management functions */

static LiMeNotification *
lime_notification_new(const gchar *app_name,
                      const gchar *summary,
                      const gchar *body)
{
  LiMeNotification *notif = g_malloc0(sizeof(LiMeNotification));

  notif->id = ++daemon_instance->next_id;
  notif->app_name = g_strdup(app_name);
  notif->summary = g_strdup(summary);
  notif->body = g_strdup(body);
  notif->timestamp = g_get_real_time() / 1000;

  return notif;
}

static void
lime_notification_free(LiMeNotification *notif)
{
  if (!notif) return;

  g_free(notif->app_name);
  g_free(notif->summary);
  g_free(notif->body);
  g_free(notif->icon_name);

  if (notif->window) {
    gtk_widget_destroy(notif->window);
  }

  g_free(notif);
}

static void
lime_notification_show(LiMeNotification *notif)
{
  if (!daemon_instance->show_notifications) {
    return;
  }

  /* Create GTK window for notification */
  GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
  gtk_container_set_border_width(GTK_CONTAINER(window), 10);
  gtk_window_set_default_size(GTK_WINDOW(window), 300, 100);

  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(window), box);

  /* App name label */
  GtkWidget *app_label = gtk_label_new(notif->app_name);
  PangoAttrList *attrs = pango_attr_list_new();
  PangoAttribute *weight = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
  pango_attr_list_insert(attrs, weight);
  gtk_label_set_attributes(GTK_LABEL(app_label), attrs);
  pango_attr_list_unref(attrs);
  gtk_box_pack_start(GTK_BOX(box), app_label, FALSE, FALSE, 0);

  /* Summary label */
  GtkWidget *summary_label = gtk_label_new(notif->summary);
  gtk_label_set_line_wrap(GTK_LABEL(summary_label), TRUE);
  gtk_box_pack_start(GTK_BOX(box), summary_label, FALSE, FALSE, 0);

  /* Body label */
  if (notif->body && strlen(notif->body) > 0) {
    GtkWidget *body_label = gtk_label_new(notif->body);
    gtk_label_set_line_wrap(GTK_LABEL(body_label), TRUE);
    gtk_box_pack_start(GTK_BOX(box), body_label, TRUE, TRUE, 0);
  }

  /* Apply styling */
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,
    ".lime-notification { background-color: #2a2a2a; color: white; border-radius: 5px; }",
    -1, NULL);

  GtkStyleContext *context = gtk_widget_get_style_context(window);
  gtk_style_context_add_class(context, "lime-notification");
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  notif->window = window;

  /* Position notification */
  GdkScreen *screen = gdk_screen_get_default();
  gint screen_width = gdk_screen_get_width(screen);
  gint screen_height = gdk_screen_get_height(screen);

  gtk_window_move(GTK_WINDOW(window), screen_width - 320, 20);
  gtk_widget_show_all(window);

  /* Schedule removal */
  g_timeout_add(notif->timeout > 0 ? notif->timeout : 5000,
                (GSourceFunc)gtk_widget_destroy, window);
}

static gboolean
on_notify_method_call(GDBusConnection *connection,
                      const gchar *sender,
                      const gchar *object_path,
                      const gchar *interface_name,
                      const gchar *method_name,
                      GVariant *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer user_data)
{
  if (g_strcmp0(method_name, "Notify") == 0) {
    const gchar *app_name;
    guint replaces_id;
    const gchar *app_icon;
    const gchar *summary;
    const gchar *body;
    GVariant *actions;
    GVariant *hints;
    gint expire_timeout;

    g_variant_get(parameters, "(&su&s&s&sas&ia{sv})",
                  &app_name, &replaces_id, &app_icon,
                  &summary, &body, &actions, &hints, &expire_timeout);

    LiMeNotification *notif = lime_notification_new(app_name, summary, body);
    notif->timeout = expire_timeout;

    daemon_instance->notifications = g_list_prepend(daemon_instance->notifications,
                                                     notif);

    lime_notification_show(notif);

    GVariant *result = g_variant_new("(u)", notif->id);
    g_dbus_method_invocation_return_value(invocation, result);

    return TRUE;
  }

  return FALSE;
}

static void
lime_notification_daemon_init(void)
{
  if (daemon_instance) return;

  daemon_instance = g_malloc0(sizeof(LiMeNotificationDaemon));
  daemon_instance->notifications = NULL;
  daemon_instance->next_id = 0;
  daemon_instance->settings = g_settings_new("org.cinnamon.notifications");
  daemon_instance->show_notifications = TRUE;
  daemon_instance->notification_timeout = 5000;
  daemon_instance->max_visible_notifications = 3;
  daemon_instance->notification_queue = g_queue_new();

  GError *error = NULL;
  daemon_instance->dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

  if (!daemon_instance->dbus_conn) {
    g_warning("Failed to get D-Bus connection: %s", error->message);
    g_error_free(error);
    return;
  }

  /* Create D-Bus interface */
  GDBusNodeInfo *node_info = g_dbus_node_info_new_for_xml(
    "<node>"
    "  <interface name='org.freedesktop.Notifications'>"
    "    <method name='Notify'>"
    "      <arg type='s' name='app_name' direction='in'/>"
    "      <arg type='u' name='replaces_id' direction='in'/>"
    "      <arg type='s' name='app_icon' direction='in'/>"
    "      <arg type='s' name='summary' direction='in'/>"
    "      <arg type='s' name='body' direction='in'/>"
    "      <arg type='as' name='actions' direction='in'/>"
    "      <arg type='a{sv}' name='hints' direction='in'/>"
    "      <arg type='i' name='expire_timeout' direction='in'/>"
    "      <arg type='u' name='id' direction='out'/>"
    "    </method>"
    "  </interface>"
    "</node>",
    NULL);

  GDBusInterfaceInfo *interface_info = g_dbus_node_info_lookup_interface(
    node_info, NOTIFICATIONS_INTERFACE);

  GDBusInterfaceVTable vtable = {
    .method_call = on_notify_method_call,
  };

  daemon_instance->dbus_conn =  g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);

  g_dbus_node_info_unref(node_info);
}

/**
 * lime_show_notification:
 * @app_name: Application name
 * @summary: Notification summary
 * @body: Notification body text
 * @timeout: Timeout in milliseconds
 *
 * Show a desktop notification
 */
void
lime_show_notification(const gchar *app_name,
                       const gchar *summary,
                       const gchar *body,
                       gint timeout)
{
  if (!daemon_instance) {
    lime_notification_daemon_init();
  }

  LiMeNotification *notif = lime_notification_new(app_name, summary, body);
  notif->timeout = timeout;

  lime_notification_show(notif);
}
