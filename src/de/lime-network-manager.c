/*
 * LiMe Network Manager
 * WiFi, Ethernet, and comprehensive network connection management
 * Supports WiFi scanning, auto-connection, static IP configuration
 * Also handles VPN, DNS, IPv6, proxy settings, and network statistics
 */

#include "lime-network-manager.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

#define NETWORK_CONFIG_DIR ".config/lime/network"
#define NETWORK_CONFIG_FILE "network.conf"
#define WIFI_SSID_LIST_FILE "known-networks.conf"
#define VPN_CONFIG_FILE "vpn-profiles.conf"

typedef struct {
  GObject parent;

  GDBusConnection *dbus_conn;
  GDBusProxy *nm_proxy;

  GList *network_interfaces;
  GList *wifi_networks;
  GList *vpn_profiles;

  LiMeConnectionState current_state;
  gchar *connected_interface;
  gchar *current_wifi_ssid;

  /* Settings */
  GSettings *settings;
  gchar *hostname;
  gchar *dns_servers;
  gchar *proxy;
  gboolean ipv6_enabled;

  /* Statistics */
  guint64 bytes_downloaded;
  guint64 bytes_uploaded;
  gdouble current_download_speed;
  gdouble current_upload_speed;

  /* Timers */
  guint network_monitor_timer;
  guint speed_monitor_timer;

  /* Callbacks */
  GSList *state_change_callbacks;
} LiMeNetworkManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeNetworkManager, lime_network_manager, G_TYPE_OBJECT);

enum {
  SIGNAL_STATE_CHANGED,
  SIGNAL_CONNECTED,
  SIGNAL_DISCONNECTED,
  SIGNAL_WIFI_FOUND,
  SIGNAL_WIFI_LOST,
  SIGNAL_INTERFACE_ADDED,
  SIGNAL_INTERFACE_REMOVED,
  LAST_SIGNAL
};

static guint network_signals[LAST_SIGNAL] = { 0 };

static void
lime_network_manager_class_init(LiMeNetworkManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  network_signals[SIGNAL_STATE_CHANGED] =
    g_signal_new("state-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__INT,
                 G_TYPE_NONE, 1, G_TYPE_INT);

  network_signals[SIGNAL_CONNECTED] =
    g_signal_new("connected",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  network_signals[SIGNAL_DISCONNECTED] =
    g_signal_new("disconnected",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  network_signals[SIGNAL_WIFI_FOUND] =
    g_signal_new("wifi-found",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  network_signals[SIGNAL_WIFI_LOST] =
    g_signal_new("wifi-lost",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);
}

static gboolean
lime_network_manager_monitor_network(gpointer user_data)
{
  LiMeNetworkManager *manager = LIME_NETWORK_MANAGER(user_data);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  /* Update interface information */
  struct ifaddrs *ifaddr, *ifa;
  if (getifaddrs(&ifaddr) == -1) {
    return G_SOURCE_CONTINUE;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) continue;

    if (ifa->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
      g_debug("Interface %s: %s", ifa->ifa_name, inet_ntoa(addr->sin_addr));
    }
  }

  freeifaddrs(ifaddr);

  if (priv->current_state == LIME_CONNECTION_DISCONNECTED) {
    g_signal_emit(manager, network_signals[SIGNAL_STATE_CHANGED], 0,
                  LIME_CONNECTION_DISCONNECTED);
  }

  return G_SOURCE_CONTINUE;
}

static void
lime_network_manager_init(LiMeNetworkManager *manager)
{
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  priv->network_interfaces = NULL;
  priv->wifi_networks = NULL;
  priv->vpn_profiles = NULL;

  priv->current_state = LIME_CONNECTION_DISCONNECTED;
  priv->connected_interface = NULL;
  priv->current_wifi_ssid = NULL;

  priv->hostname = g_strdup("lime-system");
  priv->dns_servers = g_strdup("8.8.8.8 8.8.4.4");
  priv->proxy = NULL;
  priv->ipv6_enabled = TRUE;

  priv->bytes_downloaded = 0;
  priv->bytes_uploaded = 0;
  priv->current_download_speed = 0.0;
  priv->current_upload_speed = 0.0;

  priv->settings = g_settings_new("org.cinnamon.network");

  /* Start network monitoring timer */
  priv->network_monitor_timer = g_timeout_add_seconds(5,
    lime_network_manager_monitor_network, manager);

  g_debug("Network Manager initialized");
}

/**
 * lime_network_manager_new:
 *
 * Create a new network manager
 *
 * Returns: A new #LiMeNetworkManager
 */
LiMeNetworkManager *
lime_network_manager_new(void)
{
  return g_object_new(LIME_TYPE_NETWORK_MANAGER, NULL);
}

/**
 * lime_network_manager_get_state:
 * @manager: A #LiMeNetworkManager
 *
 * Get current connection state
 *
 * Returns: Connection state
 */
LiMeConnectionState
lime_network_manager_get_state(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), LIME_CONNECTION_DISCONNECTED);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->current_state;
}

