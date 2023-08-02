/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
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

#include <math.h>
#include "gtkiconhelperprivate.h"

G_DEFINE_TYPE (GtkIconHelper, _gtk_icon_helper, G_TYPE_OBJECT)

struct _GtkIconHelperPrivate {
  GtkImageType storage_type;

  GdkWindow *window;

  GdkPixbuf *orig_pixbuf;
  int orig_pixbuf_scale;
  GdkPixbufAnimation *animation;
  GIcon *gicon;
  GtkIconSet *icon_set;
  gchar *icon_name;
  gchar *stock_id;
  cairo_surface_t *orig_surface;

  GtkIconSize icon_size;
  gint pixel_size;

  guint use_fallback : 1;
  guint force_scale_pixbuf : 1;

  GdkPixbuf *rendered_pixbuf;
  GtkStateFlags last_rendered_state;

  cairo_surface_t *rendered_surface;
  gint rendered_surface_width;
  gint rendered_surface_height;
  GtkStateFlags last_surface_state;
  gint last_surface_scale;
};

void
_gtk_icon_helper_clear (GtkIconHelper *self)
{
  g_clear_object (&self->priv->gicon);
  g_clear_object (&self->priv->orig_pixbuf);
  g_clear_object (&self->priv->animation);
  g_clear_object (&self->priv->rendered_pixbuf);
  g_clear_object (&self->priv->window);

  if (self->priv->orig_surface)
    {
      cairo_surface_destroy (self->priv->orig_surface);
      self->priv->orig_surface = NULL;
    }

  if (self->priv->rendered_surface)
    {
      cairo_surface_destroy (self->priv->rendered_surface);
      self->priv->rendered_surface = NULL;
    }

  if (self->priv->icon_set != NULL)
    {
      gtk_icon_set_unref (self->priv->icon_set);
      self->priv->icon_set = NULL;
    }

  g_free (self->priv->icon_name);
  self->priv->icon_name = NULL;

  g_free (self->priv->stock_id);
  self->priv->stock_id = NULL;

  self->priv->storage_type = GTK_IMAGE_EMPTY;
  self->priv->icon_size = GTK_ICON_SIZE_INVALID;
  self->priv->last_rendered_state = GTK_STATE_FLAG_NORMAL;
  self->priv->last_surface_state = GTK_STATE_FLAG_NORMAL;
  self->priv->last_surface_scale = 0;
}

void
_gtk_icon_helper_invalidate (GtkIconHelper *self)
{
  g_clear_object (&self->priv->rendered_pixbuf);
  if (self->priv->rendered_surface != NULL)
    {
      cairo_surface_destroy (self->priv->rendered_surface);
      self->priv->rendered_surface = NULL;
    }
}

void
 _gtk_icon_helper_set_window (GtkIconHelper *self,
			      GdkWindow *window)
{
  if (window)
    g_object_ref (window);
  g_clear_object (&self->priv->window);
  self->priv->window = window;

}

static void
gtk_icon_helper_finalize (GObject *object)
{
  GtkIconHelper *self = GTK_ICON_HELPER (object);

  _gtk_icon_helper_clear (self);
  
  G_OBJECT_CLASS (_gtk_icon_helper_parent_class)->finalize (object);
}

static void
_gtk_icon_helper_class_init (GtkIconHelperClass *klass)
{
  GObjectClass *oclass;

  oclass = G_OBJECT_CLASS (klass);
  oclass->finalize = gtk_icon_helper_finalize;

  g_type_class_add_private (klass, sizeof (GtkIconHelperPrivate));
}

static void
_gtk_icon_helper_init (GtkIconHelper *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_ICON_HELPER, GtkIconHelperPrivate);

  self->priv->storage_type = GTK_IMAGE_EMPTY;
  self->priv->icon_size = GTK_ICON_SIZE_INVALID;
  self->priv->pixel_size = -1;
  self->priv->last_rendered_state = GTK_STATE_FLAG_NORMAL;
  self->priv->orig_pixbuf_scale = 1;
}

