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

#ifndef __GTK_CSS_COMPUTED_VALUES_PRIVATE_H__
#define __GTK_CSS_COMPUTED_VALUES_PRIVATE_H__

#include <glib-object.h>

#include "gtk/gtkcsssection.h"
#include "gtk/gtkstylecontext.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_COMPUTED_VALUES           (_gtk_css_computed_values_get_type ())
#define GTK_CSS_COMPUTED_VALUES(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_COMPUTED_VALUES, GtkCssComputedValues))
#define GTK_CSS_COMPUTED_VALUES_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_COMPUTED_VALUES, GtkCssComputedValuesClass))
#define GTK_IS_CSS_COMPUTED_VALUES(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_COMPUTED_VALUES))
#define GTK_IS_CSS_COMPUTED_VALUES_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_COMPUTED_VALUES))
#define GTK_CSS_COMPUTED_VALUES_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_COMPUTED_VALUES, GtkCssComputedValuesClass))

typedef struct _GtkCssComputedValues           GtkCssComputedValues;
typedef struct _GtkCssComputedValuesClass      GtkCssComputedValuesClass;

struct _GtkCssComputedValues
{
  GObject parent;

  GValueArray           *values;
  GPtrArray             *sections;
};

struct _GtkCssComputedValuesClass
{
  GObjectClass parent_class;
};

GType                   _gtk_css_computed_values_get_type             (void) G_GNUC_CONST;

GtkCssComputedValues *  _gtk_css_computed_values_new                  (void);

void                    _gtk_css_computed_values_compute_value        (GtkCssComputedValues     *values,
                                                                       GtkStyleContext          *context,
                                                                       guint                     id,
                                                                       const GValue             *specified,
                                                                       GtkCssSection            *section);
void                    _gtk_css_computed_values_set_value            (GtkCssComputedValues     *values,
                                                                       guint                     id,
                                                                       const GValue             *value,
                                                                       GtkCssSection            *section);
                                                                        
const GValue *          _gtk_css_computed_values_get_value            (GtkCssComputedValues     *values,
                                                                       guint                     id);
const GValue *          _gtk_css_computed_values_get_value_by_name    (GtkCssComputedValues     *values,
                                                                       const char               *name);
GtkCssSection *         _gtk_css_computed_values_get_section          (GtkCssComputedValues     *values,
                                                                       guint                     id);


G_END_DECLS

#endif /* __GTK_CSS_COMPUTED_VALUES_PRIVATE_H__ */