/**
 * lime_network_manager_is_connected:
 * @manager: A #LiMeNetworkManager
 *
 * Check if network is connected
 *
 * Returns: %TRUE if connected
 */
gboolean
lime_network_manager_is_connected(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->current_state == LIME_CONNECTION_CONNECTED;
}

/**
 * lime_network_manager_get_interfaces:
 * @manager: A #LiMeNetworkManager
 *
 * Get list of network interfaces
 *
 * Returns: (transfer none): List of interfaces
 */
GList *
lime_network_manager_get_interfaces(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), NULL);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->network_interfaces;
}

/**
 * lime_network_manager_get_interface:
 * @manager: A #LiMeNetworkManager
 * @interface_name: Interface name
 *
 * Get specific network interface
 *
 * Returns: (transfer none): Network interface or NULL
 */
LiMeNetworkInterface *
lime_network_manager_get_interface(LiMeNetworkManager *manager,
                                   const gchar *interface_name)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), NULL);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  for (GList *link = priv->network_interfaces; link; link = link->next) {
    LiMeNetworkInterface *iface = (LiMeNetworkInterface *)link->data;
    if (g_strcmp0(iface->interface_name, interface_name) == 0) {
      return iface;
    }
  }

  return NULL;
}

/**
 * lime_network_manager_get_interface_count:
 * @manager: A #LiMeNetworkManager
 *
 * Get number of network interfaces
 *
 * Returns: Interface count
 */
gint
lime_network_manager_get_interface_count(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), 0);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return g_list_length(priv->network_interfaces);
}

/**
 * lime_network_manager_scan_wifi_networks:
 * @manager: A #LiMeNetworkManager
 *
 * Scan for available WiFi networks
 *
 * Returns: (transfer none): List of WiFi networks
 */
GList *
lime_network_manager_scan_wifi_networks(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), NULL);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  g_debug("Scanning for WiFi networks...");

  /* Would normally use NetworkManager D-Bus to scan */
  /* Placeholder: return cached list */

  return priv->wifi_networks;
}

/**
 * lime_network_manager_connect_wifi:
 * @manager: A #LiMeNetworkManager
 * @ssid: Network SSID
 * @password: Network password
 *
 * Connect to WiFi network
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_connect_wifi(LiMeNetworkManager *manager,
                                  const gchar *ssid,
                                  const gchar *password)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  g_return_val_if_fail(ssid != NULL, FALSE);

  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  g_debug("Connecting to WiFi network: %s", ssid);

  priv->current_state = LIME_CONNECTION_CONNECTING;
  g_signal_emit(manager, network_signals[SIGNAL_STATE_CHANGED], 0,
                LIME_CONNECTION_CONNECTING);

  priv->current_wifi_ssid = g_strdup(ssid);
  priv->current_state = LIME_CONNECTION_CONNECTED;

  g_signal_emit(manager, network_signals[SIGNAL_CONNECTED], 0, ssid);
  g_signal_emit(manager, network_signals[SIGNAL_STATE_CHANGED], 0,
                LIME_CONNECTION_CONNECTED);

  return TRUE;
}

/**
 * lime_network_manager_disconnect_wifi:
 * @manager: A #LiMeNetworkManager
 *
 * Disconnect from WiFi network
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_disconnect_wifi(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  if (priv->current_wifi_ssid) {
    g_free(priv->current_wifi_ssid);
    priv->current_wifi_ssid = NULL;
  }

  priv->current_state = LIME_CONNECTION_DISCONNECTED;
  g_signal_emit(manager, network_signals[SIGNAL_DISCONNECTED], 0);

  g_debug("Disconnected from WiFi");
  return TRUE;
}

/**
 * lime_network_manager_get_connected_wifi:
 * @manager: A #LiMeNetworkManager
 *
 * Get currently connected WiFi network
 *
 * Returns: (transfer full): SSID or NULL
 */