static void
ensure_icon_size (GtkIconHelper *self,
                  GtkStyleContext *context,
		  gint *width_out,
		  gint *height_out)
{
  gint width, height;
  GtkSettings *settings;
  GdkScreen *screen;

  screen = gtk_style_context_get_screen (context);
  settings = gtk_settings_get_for_screen (screen);

  if (self->priv->pixel_size != -1)
    {
      width = height = self->priv->pixel_size;
    }
  else if (!gtk_icon_size_lookup_for_settings (settings,
					       self->priv->icon_size,
					       &width, &height))
    {
      if (self->priv->icon_size == GTK_ICON_SIZE_INVALID)
        {
          width = height = 0;
        }
      else
        {
          g_warning ("Invalid icon size %d\n", self->priv->icon_size);
          width = height = 24;
        }
    }

  *width_out = width;
  *height_out = height;
}

static GdkPixbuf *
ensure_stated_pixbuf_from_pixbuf (GtkIconHelper   *self,
				  GtkStyleContext *context,
				  GdkPixbuf       *pixbuf)
{
  GdkPixbuf *rendered;
  GtkIconSource *source;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  /* FIXME: use gtk_icon_info_load_icon? */

  source = gtk_icon_source_new ();
  gtk_icon_source_set_pixbuf (source, pixbuf);
  /* The size here is arbitrary; since size isn't
   * wildcarded in the source, it isn't supposed to be
   * scaled by the engine function
   */
  gtk_icon_source_set_size (source,
			    GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_icon_source_set_size_wildcarded (source, FALSE);

  rendered = gtk_render_icon_pixbuf (context, source, (GtkIconSize) -1);
  gtk_icon_source_free (source);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return rendered;
}

static GdkPixbuf *
ensure_stated_icon_from_info (GtkIconHelper *self,
                              GtkStyleContext *context,
			      GtkIconInfo *info)
{
  GdkPixbuf *destination = NULL;
  gboolean symbolic;

  symbolic = FALSE;

  if (info)
    destination =
      gtk_icon_info_load_symbolic_for_context (info,
					       context,
					       &symbolic,
					       NULL);

  if (destination == NULL)
    {
      GtkIconSet *icon_set;
      icon_set = gtk_style_context_lookup_icon_set (context, GTK_STOCK_MISSING_IMAGE);

      destination =
        gtk_icon_set_render_icon_pixbuf (icon_set, context, self->priv->icon_size);
    }
  else if (!symbolic)
    {
      GdkPixbuf *rendered;

	  rendered = ensure_stated_pixbuf_from_pixbuf (self, context, destination);	
      g_object_unref (destination);
      destination = rendered;
    }

  return destination;
}

static gboolean
check_invalidate_pixbuf (GtkIconHelper *self,
                         GtkStyleContext *context)
{
  GtkStateFlags state;

  state = gtk_style_context_get_state (context);

  if ((self->priv->rendered_pixbuf != NULL) &&
      (self->priv->last_rendered_state == state))
    return FALSE;

  self->priv->last_rendered_state = state;
  g_clear_object (&self->priv->rendered_pixbuf);

  return TRUE;
}

static GtkIconLookupFlags
get_icon_lookup_flags (GtkIconHelper *self)
{
  GtkIconLookupFlags flags = GTK_ICON_LOOKUP_USE_BUILTIN;

  if (self->priv->use_fallback)
    flags |= GTK_ICON_LOOKUP_GENERIC_FALLBACK;
  if (self->priv->pixel_size != -1)
    flags |= GTK_ICON_LOOKUP_FORCE_SIZE;

  return flags;
}

static void
ensure_pixbuf_for_icon_name_or_gicon (GtkIconHelper *self,
                                      GtkStyleContext *context)
{
  GtkIconTheme *icon_theme;
  gint width, height;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;

  if (!check_invalidate_pixbuf (self, context))
    return;

  icon_theme = gtk_icon_theme_get_default ();
  flags = get_icon_lookup_flags (self);

  ensure_icon_size (self, context, &width, &height);

  if (self->priv->storage_type == GTK_IMAGE_ICON_NAME &&
      self->priv->icon_name != NULL)
    {
      info = gtk_icon_theme_lookup_icon (icon_theme,
                                         self->priv->icon_name,
                                         MIN (width, height), flags);
    }
  else if (self->priv->storage_type == GTK_IMAGE_GICON &&
           self->priv->gicon != NULL)
    {
      info = gtk_icon_theme_lookup_by_gicon (icon_theme,
                                             self->priv->gicon,
                                             MIN (width, height), flags);
    }
  else
    {
      g_assert_not_reached ();
      return;
    }

  self->priv->rendered_pixbuf = ensure_stated_icon_from_info (self, context, info);

  if (info)
    gtk_icon_info_free (info);
}

static void
ensure_pixbuf_for_icon_set (GtkIconHelper *self,
                            GtkStyleContext *context,
                            GtkIconSet *icon_set)
{
  if (!check_invalidate_pixbuf (self, context))
    return;

  self->priv->rendered_pixbuf = 
    gtk_icon_set_render_icon_pixbuf (icon_set, context, self->priv->icon_size);
}

static void
get_surface_size (GtkIconHelper   *self,
		  GtkStyleContext *context,
		  cairo_surface_t *surface,
		  int *width,
		  int *height)
{
  double x_scale, y_scale;

  if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE)
    {
      x_scale = y_scale = 1;

#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
      cairo_surface_get_device_scale (surface, &x_scale, &y_scale);
#endif

      /* Assume any set scaling is icon scale */
      *width =
	ceil (cairo_image_surface_get_width (surface) / x_scale);
      *height =
	ceil (cairo_image_surface_get_height (surface) / y_scale);
    }
  else
    ensure_icon_size (self, context, width, height);
}

