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

#include "gtkcssstylefuncsprivate.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo-gobject.h>

#include "gtkanimationdescription.h"
#include "gtkcssimagegradientprivate.h"
#include "gtkcssprovider.h"
#include "gtkcsstypesprivate.h"
#include "gtkgradient.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkshadowprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkthemingengine.h"
#include "gtktypebuiltins.h"
#include "gtkwin32themeprivate.h"

/* this is in case round() is not provided by the compiler, 
 * such as in the case of C89 compilers, like MSVC
 */
#include "fallback-c89.c"

static GHashTable *parse_funcs = NULL;
static GHashTable *print_funcs = NULL;
static GHashTable *compute_funcs = NULL;

typedef gboolean         (* GtkStyleParseFunc)             (GtkCssParser           *parser,
                                                            GFile                  *base,
                                                            GValue                 *value);
typedef void             (* GtkStylePrintFunc)             (const GValue           *value,
                                                            GString                *string);
typedef void             (* GtkStylePrintFunc)             (const GValue           *value,
                                                            GString                *string);
typedef void             (* GtkStyleComputeFunc)           (GValue                 *computed,
                                                            GtkStyleContext        *context,
                                                            const GValue           *specified);

static void
register_conversion_function (GType               type,
                              GtkStyleParseFunc   parse,
                              GtkStylePrintFunc   print,
                              GtkStyleComputeFunc compute)
{
  if (parse)
    g_hash_table_insert (parse_funcs, GSIZE_TO_POINTER (type), parse);
  if (print)
    g_hash_table_insert (print_funcs, GSIZE_TO_POINTER (type), print);
  if (compute)
    g_hash_table_insert (compute_funcs, GSIZE_TO_POINTER (type), compute);
}

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
enum_parse (GtkCssParser *parser,
	    GType         type,
	    int          *res)
{
  char *str;

  if (_gtk_css_parser_try_enum (parser, type, res))
    return TRUE;

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected an identifier");
      return FALSE;
    }

  _gtk_css_parser_error (parser,
			 "Unknown value '%s' for enum type '%s'",
			 str, g_type_name (type));
  g_free (str);

  return FALSE;
}

static void
enum_print (int         value,
	    GType       type,
	    GString    *string)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;

  enum_class = g_type_class_ref (type);
  enum_value = g_enum_get_value (enum_class, value);

  g_string_append (string, enum_value->value_nick);

  g_type_class_unref (enum_class);
}

static gboolean
rgba_value_parse (GtkCssParser *parser,
                  GFile        *base,
                  GValue       *value)
{
  GtkSymbolicColor *symbolic;
  GdkRGBA rgba;

  if (_gtk_css_parser_try (parser, "currentcolor", TRUE))
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_CSS_SPECIAL_VALUE);
      g_value_set_enum (value, GTK_CSS_CURRENT_COLOR);
      return TRUE;
    }

  symbolic = _gtk_css_parser_read_symbolic_color (parser);
  if (symbolic == NULL)
    return FALSE;

  if (gtk_symbolic_color_resolve (symbolic, NULL, &rgba))
    {
      g_value_set_boxed (value, &rgba);
      gtk_symbolic_color_unref (symbolic);
    }
  else
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
      g_value_take_boxed (value, symbolic);
    }

  return TRUE;
}

static void
rgba_value_print (const GValue *value,
                  GString      *string)
{
  const GdkRGBA *rgba = g_value_get_boxed (value);

  if (rgba == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gdk_rgba_to_string (rgba);
      g_string_append (string, s);
      g_free (s);
    }
}

static void
rgba_value_compute (GValue          *computed,
                    GtkStyleContext *context,
                    const GValue    *specified)
{
  GdkRGBA rgba, white = { 1, 1, 1, 1 };

  if (G_VALUE_HOLDS (specified, GTK_TYPE_CSS_SPECIAL_VALUE))
    {
      g_assert (g_value_get_enum (specified) == GTK_CSS_CURRENT_COLOR);
      g_value_copy (_gtk_style_context_peek_property (context, "color"), computed);
    }
  else if (G_VALUE_HOLDS (specified, GTK_TYPE_SYMBOLIC_COLOR))
    {
      if (_gtk_style_context_resolve_color (context,
                                            g_value_get_boxed (specified),
                                            &rgba))
        g_value_set_boxed (computed, &rgba);
      else
        g_value_set_boxed (computed, &white);
    }
  else
    g_value_copy (specified, computed);
}

