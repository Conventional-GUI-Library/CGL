/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include "gtkstylepropertyprivate.h"

#include <gobject/gvaluecollector.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkcssparserprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkintl.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkstylepropertiesprivate.h"

/* the actual parsers we have */
#include "gtkanimationdescription.h"
#include "gtkbindings.h"
#include "gtkcssimageprivate.h"
#include "gtkgradient.h"
#include "gtkshadowprivate.h"
#include "gtkthemingengine.h"
#include "gtktypebuiltins.h"
#include "gtkwin32themeprivate.h"

/*** REGISTRATION ***/

static void
color_compute (GtkCssStyleProperty    *property,
               GValue                 *computed,
               GtkStyleContext        *context,
               const GValue           *specified)
{
  g_value_init (computed, GDK_TYPE_RGBA);

  /* for when resolvage fails */
restart:

  if (G_VALUE_HOLDS (specified, GTK_TYPE_CSS_SPECIAL_VALUE))
    {
      g_assert (g_value_get_enum (specified) == GTK_CSS_CURRENT_COLOR);

      /* The computed value of the ‘currentColor’ keyword is the computed
       * value of the ‘color’ property. If the ‘currentColor’ keyword is
       * set on the ‘color’ property itself, it is treated as ‘color: inherit’. 
       */
      if (g_str_equal (_gtk_style_property_get_name (GTK_STYLE_PROPERTY (property)), "color"))
        {
          GtkStyleContext *parent = gtk_style_context_get_parent (context);

          if (parent)
            g_value_copy (_gtk_style_context_peek_property (parent, "color"), computed);
          else
            _gtk_css_style_compute_value (computed,
                                          context,
                                          _gtk_css_style_property_get_initial_value (property));
        }
      else
        {
          g_value_copy (_gtk_style_context_peek_property (context, "color"), computed);
        }
    }
  else if (G_VALUE_HOLDS (specified, GTK_TYPE_SYMBOLIC_COLOR))
    {
      GdkRGBA rgba;

      if (!_gtk_style_context_resolve_color (context,
                                             g_value_get_boxed (specified),
                                             &rgba))
        {
          specified = _gtk_css_style_property_get_initial_value (property);
          goto restart;
        }

      g_value_set_boxed (computed, &rgba);
    }
  else
    g_value_copy (specified, computed);
}

static void
_gtk_style_property_register (const char *                   name,
                              GType                          value_type,
                              GtkStylePropertyFlags          flags,
                              GtkCssStylePropertyParseFunc   parse_value,
                              GtkCssStylePropertyPrintFunc   print_value,
                              GtkCssStylePropertyComputeFunc compute_value,
                              const GValue *                 initial_value)
{
  GtkCssStyleProperty *node;

  node = g_object_new (GTK_TYPE_CSS_STYLE_PROPERTY,
                       "inherit", (flags & GTK_STYLE_PROPERTY_INHERIT) ? TRUE : FALSE,
                       "initial-value", initial_value,
                       "name", name,
                       "value-type", value_type,
                       NULL);
  
  if (parse_value)
    node->parse_value = parse_value;
  if (print_value)
    node->print_value = print_value;
  if (compute_value)
    node->compute_value = compute_value;
}

static void
gtk_style_property_register (const char *                   name,
                             GType                          value_type,
                             GtkStylePropertyFlags          flags,
                             GtkCssStylePropertyParseFunc   parse_value,
                             GtkCssStylePropertyPrintFunc   print_value,
                             GtkCssStylePropertyComputeFunc compute_value,
                             ...)
{
  GValue initial_value = G_VALUE_INIT;
  char *error = NULL;
  va_list args;

  va_start (args, compute_value);
  G_VALUE_COLLECT_INIT (&initial_value, value_type,
                        args, 0, &error);
  if (error)
    {
      g_error ("property `%s' initial value is broken: %s", name, error);
      g_value_unset (&initial_value);
      return;
    }

  va_end (args);

  _gtk_style_property_register (name,
                                value_type,
                                flags,
                                parse_value,
                                print_value,
                                compute_value,
                                &initial_value);

  g_value_unset (&initial_value);
}

