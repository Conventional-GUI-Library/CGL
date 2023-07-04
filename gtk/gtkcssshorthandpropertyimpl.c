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

#include "gtkcssshorthandpropertyprivate.h"

#include <cairo-gobject.h>
#include <math.h>

#include "gtkcssimageprivate.h"
#include "gtkcssstylefuncsprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtktypebuiltins.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

/*** PARSING ***/

static gboolean
value_is_done_parsing (GtkCssParser *parser)
{
  return _gtk_css_parser_is_eof (parser) ||
         _gtk_css_parser_begins_with (parser, ';') ||
         _gtk_css_parser_begins_with (parser, '}');
}

static gboolean
parse_border_width (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GFile                   *base)
{
  GValue temp = G_VALUE_INIT;
  GtkBorder *border;

  g_value_init (&temp, GTK_TYPE_BORDER);
  if (!_gtk_css_style_parse_value (&temp, parser, base))
    {
      g_value_unset (&temp);
      return FALSE;
    }

  border = g_value_get_boxed (&temp);

  g_value_init (&values[0], G_TYPE_INT);
  g_value_init (&values[1], G_TYPE_INT);
  g_value_init (&values[2], G_TYPE_INT);
  g_value_init (&values[3], G_TYPE_INT);
  g_value_set_int (&values[0], border->top);
  g_value_set_int (&values[1], border->right);
  g_value_set_int (&values[2], border->bottom);
  g_value_set_int (&values[3], border->left);

  g_value_unset (&temp);

  return TRUE;
}

static gboolean 
parse_border_radius (GtkCssShorthandProperty *shorthand,
                     GValue                  *values,
                     GtkCssParser            *parser,
                     GFile                   *base)
{
  GtkCssBorderCornerRadius borders[4];
  guint i;

  for (i = 0; i < G_N_ELEMENTS (borders); i++)
    {
      if (!_gtk_css_parser_try_double (parser, &borders[i].horizontal))
        break;
      if (borders[i].horizontal < 0)
        {
          _gtk_css_parser_error (parser, "Border radius values cannot be negative");
          return FALSE;
        }
    }

  if (i == 0)
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }

  /* The magic (i - 1) >> 1 below makes it take the correct value
   * according to spec. Feel free to check the 4 cases */
  for (; i < G_N_ELEMENTS (borders); i++)
    borders[i].horizontal = borders[(i - 1) >> 1].horizontal;

  if (_gtk_css_parser_try (parser, "/", TRUE))
    {
      for (i = 0; i < G_N_ELEMENTS (borders); i++)
        {
          if (!_gtk_css_parser_try_double (parser, &borders[i].vertical))
            break;
          if (borders[i].vertical < 0)
            {
              _gtk_css_parser_error (parser, "Border radius values cannot be negative");
              return FALSE;
            }
        }

      if (i == 0)
        {
          _gtk_css_parser_error (parser, "Expected a number");
          return FALSE;
        }

      for (; i < G_N_ELEMENTS (borders); i++)
        borders[i].vertical = borders[(i - 1) >> 1].vertical;

    }
  else
    {
      for (i = 0; i < G_N_ELEMENTS (borders); i++)
        borders[i].vertical = borders[i].horizontal;
    }

  for (i = 0; i < G_N_ELEMENTS (borders); i++)
    {
      g_value_init (&values[i], GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
      g_value_set_boxed (&values[i], &borders[i]);
    }

  return TRUE;
}

static gboolean 
parse_border_color (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GFile                   *base)
{
  GtkSymbolicColor *symbolic;
  guint i;

  for (i = 0; i < 4; i++)
    {
      symbolic = _gtk_css_parser_read_symbolic_color (parser);
      if (symbolic == NULL)
        return FALSE;

      g_value_init (&values[i], GTK_TYPE_SYMBOLIC_COLOR);
      g_value_set_boxed (&values[i], symbolic);

      if (value_is_done_parsing (parser))
        break;
    }

  for (i++; i < 4; i++)
    {
      g_value_init (&values[i], GTK_TYPE_SYMBOLIC_COLOR);
      g_value_copy (&values[(i - 1) >> 1], &values[i]);
    }

  return TRUE;
}