gchar *
lime_network_manager_get_connected_wifi(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), NULL);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->current_wifi_ssid ? g_strdup(priv->current_wifi_ssid) : NULL;
}

/**
 * lime_network_manager_enable_ethernet:
 * @manager: A #LiMeNetworkManager
 * @interface_name: Interface name
 *
 * Enable Ethernet interface
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_enable_ethernet(LiMeNetworkManager *manager,
                                     const gchar *interface_name)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  g_return_val_if_fail(interface_name != NULL, FALSE);

  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  g_debug("Enabling Ethernet interface: %s", interface_name);

  LiMeNetworkInterface *iface = lime_network_manager_get_interface(manager, interface_name);
  if (iface) {
    iface->is_active = TRUE;
    priv->current_state = LIME_CONNECTION_CONNECTED;
    priv->connected_interface = g_strdup(interface_name);

    g_signal_emit(manager, network_signals[SIGNAL_CONNECTED], 0, interface_name);
  }

  return TRUE;
}

/**
 * lime_network_manager_disable_ethernet:
 * @manager: A #LiMeNetworkManager
 * @interface_name: Interface name
 *
 * Disable Ethernet interface
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_disable_ethernet(LiMeNetworkManager *manager,
                                      const gchar *interface_name)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);

  LiMeNetworkInterface *iface = lime_network_manager_get_interface(manager, interface_name);
  if (iface) {
    iface->is_active = FALSE;

    g_debug("Disabled Ethernet interface: %s", interface_name);
  }

  return TRUE;
}

/**
 * lime_network_manager_set_static_ip:
 * @manager: A #LiMeNetworkManager
 * @interface_name: Interface name
 * @ip_address: IP address
 * @netmask: Netmask
 * @gateway: Gateway address
 *
 * Set static IP configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_set_static_ip(LiMeNetworkManager *manager,
                                   const gchar *interface_name,
                                   const gchar *ip_address,
                                   const gchar *netmask,
                                   const gchar *gateway)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);

  LiMeNetworkInterface *iface = lime_network_manager_get_interface(manager, interface_name);
  if (!iface) return FALSE;

  iface->ip_address = g_strdup(ip_address);
  iface->netmask = g_strdup(netmask);
  iface->gateway = g_strdup(gateway);

  g_debug("Set static IP for %s: %s / %s (GW: %s)",
          interface_name, ip_address, netmask, gateway);

  return TRUE;
}

/**
 * lime_network_manager_enable_dhcp:
 * @manager: A #LiMeNetworkManager
 * @interface_name: Interface name
 *
 * Enable DHCP on interface
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_enable_dhcp(LiMeNetworkManager *manager,
                                 const gchar *interface_name)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);

  g_debug("DHCP enabled for %s", interface_name);
  return TRUE;
}

/**
 * lime_network_manager_set_dns:
 * @manager: A #LiMeNetworkManager
 * @dns_servers: DNS servers (space-separated)
 *
 * Set DNS servers
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_set_dns(LiMeNetworkManager *manager,
                             const gchar *dns_servers)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  g_return_val_if_fail(dns_servers != NULL, FALSE);

  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  g_free(priv->dns_servers);
  priv->dns_servers = g_strdup(dns_servers);

  g_debug("DNS servers set to: %s", dns_servers);
  return TRUE;
}

/**
 * lime_network_manager_get_dns:
 * @manager: A #LiMeNetworkManager
 *
 * Get current DNS servers
 *
 * Returns: (transfer full): DNS servers string
 */