/*** HELPERS ***/

static void
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_dtostr (buf, sizeof (buf), d);
  g_string_append (string, buf);
}

static void
string_append_string (GString    *str,
                      const char *string)
{
  gsize len;

  g_string_append_c (str, '"');

  do {
    len = strcspn (string, "\"\n\r\f");
    g_string_append (str, string);
    string += len;
    switch (*string)
      {
      case '\0':
        break;
      case '\n':
        g_string_append (str, "\\A ");
        break;
      case '\r':
        g_string_append (str, "\\D ");
        break;
      case '\f':
        g_string_append (str, "\\C ");
        break;
      case '\"':
        g_string_append (str, "\\\"");
        break;
      default:
        g_assert_not_reached ();
        break;
      }
  } while (*string);

  g_string_append_c (str, '"');
}

/*** IMPLEMENTATIONS ***/

static gboolean
font_family_parse (GtkCssStyleProperty *property,
                   GValue              *value,
                   GtkCssParser        *parser,
                   GFile               *base)
{
  GPtrArray *names;
  char *name;

  /* We don't special case generic families. Pango should do
   * that for us */

  names = g_ptr_array_new ();

  do {
    name = _gtk_css_parser_try_ident (parser, TRUE);
    if (name)
      {
        GString *string = g_string_new (name);
        g_free (name);
        while ((name = _gtk_css_parser_try_ident (parser, TRUE)))
          {
            g_string_append_c (string, ' ');
            g_string_append (string, name);
            g_free (name);
          }
        name = g_string_free (string, FALSE);
      }
    else 
      {
        name = _gtk_css_parser_read_string (parser);
        if (name == NULL)
          {
            g_ptr_array_free (names, TRUE);
            return FALSE;
          }
      }

    g_ptr_array_add (names, name);
  } while (_gtk_css_parser_try (parser, ",", TRUE));

  /* NULL-terminate array */
  g_ptr_array_add (names, NULL);
  g_value_set_boxed (value, g_ptr_array_free (names, FALSE));
  return TRUE;
}

static void
font_family_value_print (GtkCssStyleProperty *property,
                         const GValue        *value,
                         GString             *string)
{
  const char **names = g_value_get_boxed (value);

  if (names == NULL || *names == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  string_append_string (string, *names);
  names++;
  while (*names)
    {
      g_string_append (string, ", ");
      string_append_string (string, *names);
      names++;
    }
}

static gboolean 
bindings_value_parse (GtkCssStyleProperty *property,
                      GValue              *value,
                      GtkCssParser        *parser,
                      GFile               *base)
{
  GPtrArray *array;
  GtkBindingSet *binding_set;
  char *name;

  array = g_ptr_array_new ();

  do {
      name = _gtk_css_parser_try_ident (parser, TRUE);
      if (name == NULL)
        {
          _gtk_css_parser_error (parser, "Not a valid binding name");
          g_ptr_array_free (array, TRUE);
          return FALSE;
        }

      binding_set = gtk_binding_set_find (name);

      if (!binding_set)
        {
          _gtk_css_parser_error (parser, "No binding set named '%s'", name);
          g_free (name);
          continue;
        }

      g_ptr_array_add (array, binding_set);
      g_free (name);
    }
  while (_gtk_css_parser_try (parser, ",", TRUE));

  g_value_take_boxed (value, array);

  return TRUE;
}

static void
bindings_value_print (GtkCssStyleProperty *property,
                      const GValue        *value,
                      GString             *string)
{
  GPtrArray *array;
  guint i;

  array = g_value_get_boxed (value);

  for (i = 0; i < array->len; i++)
    {
      GtkBindingSet *binding_set = g_ptr_array_index (array, i);

      if (i > 0)
        g_string_append (string, ", ");
      g_string_append (string, binding_set->set_name);
    }
}

static gboolean 
border_corner_radius_value_parse (GtkCssStyleProperty *property,
                                  GValue              *value,
                                  GtkCssParser        *parser,
                                  GFile               *base)
{
  GtkCssBorderCornerRadius corner;

  if (!_gtk_css_parser_try_double (parser, &corner.horizontal))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }
  else if (corner.horizontal < 0)
    goto negative;

  if (!_gtk_css_parser_try_double (parser, &corner.vertical))
    corner.vertical = corner.horizontal;
  else if (corner.vertical < 0)
    goto negative;

  g_value_set_boxed (value, &corner);
  return TRUE;

negative:
  _gtk_css_parser_error (parser, "Border radius values cannot be negative");
  return FALSE;
}

