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

#include "config.h"

#include "gtkcssshadowvalueprivate.h"

#include "gtkcairoblurprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtkthemingengineprivate.h"
#include "gtkpango.h"

#include <math.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint inset :1;

  GtkCssValue *hoffset;
  GtkCssValue *voffset;
  GtkCssValue *radius;
  GtkCssValue *spread;

  GtkCssValue *color;
};

static GtkCssValue *    gtk_css_shadow_value_new (GtkCssValue *hoffset,
                                                  GtkCssValue *voffset,
                                                  GtkCssValue *radius,
                                                  GtkCssValue *spread,
                                                  gboolean     inset,
                                                  GtkCssValue *color);

static void
gtk_css_value_shadow_free (GtkCssValue *shadow)
{
  _gtk_css_value_unref (shadow->hoffset);
  _gtk_css_value_unref (shadow->voffset);
  _gtk_css_value_unref (shadow->radius);
  _gtk_css_value_unref (shadow->spread);
  _gtk_css_value_unref (shadow->color);

  g_slice_free (GtkCssValue, shadow);
}

static GtkCssValue *
gtk_css_value_shadow_compute (GtkCssValue             *shadow,
                              guint                    property_id,
                              GtkStyleProviderPrivate *provider,
			      int                      scale,
                              GtkCssComputedValues    *values,
                              GtkCssComputedValues    *parent_values,
                              GtkCssDependencies      *dependencies)
{
  GtkCssValue *hoffset, *voffset, *radius, *spread, *color;
  GtkCssDependencies child_deps;

  child_deps = 0;
  hoffset = _gtk_css_value_compute (shadow->hoffset, property_id, provider, scale, values, parent_values, &child_deps);
  *dependencies = _gtk_css_dependencies_union (*dependencies, child_deps);

  child_deps = 0;
  voffset = _gtk_css_value_compute (shadow->voffset, property_id, provider, scale, values, parent_values, &child_deps);
  *dependencies = _gtk_css_dependencies_union (*dependencies, child_deps);

  child_deps = 0;
  radius = _gtk_css_value_compute (shadow->radius, property_id, provider, scale, values, parent_values, &child_deps);
  *dependencies = _gtk_css_dependencies_union (*dependencies, child_deps);

  child_deps = 0;
  spread = _gtk_css_value_compute (shadow->spread, property_id, provider, scale, values, parent_values, &child_deps),
  *dependencies = _gtk_css_dependencies_union (*dependencies, child_deps);

  child_deps = 0;
  color = _gtk_css_value_compute (shadow->color, property_id, provider, scale, values, parent_values, &child_deps);
  *dependencies = _gtk_css_dependencies_union (*dependencies, child_deps);

  return gtk_css_shadow_value_new (hoffset, voffset, radius, spread, shadow->inset, color);
}

static gboolean
gtk_css_value_shadow_equal (const GtkCssValue *shadow1,
                            const GtkCssValue *shadow2)
{
  return shadow1->inset == shadow2->inset
      && _gtk_css_value_equal (shadow1->hoffset, shadow2->hoffset)
      && _gtk_css_value_equal (shadow1->voffset, shadow2->voffset)
      && _gtk_css_value_equal (shadow1->radius, shadow2->radius)
      && _gtk_css_value_equal (shadow1->spread, shadow2->spread)
      && _gtk_css_value_equal (shadow1->color, shadow2->color);
}

static GtkCssValue *
gtk_css_value_shadow_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  if (start->inset != end->inset)
    return NULL;

  return gtk_css_shadow_value_new (_gtk_css_value_transition (start->hoffset, end->hoffset, property_id, progress),
                                   _gtk_css_value_transition (start->voffset, end->voffset, property_id, progress),
                                   _gtk_css_value_transition (start->radius, end->radius, property_id, progress),
                                   _gtk_css_value_transition (start->spread, end->spread, property_id, progress),
                                   start->inset,
                                   _gtk_css_value_transition (start->color, end->color, property_id, progress));
}

static void
gtk_css_value_shadow_print (const GtkCssValue *shadow,
                            GString           *string)
{
  _gtk_css_value_print (shadow->hoffset, string);
  g_string_append_c (string, ' ');
  _gtk_css_value_print (shadow->voffset, string);
  g_string_append_c (string, ' ');
  if (_gtk_css_number_value_get (shadow->radius, 100) != 0 ||
      _gtk_css_number_value_get (shadow->spread, 100) != 0)
    {
      _gtk_css_value_print (shadow->radius, string);
      g_string_append_c (string, ' ');
    }

  if (_gtk_css_number_value_get (shadow->spread, 100) != 0)
    {
      _gtk_css_value_print (shadow->spread, string);
      g_string_append_c (string, ' ');
    }

  _gtk_css_value_print (shadow->color, string);

  if (shadow->inset)
    g_string_append (string, " inset");

}