static gboolean 
color_value_parse (GtkCssParser *parser,
                   GFile        *base,
                   GValue       *value)
{
  GtkSymbolicColor *symbolic;
  GdkRGBA rgba;

  symbolic = _gtk_css_parser_read_symbolic_color (parser);
  if (symbolic == NULL)
    return FALSE;

  if (gtk_symbolic_color_resolve (symbolic, NULL, &rgba))
    {
      GdkColor color;

      color.red = rgba.red * 65535. + 0.5;
      color.green = rgba.green * 65535. + 0.5;
      color.blue = rgba.blue * 65535. + 0.5;

      g_value_set_boxed (value, &color);
    }
  else
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_SYMBOLIC_COLOR);
      g_value_take_boxed (value, symbolic);
    }

  return TRUE;
}

static void
color_value_print (const GValue *value,
                   GString      *string)
{
  const GdkColor *color = g_value_get_boxed (value);

  if (color == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gdk_color_to_string (color);
      g_string_append (string, s);
      g_free (s);
    }
}

static void
color_value_compute (GValue          *computed,
                     GtkStyleContext *context,
                     const GValue    *specified)
{
  GdkRGBA rgba;
  GdkColor color = { 0, 65535, 65535, 65535 };

  if (G_VALUE_HOLDS (specified, GTK_TYPE_SYMBOLIC_COLOR))
    {
      if (_gtk_style_context_resolve_color (context,
                                            g_value_get_boxed (specified),
                                            &rgba))
        {
          color.red = rgba.red * 65535. + 0.5;
          color.green = rgba.green * 65535. + 0.5;
          color.blue = rgba.blue * 65535. + 0.5;
        }
      
      g_value_set_boxed (computed, &color);
    }
  else
    g_value_copy (specified, computed);
}

static gboolean
symbolic_color_value_parse (GtkCssParser *parser,
                            GFile        *base,
                            GValue       *value)
{
  GtkSymbolicColor *symbolic;

  symbolic = _gtk_css_parser_read_symbolic_color (parser);
  if (symbolic == NULL)
    return FALSE;

  g_value_take_boxed (value, symbolic);
  return TRUE;
}

static void
symbolic_color_value_print (const GValue *value,
                            GString      *string)
{
  GtkSymbolicColor *symbolic = g_value_get_boxed (value);

  if (symbolic == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gtk_symbolic_color_to_string (symbolic);
      g_string_append (string, s);
      g_free (s);
    }
}

static gboolean 
font_description_value_parse (GtkCssParser *parser,
                              GFile        *base,
                              GValue       *value)
{
  PangoFontDescription *font_desc;
  guint mask;
  char *str;

  str = _gtk_css_parser_read_value (parser);
  if (str == NULL)
    return FALSE;

  font_desc = pango_font_description_from_string (str);
  mask = pango_font_description_get_set_fields (font_desc);
  /* These values are not really correct,
   * but the fields must be set, so we set them to something */
  if ((mask & PANGO_FONT_MASK_FAMILY) == 0)
    pango_font_description_set_family_static (font_desc, "Sans");
  if ((mask & PANGO_FONT_MASK_SIZE) == 0)
    pango_font_description_set_size (font_desc, 10 * PANGO_SCALE);
  g_free (str);
  g_value_take_boxed (value, font_desc);
  return TRUE;
}

static void
font_description_value_print (const GValue *value,
                              GString      *string)
{
  const PangoFontDescription *desc = g_value_get_boxed (value);

  if (desc == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = pango_font_description_to_string (desc);
      g_string_append (string, s);
      g_free (s);
    }
}

static gboolean 
boolean_value_parse (GtkCssParser *parser,
                     GFile        *base,
                     GValue       *value)
{
  if (_gtk_css_parser_try (parser, "true", TRUE) ||
      _gtk_css_parser_try (parser, "1", TRUE))
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (_gtk_css_parser_try (parser, "false", TRUE) ||
           _gtk_css_parser_try (parser, "0", TRUE))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  else
    {
      _gtk_css_parser_error (parser, "Expected a boolean value");
      return FALSE;
    }
}