static void
border_corner_radius_value_print (GtkCssStyleProperty *property,
                                  const GValue        *value,
                                  GString             *string)
{
  GtkCssBorderCornerRadius *corner;

  corner = g_value_get_boxed (value);

  if (corner == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  string_append_double (string, corner->horizontal);
  if (corner->horizontal != corner->vertical)
    {
      g_string_append_c (string, ' ');
      string_append_double (string, corner->vertical);
    }
}

static gboolean 
css_image_value_parse (GtkCssStyleProperty *property,
                       GValue              *value,
                       GtkCssParser        *parser,
                       GFile               *base)
{
  GtkCssImage *image;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    image = NULL;
  else
    {
      image = _gtk_css_image_new_parse (parser, base);
      if (image == NULL)
        return FALSE;
    }

  g_value_unset (value);
  g_value_init (value, GTK_TYPE_CSS_IMAGE);
  g_value_take_object (value, image);
  return TRUE;
}

static void
css_image_value_print (GtkCssStyleProperty *property,
                       const GValue        *value,
                       GString             *string)
{
  GtkCssImage *image = g_value_get_object (value);

  if (image)
    _gtk_css_image_print (image, string);
  else
    g_string_append (string, "none");
}

static void
css_image_value_compute (GtkCssStyleProperty    *property,
                         GValue                 *computed,
                         GtkStyleContext        *context,
                         const GValue           *specified)
{
  GtkCssImage *image = g_value_get_object (specified);

  if (image)
    image = _gtk_css_image_compute (image, context);

  g_value_init (computed, GTK_TYPE_CSS_IMAGE);
  g_value_take_object (computed, image);
}

static void
compute_border_width (GtkCssStyleProperty    *property,
                      GValue                 *computed,
                      GtkStyleContext        *context,
                      const GValue           *specified)
{
  GtkCssStyleProperty *style;
  GtkBorderStyle border_style;
  
  /* The -1 is magic that is only true because we register the style
   * properties directly after the width properties.
   */
  style = _gtk_css_style_property_lookup_by_id (_gtk_css_style_property_get_id (property) - 1);
  border_style = g_value_get_enum (_gtk_style_context_peek_property (context, _gtk_style_property_get_name (GTK_STYLE_PROPERTY (style))));

  g_value_init (computed, G_TYPE_INT);
  if (border_style == GTK_BORDER_STYLE_NONE ||
      border_style == GTK_BORDER_STYLE_HIDDEN)
    g_value_set_int (computed, 0);
  else
    g_value_copy (specified, computed);
}

static gboolean
background_repeat_value_parse (GtkCssStyleProperty *property,
                               GValue              *value,
                               GtkCssParser        *parser,
                               GFile               *base)
{
  int repeat, vertical;

  if (!_gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BACKGROUND_REPEAT, &repeat))
    {
      _gtk_css_parser_error (parser, "Not a valid value");
      return FALSE;
    }

  if (repeat <= GTK_CSS_BACKGROUND_REPEAT_MASK)
    {
      if (_gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BACKGROUND_REPEAT, &vertical))
        {
          if (vertical >= GTK_CSS_BACKGROUND_REPEAT_MASK)
            {
              _gtk_css_parser_error (parser, "Not a valid 2nd value");
              return FALSE;
            }
          else
            repeat |= vertical << GTK_CSS_BACKGROUND_REPEAT_SHIFT;
        }
      else
        repeat |= repeat << GTK_CSS_BACKGROUND_REPEAT_SHIFT;
    }

  g_value_set_enum (value, repeat);
  return TRUE;
}