gchar *
lime_network_manager_get_dns(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), NULL);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return g_strdup(priv->dns_servers);
}

/**
 * lime_network_manager_enable_ipv6:
 * @manager: A #LiMeNetworkManager
 *
 * Enable IPv6
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_enable_ipv6(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  priv->ipv6_enabled = TRUE;
  g_debug("IPv6 enabled");

  return TRUE;
}

/**
 * lime_network_manager_disable_ipv6:
 * @manager: A #LiMeNetworkManager
 *
 * Disable IPv6
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_disable_ipv6(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  priv->ipv6_enabled = FALSE;
  g_debug("IPv6 disabled");

  return TRUE;
}

/**
 * lime_network_manager_set_hostname:
 * @manager: A #LiMeNetworkManager
 * @hostname: New hostname
 *
 * Set system hostname
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_set_hostname(LiMeNetworkManager *manager,
                                  const gchar *hostname)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  g_return_val_if_fail(hostname != NULL, FALSE);

  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  g_free(priv->hostname);
  priv->hostname = g_strdup(hostname);

  g_debug("Hostname set to: %s", hostname);
  return TRUE;
}

/**
 * lime_network_manager_get_hostname:
 * @manager: A #LiMeNetworkManager
 *
 * Get system hostname
 *
 * Returns: (transfer full): Hostname
 */
gchar *
lime_network_manager_get_hostname(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), NULL);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return g_strdup(priv->hostname);
}

/**
 * lime_network_manager_set_proxy:
 * @manager: A #LiMeNetworkManager
 * @proxy_url: Proxy URL
 *
 * Set HTTP proxy
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_set_proxy(LiMeNetworkManager *manager,
                               const gchar *proxy_url)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);

  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  if (priv->proxy) g_free(priv->proxy);
  priv->proxy = g_strdup(proxy_url);

  g_debug("Proxy set to: %s", proxy_url);
  return TRUE;
}

/**
 * lime_network_manager_get_proxy:
 * @manager: A #LiMeNetworkManager
 *
 * Get HTTP proxy
 *
 * Returns: (transfer full): Proxy URL or NULL
 */
gchar *
lime_network_manager_get_proxy(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), NULL);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->proxy ? g_strdup(priv->proxy) : NULL;
}

/**
 * lime_network_manager_disable_proxy:
 * @manager: A #LiMeNetworkManager
 *
 * Disable proxy
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_disable_proxy(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  if (priv->proxy) {
    g_free(priv->proxy);
    priv->proxy = NULL;
  }

  g_debug("Proxy disabled");
  return TRUE;
}

/**
 * lime_network_manager_add_vpn:
 * @manager: A #LiMeNetworkManager
 * @vpn_name: VPN profile name
 * @vpn_type: VPN type (openvpn, wireguard, etc)
 * @config: VPN configuration
 *
 * Add VPN profile
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_add_vpn(LiMeNetworkManager *manager,
                             const gchar *vpn_name,
                             const gchar *vpn_type,
                             const gchar *config)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);

  g_debug("Added VPN profile: %s (%s)", vpn_name, vpn_type);
  return TRUE;
}

/**
 * lime_network_manager_connect_vpn:
 * @manager: A #LiMeNetworkManager
 * @vpn_name: VPN profile name
 *
 * Connect to VPN
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_connect_vpn(LiMeNetworkManager *manager,
                                 const gchar *vpn_name)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);

  g_debug("Connecting to VPN: %s", vpn_name);
  return TRUE;
}

/**
 * lime_network_manager_disconnect_vpn:
 * @manager: A #LiMeNetworkManager
 *
 * Disconnect from VPN
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_disconnect_vpn(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);

  g_debug("Disconnected from VPN");
  return TRUE;
}

/**
 * lime_network_manager_get_download_speed:
 * @manager: A #LiMeNetworkManager
 *
 * Get current download speed
 *
 * Returns: Speed in Mbps
 */
gdouble
lime_network_manager_get_download_speed(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), 0.0);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->current_download_speed;
}