static void
boolean_value_print (const GValue *value,
                     GString      *string)
{
  if (g_value_get_boolean (value))
    g_string_append (string, "true");
  else
    g_string_append (string, "false");
}

static gboolean 
int_value_parse (GtkCssParser *parser,
                 GFile        *base,
                 GValue       *value)
{
  gint i;

  if (_gtk_css_parser_begins_with (parser, '-'))
    {
      int res = _gtk_win32_theme_int_parse (parser, base, &i);
      if (res >= 0)
	{
	  g_value_set_int (value, i);
	  return res > 0;
	}
      /* < 0 => continue */
    }

  if (!_gtk_css_parser_try_int (parser, &i))
    {
      _gtk_css_parser_error (parser, "Expected a valid integer value");
      return FALSE;
    }

  g_value_set_int (value, i);
  return TRUE;
}

static void
int_value_print (const GValue *value,
                 GString      *string)
{
  g_string_append_printf (string, "%d", g_value_get_int (value));
}

static gboolean 
uint_value_parse (GtkCssParser *parser,
                  GFile        *base,
                  GValue       *value)
{
  guint u;

  if (!_gtk_css_parser_try_uint (parser, &u))
    {
      _gtk_css_parser_error (parser, "Expected a valid unsigned value");
      return FALSE;
    }

  g_value_set_uint (value, u);
  return TRUE;
}

static void
uint_value_print (const GValue *value,
                  GString      *string)
{
  g_string_append_printf (string, "%u", g_value_get_uint (value));
}

static gboolean 
double_value_parse (GtkCssParser *parser,
                    GFile        *base,
                    GValue       *value)
{
  gdouble d;

  if (!_gtk_css_parser_try_double (parser, &d))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }

  g_value_set_double (value, d);
  return TRUE;
}

static void
double_value_print (const GValue *value,
                    GString      *string)
{
  string_append_double (string, g_value_get_double (value));
}

static gboolean 
float_value_parse (GtkCssParser *parser,
                   GFile        *base,
                   GValue       *value)
{
  gdouble d;

  if (!_gtk_css_parser_try_double (parser, &d))
    {
      _gtk_css_parser_error (parser, "Expected a number");
      return FALSE;
    }

  g_value_set_float (value, d);
  return TRUE;
}

static void
float_value_print (const GValue *value,
                   GString      *string)
{
  string_append_double (string, g_value_get_float (value));
}

static gboolean 
string_value_parse (GtkCssParser *parser,
                    GFile        *base,
                    GValue       *value)
{
  char *str = _gtk_css_parser_read_string (parser);

  if (str == NULL)
    return FALSE;

  g_value_take_string (value, str);
  return TRUE;
}

static void
string_value_print (const GValue *value,
                    GString      *str)
{
  string_append_string (str, g_value_get_string (value));
}

static gboolean 
theming_engine_value_parse (GtkCssParser *parser,
                            GFile        *base,
                            GValue       *value)
{
  GtkThemingEngine *engine;
  char *str;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    {
      g_value_set_object (value, gtk_theming_engine_load (NULL));
      return TRUE;
    }

  str = _gtk_css_parser_try_ident (parser, TRUE);
  if (str == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a valid theme name");
      return FALSE;
    }

  engine = gtk_theming_engine_load (str);

  if (engine == NULL)
    {
      _gtk_css_parser_error (parser, "Themeing engine '%s' not found", str);
      g_free (str);
      return FALSE;
    }

  g_value_set_object (value, engine);
  g_free (str);
  return TRUE;
}

static void
theming_engine_value_print (const GValue *value,
                            GString      *string)
{
  GtkThemingEngine *engine;
  char *name;

  engine = g_value_get_object (value);
  if (engine == NULL)
    g_string_append (string, "none");
  else
    {
      /* XXX: gtk_theming_engine_get_name()? */
      g_object_get (engine, "name", &name, NULL);
      g_string_append (string, name ? name : "none");
      g_free (name);
    }
}

