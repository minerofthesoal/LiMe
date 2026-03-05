/*
 * Shell utilities for LiMe
 */

#ifndef __SHELL_UTIL_H__
#define __SHELL_UTIL_H__

#include <glib.h>

gchar * shell_util_get_app_path (const char *app_name);
gboolean shell_util_exec_app (const char *command);
gchar * shell_util_get_user_data_dir (void);

#endif /* __SHELL_UTIL_H__ */
