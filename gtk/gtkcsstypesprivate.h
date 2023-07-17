/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_CSS_TYPES_PRIVATE_H__
#define __GTK_CSS_TYPES_PRIVATE_H__

#include <glib-object.h>
#include <gtk/gtkstylecontext.h>

G_BEGIN_DECLS

typedef enum { /*< skip >*/
  GTK_CSS_CHANGE_CLASS                    = (1 <<  0),
  GTK_CSS_CHANGE_NAME                     = (1 <<  1),
  GTK_CSS_CHANGE_ID                       = GTK_CSS_CHANGE_NAME,
  GTK_CSS_CHANGE_REGION                   = GTK_CSS_CHANGE_NAME,
  GTK_CSS_CHANGE_POSITION                 = (1 <<  2),
  GTK_CSS_CHANGE_STATE                    = (1 <<  3),
  GTK_CSS_CHANGE_SIBLING_CLASS            = (1 <<  4),
  GTK_CSS_CHANGE_SIBLING_NAME             = (1 <<  5),
  GTK_CSS_CHANGE_SIBLING_POSITION         = (1 <<  6),
  GTK_CSS_CHANGE_SIBLING_STATE            = (1 <<  7),
  GTK_CSS_CHANGE_PARENT_CLASS             = (1 <<  8),
  GTK_CSS_CHANGE_PARENT_NAME              = (1 <<  9),
  GTK_CSS_CHANGE_PARENT_POSITION          = (1 << 10),
  GTK_CSS_CHANGE_PARENT_STATE             = (1 << 11),
  GTK_CSS_CHANGE_PARENT_SIBLING_CLASS     = (1 << 12),
  GTK_CSS_CHANGE_PARENT_SIBLING_NAME      = (1 << 13),
  GTK_CSS_CHANGE_PARENT_SIBLING_POSITION  = (1 << 14),
  GTK_CSS_CHANGE_PARENT_SIBLING_STATE     = (1 << 15),
  /* add more */
  GTK_CSS_CHANGE_SOURCE                   = (1 << 16),
  GTK_CSS_CHANGE_ANIMATE                  = (1 << 17)
} GtkCssChange;

#define GTK_CSS_CHANGE_ANY ((1 << 18) - 1)
#define GTK_CSS_CHANGE_ANY_SELF (GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_POSITION | GTK_CSS_CHANGE_STATE)
#define GTK_CSS_CHANGE_ANY_SIBLING (GTK_CSS_CHANGE_SIBLING_CLASS | GTK_CSS_CHANGE_SIBLING_NAME | \
                                    GTK_CSS_CHANGE_SIBLING_POSITION | GTK_CSS_CHANGE_SIBLING_STATE)
#define GTK_CSS_CHANGE_ANY_PARENT (GTK_CSS_CHANGE_PARENT_CLASS | GTK_CSS_CHANGE_PARENT_SIBLING_CLASS | \
                                   GTK_CSS_CHANGE_PARENT_NAME | GTK_CSS_CHANGE_PARENT_SIBLING_NAME | \
                                   GTK_CSS_CHANGE_PARENT_POSITION | GTK_CSS_CHANGE_PARENT_SIBLING_POSITION | \
                                   GTK_CSS_CHANGE_PARENT_STATE | GTK_CSS_CHANGE_PARENT_SIBLING_STATE)

typedef enum /*< skip >*/ {
  GTK_CSS_DEPENDS_ON_PARENT = (1 << 0),
  GTK_CSS_EQUALS_PARENT = (1 << 1),
  GTK_CSS_DEPENDS_ON_COLOR = (1 << 2),
  GTK_CSS_DEPENDS_ON_FONT_SIZE = (1 << 3)
} GtkCssDependencies;