static gboolean 
animation_description_value_parse (GtkCssParser *parser,
                                   GFile        *base,
                                   GValue       *value)
{
  GtkAnimationDescription *desc;
  char *str;

  str = _gtk_css_parser_read_value (parser);
  if (str == NULL)
    return FALSE;

  desc = _gtk_animation_description_from_string (str);
  g_free (str);

  if (desc == NULL)
    {
      _gtk_css_parser_error (parser, "Invalid animation description");
      return FALSE;
    }
  
  g_value_take_boxed (value, desc);
  return TRUE;
}

static void
animation_description_value_print (const GValue *value,
                                   GString      *string)
{
  GtkAnimationDescription *desc = g_value_get_boxed (value);

  if (desc == NULL)
    g_string_append (string, "none");
  else
    _gtk_animation_description_print (desc, string);
}

static gboolean 
border_value_parse (GtkCssParser *parser,
                    GFile        *base,
                    GValue       *value)
{
  GtkBorder border = { 0, };
  guint i;
  int numbers[4];

  for (i = 0; i < G_N_ELEMENTS (numbers); i++)
    {
      if (_gtk_css_parser_begins_with (parser, '-'))
	{
	  /* These are strictly speaking signed, but we want to be able to use them
	     for unsigned types too, as the actual ranges of values make this safe */
	  int res = _gtk_win32_theme_int_parse (parser, base, &numbers[i]);

	  if (res == 0) /* Parse error, report */
	    return FALSE;

	  if (res < 0) /* Nothing known to expand */
	    break;
	}
      else
        {
          if (!_gtk_css_parser_try_length (parser, &numbers[i]))
            break;
        }
    }

  if (i == 0)
    {
      _gtk_css_parser_error (parser, "Expected valid border");
      return FALSE;
    }

  border.top = numbers[0];
  if (i > 1)
    border.right = numbers[1];
  else
    border.right = border.top;
  if (i > 2)
    border.bottom = numbers[2];
  else
    border.bottom = border.top;
  if (i > 3)
    border.left = numbers[3];
  else
    border.left = border.right;

  g_value_set_boxed (value, &border);
  return TRUE;
}

static void
border_value_print (const GValue *value, GString *string)
{
  const GtkBorder *border = g_value_get_boxed (value);

  if (border == NULL)
    g_string_append (string, "none");
  else if (border->left != border->right)
    g_string_append_printf (string, "%d %d %d %d", border->top, border->right, border->bottom, border->left);
  else if (border->top != border->bottom)
    g_string_append_printf (string, "%d %d %d", border->top, border->right, border->bottom);
  else if (border->top != border->left)
    g_string_append_printf (string, "%d %d", border->top, border->right);
  else
    g_string_append_printf (string, "%d", border->top);
}

static gboolean 
gradient_value_parse (GtkCssParser *parser,
                      GFile        *base,
                      GValue       *value)
{
  GtkGradient *gradient;

  gradient = _gtk_gradient_parse (parser);
  if (gradient == NULL)
    return FALSE;

  g_value_take_boxed (value, gradient);
  return TRUE;
}

static void
gradient_value_print (const GValue *value,
                      GString      *string)
{
  GtkGradient *gradient = g_value_get_boxed (value);

  if (gradient == NULL)
    g_string_append (string, "none");
  else
    {
      char *s = gtk_gradient_to_string (gradient);
      g_string_append (string, s);
      g_free (s);
    }
}

static GFile *
gtk_css_parse_url (GtkCssParser *parser,
                   GFile        *base)
{
  gchar *path;
  GFile *file;

  if (_gtk_css_parser_try (parser, "url", FALSE))
    {
      if (!_gtk_css_parser_try (parser, "(", TRUE))
        {
          _gtk_css_parser_skip_whitespace (parser);
          if (_gtk_css_parser_try (parser, "(", TRUE))
            {
              GError *error;
              
              error = g_error_new_literal (GTK_CSS_PROVIDER_ERROR,
                                           GTK_CSS_PROVIDER_ERROR_DEPRECATED,
                                           "Whitespace between 'url' and '(' is deprecated");
                             
              _gtk_css_parser_take_error (parser, error);
            }
          else
            {
              _gtk_css_parser_error (parser, "Expected '(' after 'url'");
              return NULL;
            }
        }

      path = _gtk_css_parser_read_string (parser);
      if (path == NULL)
        return NULL;

      if (!_gtk_css_parser_try (parser, ")", TRUE))
        {
          _gtk_css_parser_error (parser, "No closing ')' found for 'url'");
          g_free (path);
          return NULL;
        }
    }
  else
    {
      path = _gtk_css_parser_try_name (parser, TRUE);
      if (path == NULL)
        {
          _gtk_css_parser_error (parser, "Not a valid url");
          return NULL;
        }
    }

  file = g_file_resolve_relative_path (base, path);
  g_free (path);

  return file;
}

