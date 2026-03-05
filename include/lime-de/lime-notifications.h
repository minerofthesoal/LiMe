/*
 * LiMe Notifications Header
 * Desktop notification system interface
 */

#ifndef __LIME_NOTIFICATIONS_H__
#define __LIME_NOTIFICATIONS_H__

#include <glib.h>

G_BEGIN_DECLS

void lime_show_notification(const gchar *app_name,
                             const gchar *summary,
                             const gchar *body,
                             gint timeout);

G_END_DECLS

#endif /* __LIME_NOTIFICATIONS_H__ */