static gboolean
parse_border_style (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GFile                   *base)
{
  GtkBorderStyle styles[4];
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (!_gtk_css_parser_try_enum (parser, GTK_TYPE_BORDER_STYLE, (int *)&styles[i]))
        break;
    }

  if (i == 0)
    {
      _gtk_css_parser_error (parser, "Expected a border style");
      return FALSE;
    }

  for (; i < G_N_ELEMENTS (styles); i++)
    styles[i] = styles[(i - 1) >> 1];

  for (i = 0; i < G_N_ELEMENTS (styles); i++)
    {
      g_value_init (&values[i], GTK_TYPE_BORDER_STYLE);
      g_value_set_enum (&values[i], styles[i]);
    }

  return TRUE;
}

static gboolean
parse_border_image (GtkCssShorthandProperty *shorthand,
                    GValue                  *values,
                    GtkCssParser            *parser,
                    GFile                   *base)
{
  GtkCssImage *image;
  
  if (_gtk_css_parser_try (parser, "none", TRUE))
    image = NULL;
  else
    {
      image = _gtk_css_image_new_parse (parser, base);
      if (!image)
        return FALSE;
    }
  g_value_init (&values[0], GTK_TYPE_CSS_IMAGE);
  g_value_set_object (&values[0], image);

  if (value_is_done_parsing (parser))
    return TRUE;

  g_value_init (&values[1], GTK_TYPE_BORDER);
  if (!_gtk_css_style_parse_value (&values[1], parser, base))
    return FALSE;

  if (_gtk_css_parser_try (parser, "/", TRUE))
    {
      g_value_init (&values[2], GTK_TYPE_BORDER);
      if (!_gtk_css_style_parse_value (&values[2], parser, base))
        return FALSE;
    }

  if (value_is_done_parsing (parser))
    return TRUE;

  g_value_init (&values[3], GTK_TYPE_CSS_BORDER_IMAGE_REPEAT);
  if (!_gtk_css_style_parse_value (&values[3], parser, base))
    return FALSE;

  return TRUE;
}

