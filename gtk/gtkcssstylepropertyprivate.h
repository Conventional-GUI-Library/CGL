/*
 * Copyright © 2011 Red Hat Inc.
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

#ifndef __GTK_CSS_STYLE_PROPERTY_PRIVATE_H__
#define __GTK_CSS_STYLE_PROPERTY_PRIVATE_H__

#include "gtk/gtkstylepropertyprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STYLE_PROPERTY           (_gtk_css_style_property_get_type ())
#define GTK_CSS_STYLE_PROPERTY(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_STYLE_PROPERTY, GtkCssStyleProperty))
#define GTK_CSS_STYLE_PROPERTY_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_STYLE_PROPERTY, GtkCssStylePropertyClass))
#define GTK_IS_CSS_STYLE_PROPERTY(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_STYLE_PROPERTY))
#define GTK_IS_CSS_STYLE_PROPERTY_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_STYLE_PROPERTY))
#define GTK_CSS_STYLE_PROPERTY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_STYLE_PROPERTY, GtkCssStylePropertyClass))

typedef struct _GtkCssStyleProperty           GtkCssStyleProperty;
typedef struct _GtkCssStylePropertyClass      GtkCssStylePropertyClass;

typedef gboolean         (* GtkCssStylePropertyParseFunc)  (GtkCssStyleProperty    *property,
                                                            GValue                 *value,
                                                            GtkCssParser           *parser,
                                                            GFile                  *base);
typedef void             (* GtkCssStylePropertyPrintFunc)  (GtkCssStyleProperty    *property,
                                                            const GValue           *value,
                                                            GString                *string);
typedef void             (* GtkCssStylePropertyComputeFunc)(GtkCssStyleProperty    *property,
                                                            GValue                 *computed,
                                                            GtkStyleContext        *context,
                                                            const GValue           *specified);
struct _GtkCssStyleProperty
{
  GtkStyleProperty parent;

  GType computed_type;
  GValue initial_value;
  guint id;
  guint inherit :1;

  GtkCssStylePropertyParseFunc parse_value;
  GtkCssStylePropertyPrintFunc print_value;
  GtkCssStylePropertyComputeFunc compute_value;
};

struct _GtkCssStylePropertyClass
{
  GtkStylePropertyClass parent_class;

  GPtrArray *style_properties;
};

GType                   _gtk_css_style_property_get_type        (void) G_GNUC_CONST;

void                    _gtk_css_style_property_init_properties (void);

guint                   _gtk_css_style_property_get_n_properties(void);
GtkCssStyleProperty *   _gtk_css_style_property_lookup_by_id    (guint                   id);

gboolean                _gtk_css_style_property_is_inherit      (GtkCssStyleProperty    *property);
guint                   _gtk_css_style_property_get_id          (GtkCssStyleProperty    *property);
const GValue *          _gtk_css_style_property_get_initial_value
                                                                (GtkCssStyleProperty    *property);
GType                   _gtk_css_style_property_get_computed_type (GtkCssStyleProperty *property);
GType                   _gtk_css_style_property_get_specified_type (GtkCssStyleProperty *property);
gboolean                _gtk_css_style_property_is_specified_type (GtkCssStyleProperty  *property,
                                                                 GType                   type);

void                    _gtk_css_style_property_compute_value   (GtkCssStyleProperty    *property,
                                                                 GValue                 *computed,
                                                                 GtkStyleContext        *context,
                                                                 const GValue           *specified);

void                    _gtk_css_style_property_print_value     (GtkCssStyleProperty    *property,
                                                                 const GValue           *value,
                                                                 GString                *string);
                                                                 

G_END_DECLS

#endif /* __GTK_CSS_STYLE_PROPERTY_PRIVATE_H__ */
