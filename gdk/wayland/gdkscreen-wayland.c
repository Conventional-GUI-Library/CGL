/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include "gdkscreenprivate.h"
#include "gdkvisualprivate.h"
#include "gdkdisplay.h"
#include "gdkdisplay-wayland.h"
#include "gdkwayland.h"
#include "gdkprivate-wayland.h"

typedef struct _GdkScreenWayland      GdkScreenWayland;
typedef struct _GdkScreenWaylandClass GdkScreenWaylandClass;

#define GDK_TYPE_SCREEN_WAYLAND              (_gdk_screen_wayland_get_type ())
#define GDK_SCREEN_WAYLAND(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SCREEN_WAYLAND, GdkScreenWayland))
#define GDK_SCREEN_WAYLAND_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_SCREEN_WAYLAND, GdkScreenWaylandClass))
#define GDK_IS_SCREEN_WAYLAND(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SCREEN_WAYLAND))
#define GDK_IS_SCREEN_WAYLAND_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_SCREEN_WAYLAND))
#define GDK_SCREEN_WAYLAND_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SCREEN_WAYLAND, GdkScreenWaylandClass))

typedef struct _GdkWaylandMonitor GdkWaylandMonitor;

struct _GdkScreenWayland
{
  GdkScreen parent_instance;

  GdkDisplay *display;
  GdkWindow *root_window;

  int width, height;
  int width_mm, height_mm;

  /* Visual Part */
  GdkVisual *argb_visual;
  GdkVisual *premultiplied_argb_visual;
  GdkVisual *rgb_visual;

  /* Xinerama/RandR 1.2 */
  gint		     n_monitors;
  GdkWaylandMonitor *monitors;
  gint               primary_monitor;
};

struct _GdkScreenWaylandClass
{
  GdkScreenClass parent_class;

  void (* window_manager_changed) (GdkScreenWayland *screen_wayland);
};

struct _GdkWaylandMonitor
{
  GdkRectangle  geometry;
  int		width_mm;
  int		height_mm;
  char *	output_name;
  char *	manufacturer;
};

G_DEFINE_TYPE (GdkScreenWayland, _gdk_screen_wayland, GDK_TYPE_SCREEN)

static void
init_monitor_geometry (GdkWaylandMonitor *monitor,
		       int x, int y, int width, int height)
{
  monitor->geometry.x = x;
  monitor->geometry.y = y;
  monitor->geometry.width = width;
  monitor->geometry.height = height;

  monitor->width_mm = -1;
  monitor->height_mm = -1;
  monitor->output_name = NULL;
  monitor->manufacturer = NULL;
}

static void
free_monitors (GdkWaylandMonitor *monitors,
               gint           n_monitors)
{
  int i;

  for (i = 0; i < n_monitors; ++i)
    {
      g_free (monitors[i].output_name);
      g_free (monitors[i].manufacturer);
    }

  g_free (monitors);
}

static void
deinit_multihead (GdkScreen *screen)
{
  GdkScreenWayland *screen_wayland = GDK_SCREEN_WAYLAND (screen);

  free_monitors (screen_wayland->monitors, screen_wayland->n_monitors);

  screen_wayland->n_monitors = 0;
  screen_wayland->monitors = NULL;
}

static void
init_multihead (GdkScreen *screen)
{
  GdkScreenWayland *screen_wayland = GDK_SCREEN_WAYLAND (screen);

  /* No multihead support of any kind for this screen */
  screen_wayland->n_monitors = 1;
  screen_wayland->monitors = g_new0 (GdkWaylandMonitor, 1);
  screen_wayland->primary_monitor = 0;

  init_monitor_geometry (screen_wayland->monitors, 0, 0,
			 screen_wayland->width, screen_wayland->height);
}

static void
gdk_wayland_screen_dispose (GObject *object)
{
  GdkScreenWayland *screen_wayland = GDK_SCREEN_WAYLAND (object);

  if (screen_wayland->root_window)
    _gdk_window_destroy (screen_wayland->root_window, TRUE);

  G_OBJECT_CLASS (_gdk_screen_wayland_parent_class)->dispose (object);
}

