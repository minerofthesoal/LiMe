/*
 * LiMe Compositor Header
 * Advanced window composition system
 */

#ifndef __LIME_COMPOSITOR_H__
#define __LIME_COMPOSITOR_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef struct _LiMeCompositor LiMeCompositor;
typedef struct _LiMeCompositorClass LiMeCompositorClass;

#define LIME_TYPE_COMPOSITOR (lime_compositor_get_type())
#define LIME_COMPOSITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), LIME_TYPE_COMPOSITOR, LiMeCompositor))
#define LIME_IS_COMPOSITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIME_TYPE_COMPOSITOR))

typedef struct {
  GObjectClass parent;
} LiMeCompositorClass;

GType lime_compositor_get_type(void);

LiMeCompositor * lime_compositor_new(void);

void lime_compositor_enable(LiMeCompositor *compositor, gboolean enable);

void lime_compositor_set_blur(LiMeCompositor *compositor,
                               gboolean enabled, gint radius);
void lime_compositor_set_shadow(LiMeCompositor *compositor,
                                 gboolean enabled, gint blur, gdouble opacity);

gdouble lime_compositor_get_fps(LiMeCompositor *compositor);
guint lime_compositor_get_frame_count(LiMeCompositor *compositor);

G_END_DECLS

#endif /* __LIME_COMPOSITOR_H__ */
