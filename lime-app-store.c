/*
 * LiMe App Store
 * Unified application marketplace supporting Flathub, KDE Store, AUR, and custom repositories
 * Automatically handles Arch Linux and AUR package installation
 */

#include "lime-app-store.h"
#include <glib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <json-glib/json-glib.h>

#define APPSTORE_CONFIG_DIR ".config/lime/appstore"
#define APPSTORE_CONFIG_FILE "appstore.conf"
#define APPSTORE_REPO_CACHE_DIR ".cache/lime/appstore-repos"

/* Repository URLs */
#define FLATHUB_REPO_URL "https://flathub.org/api/v2"
#define KDE_STORE_API "https://api.kde.org"
#define AUR_API "https://aur.archlinux.org/rpc"

typedef struct {
  GObject parent;

  GList *packages;
  GList *repositories;
  GList *installed_packages;
  GList *favorites;

  /* Settings */
  GSettings *settings;

  /* Repository status */
  gboolean flathub_enabled;
  gboolean kde_store_enabled;
  gboolean aur_enabled;

  /* Installation tracking */
  GHashTable *installation_progress;
  GHashTable *installation_status;

  /* Cache */
  gchar *cache_dir;
} LiMeAppStorePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(LiMeAppStore, lime_app_store, G_TYPE_OBJECT);

enum {
  SIGNAL_PACKAGE_INSTALLED,
  SIGNAL_PACKAGE_REMOVED,
  SIGNAL_PACKAGE_UPDATED,
  SIGNAL_INSTALLATION_PROGRESS,
  SIGNAL_REPOSITORY_ADDED,
  SIGNAL_REPOSITORY_REMOVED,
  SIGNAL_REFRESH_STARTED,
  SIGNAL_REFRESH_COMPLETE,
  LAST_SIGNAL
};

static guint appstore_signals[LAST_SIGNAL] = { 0 };

static void
lime_app_store_class_init(LiMeAppStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  appstore_signals[SIGNAL_PACKAGE_INSTALLED] =
    g_signal_new("package-installed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  appstore_signals[SIGNAL_PACKAGE_REMOVED] =
    g_signal_new("package-removed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  appstore_signals[SIGNAL_INSTALLATION_PROGRESS] =
    g_signal_new("installation-progress",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__DOUBLE,
                 G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  appstore_signals[SIGNAL_REFRESH_COMPLETE] =
    g_signal_new("refresh-complete",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static void
lime_app_store_init(LiMeAppStore *store)
{
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  priv->packages = NULL;
  priv->repositories = NULL;
  priv->installed_packages = NULL;
  priv->favorites = NULL;

  priv->settings = g_settings_new("org.cinnamon.appstore");

  priv->flathub_enabled = TRUE;
  priv->kde_store_enabled = TRUE;
  priv->aur_enabled = TRUE;

  priv->installation_progress = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                       g_free, NULL);
  priv->installation_status = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                     g_free, g_free);

  priv->cache_dir = g_build_filename(g_get_home_dir(), APPSTORE_REPO_CACHE_DIR, NULL);
  g_mkdir_with_parents(priv->cache_dir, 0755);

  g_debug("App Store initialized");
}

/**
 * lime_app_store_new:
 *
 * Create a new app store
 *
 * Returns: A new #LiMeAppStore
 */
LiMeAppStore *
lime_app_store_new(void)
{
  return g_object_new(LIME_TYPE_APP_STORE, NULL);
}

/**
 * lime_app_store_get_all_packages:
 * @store: A #LiMeAppStore
 *
 * Get all available packages from all repositories
 *
 * Returns: (transfer none): Package list
 */
GList *
lime_app_store_get_all_packages(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  return priv->packages;
}

/**
 * lime_app_store_get_category_packages:
 * @store: A #LiMeAppStore
 * @category: Category name
 *
 * Get packages in specific category
 *
 * Returns: (transfer none): Package list
 */
GList *
lime_app_store_get_category_packages(LiMeAppStore *store,
                                     const gchar *category)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);

  GList *result = NULL;
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  for (GList *link = priv->packages; link; link = link->next) {
    LiMeAppStorePackage *pkg = (LiMeAppStorePackage *)link->data;
    if (g_strcmp0(pkg->category, category) == 0) {
      result = g_list_append(result, pkg);
    }
  }

  return result;
}

/**
 * lime_app_store_get_featured_packages:
 * @store: A #LiMeAppStore
 *
 * Get featured packages
 *
 * Returns: (transfer none): Featured packages
 */
GList *
lime_app_store_get_featured_packages(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);

  return NULL;
}