static gboolean 
pattern_value_parse (GtkCssParser *parser,
                     GFile        *base,
                     GValue       *value)
{
  if (_gtk_css_parser_try (parser, "none", TRUE))
    {
      /* nothing to do here */
    }
  else if (_gtk_css_parser_begins_with (parser, '-'))
    {
      g_value_unset (value);
      g_value_init (value, GTK_TYPE_GRADIENT);
      return gradient_value_parse (parser, base, value);
    }
  else
    {
      GError *error = NULL;
      gchar *path;
      GdkPixbuf *pixbuf;
      GFile *file;
      cairo_surface_t *surface;
      cairo_pattern_t *pattern;
      cairo_t *cr;
      cairo_matrix_t matrix;

      file = gtk_css_parse_url (parser, base);
      if (file == NULL)
        return FALSE;

      path = g_file_get_path (file);
      g_object_unref (file);

      pixbuf = gdk_pixbuf_new_from_file (path, &error);
      g_free (path);
      if (pixbuf == NULL)
        {
          _gtk_css_parser_take_error (parser, error);
          return FALSE;
        }

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            gdk_pixbuf_get_width (pixbuf),
                                            gdk_pixbuf_get_height (pixbuf));
      cr = cairo_create (surface);
      gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
      cairo_paint (cr);
      pattern = cairo_pattern_create_for_surface (surface);

      cairo_matrix_init_scale (&matrix,
                               gdk_pixbuf_get_width (pixbuf),
                               gdk_pixbuf_get_height (pixbuf));
      cairo_pattern_set_matrix (pattern, &matrix);

      cairo_surface_destroy (surface);
      cairo_destroy (cr);
      g_object_unref (pixbuf);

      g_value_take_boxed (value, pattern);
    }
  
  return TRUE;
}

static cairo_status_t
surface_write (void                *closure,
               const unsigned char *data,
               unsigned int         length)
{
  g_byte_array_append (closure, data, length);

  return CAIRO_STATUS_SUCCESS;
}

static void
surface_print (cairo_surface_t *surface,
               GString *        string)
{
#if CAIRO_HAS_PNG_FUNCTIONS
  GByteArray *array;
  char *base64;
  
  array = g_byte_array_new ();
  cairo_surface_write_to_png_stream (surface, surface_write, array);
  base64 = g_base64_encode (array->data, array->len);
  g_byte_array_free (array, TRUE);

  g_string_append (string, "url(\"data:image/png;base64,");
  g_string_append (string, base64);
  g_string_append (string, "\")");

  g_free (base64);
#else
  g_string_append (string, "none /* you need cairo png functions enabled to make this work */");
#endif
}