static gboolean
parse_border_side (GtkCssShorthandProperty *shorthand,
                   GValue                  *values,
                   GtkCssParser            *parser,
                   GFile                   *base)
{
  int width;
  int style;

  do
  {
    if (!G_IS_VALUE (&values[0]) &&
         _gtk_css_parser_try_length (parser, &width))
      {
        g_value_init (&values[0], G_TYPE_INT);
        g_value_set_int (&values[0], width);
      }
    else if (!G_IS_VALUE (&values[1]) &&
             _gtk_css_parser_try_enum (parser, GTK_TYPE_BORDER_STYLE, &style))
      {
        g_value_init (&values[1], GTK_TYPE_BORDER_STYLE);
        g_value_set_enum (&values[1], style);
      }
    else if (!G_IS_VALUE (&values[2]))
      {
        GtkSymbolicColor *symbolic;

        symbolic = _gtk_css_parser_read_symbolic_color (parser);
        if (symbolic == NULL)
          return FALSE;

        g_value_init (&values[2], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_take_boxed (&values[2], symbolic);
      }
    else
      {
        /* We parsed everything and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error */
        break;
      }
  }
  while (!value_is_done_parsing (parser));

  return TRUE;
}

static gboolean
parse_border (GtkCssShorthandProperty *shorthand,
              GValue                  *values,
              GtkCssParser            *parser,
              GFile                   *base)
{
  int width;
  int style;

  do
  {
    if (!G_IS_VALUE (&values[0]) &&
         _gtk_css_parser_try_length (parser, &width))
      {
        g_value_init (&values[0], G_TYPE_INT);
        g_value_init (&values[1], G_TYPE_INT);
        g_value_init (&values[2], G_TYPE_INT);
        g_value_init (&values[3], G_TYPE_INT);
        g_value_set_int (&values[0], width);
        g_value_set_int (&values[1], width);
        g_value_set_int (&values[2], width);
        g_value_set_int (&values[3], width);
      }
    else if (!G_IS_VALUE (&values[4]) &&
             _gtk_css_parser_try_enum (parser, GTK_TYPE_BORDER_STYLE, &style))
      {
        g_value_init (&values[4], GTK_TYPE_BORDER_STYLE);
        g_value_init (&values[5], GTK_TYPE_BORDER_STYLE);
        g_value_init (&values[6], GTK_TYPE_BORDER_STYLE);
        g_value_init (&values[7], GTK_TYPE_BORDER_STYLE);
        g_value_set_enum (&values[4], style);
        g_value_set_enum (&values[5], style);
        g_value_set_enum (&values[6], style);
        g_value_set_enum (&values[7], style);
      }
    else if (!G_IS_VALUE (&values[8]))
      {
        GtkSymbolicColor *symbolic;

        symbolic = _gtk_css_parser_read_symbolic_color (parser);
        if (symbolic == NULL)
          return FALSE;

        g_value_init (&values[8], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_init (&values[9], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_init (&values[10], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_init (&values[11], GTK_TYPE_SYMBOLIC_COLOR);
        g_value_set_boxed (&values[8], symbolic);
        g_value_set_boxed (&values[9], symbolic);
        g_value_set_boxed (&values[10], symbolic);
        g_value_take_boxed (&values[11], symbolic);
      }
    else
      {
        /* We parsed everything and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error */
        break;
      }
  }
  while (!value_is_done_parsing (parser));

  /* Note that border-image values are not set: according to the spec
     they just need to be reset when using the border shorthand */

  return TRUE;
}

static gboolean
parse_font (GtkCssShorthandProperty *shorthand,
            GValue                  *values,
            GtkCssParser            *parser,
            GFile                   *base)
{
  PangoFontDescription *desc;
  guint mask;
  char *str;

  str = _gtk_css_parser_read_value (parser);
  if (str == NULL)
    return FALSE;

  desc = pango_font_description_from_string (str);
  g_free (str);

  mask = pango_font_description_get_set_fields (desc);

  if (mask & PANGO_FONT_MASK_FAMILY)
    {
      GPtrArray *strv = g_ptr_array_new ();

      g_ptr_array_add (strv, g_strdup (pango_font_description_get_family (desc)));
      g_ptr_array_add (strv, NULL);
      g_value_init (&values[0], G_TYPE_STRV);
      g_value_take_boxed (&values[0], g_ptr_array_free (strv, FALSE));
    }
  if (mask & PANGO_FONT_MASK_STYLE)
    {
      g_value_init (&values[1], PANGO_TYPE_STYLE);
      g_value_set_enum (&values[1], pango_font_description_get_style (desc));
    }
  if (mask & PANGO_FONT_MASK_VARIANT)
    {
      g_value_init (&values[2], PANGO_TYPE_VARIANT);
      g_value_set_enum (&values[2], pango_font_description_get_variant (desc));
    }
  if (mask & PANGO_FONT_MASK_WEIGHT)
    {
      g_value_init (&values[3], PANGO_TYPE_WEIGHT);
      g_value_set_enum (&values[3], pango_font_description_get_weight (desc));
    }
  if (mask & PANGO_FONT_MASK_SIZE)
    {
      g_value_init (&values[4], G_TYPE_DOUBLE);
      g_value_set_double (&values[4],
                          (double) pango_font_description_get_size (desc) / PANGO_SCALE);
    }

  pango_font_description_free (desc);

  return TRUE;
}

static gboolean
parse_background (GtkCssShorthandProperty *shorthand,
                  GValue                  *values,
                  GtkCssParser            *parser,
                  GFile                   *base)
{
  int enum_value;

  do
    {
      /* the image part */
      if (!G_IS_VALUE (&values[0]) &&
          (_gtk_css_parser_has_prefix (parser, "none") ||
           _gtk_css_image_can_parse (parser)))
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

          g_value_init (&values[0], GTK_TYPE_CSS_IMAGE);
          g_value_take_object (&values[0], image);
        }
      else if (!G_IS_VALUE (&values[1]) &&
               _gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BACKGROUND_REPEAT, &enum_value))
        {
          if (enum_value <= GTK_CSS_BACKGROUND_REPEAT_MASK)
            {
              int vertical;

              if (_gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BACKGROUND_REPEAT, &vertical))
                {
                  if (vertical >= GTK_CSS_BACKGROUND_REPEAT_MASK)
                    {
                      _gtk_css_parser_error (parser, "Not a valid 2nd value for border-repeat");
                      return FALSE;
                    }
                  else
                    enum_value |= vertical << GTK_CSS_BACKGROUND_REPEAT_SHIFT;
                }
              else
                enum_value |= enum_value << GTK_CSS_BACKGROUND_REPEAT_SHIFT;
            }

          g_value_init (&values[1], GTK_TYPE_CSS_BACKGROUND_REPEAT);
          g_value_set_enum (&values[1], enum_value);
        }
      else if ((!G_IS_VALUE (&values[2]) || !G_IS_VALUE (&values[3])) &&
               _gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_AREA, &enum_value))
        {
          guint idx = !G_IS_VALUE (&values[2]) ? 2 : 3;
          g_value_init (&values[idx], GTK_TYPE_CSS_AREA);
          g_value_set_enum (&values[idx], enum_value);
        }
      else if (!G_IS_VALUE (&values[4]))
        {
          GtkSymbolicColor *symbolic;
          
          symbolic = _gtk_css_parser_read_symbolic_color (parser);
          if (symbolic == NULL)
            return FALSE;

          g_value_init (&values[4], GTK_TYPE_SYMBOLIC_COLOR);
          g_value_take_boxed (&values[4], symbolic);
        }
      else
        {
          /* We parsed everything and there's still stuff left?
           * Pretend we didn't notice and let the normal code produce
           * a 'junk at end of value' error */
          break;
        }
    }
  while (!value_is_done_parsing (parser));

  return TRUE;
}

