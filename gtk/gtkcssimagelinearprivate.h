/*
 * Copyright © 2012 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_CSS_IMAGE_LINEAR_PRIVATE_H__
#define __GTK_CSS_IMAGE_LINEAR_PRIVATE_H__

#include "gtk/gtkcssimageprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_LINEAR           (_gtk_css_image_linear_get_type ())
#define GTK_CSS_IMAGE_LINEAR(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_LINEAR, GtkCssImageLinear))
#define GTK_CSS_IMAGE_LINEAR_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_LINEAR, GtkCssImageLinearClass))
#define GTK_IS_CSS_IMAGE_LINEAR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_LINEAR))
#define GTK_IS_CSS_IMAGE_LINEAR_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_LINEAR))
#define GTK_CSS_IMAGE_LINEAR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_LINEAR, GtkCssImageLinearClass))

typedef struct _GtkCssImageLinear           GtkCssImageLinear;
typedef struct _GtkCssImageLinearClass      GtkCssImageLinearClass;
typedef struct _GtkCssImageLinearColorStop  GtkCssImageLinearColorStop;

struct _GtkCssImageLinearColorStop {
  GtkCssNumber        offset;
  union {
    GtkSymbolicColor *symbolic;
    GdkRGBA           rgba;
  }                   color;
};

struct _GtkCssImageLinear
{
  GtkCssImage parent;

  GtkCssNumber angle; /* warning: We use GTK_CSS_NUMBER as an enum for the corners */
  GArray *stops;
  guint is_computed :1;
  guint repeating :1;
};

struct _GtkCssImageLinearClass
{
  GtkCssImageClass parent_class;
};

GType          _gtk_css_image_linear_get_type             (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GTK_CSS_IMAGE_LINEAR_PRIVATE_H__ */
