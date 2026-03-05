/*
 * LiMe Effects Engine Header
 * Visual effects and animations system
 */

#ifndef __LIME_EFFECTS_ENGINE_H__
#define __LIME_EFFECTS_ENGINE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _LiMeEffectsEngine LiMeEffectsEngine;
typedef struct _LiMeEffectsEngineClass LiMeEffectsEngineClass;

#define LIME_TYPE_EFFECTS_ENGINE (lime_effects_engine_get_type())
#define LIME_EFFECTS_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_EFFECTS_ENGINE, LiMeEffectsEngine))
#define LIME_IS_EFFECTS_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_EFFECTS_ENGINE))

typedef struct {
  GObjectClass parent;
} LiMeEffectsEngineClass;

GType lime_effects_engine_get_type(void);

LiMeEffectsEngine * lime_effects_engine_new(void);

void lime_effects_engine_add_effect(LiMeEffectsEngine *engine,
                                     const gchar *window_id,
                                     const gchar *effect_name);
void lime_effects_engine_remove_effect(LiMeEffectsEngine *engine,
                                        const gchar *window_id);

void lime_effects_engine_enable_effects(LiMeEffectsEngine *engine, gboolean enable);
void lime_effects_engine_set_animation_speed(LiMeEffectsEngine *engine, gdouble speed);
void lime_effects_engine_set_preset(LiMeEffectsEngine *engine, const gchar *preset);

G_END_DECLS

#endif /* __LIME_EFFECTS_ENGINE_H__ */
