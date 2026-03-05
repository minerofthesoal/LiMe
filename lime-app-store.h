/*
 * LiMe App Store Header
 * Unified application marketplace supporting Flathub, KDE Store, AUR, and custom repositories
 */

#ifndef __LIME_APP_STORE_H__
#define __LIME_APP_STORE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_APP_STORE (lime_app_store_get_type())
#define LIME_APP_STORE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_APP_STORE, LiMeAppStore))
#define LIME_IS_APP_STORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_APP_STORE))

typedef struct _LiMeAppStore LiMeAppStore;
typedef struct _LiMeAppStoreClass LiMeAppStoreClass;

typedef enum {
  LIME_PACKAGE_SOURCE_FLATHUB,
  LIME_PACKAGE_SOURCE_KDE_STORE,
  LIME_PACKAGE_SOURCE_AUR,
  LIME_PACKAGE_SOURCE_ARCH,
  LIME_PACKAGE_SOURCE_CUSTOM
} LiMePackageSource;

typedef enum {
  LIME_PACKAGE_STATUS_NOT_INSTALLED,
  LIME_PACKAGE_STATUS_INSTALLED,
  LIME_PACKAGE_STATUS_UPDATE_AVAILABLE,
  LIME_PACKAGE_STATUS_INSTALLING,
  LIME_PACKAGE_STATUS_REMOVING,
  LIME_PACKAGE_STATUS_BROKEN
} LiMePackageStatus;

typedef struct {
  gchar *id;
  gchar *name;
  gchar *description;
  gchar *long_description;
  gchar *version;
  gchar *latest_version;
  LiMePackageSource source;
  LiMePackageStatus status;
  gchar *icon_url;
  gchar *screenshot_urls;
  gdouble rating;
  gint download_count;
  gchar *category;
  gchar *author;
  gchar *license;
  gchar *homepage;
  guint64 size;
  gboolean requires_runtime;
  GList *dependencies;
  GList *tags;
} LiMeAppStorePackage;

typedef struct {
  gchar *repo_id;
  gchar *repo_name;
  gchar *repo_url;
  LiMePackageSource repo_type;
  gboolean is_enabled;
} LiMeRepository;

GType lime_app_store_get_type(void);

LiMeAppStore * lime_app_store_new(void);

/* Browsing */
GList * lime_app_store_get_all_packages(LiMeAppStore *store);
GList * lime_app_store_get_category_packages(LiMeAppStore *store,
                                              const gchar *category);
GList * lime_app_store_get_featured_packages(LiMeAppStore *store);
GList * lime_app_store_get_trending_packages(LiMeAppStore *store);

/* Search */
GList * lime_app_store_search_packages(LiMeAppStore *store,
                                       const gchar *search_term);
GList * lime_app_store_search_by_category(LiMeAppStore *store,
                                          const gchar *category);
GList * lime_app_store_search_by_tag(LiMeAppStore *store,
                                      const gchar *tag);

/* Package information */
LiMeAppStorePackage * lime_app_store_get_package(LiMeAppStore *store,
                                                  const gchar *package_id);
GList * lime_app_store_get_installed_packages(LiMeAppStore *store);
GList * lime_app_store_get_updates(LiMeAppStore *store);

/* Installation and removal */
gboolean lime_app_store_install_package(LiMeAppStore *store,
                                        const gchar *package_id);
gboolean lime_app_store_remove_package(LiMeAppStore *store,
                                       const gchar *package_id);
gboolean lime_app_store_update_package(LiMeAppStore *store,
                                       const gchar *package_id);
gboolean lime_app_store_update_all_packages(LiMeAppStore *store);

/* Installation progress */
gdouble lime_app_store_get_installation_progress(LiMeAppStore *store,
                                                  const gchar *package_id);
gchar * lime_app_store_get_installation_status(LiMeAppStore *store,
                                                const gchar *package_id);

/* Repository management */
GList * lime_app_store_get_repositories(LiMeAppStore *store);
gboolean lime_app_store_add_repository(LiMeAppStore *store,
                                       const gchar *repo_url,
                                       const gchar *repo_name);
gboolean lime_app_store_remove_repository(LiMeAppStore *store,
                                          const gchar *repo_id);
gboolean lime_app_store_enable_repository(LiMeAppStore *store,
                                          const gchar *repo_id);
gboolean lime_app_store_disable_repository(LiMeAppStore *store,
                                           const gchar *repo_id);

/* Flathub integration */
gboolean lime_app_store_enable_flathub(LiMeAppStore *store);
gboolean lime_app_store_is_flathub_enabled(LiMeAppStore *store);

/* KDE Store integration */
gboolean lime_app_store_enable_kde_store(LiMeAppStore *store);
gboolean lime_app_store_is_kde_store_enabled(LiMeAppStore *store);

/* AUR integration */
gboolean lime_app_store_enable_aur(LiMeAppStore *store);
gboolean lime_app_store_is_aur_enabled(LiMeAppStore *store);
gboolean lime_app_store_install_aur_package(LiMeAppStore *store,
                                             const gchar *package_name);
gboolean lime_app_store_install_arch_package(LiMeAppStore *store,
                                              const gchar *package_name);

/* Ratings and reviews */
gdouble lime_app_store_get_rating(LiMeAppStore *store,
                                   const gchar *package_id);
gint lime_app_store_get_review_count(LiMeAppStore *store,
                                      const gchar *package_id);
gboolean lime_app_store_submit_review(LiMeAppStore *store,
                                       const gchar *package_id,
                                       gdouble rating,
                                       const gchar *review_text);

/* Favorites and recommendations */
gboolean lime_app_store_add_to_favorites(LiMeAppStore *store,
                                         const gchar *package_id);
gboolean lime_app_store_remove_from_favorites(LiMeAppStore *store,
                                              const gchar *package_id);
GList * lime_app_store_get_favorites(LiMeAppStore *store);
GList * lime_app_store_get_recommendations(LiMeAppStore *store);

/* System updates */
gboolean lime_app_store_check_system_updates(LiMeAppStore *store);
gboolean lime_app_store_install_system_updates(LiMeAppStore *store);
gint lime_app_store_get_pending_update_count(LiMeAppStore *store);

/* Configuration and caching */
gboolean lime_app_store_refresh_repositories(LiMeAppStore *store);
gboolean lime_app_store_clear_cache(LiMeAppStore *store);
gboolean lime_app_store_save_config(LiMeAppStore *store);
gboolean lime_app_store_load_config(LiMeAppStore *store);

G_END_DECLS

#endif /* __LIME_APP_STORE_H__ */
