/*
 * LiMe Launcher Header
 * Application launcher interface
 */

#ifndef __LIME_LAUNCHER_H__
#define __LIME_LAUNCHER_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _LiMeLauncher LiMeLauncher;
typedef struct _LiMeLauncherClass LiMeLauncherClass;

#define LIME_TYPE_LAUNCHER (lime_launcher_get_type())
#define LIME_LAUNCHER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_LAUNCHER, LiMeLauncher))
#define LIME_IS_LAUNCHER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_LAUNCHER))

typedef struct {
  GObjectClass parent;
} LiMeLauncherClass;

GType lime_launcher_get_type(void);

LiMeLauncher * lime_launcher_new(void);

GList * lime_launcher_get_applications(LiMeLauncher *launcher);
GList * lime_launcher_search(LiMeLauncher *launcher, const gchar *query);

gboolean lime_launcher_launch_app(LiMeLauncher *launcher, GAppInfo *app);

void lime_launcher_add_favorite(LiMeLauncher *launcher, GAppInfo *app);
void lime_launcher_remove_favorite(LiMeLauncher *launcher, GAppInfo *app);
GList * lime_launcher_get_favorites(LiMeLauncher *launcher);
gboolean lime_launcher_is_favorite(LiMeLauncher *launcher, GAppInfo *app);

GAppInfo * lime_launcher_get_application(LiMeLauncher *launcher, const gchar *app_id);
guint lime_launcher_get_num_applications(LiMeLauncher *launcher);

G_END_DECLS

#endif /* __LIME_LAUNCHER_H__ */