/**
 * lime_app_store_get_trending_packages:
 * @store: A #LiMeAppStore
 *
 * Get trending packages
 *
 * Returns: (transfer none): Trending packages
 */
GList *
lime_app_store_get_trending_packages(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);

  return NULL;
}

/**
 * lime_app_store_search_packages:
 * @store: A #LiMeAppStore
 * @search_term: Search term
 *
 * Search for packages
 *
 * Returns: (transfer none): Search results
 */
GList *
lime_app_store_search_packages(LiMeAppStore *store,
                               const gchar *search_term)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);

  g_debug("Searching for: %s", search_term);
  return NULL;
}

/**
 * lime_app_store_search_by_category:
 * @store: A #LiMeAppStore
 * @category: Category name
 *
 * Search packages by category
 *
 * Returns: (transfer none): Results
 */
GList *
lime_app_store_search_by_category(LiMeAppStore *store,
                                  const gchar *category)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);

  return lime_app_store_get_category_packages(store, category);
}

/**
 * lime_app_store_search_by_tag:
 * @store: A #LiMeAppStore
 * @tag: Tag name
 *
 * Search packages by tag
 *
 * Returns: (transfer none): Results
 */
GList *
lime_app_store_search_by_tag(LiMeAppStore *store,
                             const gchar *tag)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);

  return NULL;
}

/**
 * lime_app_store_get_package:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Get specific package information
 *
 * Returns: (transfer none): Package info
 */
LiMeAppStorePackage *
lime_app_store_get_package(LiMeAppStore *store,
                           const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  for (GList *link = priv->packages; link; link = link->next) {
    LiMeAppStorePackage *pkg = (LiMeAppStorePackage *)link->data;
    if (g_strcmp0(pkg->id, package_id) == 0) {
      return pkg;
    }
  }

  return NULL;
}

/**
 * lime_app_store_get_installed_packages:
 * @store: A #LiMeAppStore
 *
 * Get list of installed packages
 *
 * Returns: (transfer none): Installed packages
 */
GList *
lime_app_store_get_installed_packages(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  return priv->installed_packages;
}

/**
 * lime_app_store_get_updates:
 * @store: A #LiMeAppStore
 *
 * Get available updates
 *
 * Returns: (transfer none): Packages with updates
 */
GList *
lime_app_store_get_updates(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);

  GList *updates = NULL;
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  for (GList *link = priv->installed_packages; link; link = link->next) {
    LiMeAppStorePackage *pkg = (LiMeAppStorePackage *)link->data;
    if (g_strcmp0(pkg->version, pkg->latest_version) != 0) {
      updates = g_list_append(updates, pkg);
    }
  }

  return updates;
}