static const GtkCssValueClass GTK_CSS_VALUE_SHADOW = {
  gtk_css_value_shadow_free,
  gtk_css_value_shadow_compute,
  gtk_css_value_shadow_equal,
  gtk_css_value_shadow_transition,
  gtk_css_value_shadow_print
};

static GtkCssValue *
gtk_css_shadow_value_new (GtkCssValue *hoffset,
                          GtkCssValue *voffset,
                          GtkCssValue *radius,
                          GtkCssValue *spread,
                          gboolean     inset,
                          GtkCssValue *color)
{
  GtkCssValue *retval;

  retval = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_SHADOW);

  retval->hoffset = hoffset;
  retval->voffset = voffset;
  retval->radius = radius;
  retval->spread = spread;
  retval->inset = inset;
  retval->color = color;

  return retval;
}                  

GtkCssValue *
_gtk_css_shadow_value_new_for_transition (GtkCssValue *target)
{
  GdkRGBA transparent = { 0, 0, 0, 0 };

  g_return_val_if_fail (target->class == &GTK_CSS_VALUE_SHADOW, NULL);

  return gtk_css_shadow_value_new (_gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   _gtk_css_number_value_new (0, GTK_CSS_PX),
                                   target->inset,
                                   _gtk_css_rgba_value_new_from_rgba (&transparent));
}

static gboolean
value_is_done_parsing (GtkCssParser *parser)
{
  return _gtk_css_parser_is_eof (parser) ||
         _gtk_css_parser_begins_with (parser, ',') ||
         _gtk_css_parser_begins_with (parser, ';') ||
         _gtk_css_parser_begins_with (parser, '}');
}

GtkCssValue *
_gtk_css_shadow_value_parse (GtkCssParser *parser)
{
  enum {
    HOFFSET,
    VOFFSET,
    RADIUS,
    SPREAD,
    COLOR,
    N_VALUES
  };
  GtkCssValue *values[N_VALUES] = { NULL, };
  gboolean inset;
  guint i;

  inset = _gtk_css_parser_try (parser, "inset", TRUE);

  do
  {
    if (values[HOFFSET] == NULL &&
         _gtk_css_parser_has_number (parser))
      {
        values[HOFFSET] = _gtk_css_number_value_parse (parser,
                                                       GTK_CSS_PARSE_LENGTH
                                                       | GTK_CSS_NUMBER_AS_PIXELS);
        if (values[HOFFSET] == NULL)
          goto fail;

        values[VOFFSET] = _gtk_css_number_value_parse (parser,
                                                       GTK_CSS_PARSE_LENGTH
                                                       | GTK_CSS_NUMBER_AS_PIXELS);
        if (values[VOFFSET] == NULL)
          goto fail;

        if (_gtk_css_parser_has_number (parser))
          {
            values[RADIUS] = _gtk_css_number_value_parse (parser,
                                                          GTK_CSS_PARSE_LENGTH
                                                          | GTK_CSS_POSITIVE_ONLY
                                                          | GTK_CSS_NUMBER_AS_PIXELS);
            if (values[RADIUS] == NULL)
              goto fail;
          }
        else
          values[RADIUS] = _gtk_css_number_value_new (0.0, GTK_CSS_PX);
                                                        
        if (_gtk_css_parser_has_number (parser))
          {
            values[SPREAD] = _gtk_css_number_value_parse (parser,
                                                          GTK_CSS_PARSE_LENGTH
                                                          | GTK_CSS_NUMBER_AS_PIXELS);
            if (values[SPREAD] == NULL)
              goto fail;
          }
        else
          values[SPREAD] = _gtk_css_number_value_new (0.0, GTK_CSS_PX);
      }
    else if (!inset && _gtk_css_parser_try (parser, "inset", TRUE))
      {
        if (values[HOFFSET] == NULL)
          goto fail;
        inset = TRUE;
        break;
      }
    else if (values[COLOR] == NULL)
      {
        values[COLOR] = _gtk_css_color_value_parse (parser);

        if (values[COLOR] == NULL)
          goto fail;
      }
    else
      {
        /* We parsed everything and there's still stuff left?
         * Pretend we didn't notice and let the normal code produce
         * a 'junk at end of value' error */
        goto fail;
      }
  }
  while (values[HOFFSET] == NULL || !value_is_done_parsing (parser));

  if (values[COLOR] == NULL)
    values[COLOR] = _gtk_css_color_value_new_current_color ();

  return gtk_css_shadow_value_new (values[HOFFSET], values[VOFFSET],
                                   values[RADIUS], values[SPREAD],
                                   inset, values[COLOR]);

fail:
  for (i = 0; i < N_VALUES; i++)
    {
      if (values[i])
        _gtk_css_value_unref (values[i]);
    }

  return NULL;
}

