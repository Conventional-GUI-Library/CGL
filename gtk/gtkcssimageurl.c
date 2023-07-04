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

#include "gtkcssimageurlprivate.h"

#include "gtkcssprovider.h"

G_DEFINE_TYPE (GtkCssImageUrl, _gtk_css_image_url, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_url_get_width (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return cairo_image_surface_get_width (url->surface);
}

static int
gtk_css_image_url_get_height (GtkCssImage *image)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  return cairo_image_surface_get_height (url->surface);
}

static void
gtk_css_image_url_draw (GtkCssImage        *image,
                        cairo_t            *cr,
                        double              width,
                        double              height)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_scale (cr,
               width / cairo_image_surface_get_width (url->surface),
               height / cairo_image_surface_get_height (url->surface));
  cairo_set_source_surface (cr, url->surface, 0, 0);
  cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_PAD);
  cairo_fill (cr);
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
gtk_css_image_url_parse (GtkCssImage  *image,
                         GtkCssParser *parser,
                         GFile        *base)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);
  GdkPixbuf *pixbuf;
  GFile *file;
  char *path;
  cairo_t *cr;
  GError *error = NULL;

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

  url->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                             gdk_pixbuf_get_width (pixbuf),
                                             gdk_pixbuf_get_height (pixbuf));
  cr = cairo_create (url->surface);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  g_object_unref (pixbuf);

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
gtk_css_image_url_print (GtkCssImage *image,
                         GString     *string)
{
#if CAIRO_HAS_PNG_FUNCTIONS
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);
  GByteArray *array;
  char *base64;
  
  array = g_byte_array_new ();
  cairo_surface_write_to_png_stream (url->surface, surface_write, array);
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
gtk_css_image_url_dispose (GObject *object)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (object);

  if (url->surface)
    {
      cairo_surface_destroy (url->surface);
      url->surface = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_image_url_parent_class)->dispose (object);
}

static void
_gtk_css_image_url_class_init (GtkCssImageUrlClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_url_get_width;
  image_class->get_height = gtk_css_image_url_get_height;
  image_class->draw = gtk_css_image_url_draw;
  image_class->parse = gtk_css_image_url_parse;
  image_class->print = gtk_css_image_url_print;

  object_class->dispose = gtk_css_image_url_dispose;
}

static void
_gtk_css_image_url_init (GtkCssImageUrl *image_url)
{
}

