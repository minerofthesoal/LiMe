/*
 * LiMe AI Integration
 * Integrates AI assistant into the desktop environment
 */

#ifndef __LIME_AI_INTEGRATION_H__
#define __LIME_AI_INTEGRATION_H__

#include <glib-object.h>

typedef struct _LiMeAIIntegration LiMeAIIntegration;
typedef struct _LiMeAIIntegrationClass LiMeAIIntegrationClass;

#define LIME_TYPE_AI_INTEGRATION (lime_ai_integration_get_type ())
#define LIME_AI_INTEGRATION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIME_TYPE_AI_INTEGRATION, LiMeAIIntegration))
#define LIME_IS_AI_INTEGRATION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIME_TYPE_AI_INTEGRATION))

GType lime_ai_integration_get_type (void);

LiMeAIIntegration * lime_ai_integration_new (void);

void lime_ai_integration_init (LiMeAIIntegration *ai);
void lime_ai_integration_show_assistant (LiMeAIIntegration *ai);
void lime_ai_integration_send_message (LiMeAIIntegration *ai, const char *message);
void lime_ai_integration_set_model (LiMeAIIntegration *ai, const char *model_path);

gchar * lime_ai_integration_get_response (LiMeAIIntegration *ai);
gboolean lime_ai_integration_is_ready (LiMeAIIntegration *ai);

#endif /* __LIME_AI_INTEGRATION_H__ */