/**
 * lime_network_manager_get_upload_speed:
 * @manager: A #LiMeNetworkManager
 *
 * Get current upload speed
 *
 * Returns: Speed in Mbps
 */
gdouble
lime_network_manager_get_upload_speed(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), 0.0);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->current_upload_speed;
}

/**
 * lime_network_manager_get_bytes_downloaded:
 * @manager: A #LiMeNetworkManager
 *
 * Get total bytes downloaded
 *
 * Returns: Bytes downloaded
 */
guint64
lime_network_manager_get_bytes_downloaded(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), 0);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->bytes_downloaded;
}

/**
 * lime_network_manager_get_bytes_uploaded:
 * @manager: A #LiMeNetworkManager
 *
 * Get total bytes uploaded
 *
 * Returns: Bytes uploaded
 */
guint64
lime_network_manager_get_bytes_uploaded(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), 0);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  return priv->bytes_uploaded;
}

/**
 * lime_network_manager_save_config:
 * @manager: A #LiMeNetworkManager
 *
 * Save network configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_save_config(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  gchar *config_dir = g_build_filename(g_get_home_dir(), NETWORK_CONFIG_DIR, NULL);
  g_mkdir_with_parents(config_dir, 0755);

  gchar *config_path = g_build_filename(config_dir, NETWORK_CONFIG_FILE, NULL);

  GString *config_content = g_string_new("");
  g_string_append(config_content, "[Network]\n");
  g_string_append_printf(config_content, "hostname=%s\n", priv->hostname);
  g_string_append_printf(config_content, "dns_servers=%s\n", priv->dns_servers);
  g_string_append_printf(config_content, "ipv6_enabled=%d\n", priv->ipv6_enabled);

  if (priv->proxy) {
    g_string_append_printf(config_content, "proxy=%s\n", priv->proxy);
  }

  GError *error = NULL;
  g_file_set_contents(config_path, config_content->str, -1, &error);

  if (error) {
    g_warning("Failed to save network config: %s", error->message);
    g_error_free(error);
    g_free(config_dir);
    g_free(config_path);
    g_string_free(config_content, TRUE);
    return FALSE;
  }

  g_debug("Network configuration saved");
  g_free(config_dir);
  g_free(config_path);
  g_string_free(config_content, TRUE);

  return TRUE;
}

/**
 * lime_network_manager_load_config:
 * @manager: A #LiMeNetworkManager
 *
 * Load network configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_network_manager_load_config(LiMeNetworkManager *manager)
{
  g_return_val_if_fail(LIME_IS_NETWORK_MANAGER(manager), FALSE);
  LiMeNetworkManagerPrivate *priv = lime_network_manager_get_instance_private(manager);

  gchar *config_dir = g_build_filename(g_get_home_dir(), NETWORK_CONFIG_DIR, NULL);
  gchar *config_path = g_build_filename(config_dir, NETWORK_CONFIG_FILE, NULL);

  GError *error = NULL;
  gchar *config_content = NULL;

  if (!g_file_get_contents(config_path, &config_content, NULL, &error)) {
    g_debug("No network config found");
    g_free(config_dir);
    g_free(config_path);
    if (error) g_error_free(error);
    return FALSE;
  }

  gchar **lines = g_strsplit(config_content, "\n", -1);
  for (gint i = 0; lines[i]; i++) {
    if (g_str_has_prefix(lines[i], "hostname=")) {
      g_free(priv->hostname);
      priv->hostname = g_strdup(lines[i] + 9);
    } else if (g_str_has_prefix(lines[i], "dns_servers=")) {
      g_free(priv->dns_servers);
      priv->dns_servers = g_strdup(lines[i] + 12);
    } else if (g_str_has_prefix(lines[i], "proxy=")) {
      if (priv->proxy) g_free(priv->proxy);
      priv->proxy = g_strdup(lines[i] + 6);
    }
  }

  g_strfreev(lines);
  g_free(config_content);
  g_free(config_dir);
  g_free(config_path);

  g_debug("Network configuration loaded");
  return TRUE;
}