/*** PACKING ***/

static GParameter *
unpack_border (const GValue *value,
               guint        *n_params,
               const char   *top,
               const char   *left,
               const char   *bottom,
               const char   *right)
{
  GParameter *parameter = g_new0 (GParameter, 4);
  GtkBorder *border = g_value_get_boxed (value);

  parameter[0].name = top;
  g_value_init (&parameter[0].value, G_TYPE_INT);
  g_value_set_int (&parameter[0].value, border->top);
  parameter[1].name = left;
  g_value_init (&parameter[1].value, G_TYPE_INT);
  g_value_set_int (&parameter[1].value, border->left);
  parameter[2].name = bottom;
  g_value_init (&parameter[2].value, G_TYPE_INT);
  g_value_set_int (&parameter[2].value, border->bottom);
  parameter[3].name = right;
  g_value_init (&parameter[3].value, G_TYPE_INT);
  g_value_set_int (&parameter[3].value, border->right);

  *n_params = 4;
  return parameter;
}

static void
pack_border (GValue             *value,
             GtkStyleProperties *props,
             GtkStateFlags       state,
             const char         *top,
             const char         *left,
             const char         *bottom,
             const char         *right)
{
  GtkBorder border;
  int t, l, b, r;

  gtk_style_properties_get (props,
                            state,
                            top, &t,
                            left, &l,
                            bottom, &b,
                            right, &r,
                            NULL);

  border.top = t;
  border.left = l;
  border.bottom = b;
  border.right = r;

  g_value_set_boxed (value, &border);
}

static GParameter *
unpack_border_width (const GValue *value,
                     guint        *n_params)
{
  return unpack_border (value, n_params,
                        "border-top-width", "border-left-width",
                        "border-bottom-width", "border-right-width");
}

static void
pack_border_width (GValue             *value,
                   GtkStyleProperties *props,
                   GtkStateFlags       state)
{
  pack_border (value, props, state,
               "border-top-width", "border-left-width",
               "border-bottom-width", "border-right-width");
}

static GParameter *
unpack_padding (const GValue *value,
                guint        *n_params)
{
  return unpack_border (value, n_params,
                        "padding-top", "padding-left",
                        "padding-bottom", "padding-right");
}

static void
pack_padding (GValue             *value,
              GtkStyleProperties *props,
              GtkStateFlags       state)
{
  pack_border (value, props, state,
               "padding-top", "padding-left",
               "padding-bottom", "padding-right");
}

static GParameter *
unpack_margin (const GValue *value,
               guint        *n_params)
{
  return unpack_border (value, n_params,
                        "margin-top", "margin-left",
                        "margin-bottom", "margin-right");
}

static void
pack_margin (GValue             *value,
             GtkStyleProperties *props,
             GtkStateFlags       state)
{
  pack_border (value, props, state,
               "margin-top", "margin-left",
               "margin-bottom", "margin-right");
}

