/*
 * LiMe Network Manager Header
 * WiFi, Ethernet, and network connection management
 */

#ifndef __LIME_NETWORK_MANAGER_H__
#define __LIME_NETWORK_MANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_NETWORK_MANAGER (lime_network_manager_get_type())
#define LIME_NETWORK_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_NETWORK_MANAGER, LiMeNetworkManager))
#define LIME_IS_NETWORK_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_NETWORK_MANAGER))

typedef struct _LiMeNetworkManager LiMeNetworkManager;
typedef struct _LiMeNetworkManagerClass LiMeNetworkManagerClass;

typedef enum {
  LIME_CONNECTION_DISCONNECTED,
  LIME_CONNECTION_CONNECTING,
  LIME_CONNECTION_CONNECTED,
  LIME_CONNECTION_FAILED
} LiMeConnectionState;

typedef enum {
  LIME_CONNECTION_ETHERNET,
  LIME_CONNECTION_WIFI,
  LIME_CONNECTION_CELLULAR,
  LIME_CONNECTION_VPN,
  LIME_CONNECTION_UNKNOWN
} LiMeConnectionType;

typedef enum {
  LIME_WIFI_SECURITY_NONE,
  LIME_WIFI_SECURITY_WEP,
  LIME_WIFI_SECURITY_WPA,
  LIME_WIFI_SECURITY_WPA2,
  LIME_WIFI_SECURITY_WPA3
} LiMeWiFiSecurity;

typedef struct {
  gchar *ssid;
  gchar *bssid;
  gint signal_strength;
  LiMeWiFiSecurity security;
  gboolean is_connected;
  gint channel;
  gchar *frequency;
} LiMeWiFiNetwork;

typedef struct {
  gchar *interface_name;
  gchar *mac_address;
  gchar *ip_address;
  gchar *netmask;
  gchar *gateway;
  gchar *dns_servers;
  LiMeConnectionType type;
  LiMeConnectionState state;
  gboolean is_active;
  gdouble speed_mbps;
} LiMeNetworkInterface;

GType lime_network_manager_get_type(void);

LiMeNetworkManager * lime_network_manager_new(void);

/* Status queries */
LiMeConnectionState lime_network_manager_get_state(LiMeNetworkManager *manager);
gboolean lime_network_manager_is_connected(LiMeNetworkManager *manager);

/* Interface management */
GList * lime_network_manager_get_interfaces(LiMeNetworkManager *manager);
LiMeNetworkInterface * lime_network_manager_get_interface(LiMeNetworkManager *manager,
                                                           const gchar *interface_name);
gint lime_network_manager_get_interface_count(LiMeNetworkManager *manager);

/* WiFi scanning and connection */
GList * lime_network_manager_scan_wifi_networks(LiMeNetworkManager *manager);
gboolean lime_network_manager_connect_wifi(LiMeNetworkManager *manager,
                                            const gchar *ssid,
                                            const gchar *password);
gboolean lime_network_manager_disconnect_wifi(LiMeNetworkManager *manager);
gchar * lime_network_manager_get_connected_wifi(LiMeNetworkManager *manager);

/* Ethernet management */
gboolean lime_network_manager_enable_ethernet(LiMeNetworkManager *manager,
                                               const gchar *interface_name);
gboolean lime_network_manager_disable_ethernet(LiMeNetworkManager *manager,
                                                const gchar *interface_name);
gboolean lime_network_manager_set_static_ip(LiMeNetworkManager *manager,
                                             const gchar *interface_name,
                                             const gchar *ip_address,
                                             const gchar *netmask,
                                             const gchar *gateway);
gboolean lime_network_manager_enable_dhcp(LiMeNetworkManager *manager,
                                           const gchar *interface_name);

/* Network settings */
gboolean lime_network_manager_set_dns(LiMeNetworkManager *manager,
                                       const gchar *dns_servers);
gchar * lime_network_manager_get_dns(LiMeNetworkManager *manager);

gboolean lime_network_manager_enable_ipv6(LiMeNetworkManager *manager);
gboolean lime_network_manager_disable_ipv6(LiMeNetworkManager *manager);

gboolean lime_network_manager_set_hostname(LiMeNetworkManager *manager,
                                            const gchar *hostname);
gchar * lime_network_manager_get_hostname(LiMeNetworkManager *manager);

/* Proxy settings */
gboolean lime_network_manager_set_proxy(LiMeNetworkManager *manager,
                                         const gchar *proxy_url);
gchar * lime_network_manager_get_proxy(LiMeNetworkManager *manager);
gboolean lime_network_manager_disable_proxy(LiMeNetworkManager *manager);

/* VPN support */
gboolean lime_network_manager_add_vpn(LiMeNetworkManager *manager,
                                       const gchar *vpn_name,
                                       const gchar *vpn_type,
                                       const gchar *config);
gboolean lime_network_manager_connect_vpn(LiMeNetworkManager *manager,
                                           const gchar *vpn_name);
gboolean lime_network_manager_disconnect_vpn(LiMeNetworkManager *manager);

/* Network statistics */
gdouble lime_network_manager_get_download_speed(LiMeNetworkManager *manager);
gdouble lime_network_manager_get_upload_speed(LiMeNetworkManager *manager);
guint64 lime_network_manager_get_bytes_downloaded(LiMeNetworkManager *manager);
guint64 lime_network_manager_get_bytes_uploaded(LiMeNetworkManager *manager);

/* Configuration persistence */
gboolean lime_network_manager_save_config(LiMeNetworkManager *manager);
gboolean lime_network_manager_load_config(LiMeNetworkManager *manager);

G_END_DECLS

#endif /* __LIME_NETWORK_MANAGER_H__ */