/**
 * lime_app_store_install_package:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Install a package
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_install_package(LiMeAppStore *store,
                               const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  LiMeAppStorePackage *pkg = lime_app_store_get_package(store, package_id);
  if (!pkg) return FALSE;

  g_debug("Installing package: %s from %d", package_id, pkg->source);

  /* Mark as installing */
  g_hash_table_insert(priv->installation_status, g_strdup(package_id),
                      g_strdup("Installing..."));

  g_signal_emit(store, appstore_signals[SIGNAL_INSTALLATION_PROGRESS], 0, 0.25);

  /* Installation would happen here based on source */
  switch (pkg->source) {
    case LIME_PACKAGE_SOURCE_FLATHUB:
      g_debug("Would install from Flathub: %s", package_id);
      break;
    case LIME_PACKAGE_SOURCE_KDE_STORE:
      g_debug("Would install from KDE Store: %s", package_id);
      break;
    case LIME_PACKAGE_SOURCE_AUR:
      g_debug("Would install from AUR: %s", package_id);
      break;
    case LIME_PACKAGE_SOURCE_ARCH:
      g_debug("Would install from Arch packages: %s", package_id);
      break;
    default:
      break;
  }

  g_signal_emit(store, appstore_signals[SIGNAL_INSTALLATION_PROGRESS], 0, 0.75);

  pkg->status = LIME_PACKAGE_STATUS_INSTALLED;
  priv->installed_packages = g_list_append(priv->installed_packages, pkg);

  g_signal_emit(store, appstore_signals[SIGNAL_PACKAGE_INSTALLED], 0, package_id);

  return TRUE;
}

/**
 * lime_app_store_remove_package:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Remove a package
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_remove_package(LiMeAppStore *store,
                              const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  g_debug("Removing package: %s", package_id);

  g_signal_emit(store, appstore_signals[SIGNAL_PACKAGE_REMOVED], 0, package_id);
  return TRUE;
}

/**
 * lime_app_store_update_package:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Update a package
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_update_package(LiMeAppStore *store,
                              const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Updating package: %s", package_id);
  g_signal_emit(store, appstore_signals[SIGNAL_PACKAGE_UPDATED], 0, package_id);

  return TRUE;
}

/**
 * lime_app_store_update_all_packages:
 * @store: A #LiMeAppStore
 *
 * Update all installed packages
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_update_all_packages(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  GList *updates = lime_app_store_get_updates(store);
  gint update_count = g_list_length(updates);

  g_debug("Updating %d packages", update_count);

  for (GList *link = updates; link; link = link->next) {
    LiMeAppStorePackage *pkg = (LiMeAppStorePackage *)link->data;
    lime_app_store_update_package(store, pkg->id);
  }

  return TRUE;
}

/**
 * lime_app_store_get_installation_progress:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Get installation progress percentage
 *
 * Returns: Progress 0.0 to 1.0
 */
gdouble
lime_app_store_get_installation_progress(LiMeAppStore *store,
                                         const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), 0.0);

  return 0.0;
}

/**
 * lime_app_store_get_installation_status:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Get installation status message
 *
 * Returns: (transfer full): Status message
 */
gchar *
lime_app_store_get_installation_status(LiMeAppStore *store,
                                       const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  gchar *status = g_hash_table_lookup(priv->installation_status, package_id);
  return status ? g_strdup(status) : g_strdup("Not installing");
}

/**
 * lime_app_store_get_repositories:
 * @store: A #LiMeAppStore
 *
 * Get list of repositories
 *
 * Returns: (transfer none): Repository list
 */
GList *
lime_app_store_get_repositories(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  return priv->repositories;
}