static void
gdk_wayland_screen_finalize (GObject *object)
{
  GdkScreenWayland *screen_wayland = GDK_SCREEN_WAYLAND (object);

  if (screen_wayland->root_window)
    g_object_unref (screen_wayland->root_window);

  /* Visual Part */
  g_object_unref (screen_wayland->argb_visual);
  g_object_unref (screen_wayland->premultiplied_argb_visual);
  g_object_unref (screen_wayland->rgb_visual);

  deinit_multihead (GDK_SCREEN (object));

  G_OBJECT_CLASS (_gdk_screen_wayland_parent_class)->finalize (object);
}

static GdkDisplay *
gdk_wayland_screen_get_display (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_WAYLAND (screen)->display;
}

static gint
gdk_wayland_screen_get_width (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_WAYLAND (screen)->width;
}

static gint
gdk_wayland_screen_get_height (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_WAYLAND (screen)->height;
}

static gint
gdk_wayland_screen_get_width_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_WAYLAND (screen)->width_mm;
}

static gint
gdk_wayland_screen_get_height_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_WAYLAND (screen)->height_mm;
}

static gint
gdk_wayland_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return 0;
}

static GdkWindow *
gdk_wayland_screen_get_root_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_WAYLAND (screen)->root_window;
}

static gint
gdk_wayland_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_WAYLAND (screen)->n_monitors;
}

static gint
gdk_wayland_screen_get_primary_monitor (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_WAYLAND (screen)->primary_monitor;
}

static gint
gdk_wayland_screen_get_monitor_width_mm	(GdkScreen *screen,
					 gint       monitor_num)
{
  GdkScreenWayland *screen_wayland = GDK_SCREEN_WAYLAND (screen);

  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);
  g_return_val_if_fail (monitor_num >= 0, -1);
  g_return_val_if_fail (monitor_num < screen_wayland->n_monitors, -1);

  return screen_wayland->monitors[monitor_num].width_mm;
}

static gint
gdk_wayland_screen_get_monitor_height_mm (GdkScreen *screen,
					  gint       monitor_num)
{
  GdkScreenWayland *screen_wayland = GDK_SCREEN_WAYLAND (screen);

  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);
  g_return_val_if_fail (monitor_num >= 0, -1);
  g_return_val_if_fail (monitor_num < screen_wayland->n_monitors, -1);

  return screen_wayland->monitors[monitor_num].height_mm;
}

static gchar *
gdk_wayland_screen_get_monitor_plug_name (GdkScreen *screen,
					  gint       monitor_num)
{
  GdkScreenWayland *screen_wayland = GDK_SCREEN_WAYLAND (screen);

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  g_return_val_if_fail (monitor_num >= 0, NULL);
  g_return_val_if_fail (monitor_num < screen_wayland->n_monitors, NULL);

  return g_strdup (screen_wayland->monitors[monitor_num].output_name);
}

static void
gdk_wayland_screen_get_monitor_geometry (GdkScreen    *screen,
					 gint          monitor_num,
					 GdkRectangle *dest)
{
  GdkScreenWayland *screen_wayland = GDK_SCREEN_WAYLAND (screen);

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (monitor_num >= 0);
  g_return_if_fail (monitor_num < screen_wayland->n_monitors);

  if (dest)
    *dest = screen_wayland->monitors[monitor_num].geometry;
}

static GdkVisual *
gdk_wayland_screen_get_system_visual (GdkScreen * screen)
{
  return (GdkVisual *) GDK_SCREEN_WAYLAND (screen)->argb_visual;
}

static GdkVisual *
gdk_wayland_screen_get_rgba_visual (GdkScreen *screen)
{
  return (GdkVisual *) GDK_SCREEN_WAYLAND (screen)->argb_visual;
}

static gboolean
gdk_wayland_screen_is_composited (GdkScreen *screen)
{
  return TRUE;
}

