/*
 * Shell utilities - Implementation
 */

#include "shell-util.h"
#include <glib/gstdio.h>

gchar *
shell_util_get_app_path (const char *app_name)
{
  g_return_val_if_fail (app_name != NULL, NULL);

  /* Search in standard paths */
  const gchar *path_dirs[] = {
    "/usr/local/bin",
    "/usr/bin",
    "/bin",
    NULL
  };

  for (int i = 0; path_dirs[i]; i++)
    {
      gchar *full_path = g_build_filename (path_dirs[i], app_name, NULL);
      if (g_file_test (full_path, G_FILE_TEST_IS_EXECUTABLE))
        return full_path;
      g_free (full_path);
    }

  return NULL;
}

gboolean
shell_util_exec_app (const char *command)
{
  g_return_val_if_fail (command != NULL, FALSE);

  GError *error = NULL;
  gint exit_status;

  if (!g_spawn_command_line_sync (command, NULL, NULL, &exit_status, &error))
    {
      g_warning ("Failed to execute: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  return exit_status == 0;
}

gchar *
shell_util_get_user_data_dir (void)
{
  return g_build_filename (g_get_user_data_dir (), "cinnamon", NULL);
}