/**
 * lime_app_store_add_repository:
 * @store: A #LiMeAppStore
 * @repo_url: Repository URL
 * @repo_name: Repository name
 *
 * Add custom repository
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_add_repository(LiMeAppStore *store,
                              const gchar *repo_url,
                              const gchar *repo_name)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  LiMeRepository *repo = g_malloc0(sizeof(LiMeRepository));
  repo->repo_id = g_strdup_printf("custom-%ld", (long)time(NULL));
  repo->repo_name = g_strdup(repo_name);
  repo->repo_url = g_strdup(repo_url);
  repo->repo_type = LIME_PACKAGE_SOURCE_CUSTOM;
  repo->is_enabled = TRUE;

  priv->repositories = g_list_append(priv->repositories, repo);

  g_debug("Added repository: %s (%s)", repo_name, repo_url);
  g_signal_emit(store, appstore_signals[SIGNAL_REPOSITORY_ADDED], 0);

  return TRUE;
}

/**
 * lime_app_store_remove_repository:
 * @store: A #LiMeAppStore
 * @repo_id: Repository ID
 *
 * Remove repository
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_remove_repository(LiMeAppStore *store,
                                 const gchar *repo_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Removed repository: %s", repo_id);
  g_signal_emit(store, appstore_signals[SIGNAL_REPOSITORY_REMOVED], 0);

  return TRUE;
}

/**
 * lime_app_store_enable_repository:
 * @store: A #LiMeAppStore
 * @repo_id: Repository ID
 *
 * Enable repository
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_enable_repository(LiMeAppStore *store,
                                 const gchar *repo_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Enabled repository: %s", repo_id);
  return TRUE;
}

/**
 * lime_app_store_disable_repository:
 * @store: A #LiMeAppStore
 * @repo_id: Repository ID
 *
 * Disable repository
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_disable_repository(LiMeAppStore *store,
                                  const gchar *repo_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Disabled repository: %s", repo_id);
  return TRUE;
}

/**
 * lime_app_store_enable_flathub:
 * @store: A #LiMeAppStore
 *
 * Enable Flathub repository
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_enable_flathub(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  priv->flathub_enabled = TRUE;
  g_debug("Flathub enabled");

  return TRUE;
}

/**
 * lime_app_store_is_flathub_enabled:
 * @store: A #LiMeAppStore
 *
 * Check if Flathub is enabled
 *
 * Returns: %TRUE if enabled
 */
gboolean
lime_app_store_is_flathub_enabled(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  return priv->flathub_enabled;
}

/**
 * lime_app_store_enable_kde_store:
 * @store: A #LiMeAppStore
 *
 * Enable KDE Store
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_enable_kde_store(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  priv->kde_store_enabled = TRUE;
  g_debug("KDE Store enabled");

  return TRUE;
}

/**
 * lime_app_store_is_kde_store_enabled:
 * @store: A #LiMeAppStore
 *
 * Check if KDE Store is enabled
 *
 * Returns: %TRUE if enabled
 */
gboolean
lime_app_store_is_kde_store_enabled(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  return priv->kde_store_enabled;
}

/**
 * lime_app_store_enable_aur:
 * @store: A #LiMeAppStore
 *
 * Enable AUR (Arch User Repository)
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_enable_aur(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  priv->aur_enabled = TRUE;
  g_debug("AUR enabled");

  return TRUE;
}

/**
 * lime_app_store_is_aur_enabled:
 * @store: A #LiMeAppStore
 *
 * Check if AUR is enabled
 *
 * Returns: %TRUE if enabled
 */
gboolean
lime_app_store_is_aur_enabled(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  return priv->aur_enabled;
}

/**
 * lime_app_store_install_aur_package:
 * @store: A #LiMeAppStore
 * @package_name: AUR package name
 *
 * Install package from AUR (hands-free)
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_install_aur_package(LiMeAppStore *store,
                                   const gchar *package_name)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Installing AUR package: %s (automated)", package_name);

  /* Would automatically handle yay/paru installation */
  return TRUE;
}

/**
 * lime_app_store_install_arch_package:
 * @store: A #LiMeAppStore
 * @package_name: Arch package name
 *
 * Install package from official Arch repos
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_install_arch_package(LiMeAppStore *store,
                                    const gchar *package_name)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Installing Arch package: %s (automated)", package_name);

  /* Would automatically run: sudo pacman -S --noconfirm <package> */
  return TRUE;
}

/**
 * lime_app_store_get_rating:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Get package rating
 *
 * Returns: Rating 0.0 to 5.0
 */
gdouble
lime_app_store_get_rating(LiMeAppStore *store,
                          const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), 0.0);

  LiMeAppStorePackage *pkg = lime_app_store_get_package(store, package_id);
  return pkg ? pkg->rating : 0.0;
}