static GParameter *
unpack_border_radius (const GValue *value,
                      guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 4);
  GtkCssBorderCornerRadius border;
  
  border.horizontal = border.vertical = g_value_get_int (value);

  parameter[0].name = "border-top-left-radius";
  g_value_init (&parameter[0].value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  g_value_set_boxed (&parameter[0].value, &border);
  parameter[1].name = "border-top-right-radius";
  g_value_init (&parameter[1].value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  g_value_set_boxed (&parameter[1].value, &border);
  parameter[2].name = "border-bottom-right-radius";
  g_value_init (&parameter[2].value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  g_value_set_boxed (&parameter[2].value, &border);
  parameter[3].name = "border-bottom-left-radius";
  g_value_init (&parameter[3].value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  g_value_set_boxed (&parameter[3].value, &border);

  *n_params = 4;
  return parameter;
}

static void
pack_border_radius (GValue             *value,
                    GtkStyleProperties *props,
                    GtkStateFlags       state)
{
  GtkCssBorderCornerRadius *top_left;

  /* NB: We are an int property, so we have to resolve to an int here.
   * So we just resolve to an int. We pick one and stick to it.
   * Lesson learned: Don't query border-radius shorthand, query the 
   * real properties instead. */
  gtk_style_properties_get (props,
                            state,
                            "border-top-left-radius", &top_left,
                            NULL);

  if (top_left)
    g_value_set_int (value, top_left->horizontal);

  g_free (top_left);
}

static GParameter *
unpack_font_description (const GValue *value,
                         guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 5);
  PangoFontDescription *description;
  PangoFontMask mask;
  guint n;
  
  /* For backwards compat, we only unpack values that are indeed set.
   * For strict CSS conformance we need to unpack all of them.
   * Note that we do set all of them in the parse function, so it
   * will not have effects when parsing CSS files. It will though
   * for custom style providers.
   */

  description = g_value_get_boxed (value);
  n = 0;

  if (description)
    mask = pango_font_description_get_set_fields (description);
  else
    mask = 0;

  if (mask & PANGO_FONT_MASK_FAMILY)
    {
      GPtrArray *strv = g_ptr_array_new ();

      g_ptr_array_add (strv, g_strdup (pango_font_description_get_family (description)));
      g_ptr_array_add (strv, NULL);
      parameter[n].name = "font-family";
      g_value_init (&parameter[n].value, G_TYPE_STRV);
      g_value_take_boxed (&parameter[n].value,
                          g_ptr_array_free (strv, FALSE));
      n++;
    }

  if (mask & PANGO_FONT_MASK_STYLE)
    {
      parameter[n].name = "font-style";
      g_value_init (&parameter[n].value, PANGO_TYPE_STYLE);
      g_value_set_enum (&parameter[n].value,
                        pango_font_description_get_style (description));
      n++;
    }

  if (mask & PANGO_FONT_MASK_VARIANT)
    {
      parameter[n].name = "font-variant";
      g_value_init (&parameter[n].value, PANGO_TYPE_VARIANT);
      g_value_set_enum (&parameter[n].value,
                        pango_font_description_get_variant (description));
      n++;
    }

  if (mask & PANGO_FONT_MASK_WEIGHT)
    {
      parameter[n].name = "font-weight";
      g_value_init (&parameter[n].value, PANGO_TYPE_WEIGHT);
      g_value_set_enum (&parameter[n].value,
                        pango_font_description_get_weight (description));
      n++;
    }

  if (mask & PANGO_FONT_MASK_SIZE)
    {
      parameter[n].name = "font-size";
      g_value_init (&parameter[n].value, G_TYPE_DOUBLE);
      g_value_set_double (&parameter[n].value,
                          (double) pango_font_description_get_size (description) / PANGO_SCALE);
      n++;
    }

  *n_params = n;

  return parameter;
}

static void
pack_font_description (GValue             *value,
                       GtkStyleProperties *props,
                       GtkStateFlags       state)
{
  PangoFontDescription *description;
  char **families;
  PangoStyle style;
  PangoVariant variant;
  PangoWeight weight;
  double size;

  gtk_style_properties_get (props,
                            state,
                            "font-family", &families,
                            "font-style", &style,
                            "font-variant", &variant,
                            "font-weight", &weight,
                            "font-size", &size,
                            NULL);

  description = pango_font_description_new ();
  /* xxx: Can we set all the families here somehow? */
  if (families)
    pango_font_description_set_family (description, families[0]);
  pango_font_description_set_size (description, round (size * PANGO_SCALE));
  pango_font_description_set_style (description, style);
  pango_font_description_set_variant (description, variant);
  pango_font_description_set_weight (description, weight);

  g_strfreev (families);

  g_value_take_boxed (value, description);
}

static GParameter *
unpack_border_color (const GValue *value,
                     guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 4);
  GType type;
  
  type = G_VALUE_TYPE (value);
  if (type == G_TYPE_PTR_ARRAY)
    type = GTK_TYPE_SYMBOLIC_COLOR;

  parameter[0].name = "border-top-color";
  g_value_init (&parameter[0].value, type);
  parameter[1].name = "border-right-color";
  g_value_init (&parameter[1].value, type);
  parameter[2].name = "border-bottom-color";
  g_value_init (&parameter[2].value, type);
  parameter[3].name = "border-left-color";
  g_value_init (&parameter[3].value, type);

  if (G_VALUE_TYPE (value) == G_TYPE_PTR_ARRAY)
    {
      GPtrArray *array = g_value_get_boxed (value);
      guint i;

      for (i = 0; i < 4; i++)
        g_value_set_boxed (&parameter[i].value, g_ptr_array_index (array, i));
    }
  else
    {
      /* can be RGBA or symbolic color */
      gpointer p = g_value_get_boxed (value);

      g_value_set_boxed (&parameter[0].value, p);
      g_value_set_boxed (&parameter[1].value, p);
      g_value_set_boxed (&parameter[2].value, p);
      g_value_set_boxed (&parameter[3].value, p);
    }

  *n_params = 4;
  return parameter;
}

static void
pack_border_color (GValue             *value,
                   GtkStyleProperties *props,
                   GtkStateFlags       state)
{
  /* NB: We are a color property, so we have to resolve to a color here.
   * So we just resolve to a color. We pick one and stick to it.
   * Lesson learned: Don't query border-color shorthand, query the 
   * real properties instead. */
  g_value_unset (value);
  gtk_style_properties_get_property (props, "border-top-color", state, value);
}

static GParameter *
unpack_border_style (const GValue *value,
                     guint        *n_params)
{
  GParameter *parameter = g_new0 (GParameter, 4);
  GtkBorderStyle style;

  style = g_value_get_enum (value);

  parameter[0].name = "border-top-style";
  g_value_init (&parameter[0].value, GTK_TYPE_BORDER_STYLE);
  g_value_set_enum (&parameter[0].value, style);
  parameter[1].name = "border-right-style";
  g_value_init (&parameter[1].value, GTK_TYPE_BORDER_STYLE);
  g_value_set_enum (&parameter[1].value, style);
  parameter[2].name = "border-bottom-style";
  g_value_init (&parameter[2].value, GTK_TYPE_BORDER_STYLE);
  g_value_set_enum (&parameter[2].value, style);
  parameter[3].name = "border-left-style";
  g_value_init (&parameter[3].value, GTK_TYPE_BORDER_STYLE);
  g_value_set_enum (&parameter[3].value, style);

  *n_params = 4;
  return parameter;
}

static void
pack_border_style (GValue             *value,
                   GtkStyleProperties *props,
                   GtkStateFlags       state)
{
  /* NB: We can just resolve to a style. We pick one and stick to it.
   * Lesson learned: Don't query border-style shorthand, query the
   * real properties instead. */
  g_value_unset (value);
  gtk_style_properties_get_property (props, "border-top-style", state, value);
}

static void
_gtk_css_shorthand_property_register (const char                        *name,
                                      GType                              value_type,
                                      const char                       **subproperties,
                                      GtkCssShorthandPropertyParseFunc   parse_func,
                                      GtkStyleUnpackFunc                 unpack_func,
                                      GtkStylePackFunc                   pack_func)
{
  GtkStyleProperty *node;

  node = g_object_new (GTK_TYPE_CSS_SHORTHAND_PROPERTY,
                       "name", name,
                       "value-type", value_type,
                       "subproperties", subproperties,
                       NULL);

  GTK_CSS_SHORTHAND_PROPERTY (node)->parse = parse_func;
  node->pack_func = pack_func;
  node->unpack_func = unpack_func;
}

void
_gtk_css_shorthand_property_init_properties (void)
{
  /* The order is important here, be careful when changing it */
  const char *font_subproperties[] = { "font-family", "font-style", "font-variant", "font-weight", "font-size", NULL };
  const char *margin_subproperties[] = { "margin-top", "margin-right", "margin-bottom", "margin-left", NULL };
  const char *padding_subproperties[] = { "padding-top", "padding-right", "padding-bottom", "padding-left", NULL };
  const char *border_width_subproperties[] = { "border-top-width", "border-right-width", "border-bottom-width", "border-left-width", NULL };
  const char *border_radius_subproperties[] = { "border-top-left-radius", "border-top-right-radius",
                                                "border-bottom-right-radius", "border-bottom-left-radius", NULL };
  const char *border_color_subproperties[] = { "border-top-color", "border-right-color", "border-bottom-color", "border-left-color", NULL };
  const char *border_style_subproperties[] = { "border-top-style", "border-right-style", "border-bottom-style", "border-left-style", NULL };
  const char *border_image_subproperties[] = { "border-image-source", "border-image-slice", "border-image-width", "border-image-repeat", NULL };
  const char *border_top_subproperties[] = { "border-top-width", "border-top-style", "border-top-color", NULL };
  const char *border_right_subproperties[] = { "border-right-width", "border-right-style", "border-right-color", NULL };
  const char *border_bottom_subproperties[] = { "border-bottom-width", "border-bottom-style", "border-bottom-color", NULL };
  const char *border_left_subproperties[] = { "border-left-width", "border-left-style", "border-left-color", NULL };
  const char *border_subproperties[] = { "border-top-width", "border-right-width", "border-bottom-width", "border-left-width",
                                         "border-top-style", "border-right-style", "border-bottom-style", "border-left-style",
                                         "border-top-color", "border-right-color", "border-bottom-color", "border-left-color",
                                         "border-image-source", "border-image-slice", "border-image-width", "border-image-repeat", NULL };
  const char *outline_subproperties[] = { "outline-width", "outline-style", "outline-color", NULL };
  const char *background_subproperties[] = { "background-image", "background-repeat", "background-clip", "background-origin",
                                             "background-color", NULL };

  _gtk_css_shorthand_property_register   ("font",
                                          PANGO_TYPE_FONT_DESCRIPTION,
                                          font_subproperties,
                                          parse_font,
                                          unpack_font_description,
                                          pack_font_description);
  _gtk_css_shorthand_property_register   ("margin",
                                          GTK_TYPE_BORDER,
                                          margin_subproperties,
                                          parse_border_width,
                                          unpack_margin,
                                          pack_margin);
  _gtk_css_shorthand_property_register   ("padding",
                                          GTK_TYPE_BORDER,
                                          padding_subproperties,
                                          parse_border_width,
                                          unpack_padding,
                                          pack_padding);
  _gtk_css_shorthand_property_register   ("border-width",
                                          GTK_TYPE_BORDER,
                                          border_width_subproperties,
                                          parse_border_width,
                                          unpack_border_width,
                                          pack_border_width);
  _gtk_css_shorthand_property_register   ("border-radius",
                                          G_TYPE_INT,
                                          border_radius_subproperties,
                                          parse_border_radius,
                                          unpack_border_radius,
                                          pack_border_radius);
  _gtk_css_shorthand_property_register   ("border-color",
                                          GDK_TYPE_RGBA,
                                          border_color_subproperties,
                                          parse_border_color,
                                          unpack_border_color,
                                          pack_border_color);
  _gtk_css_shorthand_property_register   ("border-style",
                                          GTK_TYPE_BORDER_STYLE,
                                          border_style_subproperties,
                                          parse_border_style,
                                          unpack_border_style,
                                          pack_border_style);
  _gtk_css_shorthand_property_register   ("border-image",
                                          G_TYPE_NONE,
                                          border_image_subproperties,
                                          parse_border_image,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-top",
                                          G_TYPE_NONE,
                                          border_top_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-right",
                                          G_TYPE_NONE,
                                          border_right_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-bottom",
                                          G_TYPE_NONE,
                                          border_bottom_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border-left",
                                          G_TYPE_NONE,
                                          border_left_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("border",
                                          G_TYPE_NONE,
                                          border_subproperties,
                                          parse_border,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("outline",
                                          G_TYPE_NONE,
                                          outline_subproperties,
                                          parse_border_side,
                                          NULL,
                                          NULL);
  _gtk_css_shorthand_property_register   ("background",
                                          G_TYPE_NONE,
                                          background_subproperties,
                                          parse_background,
                                          NULL,
                                          NULL);
}
