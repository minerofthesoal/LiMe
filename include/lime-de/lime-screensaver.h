/*
 * LiMe Screensaver and Lock Screen Header
 * Screensaver management and security lock screen
 */

#ifndef __LIME_SCREENSAVER_H__
#define __LIME_SCREENSAVER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define LIME_TYPE_SCREENSAVER (lime_screensaver_get_type())
#define LIME_SCREENSAVER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_SCREENSAVER, LiMeScreensaver))
#define LIME_IS_SCREENSAVER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_SCREENSAVER))

typedef struct _LiMeScreensaver LiMeScreensaver;
typedef struct _LiMeScreensaverClass LiMeScreensaverClass;

typedef enum {
  LIME_SCREENSAVER_BLANK,
  LIME_SCREENSAVER_PATTERNS,
  LIME_SCREENSAVER_SLIDESHOW,
  LIME_SCREENSAVER_CUSTOM
} LiMeScreensaverType;

GType lime_screensaver_get_type(void);

LiMeScreensaver * lime_screensaver_new(void);

/* Screensaver control */
gboolean lime_screensaver_enable(LiMeScreensaver *saver);
gboolean lime_screensaver_disable(LiMeScreensaver *saver);
gboolean lime_screensaver_is_enabled(LiMeScreensaver *saver);

/* Screensaver settings */
gboolean lime_screensaver_set_timeout(LiMeScreensaver *saver, gint timeout_seconds);
gint lime_screensaver_get_timeout(LiMeScreensaver *saver);

gboolean lime_screensaver_set_type(LiMeScreensaver *saver, LiMeScreensaverType type);
LiMeScreensaverType lime_screensaver_get_type_active(LiMeScreensaver *saver);

gboolean lime_screensaver_set_lock_on_screensaver(LiMeScreensaver *saver, gboolean enabled);
gboolean lime_screensaver_get_lock_on_screensaver(LiMeScreensaver *saver);

/* Screensaver activation */
gboolean lime_screensaver_activate(LiMeScreensaver *saver);
gboolean lime_screensaver_deactivate(LiMeScreensaver *saver);
gboolean lime_screensaver_is_active(LiMeScreensaver *saver);

/* Lock screen */
gboolean lime_screensaver_lock(LiMeScreensaver *saver);
gboolean lime_screensaver_unlock(LiMeScreensaver *saver);
gboolean lime_screensaver_is_locked(LiMeScreensaver *saver);

/* Lock screen settings */
gboolean lime_screensaver_set_lock_timeout(LiMeScreensaver *saver, gint timeout_seconds);
gint lime_screensaver_get_lock_timeout(LiMeScreensaver *saver);

gboolean lime_screensaver_set_show_clock(LiMeScreensaver *saver, gboolean show);
gboolean lime_screensaver_set_show_notifications(LiMeScreensaver *saver, gboolean show);
gboolean lime_screensaver_set_show_user_image(LiMeScreensaver *saver, gboolean show);

/* Idle management */
gboolean lime_screensaver_set_idle_timeout(LiMeScreensaver *saver, gint timeout_seconds);
gint lime_screensaver_get_idle_timeout(LiMeScreensaver *saver);

/* Configuration */
gboolean lime_screensaver_save_config(LiMeScreensaver *saver);
gboolean lime_screensaver_load_config(LiMeScreensaver *saver);

G_END_DECLS

#endif /* __LIME_SCREENSAVER_H__ */