static void
ensure_pixbuf_from_surface (GtkIconHelper   *self,
			    GtkStyleContext *context)
{
  cairo_surface_t *surface;
  gint width, height;
  cairo_t *cr;


  if (!check_invalidate_pixbuf (self, context))
    return;

  if (self->priv->rendered_pixbuf)
    return;

  get_surface_size (self, context, self->priv->orig_surface, &width, &height);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					width, height);

  cr = cairo_create (surface);
  cairo_set_source_surface (cr, self->priv->orig_surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  self->priv->rendered_pixbuf =
    gdk_pixbuf_get_from_surface (surface, 0, 0, width, height);

  cairo_surface_destroy (surface);
}

static void
ensure_pixbuf_at_size (GtkIconHelper   *self,
                       GtkStyleContext *context)
{
  gint width, height;

  if (!check_invalidate_pixbuf (self, context))
    return;

  if (self->priv->rendered_pixbuf)
    return;

  if (self->priv->force_scale_pixbuf &&
      (self->priv->pixel_size != -1 ||
       self->priv->icon_size != GTK_ICON_SIZE_INVALID))
    {
      ensure_icon_size (self, context, &width, &height);

      if (self->priv->orig_pixbuf_scale > 1 ||
	  /* These should divide the orig_pixbuf size by scale, but need not 
	     due to the above scale > 1 check */
	  width < gdk_pixbuf_get_width (self->priv->orig_pixbuf) ||
          height < gdk_pixbuf_get_height (self->priv->orig_pixbuf))
	{
	  width = MIN (width, gdk_pixbuf_get_width (self->priv->orig_pixbuf) / self->priv->orig_pixbuf_scale);
	  height = MIN (height, gdk_pixbuf_get_height (self->priv->orig_pixbuf) / self->priv->orig_pixbuf_scale);
	  self->priv->rendered_pixbuf =
	    gdk_pixbuf_scale_simple (self->priv->orig_pixbuf,
				     width, height,
				     GDK_INTERP_BILINEAR);
	}
    }
  else if (self->priv->orig_pixbuf_scale > 1)
    {
	  width = gdk_pixbuf_get_width (self->priv->orig_pixbuf) / self->priv->orig_pixbuf_scale;
	  height =gdk_pixbuf_get_height (self->priv->orig_pixbuf) / self->priv->orig_pixbuf_scale;
	  self->priv->rendered_pixbuf =
	    gdk_pixbuf_scale_simple (self->priv->orig_pixbuf,
				     width, height,
				     GDK_INTERP_BILINEAR);
    }

  if (!self->priv->rendered_pixbuf)
    self->priv->rendered_pixbuf = g_object_ref (self->priv->orig_pixbuf);
}

