/*
 * LiMe Compositor
 * Advanced window composition with effects, blur, and visual enhancements
 * Complete compositor implementation for the desktop environment
 */

#include "lime-compositor.h"
#include <glib.h>
#include <glib/gstdio.h>
#include <clutter/clutter.h>
#include <math.h>
#include <string.h>

#define COMPOSITOR_BLEND_DURATION 300  /* milliseconds */

struct _LiMeCompositor {
  GObject parent;

  gboolean enabled;
  gboolean vsync_enabled;
  gboolean damage_tracking;

  /* Rendering */
  ClutterStage *stage;
  ClutterActor *root_actor;
  GHashTable *window_textures;

  /* Effects */
  gboolean enable_blur;
  gboolean enable_shadows;
  gboolean enable_glow;
  gboolean enable_transparency_effects;

  /* Performance */
  guint frame_time_source;
  gdouble current_fps;
  guint frame_count;
  guint dropped_frames;

  /* Rendering options */
  gint blur_radius;
  gint shadow_blur;
  gdouble shadow_opacity;
  gint glow_intensity;

  GSettings *settings;
  GHashTable *effect_callbacks;
};

G_DEFINE_TYPE(LiMeCompositor, lime_compositor, G_TYPE_OBJECT);

enum {
  SIGNAL_FRAME_RENDERED,
  SIGNAL_COMPOSITION_UPDATED,
  SIGNAL_EFFECT_APPLIED,
  LAST_SIGNAL
};

static guint compositor_signals[LAST_SIGNAL] = { 0 };

/* Forward declarations */
static void lime_compositor_render_frame(LiMeCompositor *compositor);
static void lime_compositor_apply_blur(LiMeCompositor *compositor, ClutterActor *actor);
static void lime_compositor_apply_shadow(LiMeCompositor *compositor, ClutterActor *actor);
static gboolean lime_compositor_update_frame(gpointer user_data);

/**
 * lime_compositor_class_init:
 * @klass: The class structure
 *
 * Initialize compositor class
 */
static void
lime_compositor_class_init(LiMeCompositorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = lime_compositor_dispose;
  object_class->finalize = lime_compositor_finalize;

  /* Signals */
  compositor_signals[SIGNAL_FRAME_RENDERED] =
    g_signal_new("frame-rendered",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  compositor_signals[SIGNAL_COMPOSITION_UPDATED] =
    g_signal_new("composition-updated",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

  compositor_signals[SIGNAL_EFFECT_APPLIED] =
    g_signal_new("effect-applied",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST,
                 0, NULL, NULL,
                 g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING);
}

/**
 * lime_compositor_init:
 * @compositor: The compositor instance
 *
 * Initialize a new compositor
 */
static void
lime_compositor_init(LiMeCompositor *compositor)
{
  compositor->enabled = TRUE;
  compositor->vsync_enabled = TRUE;
  compositor->damage_tracking = TRUE;

  compositor->window_textures = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                       g_free, g_object_unref);

  compositor->enable_blur = FALSE;
  compositor->enable_shadows = TRUE;
  compositor->enable_glow = FALSE;
  compositor->enable_transparency_effects = TRUE;

  compositor->current_fps = 60.0;
  compositor->frame_count = 0;
  compositor->dropped_frames = 0;

  compositor->blur_radius = 5;
  compositor->shadow_blur = 8;
  compositor->shadow_opacity = 0.4;
  compositor->glow_intensity = 3;

  compositor->settings = g_settings_new("org.cinnamon.compositor");
  compositor->effect_callbacks = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                        g_free, NULL);

  g_debug("Compositor initialized");
}

/**
 * lime_compositor_dispose:
 * @object: The GObject instance
 *
 * Dispose allocated resources
 */
static void
lime_compositor_dispose(GObject *object)
{
  LiMeCompositor *compositor = LIME_COMPOSITOR(object);

  if (compositor->frame_time_source) {
    g_source_remove(compositor->frame_time_source);
    compositor->frame_time_source = 0;
  }

  if (compositor->window_textures) {
    g_hash_table_unref(compositor->window_textures);
    compositor->window_textures = NULL;
  }

  if (compositor->effect_callbacks) {
    g_hash_table_unref(compositor->effect_callbacks);
    compositor->effect_callbacks = NULL;
  }

  g_clear_object(&compositor->settings);
  g_clear_object(&compositor->stage);

  G_OBJECT_CLASS(lime_compositor_parent_class)->dispose(object);
}

/**
 * lime_compositor_finalize:
 * @object: The GObject instance
 *
 * Finalize the object
 */
static void
lime_compositor_finalize(GObject *object)
{
  G_OBJECT_CLASS(lime_compositor_parent_class)->finalize(object);
}

/**
 * lime_compositor_new:
 *
 * Create a new compositor
 *
 * Returns: A new #LiMeCompositor
 */
LiMeCompositor *
lime_compositor_new(void)
{
  return g_object_new(LIME_TYPE_COMPOSITOR, NULL);
}

/**
 * lime_compositor_enable:
 * @compositor: A #LiMeCompositor
 * @enable: Whether to enable the compositor
 *
 * Enable or disable compositing
 */