static const cairo_user_data_key_t shadow_key;

static cairo_t *
gtk_css_shadow_value_start_drawing (const GtkCssValue *shadow,
                                    cairo_t           *cr)
{
  cairo_rectangle_int_t clip_rect;
  cairo_surface_t *surface;
  cairo_t *blur_cr;
  gdouble radius, clip_radius;

  radius = _gtk_css_number_value_get (shadow->radius, 0);
  if (radius == 0.0)
    return cr;

  gdk_cairo_get_clip_rectangle (cr, &clip_rect);

  clip_radius = _gtk_cairo_blur_compute_pixels (radius);

  /* Create a larger surface to center the blur. */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        clip_rect.width + 2 * clip_radius,
                                        clip_rect.height + 2 * clip_radius);
  cairo_surface_set_device_offset (surface, clip_radius - clip_rect.x, clip_radius - clip_rect.y);
  blur_cr = cairo_create (surface);
  cairo_set_user_data (blur_cr, &shadow_key, cairo_reference (cr), (cairo_destroy_func_t) cairo_destroy);

  if (cairo_has_current_point (cr))
    {
      double x, y;
      
      cairo_get_current_point (cr, &x, &y);
      cairo_move_to (blur_cr, x, y);
    }

  return blur_cr;
}

static cairo_t *
gtk_css_shadow_value_finish_drawing (const GtkCssValue *shadow,
                                     cairo_t           *cr)
{
  gdouble radius;
  cairo_t *original_cr;
  cairo_surface_t *surface;

  radius = _gtk_css_number_value_get (shadow->radius, 0);
  if (radius == 0.0)
    return cr;

  surface = cairo_get_target (cr);
  original_cr = cairo_get_user_data (cr, &shadow_key);

  /* Blur the surface. */
  _gtk_cairo_blur_surface (surface, radius);

  cairo_set_source_surface (original_cr, surface, 0, 0);
  cairo_paint (original_cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  return original_cr;
}

void
_gtk_css_shadow_value_paint_layout (const GtkCssValue *shadow,
                                    cairo_t           *cr,
                                    PangoLayout       *layout)
{
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  if (!cairo_has_current_point (cr))
    cairo_move_to (cr, 0, 0);

  cairo_save (cr);

  cairo_rel_move_to (cr, 
                     _gtk_css_number_value_get (shadow->hoffset, 0),
                     _gtk_css_number_value_get (shadow->voffset, 0));

  cr = gtk_css_shadow_value_start_drawing (shadow, cr);

  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  _gtk_pango_fill_layout (cr, layout);

  cr = gtk_css_shadow_value_finish_drawing (shadow, cr);

  cairo_rel_move_to (cr,
                     - _gtk_css_number_value_get (shadow->hoffset, 0),
                     - _gtk_css_number_value_get (shadow->voffset, 0));
  cairo_restore (cr);
}

void
_gtk_css_shadow_value_paint_icon (const GtkCssValue *shadow,
			          cairo_t           *cr)
{
  cairo_pattern_t *pattern;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  cairo_save (cr);
  pattern = cairo_pattern_reference (cairo_get_source (cr));

  cr = gtk_css_shadow_value_start_drawing (shadow, cr);

  gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (shadow->color));

  cairo_translate (cr,
                   _gtk_css_number_value_get (shadow->hoffset, 0),
                   _gtk_css_number_value_get (shadow->voffset, 0));
  cairo_mask (cr, pattern);

  cr = gtk_css_shadow_value_finish_drawing (shadow, cr);

  cairo_restore (cr);
  cairo_pattern_destroy (pattern);
}

void
_gtk_css_shadow_value_paint_spinner (const GtkCssValue *shadow,
                                     cairo_t           *cr,
                                     gdouble            radius,
                                     gdouble            progress)
{
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  cairo_save (cr);

  cr = gtk_css_shadow_value_start_drawing (shadow, cr);

  cairo_translate (cr,
                   _gtk_css_number_value_get (shadow->hoffset, 0),
                   _gtk_css_number_value_get (shadow->voffset, 0));
  _gtk_theming_engine_paint_spinner (cr,
                                     radius, progress,
                                     _gtk_css_rgba_value_get_rgba (shadow->color));

  cr = gtk_css_shadow_value_finish_drawing (shadow, cr);

  cairo_restore (cr);
}