GdkPixbuf *
_gtk_icon_helper_ensure_pixbuf (GtkIconHelper *self,
                                GtkStyleContext *context)
{
  GdkPixbuf *pixbuf = NULL;
  GtkIconSet *icon_set;

  switch (self->priv->storage_type)
    {
    case GTK_IMAGE_SURFACE:
      ensure_pixbuf_from_surface (self, context);
      break;

    case GTK_IMAGE_PIXBUF:
      ensure_pixbuf_at_size (self, context);
      break;

    case GTK_IMAGE_STOCK:
      icon_set = gtk_style_context_lookup_icon_set (context, self->priv->stock_id);
      if (icon_set != NULL)
	ensure_pixbuf_for_icon_set (self, context, icon_set);
      else
	pixbuf = NULL;
      break;

    case GTK_IMAGE_ICON_SET:
      icon_set = self->priv->icon_set;
      ensure_pixbuf_for_icon_set (self, context, icon_set);
      break;

    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      ensure_pixbuf_for_icon_name_or_gicon (self, context);
      break;

    case GTK_IMAGE_ANIMATION:
    case GTK_IMAGE_EMPTY:
    default:
      pixbuf = NULL;
      break;
    }

  if (pixbuf == NULL &&
      self->priv->rendered_pixbuf != NULL)
    pixbuf = g_object_ref (self->priv->rendered_pixbuf);

  return pixbuf;
}

static gint
get_scale_factor (GtkIconHelper *self,
		  GtkStyleContext *context)
{
  GdkScreen *screen;

  if (self->priv->window)
    return gdk_window_get_scale_factor (self->priv->window);

  screen = gtk_style_context_get_screen (context);

  /* else fall back to something that is more likely to be right than
   * just returning 1:
   */
  return gdk_screen_get_monitor_scale_factor (screen, 0);
}

static gboolean
check_invalidate_surface (GtkIconHelper *self,
			  GtkStyleContext *context)
{
  GtkStateFlags state;
  int scale;

  state = gtk_style_context_get_state (context);
  scale = get_scale_factor (self, context);

  if ((self->priv->rendered_surface != NULL) &&
      (self->priv->last_surface_state == state) &&
      (self->priv->last_surface_scale == scale))
    return FALSE;

  self->priv->last_surface_state = state;
  self->priv->last_surface_scale = scale;

  if (self->priv->rendered_surface)
    cairo_surface_destroy (self->priv->rendered_surface);
  self->priv->rendered_surface = NULL;

  return TRUE;
}

static void
ensure_surface_from_surface (GtkIconHelper   *self,
			     GtkStyleContext *context)
{
  if (!check_invalidate_surface (self, context))
    return;

  if (self->priv->rendered_surface)
    return;

  self->priv->rendered_surface =
    cairo_surface_reference (self->priv->orig_surface);

  get_surface_size (self, context, self->priv->orig_surface,
		    &self->priv->rendered_surface_width,
		    &self->priv->rendered_surface_height);
}

static void
ensure_surface_from_pixbuf (GtkIconHelper   *self,
			    GtkStyleContext *context)
{
  gint width, height;
  GdkPixbuf *pixbuf;
  int scale;

  if (!check_invalidate_surface (self, context))
    return;

  if (self->priv->rendered_surface)
    return;

  scale = get_scale_factor (self, context);

  if (self->priv->force_scale_pixbuf &&
      (self->priv->pixel_size != -1 ||
       self->priv->icon_size != GTK_ICON_SIZE_INVALID))
    {
      ensure_icon_size (self, context, &width, &height);

      if (scale != self->priv->orig_pixbuf_scale ||
	  width < gdk_pixbuf_get_width (self->priv->orig_pixbuf) / self->priv->orig_pixbuf_scale ||
          height < gdk_pixbuf_get_height (self->priv->orig_pixbuf) / self->priv->orig_pixbuf_scale)
	{
	  width = MIN (width * scale, gdk_pixbuf_get_width (self->priv->orig_pixbuf) * scale / self->priv->orig_pixbuf_scale);
	  height = MIN (height * scale, gdk_pixbuf_get_height (self->priv->orig_pixbuf) * scale / self->priv->orig_pixbuf_scale);

	  pixbuf = gdk_pixbuf_scale_simple (self->priv->orig_pixbuf,
					    width, height,
					    GDK_INTERP_BILINEAR);
	}
      else
	{
	  pixbuf = g_object_ref (self->priv->orig_pixbuf); 
	  scale = self->priv->orig_pixbuf_scale;
	}
    }
  else
    {
      pixbuf = g_object_ref (self->priv->orig_pixbuf);
      scale = self->priv->orig_pixbuf_scale;
    }

  self->priv->rendered_surface_width = (gdk_pixbuf_get_width (pixbuf) + scale - 1) / scale;
  self->priv->rendered_surface_height = (gdk_pixbuf_get_height (pixbuf) + scale - 1) / scale;

  self->priv->rendered_surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, self->priv->window);
  g_object_unref (pixbuf);
}