static void
pattern_value_print (const GValue *value,
                     GString      *string)
{
  cairo_pattern_t *pattern;
  cairo_surface_t *surface;

  pattern = g_value_get_boxed (value);

  if (pattern == NULL)
    {
      g_string_append (string, "none");
      return;
    }

  switch (cairo_pattern_get_type (pattern))
    {
    case CAIRO_PATTERN_TYPE_SURFACE:
      if (cairo_pattern_get_surface (pattern, &surface) != CAIRO_STATUS_SUCCESS)
        {
          g_assert_not_reached ();
        }
      surface_print (surface, string);
      break;
    case CAIRO_PATTERN_TYPE_SOLID:
    case CAIRO_PATTERN_TYPE_LINEAR:
    case CAIRO_PATTERN_TYPE_RADIAL:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
pattern_value_compute (GValue          *computed,
                       GtkStyleContext *context,
                       const GValue    *specified)
{
  if (G_VALUE_HOLDS (specified, GTK_TYPE_GRADIENT))
    {
      cairo_pattern_t *gradient;
      
      gradient = gtk_gradient_resolve_for_context (g_value_get_boxed (specified), context);

      g_value_take_boxed (computed, gradient);
    }
  else
    g_value_copy (specified, computed);
}

static gboolean
shadow_value_parse (GtkCssParser *parser,
                    GFile *base,
                    GValue *value)
{
  gboolean have_inset, have_color, have_lengths;
  gdouble hoffset, voffset, blur, spread;
  GtkSymbolicColor *color;
  GtkShadow *shadow;
  guint i;

  shadow = _gtk_shadow_new ();

  if (_gtk_css_parser_try (parser, "none", TRUE))
    return TRUE;

  do
    {
      have_inset = have_lengths = have_color = FALSE;

      for (i = 0; i < 3; i++)
        {
          if (!have_inset && 
              _gtk_css_parser_try (parser, "inset", TRUE))
            {
              have_inset = TRUE;
              continue;
            }
            
          if (!have_lengths &&
              _gtk_css_parser_try_double (parser, &hoffset))
            {
              have_lengths = TRUE;

              if (!_gtk_css_parser_try_double (parser, &voffset))
                {
                  _gtk_css_parser_error (parser, "Horizontal and vertical offsets are required");
                  _gtk_shadow_unref (shadow);
                  return FALSE;
                }

              if (!_gtk_css_parser_try_double (parser, &blur))
                blur = 0;

              if (!_gtk_css_parser_try_double (parser, &spread))
                spread = 0;

              continue;
            }

          if (!have_color)
            {
              have_color = TRUE;

              /* XXX: the color is optional and UA-defined if it's missing,
               * but it doesn't really make sense for us...
               */
              color = _gtk_css_parser_read_symbolic_color (parser);

              if (color == NULL)
                {
                  _gtk_shadow_unref (shadow);
                  return FALSE;
                }
            }
        }

      if (!have_color || !have_lengths)
        {
          _gtk_css_parser_error (parser, "Must specify at least color and offsets");
          _gtk_shadow_unref (shadow);
          return FALSE;
        }

      _gtk_shadow_append (shadow,
                          hoffset, voffset,
                          blur, spread,
                          have_inset, color);

      gtk_symbolic_color_unref (color);

    }
  while (_gtk_css_parser_try (parser, ",", TRUE));

  g_value_take_boxed (value, shadow);
  return TRUE;
}

static void
shadow_value_print (const GValue *value,
                    GString      *string)
{
  GtkShadow *shadow;

  shadow = g_value_get_boxed (value);

  if (shadow == NULL)
    g_string_append (string, "none");
  else
    _gtk_shadow_print (shadow, string);
}

static void
shadow_value_compute (GValue          *computed,
                      GtkStyleContext *context,
                      const GValue    *specified)
{
  GtkShadow *shadow;

  shadow = g_value_get_boxed (specified);
  if (shadow)
    shadow = _gtk_shadow_resolve (shadow, context);

  g_value_take_boxed (computed, shadow);
}

static gboolean
border_image_repeat_value_parse (GtkCssParser *parser,
                                 GFile *file,
                                 GValue *value)
{
  GtkCssBorderImageRepeat image_repeat;
  GtkCssBorderRepeatStyle styles[2];
  gint i, v;

  for (i = 0; i < 2; i++)
    {
      if (_gtk_css_parser_try_enum (parser, GTK_TYPE_CSS_BORDER_REPEAT_STYLE, &v))
        styles[i] = v;
      else if (i == 0)
        {
          styles[1] = styles[0] = GTK_CSS_REPEAT_STYLE_STRETCH;
          break;
        }
      else
        styles[i] = styles[0];
    }

  image_repeat.hrepeat = styles[0];
  image_repeat.vrepeat = styles[1];

  g_value_set_boxed (value, &image_repeat);

  return TRUE;
}

static void
border_image_repeat_value_print (const GValue *value,
                                 GString      *string)
{
  GtkCssBorderImageRepeat *image_repeat;

  image_repeat = g_value_get_boxed (value);

  enum_print (image_repeat->hrepeat, GTK_TYPE_CSS_BORDER_REPEAT_STYLE, string);
  if (image_repeat->hrepeat != image_repeat->vrepeat)
    {
      g_string_append (string, " ");
      enum_print (image_repeat->vrepeat, GTK_TYPE_CSS_BORDER_REPEAT_STYLE, string);
    }
}

static gboolean 
enum_value_parse (GtkCssParser *parser,
                  GFile        *base,
                  GValue       *value)
{
  int v;

  if (enum_parse (parser, G_VALUE_TYPE (value), &v))
    {
      g_value_set_enum (value, v);
      return TRUE;
    }

  return FALSE;
}

static void
enum_value_print (const GValue *value,
                  GString      *string)
{
  enum_print (g_value_get_enum (value), G_VALUE_TYPE (value), string);
}

static gboolean 
flags_value_parse (GtkCssParser *parser,
                   GFile        *base,
                   GValue       *value)
{
  GFlagsClass *flags_class;
  GFlagsValue *flag_value;
  guint flags = 0;
  char *str;

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));

  do {
    str = _gtk_css_parser_try_ident (parser, TRUE);
    if (str == NULL)
      {
        _gtk_css_parser_error (parser, "Expected an identifier");
        g_type_class_unref (flags_class);
        return FALSE;
      }

      flag_value = g_flags_get_value_by_nick (flags_class, str);
      if (!flag_value)
        {
          _gtk_css_parser_error (parser,
                                 "Unknown flag value '%s' for type '%s'",
                                 str, g_type_name (G_VALUE_TYPE (value)));
          /* XXX Do we want to return FALSE here? We can get
           * forward-compatibility for new values this way
           */
          g_free (str);
          g_type_class_unref (flags_class);
          return FALSE;
        }

      g_free (str);
    }
  while (_gtk_css_parser_try (parser, ",", FALSE));

  g_type_class_unref (flags_class);

  g_value_set_enum (value, flags);

  return TRUE;
}