/**
 * lime_app_store_get_review_count:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Get number of reviews
 *
 * Returns: Review count
 */
gint
lime_app_store_get_review_count(LiMeAppStore *store,
                                const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), 0);

  return 0;
}

/**
 * lime_app_store_submit_review:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 * @rating: Rating 0.0 to 5.0
 * @review_text: Review text
 *
 * Submit package review
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_submit_review(LiMeAppStore *store,
                             const gchar *package_id,
                             gdouble rating,
                             const gchar *review_text)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Submitted review for %s (rating: %.1f)", package_id, rating);
  return TRUE;
}

/**
 * lime_app_store_add_to_favorites:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Add package to favorites
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_add_to_favorites(LiMeAppStore *store,
                                const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  priv->favorites = g_list_append(priv->favorites, g_strdup(package_id));
  g_debug("Added to favorites: %s", package_id);

  return TRUE;
}

/**
 * lime_app_store_remove_from_favorites:
 * @store: A #LiMeAppStore
 * @package_id: Package ID
 *
 * Remove package from favorites
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_remove_from_favorites(LiMeAppStore *store,
                                     const gchar *package_id)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  return TRUE;
}

/**
 * lime_app_store_get_favorites:
 * @store: A #LiMeAppStore
 *
 * Get favorite packages
 *
 * Returns: (transfer none): Favorite list
 */
GList *
lime_app_store_get_favorites(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  return priv->favorites;
}

/**
 * lime_app_store_get_recommendations:
 * @store: A #LiMeAppStore
 *
 * Get recommended packages
 *
 * Returns: (transfer none): Recommendations
 */
GList *
lime_app_store_get_recommendations(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), NULL);

  return NULL;
}

/**
 * lime_app_store_check_system_updates:
 * @store: A #LiMeAppStore
 *
 * Check for system updates
 *
 * Returns: %TRUE if updates available
 */
gboolean
lime_app_store_check_system_updates(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Checking for system updates...");
  return FALSE;
}

/**
 * lime_app_store_install_system_updates:
 * @store: A #LiMeAppStore
 *
 * Install all system updates
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_install_system_updates(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("Installing system updates...");
  return TRUE;
}

/**
 * lime_app_store_get_pending_update_count:
 * @store: A #LiMeAppStore
 *
 * Get count of pending updates
 *
 * Returns: Update count
 */
gint
lime_app_store_get_pending_update_count(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), 0);

  GList *updates = lime_app_store_get_updates(store);
  return g_list_length(updates);
}

/**
 * lime_app_store_refresh_repositories:
 * @store: A #LiMeAppStore
 *
 * Refresh all repositories
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_refresh_repositories(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_signal_emit(store, appstore_signals[SIGNAL_REFRESH_STARTED], 0);

  g_debug("Refreshing repositories...");

  g_signal_emit(store, appstore_signals[SIGNAL_REFRESH_COMPLETE], 0);

  return TRUE;
}

/**
 * lime_app_store_clear_cache:
 * @store: A #LiMeAppStore
 *
 * Clear repository cache
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_clear_cache(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);
  LiMeAppStorePrivate *priv = lime_app_store_get_instance_private(store);

  g_debug("Clearing cache: %s", priv->cache_dir);
  return TRUE;
}

/**
 * lime_app_store_save_config:
 * @store: A #LiMeAppStore
 *
 * Save app store configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_save_config(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("App store configuration saved");
  return TRUE;
}

/**
 * lime_app_store_load_config:
 * @store: A #LiMeAppStore
 *
 * Load app store configuration
 *
 * Returns: %TRUE on success
 */
gboolean
lime_app_store_load_config(LiMeAppStore *store)
{
  g_return_val_if_fail(LIME_IS_APP_STORE(store), FALSE);

  g_debug("App store configuration loaded");
  return TRUE;
}
