/*
 * LiMe Effects Engine
 * Window effects, animations, and visual enhancements
 * Comprehensive effects system for the desktop
 */

#include "lime-effects-engine.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <clutter/clutter.h>
#include <string.h>
#include <math.h>

#define ANIMATION_DURATION 500 /* milliseconds */

struct _LiMeEffectsEngine {
  GObject parent;

  GSettings *settings;

  gboolean enable_effects;
  gboolean enable_animations;
  gdouble animation_speed;

  GHashTable *active_effects;
  GHashTable *effect_pipelines;

  /* Presets */
  gboolean preset_minimal;
  gboolean preset_normal;
  gboolean preset_fancy;

  /* Individual effects */
  gboolean enable_shadows;
  gboolean enable_blur;
  gboolean enable_transparency;
  gboolean enable_wobble;
  gboolean enable_fade;
  gboolean enable_scale;
  gboolean enable_rotate;

  /* Timing */
  guint update_timeout;
  guint cleanup_timeout;

  ClutterActor *stage;
};

G_DEFINE_TYPE(LiMeEffectsEngine, lime_effects_engine, G_TYPE_OBJECT);

enum {
  SIGNAL_EFFECT_STARTED,
  SIGNAL_EFFECT_COMPLETED,
  SIGNAL_EFFECTS_CHANGED,
  LAST_SIGNAL
};

static guint effects_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations */
static void lime_effects_engine_init_presets(LiMeEffectsEngine *engine);
static void lime_effects_engine_apply_effect(LiMeEffectsEngine *engine,
                                             const gchar *window_id,
                                             const gchar *effect_name);
static gboolean lime_effects_engine_update(gpointer user_data);

/**
 * lime_effects_engine_class_init:
 * @klass: The class structure
 *
 * Initialize effects engine class
 */