static void
ensure_surface_for_icon_set (GtkIconHelper *self,
			     GtkStyleContext *context,
			     GtkIconSet *icon_set)
{
  gint scale;

  if (!check_invalidate_surface (self, context))
    return;

  scale = get_scale_factor (self, context);

  self->priv->rendered_surface =
    gtk_icon_set_render_icon_surface (icon_set, context, 
				      self->priv->icon_size,
				      scale, self->priv->window);

  if (self->priv->rendered_surface)
    get_surface_size (self, context, self->priv->rendered_surface, 
		      &self->priv->rendered_surface_width, 
		      &self->priv->rendered_surface_height);
}

static void
ensure_stated_surface_from_info (GtkIconHelper *self,
				 GtkStyleContext *context,
				 GtkIconInfo *info,
				 int scale)
{
  GdkPixbuf *destination = NULL;
  cairo_surface_t *surface;
  gboolean symbolic;

  symbolic = FALSE;

  if (info)
    destination =
      gtk_icon_info_load_symbolic_for_context (info,
					       context,
					       &symbolic,
					       NULL);

  if (destination == NULL)
    {
      GtkIconSet *icon_set;
      icon_set = gtk_style_context_lookup_icon_set (context, GTK_STOCK_MISSING_IMAGE);

      destination =
        gtk_icon_set_render_icon_pixbuf (icon_set, context, self->priv->icon_size);
    }
  else if (!symbolic)
    {
      GdkPixbuf *rendered;

      rendered = ensure_stated_pixbuf_from_pixbuf (self, context, destination);
      g_object_unref (destination);
      destination = rendered;
    }

  surface = NULL;
  if (destination)
    {
      surface = gdk_cairo_surface_create_from_pixbuf (destination, scale, self->priv->window);

      self->priv->rendered_surface_width = 
	(gdk_pixbuf_get_width (destination) + scale - 1) / scale;
      self->priv->rendered_surface_height = 
	(gdk_pixbuf_get_height (destination) + scale - 1) / scale;
    }

  self->priv->rendered_surface = surface;
}

static void
ensure_surface_for_icon_name_or_gicon (GtkIconHelper *self,
				       GtkStyleContext *context)
{
  GtkIconTheme *icon_theme;
  gint width, height, scale;
  GtkIconInfo *info;
  GtkIconLookupFlags flags;

  if (!check_invalidate_surface (self, context))
    return;

  icon_theme = gtk_icon_theme_get_default ();
  flags = get_icon_lookup_flags (self);

  ensure_icon_size (self, context, &width, &height);
  scale = get_scale_factor (self, context);

  if (self->priv->storage_type == GTK_IMAGE_ICON_NAME &&
      self->priv->icon_name != NULL)
    {
      info = gtk_icon_theme_lookup_icon_for_scale (icon_theme,
						   self->priv->icon_name,
						   MIN (width, height),
						   scale, flags);
    }
  else if (self->priv->storage_type == GTK_IMAGE_GICON &&
           self->priv->gicon != NULL)
    {
      info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme,
						       self->priv->gicon,
						       MIN (width, height),
						       scale, flags);
    }
  else
    {
      g_assert_not_reached ();
      return;
    }

  ensure_stated_surface_from_info (self, context, info, scale);

  if (info)
    g_object_unref (info);
}