gboolean
_gtk_css_shadow_value_get_inset (const GtkCssValue *shadow)
{
  g_return_val_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW, FALSE);

  return shadow->inset;
}

void
_gtk_css_shadow_value_get_geometry (const GtkCssValue *shadow,
                                    gdouble           *hoffset,
                                    gdouble           *voffset,
                                    gdouble           *radius,
                                    gdouble           *spread)
{
  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  if (hoffset != NULL)
    *hoffset = _gtk_css_number_value_get (shadow->hoffset, 0);
  if (voffset != NULL)
    *voffset = _gtk_css_number_value_get (shadow->voffset, 0);

  if (radius != NULL)
    *radius = _gtk_css_number_value_get (shadow->radius, 0);
  if (spread != NULL)
    *spread = _gtk_css_number_value_get (shadow->spread, 0);
}

static gboolean
has_empty_clip (cairo_t *cr)
{
  double x1, y1, x2, y2;

  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
  return x1 == x2 && y1 == y2;
}

static void
draw_shadow (const GtkCssValue   *shadow,
	     cairo_t             *cr,
	     GtkRoundedBox       *box,
	     GtkRoundedBox       *clip_box,
	     gboolean             blur)
{
  cairo_t *shadow_cr;

  if (has_empty_clip (cr))
    return;

  if (blur)
    shadow_cr = gtk_css_shadow_value_start_drawing (shadow, cr);
  else
    shadow_cr = cr;

  cairo_set_fill_rule (shadow_cr, CAIRO_FILL_RULE_EVEN_ODD);
  _gtk_rounded_box_path (box, shadow_cr);
  if (shadow->inset)
    _gtk_rounded_box_clip_path (clip_box, shadow_cr);

  gdk_cairo_set_source_rgba (shadow_cr, _gtk_css_rgba_value_get_rgba (shadow->color));
  cairo_fill (shadow_cr);

  if (blur)
    gtk_css_shadow_value_finish_drawing (shadow, shadow_cr);
}