static void
background_repeat_value_print (GtkCssStyleProperty *property,
                               const GValue        *value,
                               GString             *string)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  GtkCssBackgroundRepeat repeat;

  repeat = g_value_get_enum (value);
  enum_class = g_type_class_ref (GTK_TYPE_CSS_BACKGROUND_REPEAT);
  enum_value = g_enum_get_value (enum_class, repeat);

  /* only triggers for 'repeat-x' and 'repeat-y' */
  if (enum_value)
    g_string_append (string, enum_value->value_nick);
  else
    {
      enum_value = g_enum_get_value (enum_class, GTK_CSS_BACKGROUND_HORIZONTAL (repeat));
      g_string_append (string, enum_value->value_nick);

      if (GTK_CSS_BACKGROUND_HORIZONTAL (repeat) != GTK_CSS_BACKGROUND_VERTICAL (repeat))
        {
          enum_value = g_enum_get_value (enum_class, GTK_CSS_BACKGROUND_VERTICAL (repeat));
          g_string_append (string, " ");
          g_string_append (string, enum_value->value_nick);
        }
    }

  g_type_class_unref (enum_class);
}

/*** REGISTRATION ***/

#define rgba_init(rgba, r, g, b, a) G_STMT_START{ \
  (rgba)->red = (r); \
  (rgba)->green = (g); \
  (rgba)->blue = (b); \
  (rgba)->alpha = (a); \
}G_STMT_END
void
_gtk_css_style_property_init_properties (void)
{
  GValue value = { 0, };
  char *default_font_family[] = { "Sans", NULL };
  GdkRGBA rgba;
  GtkCssBorderCornerRadius no_corner_radius = { 0, };
  GtkBorder border_of_ones = { 1, 1, 1, 1 };
  GtkCssBorderImageRepeat border_image_repeat = { GTK_CSS_REPEAT_STYLE_STRETCH, GTK_CSS_REPEAT_STYLE_STRETCH };

  /* Initialize "color" and "font-size" first,
   * so that when computing values later they are
   * done first. That way, 'currentColor' and font
   * sizes in em can be looked up properly */
  rgba_init (&rgba, 1, 1, 1, 1);
  gtk_style_property_register            ("color",
                                          GDK_TYPE_RGBA,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          &rgba);
  gtk_style_property_register            ("font-size",
                                          G_TYPE_DOUBLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          10.0);

  /* properties that aren't referenced when computing values
   * start here */
  rgba_init (&rgba, 0, 0, 0, 0);
  gtk_style_property_register            ("background-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          &rgba);

  gtk_style_property_register            ("font-family",
                                          G_TYPE_STRV,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          font_family_parse,
                                          font_family_value_print,
                                          NULL,
                                          default_font_family);
  gtk_style_property_register            ("font-style",
                                          PANGO_TYPE_STYLE,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          PANGO_STYLE_NORMAL);
  gtk_style_property_register            ("font-variant",
                                          PANGO_TYPE_VARIANT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          PANGO_VARIANT_NORMAL);
  /* xxx: need to parse this properly, ie parse the numbers */
  gtk_style_property_register            ("font-weight",
                                          PANGO_TYPE_WEIGHT,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          PANGO_WEIGHT_NORMAL);

  gtk_style_property_register            ("text-shadow",
                                          GTK_TYPE_SHADOW,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_style_property_register            ("icon-shadow",
                                          GTK_TYPE_SHADOW,
                                          GTK_STYLE_PROPERTY_INHERIT,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_style_property_register            ("box-shadow",
                                          GTK_TYPE_SHADOW,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  gtk_style_property_register            ("margin-top",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("margin-left",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("margin-bottom",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("margin-right",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("padding-top",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("padding-left",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("padding-bottom",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);
  gtk_style_property_register            ("padding-right",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);
  /* IMPORTANT: compute_border_width() requires that the border-width
   * properties be immeditaly followed by the border-style properties
   */
  gtk_style_property_register            ("border-top-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_style_property_register            ("border-top-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          compute_border_width,
                                          0);
  gtk_style_property_register            ("border-left-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_style_property_register            ("border-left-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          compute_border_width,
                                          0);
  gtk_style_property_register            ("border-bottom-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_style_property_register            ("border-bottom-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          compute_border_width,
                                          0);
  gtk_style_property_register            ("border-right-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_style_property_register            ("border-right-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          compute_border_width,
                                          0);

  gtk_style_property_register            ("border-top-left-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          &no_corner_radius);
  gtk_style_property_register            ("border-top-right-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          &no_corner_radius);
  gtk_style_property_register            ("border-bottom-right-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          &no_corner_radius);
  gtk_style_property_register            ("border-bottom-left-radius",
                                          GTK_TYPE_CSS_BORDER_CORNER_RADIUS,
                                          0,
                                          border_corner_radius_value_parse,
                                          border_corner_radius_value_print,
                                          NULL,
                                          &no_corner_radius);

  gtk_style_property_register            ("outline-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_BORDER_STYLE_NONE);
  gtk_style_property_register            ("outline-width",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          compute_border_width,
                                          0);
  gtk_style_property_register            ("outline-offset",
                                          G_TYPE_INT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          0);

  gtk_style_property_register            ("background-clip",
                                          GTK_TYPE_CSS_AREA,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_CSS_AREA_BORDER_BOX);
                                        
  gtk_style_property_register            ("background-origin",
                                          GTK_TYPE_CSS_AREA,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          GTK_CSS_AREA_PADDING_BOX);

  g_value_init (&value, GTK_TYPE_CSS_SPECIAL_VALUE);
  g_value_set_enum (&value, GTK_CSS_CURRENT_COLOR);
  _gtk_style_property_register           ("border-top-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          &value);
  _gtk_style_property_register           ("border-right-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          &value);
  _gtk_style_property_register           ("border-bottom-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          &value);
  _gtk_style_property_register           ("border-left-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          &value);
  _gtk_style_property_register           ("outline-color",
                                          GDK_TYPE_RGBA,
                                          0,
                                          NULL,
                                          NULL,
                                          color_compute,
                                          &value);
  g_value_unset (&value);

  gtk_style_property_register            ("background-repeat",
                                          GTK_TYPE_CSS_BACKGROUND_REPEAT,
                                          0,
                                          background_repeat_value_parse,
                                          background_repeat_value_print,
                                          NULL,
                                          GTK_CSS_BACKGROUND_REPEAT | (GTK_CSS_BACKGROUND_REPEAT << GTK_CSS_BACKGROUND_REPEAT_SHIFT));
  g_value_init (&value, GTK_TYPE_CSS_IMAGE);
  _gtk_style_property_register           ("background-image",
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          0,
                                          css_image_value_parse,
                                          css_image_value_print,
                                          css_image_value_compute,
                                          &value);

  _gtk_style_property_register           ("border-image-source",
                                          CAIRO_GOBJECT_TYPE_PATTERN,
                                          0,
                                          css_image_value_parse,
                                          css_image_value_print,
                                          css_image_value_compute,
                                          &value);
  g_value_unset (&value);
  gtk_style_property_register            ("border-image-repeat",
                                          GTK_TYPE_CSS_BORDER_IMAGE_REPEAT,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &border_image_repeat);

  /* XXX: The initial vaue is wrong, it should be 100% */
  gtk_style_property_register            ("border-image-slice",
                                          GTK_TYPE_BORDER,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          &border_of_ones);
  gtk_style_property_register            ("border-image-width",
                                          GTK_TYPE_BORDER,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);
  gtk_style_property_register            ("engine",
                                          GTK_TYPE_THEMING_ENGINE,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          gtk_theming_engine_load (NULL));
  gtk_style_property_register            ("transition",
                                          GTK_TYPE_ANIMATION_DESCRIPTION,
                                          0,
                                          NULL,
                                          NULL,
                                          NULL,
                                          NULL);

  /* Private property holding the binding sets */
  gtk_style_property_register            ("gtk-key-bindings",
                                          G_TYPE_PTR_ARRAY,
                                          0,
                                          bindings_value_parse,
                                          bindings_value_print,
                                          NULL,
                                          NULL);
}