cairo_surface_t *
_gtk_icon_helper_ensure_surface (GtkIconHelper *self,
				 GtkStyleContext *context)
{
  cairo_surface_t *surface = NULL;
  GtkIconSet *icon_set;

  switch (self->priv->storage_type)
    {
    case GTK_IMAGE_SURFACE:
      ensure_surface_from_surface (self, context);
      break;

    case GTK_IMAGE_PIXBUF:
      ensure_surface_from_pixbuf (self, context);
      break;

    case GTK_IMAGE_STOCK:
      icon_set = gtk_style_context_lookup_icon_set (context, self->priv->stock_id);
      if (icon_set != NULL)
	ensure_surface_for_icon_set (self, context, icon_set);
      else
	surface = NULL;
      break;

    case GTK_IMAGE_ICON_SET:
      icon_set = self->priv->icon_set;
      ensure_surface_for_icon_set (self, context, icon_set);
      break;

    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      ensure_surface_for_icon_name_or_gicon (self, context);
      break;

    case GTK_IMAGE_ANIMATION:
    case GTK_IMAGE_EMPTY:
    default:
      surface = NULL;
      break;
    }

  if (surface == NULL &&
      self->priv->rendered_surface != NULL)
    surface = cairo_surface_reference (self->priv->rendered_surface);

  return surface;
}

void
_gtk_icon_helper_get_size (GtkIconHelper *self,
                           GtkStyleContext *context,
                           gint *width_out,
                           gint *height_out)
{
  cairo_surface_t *surface;
  gint width, height;

  width = height = 0;
  surface = _gtk_icon_helper_ensure_surface (self, context);

  if (surface != NULL)
    {
      width = self->priv->rendered_surface_width;
      height = self->priv->rendered_surface_height;
      cairo_surface_destroy (surface);
    }
  else if (self->priv->storage_type == GTK_IMAGE_ANIMATION)
    {
      width = gdk_pixbuf_animation_get_width (self->priv->animation);
      height = gdk_pixbuf_animation_get_height (self->priv->animation);
    }
  else if (self->priv->icon_size != -1)
    {
      ensure_icon_size (self, context, &width, &height);
    }

  if (width_out)
    *width_out = width;
  if (height_out)
    *height_out = height;
}

void 
_gtk_icon_helper_set_gicon (GtkIconHelper *self,
                            GIcon *gicon,
                            GtkIconSize icon_size)
{
  _gtk_icon_helper_clear (self);

  if (gicon != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_GICON;
      self->priv->gicon = g_object_ref (gicon);
      _gtk_icon_helper_set_icon_size (self, icon_size);
    }
}

void 
_gtk_icon_helper_set_icon_name (GtkIconHelper *self,
                                const gchar *icon_name,
                                GtkIconSize icon_size)
{
  _gtk_icon_helper_clear (self);

  if (icon_name != NULL &&
      icon_name[0] != '\0')
    {
      self->priv->storage_type = GTK_IMAGE_ICON_NAME;
      self->priv->icon_name = g_strdup (icon_name);
      _gtk_icon_helper_set_icon_size (self, icon_size);
    }
}

void 
_gtk_icon_helper_set_icon_set (GtkIconHelper *self,
                               GtkIconSet *icon_set,
                               GtkIconSize icon_size)
{
  _gtk_icon_helper_clear (self);

  if (icon_set != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_ICON_SET;
      self->priv->icon_set = gtk_icon_set_ref (icon_set);
      _gtk_icon_helper_set_icon_size (self, icon_size);
    }
}

void 
_gtk_icon_helper_set_pixbuf (GtkIconHelper *self,
                             GdkPixbuf *pixbuf)
{
  _gtk_icon_helper_clear (self);

  if (pixbuf != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_PIXBUF;
      self->priv->orig_pixbuf = g_object_ref (pixbuf);
    }
}

void 
_gtk_icon_helper_set_animation (GtkIconHelper *self,
                                GdkPixbufAnimation *animation)
{
  _gtk_icon_helper_clear (self);

  if (animation != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_ANIMATION;
      self->priv->animation = g_object_ref (animation);
    }
}

void 
_gtk_icon_helper_set_surface (GtkIconHelper *self,
			      cairo_surface_t *surface)
{
  _gtk_icon_helper_clear (self);

  if (surface != NULL)
    {
      self->priv->storage_type = GTK_IMAGE_SURFACE;
      self->priv->orig_surface = cairo_surface_reference (surface);
    }
}

void 
_gtk_icon_helper_set_stock_id (GtkIconHelper *self,
                               const gchar *stock_id,
                               GtkIconSize icon_size)
{
  _gtk_icon_helper_clear (self);

  if (stock_id != NULL &&
      stock_id[0] != '\0')
    {
      self->priv->storage_type = GTK_IMAGE_STOCK;
      self->priv->stock_id = g_strdup (stock_id);
      _gtk_icon_helper_set_icon_size (self, icon_size);
    }
}

