/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 * 
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_THEMING_BACKGROUND_PRIVATE_H__
#define __GTK_THEMING_BACKGROUND_PRIVATE_H__

#include <glib-object.h>
#include <cairo.h>

#include "gtkcssimageprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkroundedboxprivate.h"
#include "gtkshadowprivate.h"

G_BEGIN_DECLS

typedef struct _GtkThemingBackground GtkThemingBackground;

struct _GtkThemingBackground {
  GtkThemingEngine *engine;

  cairo_rectangle_t paint_area;
  cairo_rectangle_t image_rect;

  GtkJunctionSides junction;
  GtkCssImage *image;

  GtkRoundedBox padding_box;
  GtkRoundedBox clip_box;

  GtkStateFlags flags;
  GtkBorder border;
  GtkBorder padding;
  GdkRGBA bg_color;
};

void _gtk_theming_background_init (GtkThemingBackground *bg,
                                   GtkThemingEngine     *engine,
                                   gdouble               x,
                                   gdouble               y,
                                   gdouble               width,
                                   gdouble               height,
                                   GtkJunctionSides      junction);

void _gtk_theming_background_render (GtkThemingBackground *bg,
                                     cairo_t              *cr);

gboolean _gtk_theming_background_has_background_image (GtkThemingBackground *bg);

G_END_DECLS

#endif /* __GTK_THEMING_BACKGROUND_PRIVATE_H__ */
