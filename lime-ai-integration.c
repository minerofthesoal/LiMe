/*
 * LiMe AI Integration - Implementation
 */

#include "lime-ai-integration.h"

struct _LiMeAIIntegration {
  GObject parent;

  gchar *model_path;
  gchar *last_response;
  gboolean is_ready;

  GDBusConnection *dbus_conn;
};

G_DEFINE_TYPE(LiMeAIIntegration, lime_ai_integration, G_TYPE_OBJECT);

static void
lime_ai_integration_dispose (GObject *object)
{
  LiMeAIIntegration *ai = LIME_AI_INTEGRATION (object);

  g_clear_pointer (&ai->model_path, g_free);
  g_clear_pointer (&ai->last_response, g_free);
  g_clear_object (&ai->dbus_conn);

  G_OBJECT_CLASS (lime_ai_integration_parent_class)->dispose (object);
}

static void
lime_ai_integration_class_init (LiMeAIIntegrationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = lime_ai_integration_dispose;
}

static void
lime_ai_integration_init (LiMeAIIntegration *ai)
{
  ai->model_path = NULL;
  ai->last_response = NULL;
  ai->is_ready = FALSE;
  ai->dbus_conn = NULL;
}

LiMeAIIntegration *
lime_ai_integration_new (void)
{
  return g_object_new (LIME_TYPE_AI_INTEGRATION, NULL);
}

void
lime_ai_integration_init (LiMeAIIntegration *ai)
{
  g_return_if_fail (LIME_IS_AI_INTEGRATION (ai));

  /* Initialize D-Bus connection */
  /* Connect to LiMe AI service */
  /* Load default model if available */

  ai->is_ready = TRUE;
}

void
lime_ai_integration_show_assistant (LiMeAIIntegration *ai)
{
  g_return_if_fail (LIME_IS_AI_INTEGRATION (ai));

  /* Show AI assistant window/panel */
}

void
lime_ai_integration_send_message (LiMeAIIntegration *ai, const char *message)
{
  g_return_if_fail (LIME_IS_AI_INTEGRATION (ai));
  g_return_if_fail (message != NULL);

  /* Send message to AI service */
  /* Get response asynchronously */
}

void
lime_ai_integration_set_model (LiMeAIIntegration *ai, const char *model_path)
{
  g_return_if_fail (LIME_IS_AI_INTEGRATION (ai));
  g_return_if_fail (model_path != NULL);

  g_free (ai->model_path);
  ai->model_path = g_strdup (model_path);
}

gchar *
lime_ai_integration_get_response (LiMeAIIntegration *ai)
{
  g_return_val_if_fail (LIME_IS_AI_INTEGRATION (ai), NULL);
  return g_strdup (ai->last_response ? ai->last_response : "");
}

gboolean
lime_ai_integration_is_ready (LiMeAIIntegration *ai)
{
  g_return_val_if_fail (LIME_IS_AI_INTEGRATION (ai), FALSE);
  return ai->is_ready;
}