void
_gtk_css_shadow_value_paint_box (const GtkCssValue   *shadow,
                                 cairo_t             *cr,
                                 const GtkRoundedBox *padding_box)
{
  GtkRoundedBox box, clip_box;
  double spread, radius, clip_radius, x, y, outside;

  g_return_if_fail (shadow->class == &GTK_CSS_VALUE_SHADOW);

  cairo_save (cr);

  spread = _gtk_css_number_value_get (shadow->spread, 0);
  radius = _gtk_css_number_value_get (shadow->radius, 0);
  clip_radius = _gtk_cairo_blur_compute_pixels (radius);
  x = _gtk_css_number_value_get (shadow->hoffset, 0);
  y = _gtk_css_number_value_get (shadow->voffset, 0);

  if (shadow->inset)
    {
      _gtk_rounded_box_path (padding_box, cr);
      cairo_clip (cr);
    }
  else
    {
      cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);
      _gtk_rounded_box_path (padding_box, cr);

      outside = spread + clip_radius + MAX (fabs (x), fabs (y));
      clip_box = *padding_box;
      _gtk_rounded_box_grow (&clip_box, outside, outside, outside, outside);
      _gtk_rounded_box_clip_path (&clip_box, cr);

      cairo_clip (cr);
    }

  if (has_empty_clip (cr))
    {
      cairo_restore (cr);
      return;
    }

  box = *padding_box;
  _gtk_rounded_box_move (&box, x, y);

  if (shadow->inset)
    _gtk_rounded_box_shrink (&box, spread, spread, spread, spread);
  else /* Outset */
    _gtk_rounded_box_grow (&box, spread, spread, spread, spread);

  clip_box = *padding_box;
  _gtk_rounded_box_shrink (&clip_box, -clip_radius, -clip_radius, -clip_radius, -clip_radius);

  if (radius == 0)
    draw_shadow (shadow, cr, &box, &clip_box, FALSE);
  else
    {
      int i, x1, x2, y1, y2;
      cairo_region_t *remaining;
      cairo_rectangle_int_t r;

      /* For the blurred case we divide the rendering into 9 parts,
       * 4 of the corners, 4 for the horizonat/vertical lines and
       * one for the interior. We make the non-interior parts
       * large enought to fit the full radius of the blur, so that
       * the interior part can be drawn solidly.
       */

      if (shadow->inset)
	{
	  /* In the inset case we want to paint the whole clip-box.
	   * We could remove the part of "box" where the blur doesn't
	   * reach, but computing that is a bit tricky since the
	   * rounded corners are on the "inside" of it. */
	  r.x = floor (clip_box.box.x);
	  r.y = floor (clip_box.box.y);
	  r.width = ceil (clip_box.box.x + clip_box.box.width) - r.x;
	  r.height = ceil (clip_box.box.y + clip_box.box.height) - r.y;
	  remaining = cairo_region_create_rectangle (&r);
	}
      else
	{
	  /* In the outset case we want to paint the entire box, plus as far
	   * as the radius reaches from it */
	  r.x = floor (box.box.x - clip_radius);
	  r.y = floor (box.box.y - clip_radius);
	  r.width = ceil (box.box.x + box.box.width + clip_radius) - r.x;
	  r.height = ceil (box.box.y + box.box.height + clip_radius) - r.y;

	  remaining = cairo_region_create_rectangle (&r);
	}

      /* First do the corners of box */
      for (i = 0; i < 4; i++)
	{
	  if (i == GTK_CSS_TOP_LEFT || i == GTK_CSS_BOTTOM_LEFT)
	    {
	      x1 = floor (box.box.x - clip_radius);
	      x2 = ceil (box.box.x + box.corner[i].horizontal + clip_radius);
	    }
	  else
	    {
	      x1 = floor (box.box.x + box.box.width - box.corner[i].horizontal - clip_radius);
	      x2 = ceil (box.box.x + box.box.width + clip_radius);
	    }

	  if (i == GTK_CSS_TOP_LEFT || i == GTK_CSS_TOP_RIGHT)
	    {
	      y1 = floor (box.box.y - clip_radius);
	      y2 = ceil (box.box.y + box.corner[i].vertical + clip_radius);
	    }
	  else
	    {
	      y1 = floor (box.box.y + box.box.height - box.corner[i].vertical - clip_radius);
	      y2 = ceil (box.box.y + box.box.height + clip_radius);
	    }


	  cairo_save (cr);
	  cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
	  cairo_clip (cr);
	  /* Also clip with remaining to ensure we never draw any area twice */
	  gdk_cairo_region (cr, remaining);
	  cairo_clip (cr);
	  draw_shadow (shadow, cr, &box, &clip_box, TRUE);
	  cairo_restore (cr);

	  /* We drew the region, remove it from remaining */
	  r.x = x1;
	  r.y = y1;
	  r.width = x2 - x1;
	  r.height = y2 - y1;
	  cairo_region_subtract_rectangle (remaining, &r);
	}

      /* Then the sides */
      for (i = 0; i < 4; i++)
	{
	  if (i == GTK_CSS_TOP || i == GTK_CSS_BOTTOM)
	    {
	      x1 = floor (box.box.x - clip_radius);
	      x2 = ceil (box.box.x + box.box.width + clip_radius);
	    }
	  else if (i == GTK_CSS_LEFT)
	    {
	      x1 = floor (box.box.x -clip_radius);
	      x2 = ceil (box.box.x + clip_radius);
	    }
	  else
	    {
	      x1 = floor (box.box.x + box.box.width -clip_radius);
	      x2 = ceil (box.box.x + box.box.width + clip_radius);
	    }

	  if (i == GTK_CSS_LEFT || i == GTK_CSS_RIGHT)
	    {
	      y1 = floor (box.box.y - clip_radius);
	      y2 = ceil (box.box.y + box.box.height + clip_radius);
	    }
	  else if (i == GTK_CSS_TOP)
	    {
	      y1 = floor (box.box.y -clip_radius);
	      y2 = ceil (box.box.y + clip_radius);
	    }
	  else
	    {
	      y1 = floor (box.box.y + box.box.height -clip_radius);
	      y2 = ceil (box.box.y + box.box.height + clip_radius);
	    }

	  cairo_save (cr);
	  cairo_rectangle (cr, x1, y1, x2 - x1, y2 - y1);
	  cairo_clip (cr);
	  /* Also clip with remaining to ensure we never draw any area twice */
	  gdk_cairo_region (cr, remaining);
	  cairo_clip (cr);
	  draw_shadow (shadow, cr, &box, &clip_box, TRUE);
	  cairo_restore (cr);

	  /* We drew the region, remove it from remaining */
	  r.x = x1;
	  r.y = y1;
	  r.width = x2 - x1;
	  r.height = y2 - y1;
	  cairo_region_subtract_rectangle (remaining, &r);
	}

      /* Then the rest, which needs no blurring */

      cairo_save (cr);
      gdk_cairo_region (cr, remaining);
      cairo_clip (cr);
      draw_shadow (shadow, cr, &box, &clip_box, FALSE);
      cairo_restore (cr);

      cairo_region_destroy (remaining);
    }

  cairo_restore (cr);
}