static void
flags_value_print (const GValue *value,
                   GString      *string)
{
  GFlagsClass *flags_class;
  guint i, flags;

  flags_class = g_type_class_ref (G_VALUE_TYPE (value));
  flags = g_value_get_flags (value);

  for (i = 0; i < flags_class->n_values; i++)
    {
      GFlagsValue *flags_value = &flags_class->values[i];

      if (flags & flags_value->value)
        {
          if (string->len != 0)
            g_string_append (string, ", ");

          g_string_append (string, flags_value->value_nick);
        }
    }

  g_type_class_unref (flags_class);
}

/*** API ***/

static void
gtk_css_style_funcs_init (void)
{
  if (G_LIKELY (parse_funcs != NULL))
    return;

  parse_funcs = g_hash_table_new (NULL, NULL);
  print_funcs = g_hash_table_new (NULL, NULL);
  compute_funcs = g_hash_table_new (NULL, NULL);

  register_conversion_function (GDK_TYPE_RGBA,
                                rgba_value_parse,
                                rgba_value_print,
                                rgba_value_compute);
  register_conversion_function (GDK_TYPE_COLOR,
                                color_value_parse,
                                color_value_print,
                                color_value_compute);
  register_conversion_function (GTK_TYPE_SYMBOLIC_COLOR,
                                symbolic_color_value_parse,
                                symbolic_color_value_print,
                                NULL);
  register_conversion_function (PANGO_TYPE_FONT_DESCRIPTION,
                                font_description_value_parse,
                                font_description_value_print,
                                NULL);
  register_conversion_function (G_TYPE_BOOLEAN,
                                boolean_value_parse,
                                boolean_value_print,
                                NULL);
  register_conversion_function (G_TYPE_INT,
                                int_value_parse,
                                int_value_print,
                                NULL);
  register_conversion_function (G_TYPE_UINT,
                                uint_value_parse,
                                uint_value_print,
                                NULL);
  register_conversion_function (G_TYPE_DOUBLE,
                                double_value_parse,
                                double_value_print,
                                NULL);
  register_conversion_function (G_TYPE_FLOAT,
                                float_value_parse,
                                float_value_print,
                                NULL);
  register_conversion_function (G_TYPE_STRING,
                                string_value_parse,
                                string_value_print,
                                NULL);
  register_conversion_function (GTK_TYPE_THEMING_ENGINE,
                                theming_engine_value_parse,
                                theming_engine_value_print,
                                NULL);
  register_conversion_function (GTK_TYPE_ANIMATION_DESCRIPTION,
                                animation_description_value_parse,
                                animation_description_value_print,
                                NULL);
  register_conversion_function (GTK_TYPE_BORDER,
                                border_value_parse,
                                border_value_print,
                                NULL);
  register_conversion_function (GTK_TYPE_GRADIENT,
                                gradient_value_parse,
                                gradient_value_print,
                                NULL);
  register_conversion_function (CAIRO_GOBJECT_TYPE_PATTERN,
                                pattern_value_parse,
                                pattern_value_print,
                                pattern_value_compute);
  register_conversion_function (GTK_TYPE_CSS_BORDER_IMAGE_REPEAT,
                                border_image_repeat_value_parse,
                                border_image_repeat_value_print,
                                NULL);
  register_conversion_function (GTK_TYPE_SHADOW,
                                shadow_value_parse,
                                shadow_value_print,
                                shadow_value_compute);
  register_conversion_function (G_TYPE_ENUM,
                                enum_value_parse,
                                enum_value_print,
                                NULL);
  register_conversion_function (G_TYPE_FLAGS,
                                flags_value_parse,
                                flags_value_print,
                                NULL);
}