static gchar *
gdk_wayland_screen_make_display_name (GdkScreen *screen)
{
  return NULL;
}

static GdkWindow *
gdk_wayland_screen_get_active_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
}

static GList *
gdk_wayland_screen_get_window_stack (GdkScreen *screen)
{
  return NULL;
}

static void
gdk_wayland_screen_broadcast_client_message (GdkScreen *screen,
					     GdkEvent  *event)
{
}

static gboolean
gdk_wayland_screen_get_setting (GdkScreen   *screen,
				const gchar *name,
				GValue      *value)
{
  return FALSE;
}

typedef struct _GdkWaylandVisual	GdkWaylandVisual;
typedef struct _GdkWaylandVisualClass	GdkWaylandVisualClass;

struct _GdkWaylandVisual
{
  GdkVisual visual;
  struct wl_visual *wl_visual;
};

struct _GdkWaylandVisualClass
{
  GdkVisualClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandVisual, _gdk_wayland_visual, GDK_TYPE_VISUAL)

static void
_gdk_wayland_visual_class_init (GdkWaylandVisualClass *klass)
{
}

static void
_gdk_wayland_visual_init (GdkWaylandVisual *visual)
{
}

static gint
gdk_wayland_screen_visual_get_best_depth (GdkScreen *screen)
{
  return 32;
}