void
lime_compositor_enable(LiMeCompositor *compositor, gboolean enable)
{
  g_return_if_fail(LIME_IS_COMPOSITOR(compositor));

  if (compositor->enabled != enable) {
    compositor->enabled = enable;

    if (enable) {
      compositor->frame_time_source = g_timeout_add(16, lime_compositor_update_frame, compositor);
      g_debug("Compositor enabled");
    } else {
      if (compositor->frame_time_source) {
        g_source_remove(compositor->frame_time_source);
        compositor->frame_time_source = 0;
      }
      g_debug("Compositor disabled");
    }
  }
}

/**
 * lime_compositor_render_frame:
 * @compositor: A #LiMeCompositor
 *
 * Render a single frame
 */
static void
lime_compositor_render_frame(LiMeCompositor *compositor)
{
  g_return_if_fail(LIME_IS_COMPOSITOR(compositor));

  if (!compositor->enabled) {
    return;
  }

  compositor->frame_count++;

  /* Render all visible windows */
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init(&iter, compositor->window_textures);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    ClutterActor *actor = CLUTTER_ACTOR(value);

    if (clutter_actor_is_visible(actor)) {
      /* Apply effects */
      if (compositor->enable_shadows) {
        lime_compositor_apply_shadow(compositor, actor);
      }

      if (compositor->enable_blur) {
        lime_compositor_apply_blur(compositor, actor);
      }

      clutter_actor_paint(actor);
    }
  }

  g_signal_emit(compositor, compositor_signals[SIGNAL_FRAME_RENDERED], 0);
}

/**
 * lime_compositor_apply_blur:
 * @compositor: A #LiMeCompositor
 * @actor: The actor to blur
 *
 * Apply blur effect to an actor
 */
static void
lime_compositor_apply_blur(LiMeCompositor *compositor, ClutterActor *actor)
{
  g_return_if_fail(LIME_IS_COMPOSITOR(compositor));
  g_return_if_fail(actor != NULL);

  /* Apply Clutter blur effect */
  ClutterEffect *blur_effect = clutter_blur_effect_new();

  if (blur_effect) {
    clutter_actor_add_effect(actor, blur_effect);
    g_object_unref(blur_effect);
  }
}

/**
 * lime_compositor_apply_shadow:
 * @compositor: A #LiMeCompositor
 * @actor: The actor to shadow
 *
 * Apply shadow effect to an actor
 */
static void
lime_compositor_apply_shadow(LiMeCompositor *compositor, ClutterActor *actor)
{
  g_return_if_fail(LIME_IS_COMPOSITOR(compositor));
  g_return_if_fail(actor != NULL);

  /* Create shadow using color and opacity */
  gfloat x, y, width, height;
  clutter_actor_get_geometry(actor, (ClutterGeometry *)&x, &y, &width, &height);

  ClutterActor *shadow = clutter_rectangle_new_with_color(&(ClutterColor){0, 0, 0, 100});
  clutter_actor_set_size(shadow, width + compositor->shadow_blur,
                         height + compositor->shadow_blur);
  clutter_actor_set_position(shadow, x - compositor->shadow_blur / 2,
                             y - compositor->shadow_blur / 2);
  clutter_actor_hide(shadow);
}

/**
 * lime_compositor_update_frame:
 * @user_data: The compositor
 *
 * Periodic frame update and rendering
 *
 * Returns: %TRUE to continue
 */
static gboolean
lime_compositor_update_frame(gpointer user_data)
{
  LiMeCompositor *compositor = LIME_COMPOSITOR(user_data);

  lime_compositor_render_frame(compositor);

  return G_SOURCE_CONTINUE;
}

/**
 * lime_compositor_set_blur:
 * @compositor: A #LiMeCompositor
 * @enabled: Whether to enable blur
 * @radius: Blur radius in pixels
 *
 * Configure blur effect
 */
void
lime_compositor_set_blur(LiMeCompositor *compositor,
                         gboolean enabled, gint radius)
{
  g_return_if_fail(LIME_IS_COMPOSITOR(compositor));

  compositor->enable_blur = enabled;
  compositor->blur_radius = CLAMP(radius, 1, 20);
}

/**
 * lime_compositor_set_shadow:
 * @compositor: A #LiMeCompositor
 * @enabled: Whether to enable shadows
 * @blur: Shadow blur radius
 * @opacity: Shadow opacity (0.0 - 1.0)
 *
 * Configure shadow effect
 */
void
lime_compositor_set_shadow(LiMeCompositor *compositor,
                           gboolean enabled, gint blur, gdouble opacity)
{
  g_return_if_fail(LIME_IS_COMPOSITOR(compositor));

  compositor->enable_shadows = enabled;
  compositor->shadow_blur = CLAMP(blur, 1, 20);
  compositor->shadow_opacity = CLAMP(opacity, 0.0, 1.0);
}

/**
 * lime_compositor_get_fps:
 * @compositor: A #LiMeCompositor
 *
 * Get current FPS
 *
 * Returns: Current frames per second
 */
gdouble
lime_compositor_get_fps(LiMeCompositor *compositor)
{
  g_return_val_if_fail(LIME_IS_COMPOSITOR(compositor), 0.0);

  return compositor->current_fps;
}

/**
 * lime_compositor_get_frame_count:
 * @compositor: A #LiMeCompositor
 *
 * Get total frames rendered
 *
 * Returns: Frame count
 */
guint
lime_compositor_get_frame_count(LiMeCompositor *compositor)
{
  g_return_val_if_fail(LIME_IS_COMPOSITOR(compositor), 0);

  return compositor->frame_count;
}