/**
 * _gtk_css_style_parse_value:
 * @value: the value to parse into. Must be a valid initialized #GValue
 * @parser: the parser to parse from
 * @base: the base URL for @parser
 *
 * This is the generic parsing function used for CSS values. If the
 * function fails to parse a value, it will emit an error on @parser,
 * return %FALSE and not touch @value.
 *
 * Returns: %TRUE if parsing succeeded.
 **/
gboolean
_gtk_css_style_parse_value (GValue       *value,
                            GtkCssParser *parser,
                            GFile        *base)
{
  GtkStyleParseFunc func;

  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (parser != NULL, FALSE);

  gtk_css_style_funcs_init ();

  func = g_hash_table_lookup (parse_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (value)));
  if (func == NULL)
    func = g_hash_table_lookup (parse_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (value))));

  if (func == NULL)
    {
      _gtk_css_parser_error (parser,
                             "Cannot convert to type '%s'",
                             g_type_name (G_VALUE_TYPE (value)));
      return FALSE;
    }

  return (*func) (parser, base, value);
}

/**
 * _gtk_css_style_print_value:
 * @value: an initialized GValue returned from _gtk_css_style_parse()
 * @string: the string to print into
 *
 * Prints @value into @string as a CSS value. If @value is not a
 * valid value, a random string will be printed instead.
 **/
void
_gtk_css_style_print_value (const GValue *value,
                            GString      *string)
{
  GtkStylePrintFunc func;

  gtk_css_style_funcs_init ();

  func = g_hash_table_lookup (print_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (value)));
  if (func == NULL)
    func = g_hash_table_lookup (print_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (value))));

  if (func == NULL)
    {
      char *s = g_strdup_value_contents (value);
      g_string_append (string, s);
      g_free (s);
      return;
    }
  
  func (value, string);
}

/**
 * _gtk_css_style_compute_value:
 * @computed: (out): a value to be filled with the result
 * @context: the context to use for computing the value
 * @specified: the value to use for the computation
 *
 * Converts the @specified value into the @computed value using the
 * information in @context. The values must have matching types, ie
 * @specified must be a result of a call to
 * _gtk_css_style_parse_value() with the same type as @computed.
 **/
void
_gtk_css_style_compute_value (GValue          *computed,
                              GtkStyleContext *context,
                              const GValue    *specified)
{
  GtkStyleComputeFunc func;

  g_return_if_fail (G_IS_VALUE (computed));
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (G_IS_VALUE (specified));

  gtk_css_style_funcs_init ();

  func = g_hash_table_lookup (compute_funcs,
                              GSIZE_TO_POINTER (G_VALUE_TYPE (computed)));
  if (func == NULL)
    func = g_hash_table_lookup (compute_funcs,
                                GSIZE_TO_POINTER (g_type_fundamental (G_VALUE_TYPE (computed))));

  if (func)
    func (computed, context, specified);
  else
    g_value_copy (specified, computed);
}