static GdkVisualType
gdk_wayland_screen_visual_get_best_type (GdkScreen *screen)
{
  return GDK_VISUAL_TRUE_COLOR;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best (GdkScreen *screen)
{
  return GDK_SCREEN_WAYLAND (screen)->argb_visual;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best_with_depth (GdkScreen *screen,
					       gint       depth)
{
  return GDK_SCREEN_WAYLAND (screen)->argb_visual;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best_with_type (GdkScreen     *screen,
					      GdkVisualType  visual_type)
{
  return GDK_SCREEN_WAYLAND (screen)->argb_visual;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best_with_both (GdkScreen     *screen,
					      gint           depth,
					      GdkVisualType  visual_type)
{
  return GDK_SCREEN_WAYLAND (screen)->argb_visual;
}

static void
gdk_wayland_screen_query_depths  (GdkScreen  *screen,
				  gint      **depths,
				  gint       *count)
{
  static gint static_depths[] = { 32 };

  *count = G_N_ELEMENTS(static_depths);
  *depths = static_depths;
}

static void
gdk_wayland_screen_query_visual_types (GdkScreen      *screen,
				       GdkVisualType **visual_types,
				       gint           *count)
{
  static GdkVisualType static_visual_types[] = { GDK_VISUAL_TRUE_COLOR };

  *count = G_N_ELEMENTS(static_visual_types);
  *visual_types = static_visual_types;
}

static GList *
gdk_wayland_screen_list_visuals (GdkScreen *screen)
{
  GList *list;
  GdkScreenWayland *screen_wayland;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_wayland = GDK_SCREEN_WAYLAND (screen);

  list = g_list_append (NULL, screen_wayland->argb_visual);
  list = g_list_append (NULL, screen_wayland->premultiplied_argb_visual);
  list = g_list_append (NULL, screen_wayland->rgb_visual);

  return list;
}

#define GDK_TYPE_WAYLAND_VISUAL              (_gdk_wayland_visual_get_type ())
#define GDK_WAYLAND_VISUAL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_VISUAL, GdkWaylandVisual))

static GdkVisual *
gdk_wayland_visual_new (GdkScreen *screen, struct wl_visual *wl_visual)
{
  GdkVisual *visual;

  visual = g_object_new (GDK_TYPE_WAYLAND_VISUAL, NULL);
  visual->screen = GDK_SCREEN (screen);
  visual->type = GDK_VISUAL_TRUE_COLOR;
  visual->depth = 32;

  GDK_WAYLAND_VISUAL (visual)->wl_visual = wl_visual;

  return visual;
}

GdkScreen *
_gdk_wayland_screen_new (GdkDisplay *display)
{
  GdkScreen *screen;
  GdkScreenWayland *screen_wayland;
  GdkDisplayWayland *display_wayland;
  struct wl_visual *visual;

  display_wayland = GDK_DISPLAY_WAYLAND (display);

  screen = g_object_new (GDK_TYPE_SCREEN_WAYLAND, NULL);

  screen_wayland = GDK_SCREEN_WAYLAND (screen);
  screen_wayland->display = display;
  screen_wayland->width = 8192;
  screen_wayland->height = 8192;

  visual = display_wayland->argb_visual;
  screen_wayland->argb_visual = gdk_wayland_visual_new (screen, visual);

  visual = display_wayland->premultiplied_argb_visual;
  screen_wayland->premultiplied_argb_visual =
    gdk_wayland_visual_new (screen, visual);

  visual = display_wayland->rgb_visual;
  screen_wayland->rgb_visual = gdk_wayland_visual_new (screen, visual);

  screen_wayland->root_window =
    _gdk_wayland_screen_create_root_window (screen,
					    screen_wayland->width,
					    screen_wayland->height);

  init_multihead (screen);

  return screen;
}

static void
_gdk_screen_wayland_class_init (GdkScreenWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_wayland_screen_dispose;
  object_class->finalize = gdk_wayland_screen_finalize;

  screen_class->get_display = gdk_wayland_screen_get_display;
  screen_class->get_width = gdk_wayland_screen_get_width;
  screen_class->get_height = gdk_wayland_screen_get_height;
  screen_class->get_width_mm = gdk_wayland_screen_get_width_mm;
  screen_class->get_height_mm = gdk_wayland_screen_get_height_mm;
  screen_class->get_number = gdk_wayland_screen_get_number;
  screen_class->get_root_window = gdk_wayland_screen_get_root_window;
  screen_class->get_n_monitors = gdk_wayland_screen_get_n_monitors;
  screen_class->get_primary_monitor = gdk_wayland_screen_get_primary_monitor;
  screen_class->get_monitor_width_mm = gdk_wayland_screen_get_monitor_width_mm;
  screen_class->get_monitor_height_mm = gdk_wayland_screen_get_monitor_height_mm;
  screen_class->get_monitor_plug_name = gdk_wayland_screen_get_monitor_plug_name;
  screen_class->get_monitor_geometry = gdk_wayland_screen_get_monitor_geometry;
  screen_class->get_system_visual = gdk_wayland_screen_get_system_visual;
  screen_class->get_rgba_visual = gdk_wayland_screen_get_rgba_visual;
  screen_class->is_composited = gdk_wayland_screen_is_composited;
  screen_class->make_display_name = gdk_wayland_screen_make_display_name;
  screen_class->get_active_window = gdk_wayland_screen_get_active_window;
  screen_class->get_window_stack = gdk_wayland_screen_get_window_stack;
  screen_class->broadcast_client_message = gdk_wayland_screen_broadcast_client_message;
  screen_class->get_setting = gdk_wayland_screen_get_setting;
  screen_class->visual_get_best_depth = gdk_wayland_screen_visual_get_best_depth;
  screen_class->visual_get_best_type = gdk_wayland_screen_visual_get_best_type;
  screen_class->visual_get_best = gdk_wayland_screen_visual_get_best;
  screen_class->visual_get_best_with_depth = gdk_wayland_screen_visual_get_best_with_depth;
  screen_class->visual_get_best_with_type = gdk_wayland_screen_visual_get_best_with_type;
  screen_class->visual_get_best_with_both = gdk_wayland_screen_visual_get_best_with_both;
  screen_class->query_depths = gdk_wayland_screen_query_depths;
  screen_class->query_visual_types = gdk_wayland_screen_query_visual_types;
  screen_class->list_visuals = gdk_wayland_screen_list_visuals;
}

static void
_gdk_screen_wayland_init (GdkScreenWayland *screen_wayland)
{
}
