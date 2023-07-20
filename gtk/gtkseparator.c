/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkorientableprivate.h"
#include "gtkseparator.h"
#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkseparator
 * @Short_description: A separator widget
 * @Title: GtkSeparator
 *
 * GtkSeparator is a horizontal or vertical separator widget, depending on the 
 * value of the #GtkOrientable:orientation property, used to group the widgets within a
 * window. It displays a line with a shadow to make it appear sunken into the 
 * interface.
 */


struct _GtkSeparatorPrivate
{
  GtkOrientation orientation;
};


enum {
  PROP_0,
  PROP_ORIENTATION
};

static void       gtk_separator_set_property (GObject        *object,
                                              guint           prop_id,
                                              const GValue   *value,
                                              GParamSpec     *pspec);
static void       gtk_separator_get_property (GObject        *object,
                                              guint           prop_id,
                                              GValue         *value,
                                              GParamSpec     *pspec);
static void       gtk_separator_get_preferred_width
                                             (GtkWidget      *widget,
                                              gint           *minimum,
                                              gint           *natural);
static void       gtk_separator_get_preferred_height
                                             (GtkWidget      *widget,
                                              gint           *minimum,
                                              gint           *natural);
static gboolean   gtk_separator_draw         (GtkWidget      *widget,
                                              cairo_t        *cr);


G_DEFINE_TYPE_WITH_CODE (GtkSeparator, gtk_separator, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE,
                                                NULL))


static void
gtk_separator_class_init (GtkSeparatorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->set_property = gtk_separator_set_property;
  object_class->get_property = gtk_separator_get_property;

  widget_class->get_preferred_width = gtk_separator_get_preferred_width;
  widget_class->get_preferred_height = gtk_separator_get_preferred_height;

  widget_class->draw = gtk_separator_draw;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_SEPARATOR);

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  g_type_class_add_private (object_class, sizeof (GtkSeparatorPrivate));
}

static void
gtk_separator_init (GtkSeparator *separator)
{
  GtkSeparatorPrivate *private;
  GtkStyleContext *context;

  separator->priv = G_TYPE_INSTANCE_GET_PRIVATE (separator,
                                                 GTK_TYPE_SEPARATOR,
                                                 GtkSeparatorPrivate);
  private = separator->priv;

  gtk_widget_set_has_window (GTK_WIDGET (separator), FALSE);

  private->orientation = GTK_ORIENTATION_HORIZONTAL;

  context = gtk_widget_get_style_context (GTK_WIDGET (separator));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_SEPARATOR);
}

static void
gtk_separator_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkSeparator *separator = GTK_SEPARATOR (object);
  GtkSeparatorPrivate *private = separator->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      private->orientation = g_value_get_enum (value);
      _gtk_orientable_set_style_classes (GTK_ORIENTABLE (object));
      gtk_widget_queue_resize (GTK_WIDGET (object));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkSeparator *separator = GTK_SEPARATOR (object);
  GtkSeparatorPrivate *private = separator->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, private->orientation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_separator_get_preferred_size (GtkWidget      *widget,
                                  GtkOrientation  orientation,
                                  gint           *minimum,
                                  gint           *natural)
{
  GtkSeparator *separator = GTK_SEPARATOR (widget);
  GtkSeparatorPrivate *private = separator->priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder border;
  gboolean wide_sep;
  gint     sep_width;
  gint     sep_height;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_border (context, state, &border);

  gtk_widget_style_get (widget,
                        "wide-separators",  &wide_sep,
                        "separator-width",  &sep_width,
                        "separator-height", &sep_height,
                        NULL);

  if (orientation == private->orientation)
    {
      *minimum = *natural = 1;
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      *minimum = *natural = wide_sep ? sep_height : border.top;
    }
  else
    {
      *minimum = *natural = wide_sep ? sep_width : border.left;
    }
}

static void
gtk_separator_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
  gtk_separator_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum, natural);
}

static void
gtk_separator_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
  gtk_separator_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, minimum, natural);
}

static gboolean
gtk_separator_draw (GtkWidget *widget,
                    cairo_t   *cr)
{
  GtkSeparator *separator = GTK_SEPARATOR (widget);
  GtkSeparatorPrivate *private = separator->priv;
  GtkStateFlags state;
  GtkStyleContext *context;
  GtkBorder padding;
  gboolean wide_separators;
  gint separator_width;
  gint separator_height;
  int width, height;

  context = gtk_widget_get_style_context (widget);
  gtk_widget_style_get (widget,
                        "wide-separators",  &wide_separators,
                        "separator-width",  &separator_width,
                        "separator-height", &separator_height,
                        NULL);

  state = gtk_widget_get_state_flags (widget);
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_style_context_get_padding (context, state, &padding);

  if (private->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (wide_separators)
        gtk_render_frame (context, cr,
                          0, (height - separator_height) / 2,
                          width, separator_height);
      else
        gtk_render_line (context, cr,
                         0, (height - padding.top) / 2,
                         width - 1, (height - padding.top) / 2);
    }
  else
    {
      if (wide_separators)
        gtk_render_frame (context, cr,
                          (width - separator_width) / 2, 0,
                          separator_width, height);
      else
        gtk_render_line (context, cr,
                         (width - padding.left) / 2, 0,
                         (width - padding.left) / 2, height - 1);
    }

  return FALSE;
}

/**
 * gtk_separator_new:
 * @orientation: the separator's orientation.
 *
 * Creates a new #GtkSeparator with the given orientation.
 *
 * Return value: a new #GtkSeparator.
 *
 * Since: 3.0
 */
GtkWidget *
gtk_separator_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_SEPARATOR,
                       "orientation", orientation,
                       NULL);
}
