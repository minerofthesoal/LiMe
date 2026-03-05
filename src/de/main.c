/*
 * LiMe Desktop Environment
 * Fork of Cinnamon with AI integration and enhanced customization
 */

#include "cinnamon-global.h"
#include "shell-util.h"
#include "lime-theme-manager.h"
#include "lime-ai-integration.h"

/* Global instance */
static LiMeGlobal *global_instance = NULL;

struct _LiMeGlobal {
  GObject parent;

  /* Theme management */
  LiMeThemeManager *theme_manager;

  /* AI integration */
  LiMeAIIntegration *ai_integration;

  /* UI components */
  GtkWidget *panel;
  GtkWidget *stage;

  /* Settings */
  GSettings *settings;
};

G_DEFINE_TYPE(LiMeGlobal, lime_global, G_TYPE_OBJECT);

static void
lime_global_dispose (GObject *object)
{
  LiMeGlobal *global = LIME_GLOBAL (object);

  g_clear_object (&global->theme_manager);
  g_clear_object (&global->ai_integration);
  g_clear_object (&global->settings);

  G_OBJECT_CLASS (lime_global_parent_class)->dispose (object);
}

static void
lime_global_finalize (GObject *object)
{
  G_OBJECT_CLASS (lime_global_parent_class)->finalize (object);
}

static void
lime_global_class_init (LiMeGlobalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = lime_global_dispose;
  object_class->finalize = lime_global_finalize;
}

static void
lime_global_init (LiMeGlobal *global)
{
  /* Initialize theme manager */
  global->theme_manager = lime_theme_manager_new ();

  /* Initialize AI integration */
  global->ai_integration = lime_ai_integration_new ();

  /* Load settings */
  global->settings = g_settings_new ("org.cinnamon");
}

/**
 * lime_global_get_default:
 *
 * Returns: (transfer none): The global LiMeGlobal instance
 */
LiMeGlobal *
lime_global_get_default (void)
{
  if (global_instance == NULL)
    global_instance = g_object_new (LIME_TYPE_GLOBAL, NULL);

  return global_instance;
}

/**
 * lime_global_get_theme_manager:
 * @global: A #LiMeGlobal
 *
 * Returns: (transfer none): The theme manager
 */
LiMeThemeManager *
lime_global_get_theme_manager (LiMeGlobal *global)
{
  g_return_val_if_fail (LIME_IS_GLOBAL (global), NULL);
  return global->theme_manager;
}

/**
 * lime_global_get_ai_integration:
 * @global: A #LiMeGlobal
 *
 * Returns: (transfer none): The AI integration module
 */
LiMeAIIntegration *
lime_global_get_ai_integration (LiMeGlobal *global)
{
  g_return_val_if_fail (LIME_IS_GLOBAL (global), NULL);
  return global->ai_integration;
}

int
main (int argc, char **argv)
{
  LiMeGlobal *global;
  GMainLoop *loop;
  int exit_status;

  g_print ("LiMe Desktop Environment starting...\n");

  /* Initialize GTK */
  gtk_init (&argc, &argv);

  /* Get global instance */
  global = lime_global_get_default ();

  /* Initialize theme */
  lime_theme_manager_load_default (global->theme_manager);

  /* Initialize AI */
  lime_ai_integration_init (global->ai_integration);

  /* Create main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_print ("LiMe DE initialized successfully\n");

  /* Run main loop */
  g_main_loop_run (loop);

  exit_status = 0;

  g_main_loop_unref (loop);
  g_object_unref (global);

  return exit_status;
}
