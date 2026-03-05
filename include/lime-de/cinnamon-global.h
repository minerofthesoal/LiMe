/*
 * LiMe Global Header
 * Main global object for LiMe Desktop Environment
 */

#ifndef __LIME_GLOBAL_H__
#define __LIME_GLOBAL_H__

#include <glib-object.h>

typedef struct _LiMeGlobal LiMeGlobal;
typedef struct _LiMeGlobalClass LiMeGlobalClass;

#define LIME_TYPE_GLOBAL (lime_global_get_type ())
#define LIME_GLOBAL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIME_TYPE_GLOBAL, LiMeGlobal))
#define LIME_IS_GLOBAL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIME_TYPE_GLOBAL))

typedef struct _LiMeThemeManager LiMeThemeManager;
typedef struct _LiMeAIIntegration LiMeAIIntegration;

GType lime_global_get_type (void);

LiMeGlobal * lime_global_get_default (void);
LiMeThemeManager * lime_global_get_theme_manager (LiMeGlobal *global);
LiMeAIIntegration * lime_global_get_ai_integration (LiMeGlobal *global);

#endif /* __LIME_GLOBAL_H__ */