void 
_gtk_icon_helper_set_icon_size (GtkIconHelper *self,
                                GtkIconSize icon_size)
{
  if (self->priv->icon_size != icon_size)
    {
      self->priv->icon_size = icon_size;
      _gtk_icon_helper_invalidate (self);
    }
}

void 
_gtk_icon_helper_set_pixel_size (GtkIconHelper *self,
                                 gint pixel_size)
{
  if (self->priv->pixel_size != pixel_size)
    {
      self->priv->pixel_size = pixel_size;
      _gtk_icon_helper_invalidate (self);
    }
}

void 
_gtk_icon_helper_set_use_fallback (GtkIconHelper *self,
                                   gboolean use_fallback)
{
  if (self->priv->use_fallback != use_fallback)
    {
      self->priv->use_fallback = use_fallback;
      _gtk_icon_helper_invalidate (self);
    }
}

GtkImageType
_gtk_icon_helper_get_storage_type (GtkIconHelper *self)
{
  return self->priv->storage_type;
}

gboolean
_gtk_icon_helper_get_use_fallback (GtkIconHelper *self)
{
  return self->priv->use_fallback;
}

GtkIconSize
_gtk_icon_helper_get_icon_size (GtkIconHelper *self)
{
  return self->priv->icon_size;
}

gint
_gtk_icon_helper_get_pixel_size (GtkIconHelper *self)
{
  return self->priv->pixel_size;
}

GdkPixbuf *
_gtk_icon_helper_peek_pixbuf (GtkIconHelper *self)
{
  return self->priv->orig_pixbuf;
}

GIcon *
_gtk_icon_helper_peek_gicon (GtkIconHelper *self)
{
  return self->priv->gicon;
}

GdkPixbufAnimation *
_gtk_icon_helper_peek_animation (GtkIconHelper *self)
{
  return self->priv->animation;
}

GtkIconSet *
_gtk_icon_helper_peek_icon_set (GtkIconHelper *self)
{
  return self->priv->icon_set;
}

cairo_surface_t *
_gtk_icon_helper_peek_surface (GtkIconHelper *self)
{
  return self->priv->orig_surface;
}

const gchar *
_gtk_icon_helper_get_stock_id (GtkIconHelper *self)
{
  return self->priv->stock_id;
}

const gchar *
_gtk_icon_helper_get_icon_name (GtkIconHelper *self)
{
  return self->priv->icon_name;
}

GtkIconHelper *
_gtk_icon_helper_new (void)
{
  return g_object_new (GTK_TYPE_ICON_HELPER, NULL);
}

void
_gtk_icon_helper_draw (GtkIconHelper *self,
                       GtkStyleContext *context,
                       cairo_t *cr,
                       gdouble x,
                       gdouble y)
{
  cairo_surface_t *surface;

  surface = _gtk_icon_helper_ensure_surface (self, context);
  if (surface != NULL)
    {
      gtk_render_icon_surface (context, cr, surface, x, y);
      cairo_surface_destroy (surface);
    }
}

gboolean
_gtk_icon_helper_get_is_empty (GtkIconHelper *self)
{
  return (self->priv->storage_type == GTK_IMAGE_EMPTY);
}

gboolean
_gtk_icon_helper_get_force_scale_pixbuf (GtkIconHelper *self)
{
  return self->priv->force_scale_pixbuf;
}

void
_gtk_icon_helper_set_force_scale_pixbuf (GtkIconHelper *self,
                                         gboolean       force_scale)
{
  if (self->priv->force_scale_pixbuf != force_scale)
    {
      self->priv->force_scale_pixbuf = force_scale;
      _gtk_icon_helper_invalidate (self);
    }
}

void 
_gtk_icon_helper_set_pixbuf_scale (GtkIconHelper *self,
				   int scale)
{
  if (self->priv->orig_pixbuf_scale != scale)
    {
      self->priv->orig_pixbuf_scale = scale;
      _gtk_icon_helper_invalidate (self);
    }
}

int
_gtk_icon_helper_get_pixbuf_scale (GtkIconHelper *self)
{
  return self->priv->orig_pixbuf_scale;
}