enum { /*< skip >*/
  GTK_CSS_PROPERTY_COLOR,
  GTK_CSS_PROPERTY_FONT_SIZE,
  GTK_CSS_PROPERTY_BACKGROUND_COLOR,
  GTK_CSS_PROPERTY_FONT_FAMILY,
  GTK_CSS_PROPERTY_FONT_STYLE,
  GTK_CSS_PROPERTY_FONT_VARIANT,
  GTK_CSS_PROPERTY_FONT_WEIGHT,
  GTK_CSS_PROPERTY_TEXT_SHADOW,
  GTK_CSS_PROPERTY_ICON_SHADOW,
  GTK_CSS_PROPERTY_BOX_SHADOW,
  GTK_CSS_PROPERTY_MARGIN_TOP,
  GTK_CSS_PROPERTY_MARGIN_LEFT,
  GTK_CSS_PROPERTY_MARGIN_BOTTOM,
  GTK_CSS_PROPERTY_MARGIN_RIGHT,
  GTK_CSS_PROPERTY_PADDING_TOP,
  GTK_CSS_PROPERTY_PADDING_LEFT,
  GTK_CSS_PROPERTY_PADDING_BOTTOM,
  GTK_CSS_PROPERTY_PADDING_RIGHT,
  GTK_CSS_PROPERTY_BORDER_TOP_STYLE,
  GTK_CSS_PROPERTY_BORDER_TOP_WIDTH,
  GTK_CSS_PROPERTY_BORDER_LEFT_STYLE,
  GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH,
  GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE,
  GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH,
  GTK_CSS_PROPERTY_BORDER_TOP_LEFT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_TOP_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_RIGHT_RADIUS,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_LEFT_RADIUS,
  GTK_CSS_PROPERTY_OUTLINE_STYLE,
  GTK_CSS_PROPERTY_OUTLINE_WIDTH,
  GTK_CSS_PROPERTY_OUTLINE_OFFSET,
  GTK_CSS_PROPERTY_BACKGROUND_CLIP,
  GTK_CSS_PROPERTY_BACKGROUND_ORIGIN,
  GTK_CSS_PROPERTY_BACKGROUND_SIZE,
  GTK_CSS_PROPERTY_BACKGROUND_POSITION,
  GTK_CSS_PROPERTY_BORDER_TOP_COLOR,
  GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR,
  GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR,
  GTK_CSS_PROPERTY_BORDER_LEFT_COLOR,
  GTK_CSS_PROPERTY_OUTLINE_COLOR,
  GTK_CSS_PROPERTY_BACKGROUND_REPEAT,
  GTK_CSS_PROPERTY_BACKGROUND_IMAGE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_REPEAT,
  GTK_CSS_PROPERTY_BORDER_IMAGE_SLICE,
  GTK_CSS_PROPERTY_BORDER_IMAGE_WIDTH,
  GTK_CSS_PROPERTY_TRANSITION_PROPERTY,
  GTK_CSS_PROPERTY_TRANSITION_DURATION,
  GTK_CSS_PROPERTY_TRANSITION_TIMING_FUNCTION,
  GTK_CSS_PROPERTY_TRANSITION_DELAY,
  GTK_CSS_PROPERTY_ENGINE,
  GTK_CSS_PROPERTY_GTK_KEY_BINDINGS,
  /* add more */
  GTK_CSS_PROPERTY_N_PROPERTIES
};

typedef enum /*< skip >*/ {
  GTK_CSS_AREA_BORDER_BOX,
  GTK_CSS_AREA_PADDING_BOX,
  GTK_CSS_AREA_CONTENT_BOX
} GtkCssArea;

/* for the order in arrays */
typedef enum /*< skip >*/ {
  GTK_CSS_TOP,
  GTK_CSS_RIGHT,
  GTK_CSS_BOTTOM,
  GTK_CSS_LEFT
} GtkCssSide;

typedef enum /*< skip >*/ {
  GTK_CSS_TOP_LEFT,
  GTK_CSS_TOP_RIGHT,
  GTK_CSS_BOTTOM_RIGHT,
  GTK_CSS_BOTTOM_LEFT
} GtkCssCorner;

typedef enum /*< skip >*/ {
  /* CSS term: <number> */
  GTK_CSS_NUMBER,
  /* CSS term: <percentage> */
  GTK_CSS_PERCENT,
  /* CSS term: <length> */
  GTK_CSS_PX,
  GTK_CSS_PT,
  GTK_CSS_EM,
  GTK_CSS_EX,
  GTK_CSS_PC,
  GTK_CSS_IN,
  GTK_CSS_CM,
  GTK_CSS_MM,
  /* CSS term: <angle> */
  GTK_CSS_RAD,
  GTK_CSS_DEG,
  GTK_CSS_GRAD,
  GTK_CSS_TURN,
  /* CSS term: <time> */
  GTK_CSS_S,
  GTK_CSS_MS,
} GtkCssUnit;

GtkCssChange            _gtk_css_change_for_sibling              (GtkCssChange       match);
GtkCssChange            _gtk_css_change_for_child                (GtkCssChange       match);
GtkCssDependencies      _gtk_css_dependencies_union              (GtkCssDependencies first,
                                                                  GtkCssDependencies second);


G_END_DECLS

#endif /* __GTK_CSS_TYPES_PRIVATE_H__ */