static void
lime_effects_engine_class_init(LiMeEffectsEngineClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_effects_engine_dispose;
  object_class->finalize = lime_effects_engine_finalize;

  /* Signals */
  effects_signals[SIGNAL_EFFECT_STARTED] =
    g_signal_new("effect-started",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  effects_signals[SIGNAL_EFFECT_COMPLETED] =
    g_signal_new("effect-completed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);

  effects_signals[SIGNAL_EFFECTS_CHANGED] =
    g_signal_new("effects-changed",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

/**
 * lime_effects_engine_init:
 * @engine: The effects engine instance
 *
 * Initialize a new effects engine
 */
static void
lime_effects_engine_init(LiMeEffectsEngine *engine)
{
  engine->settings = g_settings_new("org.cinnamon.effects");

  engine->enable_effects = TRUE;
  engine->enable_animations = TRUE;
  engine->animation_speed = 1.0;

  engine->active_effects = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, NULL);
  engine->effect_pipelines = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                    g_free, NULL);

  engine->preset_minimal = FALSE;
  engine->preset_normal = TRUE;
  engine->preset_fancy = FALSE;

  engine->enable_shadows = TRUE;
  engine->enable_blur = FALSE;
  engine->enable_transparency = TRUE;
  engine->enable_wobble = FALSE;
  engine->enable_fade = TRUE;
  engine->enable_scale = TRUE;
  engine->enable_rotate = FALSE;

  lime_effects_engine_init_presets(engine);

  g_debug("Effects engine initialized");
}

/**
 * lime_effects_engine_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_effects_engine_dispose(GObject *object)
{
  LiMeEffectsEngine *engine = LIME_EFFECTS_ENGINE(object);

  if (engine->update_timeout) {
    g_source_remove(engine->update_timeout);
    engine->update_timeout = 0;
  }

  if (engine->cleanup_timeout) {
    g_source_remove(engine->cleanup_timeout);
    engine->cleanup_timeout = 0;
  }

  if (engine->active_effects) {
    g_hash_table_unref(engine->active_effects);
    engine->active_effects = NULL;
  }

  if (engine->effect_pipelines) {
    g_hash_table_unref(engine->effect_pipelines);
    engine->effect_pipelines = NULL;
  }

  g_clear_object(&engine->settings);

  G_OBJECT_CLASS(lime_effects_engine_parent_class)->dispose(object);
}

/**
 * lime_effects_engine_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_effects_engine_finalize(GObject *object)
{
  G_OBJECT_CLASS(lime_effects_engine_parent_class)->finalize(object);
}

/**
 * lime_effects_engine_new:
 *
 * Create a new effects engine
 *
 * Returns: A new #LiMeEffectsEngine
 */
LiMeEffectsEngine *
lime_effects_engine_new(void)
{
  return g_object_new(LIME_TYPE_EFFECTS_ENGINE, NULL);
}

/**
 * lime_effects_engine_init_presets:
 * @engine: A #LiMeEffectsEngine
 *
 * Initialize effect presets
 */
static void
lime_effects_engine_init_presets(LiMeEffectsEngine *engine)
{
  g_return_if_fail(LIME_IS_EFFECTS_ENGINE(engine));

  if (engine->preset_minimal) {
    engine->enable_shadows = FALSE;
    engine->enable_blur = FALSE;
    engine->enable_transparency = TRUE;
    engine->enable_wobble = FALSE;
    engine->enable_fade = FALSE;
    engine->enable_scale = FALSE;
    engine->enable_rotate = FALSE;
  } else if (engine->preset_fancy) {
    engine->enable_shadows = TRUE;
    engine->enable_blur = TRUE;
    engine->enable_transparency = TRUE;
    engine->enable_wobble = TRUE;
    engine->enable_fade = TRUE;
    engine->enable_scale = TRUE;
    engine->enable_rotate = TRUE;
    engine->animation_speed = 1.5;
  } else {
    /* Normal preset (default) */
    engine->enable_shadows = TRUE;
    engine->enable_blur = FALSE;
    engine->enable_transparency = TRUE;
    engine->enable_wobble = FALSE;
    engine->enable_fade = TRUE;
    engine->enable_scale = TRUE;
    engine->enable_rotate = FALSE;
    engine->animation_speed = 1.0;
  }
}

/**
 * lime_effects_engine_add_effect:
 * @engine: A #LiMeEffectsEngine
 * @window_id: Window identifier
 * @effect_name: Name of the effect to apply
 *
 * Add an effect to a window
 */
void
lime_effects_engine_add_effect(LiMeEffectsEngine *engine,
                               const gchar *window_id,
                               const gchar *effect_name)
{
  g_return_if_fail(LIME_IS_EFFECTS_ENGINE(engine));
  g_return_if_fail(window_id != NULL);
  g_return_if_fail(effect_name != NULL);

  if (!engine->enable_effects) {
    return;
  }

  g_debug("Adding effect '%s' to window %s", effect_name, window_id);

  lime_effects_engine_apply_effect(engine, window_id, effect_name);
  g_hash_table_insert(engine->active_effects, g_strdup(window_id),
                      GINT_TO_POINTER(1));

  g_signal_emit(engine, effects_signals[SIGNAL_EFFECT_STARTED], 0, effect_name);
}

/**
 * lime_effects_engine_apply_effect:
 * @engine: A #LiMeEffectsEngine
 * @window_id: Window identifier
 * @effect_name: Name of the effect
 *
 * Apply an effect to a window
 */
static void
lime_effects_engine_apply_effect(LiMeEffectsEngine *engine,
                                 const gchar *window_id,
                                 const gchar *effect_name)
{
  g_return_if_fail(LIME_IS_EFFECTS_ENGINE(engine));

  if (g_strcmp0(effect_name, "minimize") == 0) {
    /* Create minimize animation */
    g_debug("Applying minimize effect");
  } else if (g_strcmp0(effect_name, "maximize") == 0) {
    /* Create maximize animation */
    g_debug("Applying maximize effect");
  } else if (g_strcmp0(effect_name, "open") == 0) {
    /* Create window open animation */
    if (engine->enable_fade) {
      g_debug("Applying fade-in effect");
    }
    if (engine->enable_scale) {
      g_debug("Applying scale-in effect");
    }
  } else if (g_strcmp0(effect_name, "close") == 0) {
    /* Create window close animation */
    if (engine->enable_fade) {
      g_debug("Applying fade-out effect");
    }
  }
}

/**
 * lime_effects_engine_remove_effect:
 * @engine: A #LiMeEffectsEngine
 * @window_id: Window identifier
 *
 * Remove effects from a window
 */
void
lime_effects_engine_remove_effect(LiMeEffectsEngine *engine,
                                  const gchar *window_id)
{
  g_return_if_fail(LIME_IS_EFFECTS_ENGINE(engine));
  g_return_if_fail(window_id != NULL);

  g_hash_table_remove(engine->active_effects, window_id);
  g_hash_table_remove(engine->effect_pipelines, window_id);

  g_signal_emit(engine, effects_signals[SIGNAL_EFFECT_COMPLETED], 0, window_id);
}

/**
 * lime_effects_engine_enable_effects:
 * @engine: A #LiMeEffectsEngine
 * @enable: Whether to enable effects
 *
 * Enable/disable all effects globally
 */
void
lime_effects_engine_enable_effects(LiMeEffectsEngine *engine, gboolean enable)
{
  g_return_if_fail(LIME_IS_EFFECTS_ENGINE(engine));

  if (engine->enable_effects != enable) {
    engine->enable_effects = enable;
    g_signal_emit(engine, effects_signals[SIGNAL_EFFECTS_CHANGED], 0);
  }
}

/**
 * lime_effects_engine_set_animation_speed:
 * @engine: A #LiMeEffectsEngine
 * @speed: Speed multiplier
 *
 * Set global animation speed
 */
void
lime_effects_engine_set_animation_speed(LiMeEffectsEngine *engine, gdouble speed)
{
  g_return_if_fail(LIME_IS_EFFECTS_ENGINE(engine));

  engine->animation_speed = CLAMP(speed, 0.1, 5.0);
}

/**
 * lime_effects_engine_update:
 * @user_data: The effects engine
 *
 * Update active effects
 *
 * Returns: %TRUE to continue
 */
static gboolean
lime_effects_engine_update(gpointer user_data)
{
  LiMeEffectsEngine *engine = LIME_EFFECTS_ENGINE(user_data);

  /* Update all active effects */

  return G_SOURCE_CONTINUE;
}

/**
 * lime_effects_engine_set_preset:
 * @engine: A #LiMeEffectsEngine
 * @preset: The preset name ("minimal", "normal", "fancy")
 *
 * Set an effect preset
 */
void
lime_effects_engine_set_preset(LiMeEffectsEngine *engine, const gchar *preset)
{
  g_return_if_fail(LIME_IS_EFFECTS_ENGINE(engine));
  g_return_if_fail(preset != NULL);

  if (g_strcmp0(preset, "minimal") == 0) {
    engine->preset_minimal = TRUE;
    engine->preset_normal = FALSE;
    engine->preset_fancy = FALSE;
  } else if (g_strcmp0(preset, "fancy") == 0) {
    engine->preset_minimal = FALSE;
    engine->preset_normal = FALSE;
    engine->preset_fancy = TRUE;
  } else {
    engine->preset_minimal = FALSE;
    engine->preset_normal = TRUE;
    engine->preset_fancy = FALSE;
  }

  lime_effects_engine_init_presets(engine);
  g_signal_emit(engine, effects_signals[SIGNAL_EFFECTS_CHANGED], 0);
}
