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

#include "config.h"

#include "gtkcsscustompropertyprivate.h"

#include <string.h>

#include "gtkcssstylefuncsprivate.h"
#include "gtkthemingengine.h"

G_DEFINE_TYPE (GtkCssCustomProperty, _gtk_css_custom_property, GTK_TYPE_CSS_STYLE_PROPERTY)

static gboolean
gtk_css_custom_property_parse_value (GtkStyleProperty *property,
                                     GValue           *value,
                                     GtkCssParser     *parser,
                                     GFile            *base)
{
  GtkCssCustomProperty *custom = GTK_CSS_CUSTOM_PROPERTY (property);
  gboolean success;

  if (custom->property_parse_func)
    {
      GError *error = NULL;
      char *value_str;
      
      g_value_init (value, _gtk_style_property_get_value_type (property));

      value_str = _gtk_css_parser_read_value (parser);
      if (value_str != NULL)
        {
          success = (* custom->property_parse_func) (value_str, value, &error);
          g_free (value_str);
        }
      else
        success = FALSE;
    }
  else
    {
      GtkCssStyleProperty *style = GTK_CSS_STYLE_PROPERTY (property);
      g_value_init (value, _gtk_css_style_property_get_specified_type (style));

      success = _gtk_css_style_parse_value (value, parser, base);
    }

  if (!success)
    g_value_unset (value);

  return success;
}

static void
_gtk_css_custom_property_class_init (GtkCssCustomPropertyClass *klass)
{
  GtkStylePropertyClass *property_class = GTK_STYLE_PROPERTY_CLASS (klass);

  property_class->parse_value = gtk_css_custom_property_parse_value;
}


static void
_gtk_css_custom_property_init (GtkCssCustomProperty *custom_property)
{
}

static GType
gtk_css_custom_property_get_specified_type (GParamSpec *pspec)
{
  if (pspec->value_type == GDK_TYPE_RGBA ||
      pspec->value_type == GDK_TYPE_COLOR)
    return GTK_TYPE_SYMBOLIC_COLOR;
  else
    return pspec->value_type;
}

static void
gtk_css_custom_property_create_initial_value (GParamSpec *pspec,
                                              GValue     *value)
{
  g_value_init (value, gtk_css_custom_property_get_specified_type (pspec));

  if (pspec->value_type == GTK_TYPE_THEMING_ENGINE)
    g_value_set_object (value, gtk_theming_engine_load (NULL));
  else if (pspec->value_type == PANGO_TYPE_FONT_DESCRIPTION)
    g_value_take_boxed (value, pango_font_description_from_string ("Sans 10"));
  else if (pspec->value_type == GDK_TYPE_RGBA ||
           pspec->value_type == GDK_TYPE_COLOR)
    {
      GdkRGBA color;
      gdk_rgba_parse (&color, "pink");
      g_value_take_boxed (value, gtk_symbolic_color_new_literal (&color));
    }
  else if (pspec->value_type == GTK_TYPE_BORDER)
    {
      g_value_take_boxed (value, gtk_border_new ());
    }
  else
    g_param_value_set_default (pspec, value);
}

/* Property registration functions */

/**
 * gtk_theming_engine_register_property: (skip)
 * @name_space: namespace for the property name
 * @parse_func: parsing function to use, or %NULL
 * @pspec: the #GParamSpec for the new property
 *
 * Registers a property so it can be used in the CSS file format,
 * on the CSS file the property will look like
 * "-${@name_space}-${property_name}". being
 * ${property_name} the given to @pspec. @name_space will usually
 * be the theme engine name.
 *
 * For any type a @parse_func may be provided, being this function
 * used for turning any property value (between ':' and ';') in
 * CSS to the #GValue needed. For basic types there is already
 * builtin parsing support, so %NULL may be provided for these
 * cases.
 *
 * <note>
 * Engines must ensure property registration happens exactly once,
 * usually GTK+ deals with theming engines as singletons, so this
 * should be guaranteed to happen once, but bear this in mind
 * when creating #GtkThemeEngine<!-- -->s yourself.
 * </note>
 *
 * <note>
 * In order to make use of the custom registered properties in
 * the CSS file, make sure the engine is loaded first by specifying
 * the engine property, either in a previous rule or within the same
 * one.
 * <programlisting>
 * &ast; {
 *     engine: someengine;
 *     -SomeEngine-custom-property: 2;
 * }
 * </programlisting>
 * </note>
 *
 * Since: 3.0
 **/
void
gtk_theming_engine_register_property (const gchar            *name_space,
                                      GtkStylePropertyParser  parse_func,
                                      GParamSpec             *pspec)
{
  GtkCssCustomProperty *node;
  GValue initial = { 0, };
  gchar *name;

  g_return_if_fail (name_space != NULL);
  g_return_if_fail (strchr (name_space, ' ') == NULL);
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  name = g_strdup_printf ("-%s-%s", name_space, pspec->name);
  gtk_css_custom_property_create_initial_value (pspec, &initial);

  node = g_object_new (GTK_TYPE_CSS_CUSTOM_PROPERTY,
                       "initial-value", &initial,
                       "name", name,
                       "computed-type", pspec->value_type,
                       "value-type", pspec->value_type,
                       NULL);
  node->pspec = pspec;
  node->property_parse_func = parse_func;

  g_value_unset (&initial);
  g_free (name);
}

/**
 * gtk_style_properties_register_property: (skip)
 * @parse_func: parsing function to use, or %NULL
 * @pspec: the #GParamSpec for the new property
 *
 * Registers a property so it can be used in the CSS file format.
 * This function is the low-level equivalent of
 * gtk_theming_engine_register_property(), if you are implementing
 * a theming engine, you want to use that function instead.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_register_property (GtkStylePropertyParser  parse_func,
                                        GParamSpec             *pspec)
{
  GtkCssCustomProperty *node;
  GValue initial = { 0, };

  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  gtk_css_custom_property_create_initial_value (pspec, &initial);

  node = g_object_new (GTK_TYPE_CSS_CUSTOM_PROPERTY,
                       "initial-value", &initial,
                       "name", pspec->name,
                       "computed-type", pspec->value_type,
                       "value-type", pspec->value_type,
                       NULL);
  node->pspec = pspec;
  node->property_parse_func = parse_func;

  g_value_unset (&initial);
}

/**
 * gtk_style_properties_lookup_property: (skip)
 * @property_name: property name to look up
 * @parse_func: (out): return location for the parse function
 * @pspec: (out) (transfer none): return location for the #GParamSpec
 *
 * Returns %TRUE if a property has been registered, if @pspec or
 * @parse_func are not %NULL, the #GParamSpec and parsing function
 * will be respectively returned.
 *
 * Returns: %TRUE if the property is registered, %FALSE otherwise
 *
 * Since: 3.0
 **/
gboolean
gtk_style_properties_lookup_property (const gchar             *property_name,
                                      GtkStylePropertyParser  *parse_func,
                                      GParamSpec             **pspec)
{
  GtkStyleProperty *node;
  gboolean found = FALSE;

  g_return_val_if_fail (property_name != NULL, FALSE);

  node = _gtk_style_property_lookup (property_name);

  if (GTK_IS_CSS_CUSTOM_PROPERTY (node))
    {
      GtkCssCustomProperty *custom = GTK_CSS_CUSTOM_PROPERTY (node);

      if (pspec)
        *pspec = custom->pspec;

      if (parse_func)
        *parse_func = custom->property_parse_func;

      found = TRUE;
    }

  return found;
}

