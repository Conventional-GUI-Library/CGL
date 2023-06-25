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

/**
 * SECTION:gtkbin
 * @Short_description: A container with just one child
 * @Title: GtkBin
 *
 * The #GtkBin widget is a container with just one child.
 * It is not very useful itself, but it is useful for deriving subclasses,
 * since it provides common code needed for handling a single child widget.
 *
 * Many GTK+ widgets are subclasses of #GtkBin, including #GtkWindow,
 * #GtkButton, #GtkFrame, #GtkHandleBox or #GtkScrolledWindow.
 */

#include "config.h"
#include "gtkbin.h"
#include "gtksizerequest.h"
#include "gtkintl.h"


struct _GtkBinPrivate
{
  GtkWidget *child;
};

static void gtk_bin_add         (GtkContainer   *container,
			         GtkWidget      *widget);
static void gtk_bin_remove      (GtkContainer   *container,
			         GtkWidget      *widget);
static void gtk_bin_forall      (GtkContainer   *container,
				 gboolean	include_internals,
				 GtkCallback     callback,
				 gpointer        callback_data);
static GType gtk_bin_child_type (GtkContainer   *container);


static void               gtk_bin_get_preferred_width_for_height  (GtkWidget           *widget,
                                                                   gint                 height,
                                                                   gint                *minimum_width,
                                                                   gint                *natural_width);
static void               gtk_bin_get_preferred_height_for_width  (GtkWidget           *widget,
                                                                   gint                 width,
                                                                   gint                *minimum_height,
                                                                   gint                *natural_height);

G_DEFINE_ABSTRACT_TYPE (GtkBin, gtk_bin, GTK_TYPE_CONTAINER)

static void
gtk_bin_class_init (GtkBinClass *class)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass*) class;
  GtkContainerClass *container_class = (GtkContainerClass*) class;

  widget_class->get_preferred_width_for_height = gtk_bin_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_bin_get_preferred_height_for_width;

  container_class->add = gtk_bin_add;
  container_class->remove = gtk_bin_remove;
  container_class->forall = gtk_bin_forall;
  container_class->child_type = gtk_bin_child_type;

  g_type_class_add_private (class, sizeof (GtkBinPrivate));
}

static void
gtk_bin_init (GtkBin *bin)
{
  GtkBinPrivate *priv;

  bin->priv = G_TYPE_INSTANCE_GET_PRIVATE (bin,
                                           GTK_TYPE_BIN,
                                           GtkBinPrivate);
  priv = bin->priv;

  gtk_widget_set_has_window (GTK_WIDGET (bin), FALSE);

  priv->child = NULL;
}


static GType
gtk_bin_child_type (GtkContainer *container)
{
  GtkBinPrivate *priv = GTK_BIN (container)->priv;

  if (!priv->child)
    return GTK_TYPE_WIDGET;
  else
    return G_TYPE_NONE;
}

static void
gtk_bin_add (GtkContainer *container,
	     GtkWidget    *child)
{
  GtkBin *bin = GTK_BIN (container);
  GtkBinPrivate *priv = bin->priv;

  if (priv->child != NULL)
    {
      g_warning ("Attempting to add a widget with type %s to a %s, "
                 "but as a GtkBin subclass a %s can only contain one widget at a time; "
                 "it already contains a widget of type %s",
                 g_type_name (G_OBJECT_TYPE (child)),
                 g_type_name (G_OBJECT_TYPE (bin)),
                 g_type_name (G_OBJECT_TYPE (bin)),
                 g_type_name (G_OBJECT_TYPE (priv->child)));
      return;
    }

  gtk_widget_set_parent (child, GTK_WIDGET (bin));
  priv->child = child;
}

static void
gtk_bin_remove (GtkContainer *container,
		GtkWidget    *child)
{
  GtkBin *bin = GTK_BIN (container);
  GtkBinPrivate *priv = bin->priv;
  gboolean widget_was_visible;

  g_return_if_fail (priv->child == child);

  widget_was_visible = gtk_widget_get_visible (child);
  
  gtk_widget_unparent (child);
  priv->child = NULL;
  
  /* queue resize regardless of gtk_widget_get_visible (container),
   * since that's what is needed by toplevels, which derive from GtkBin.
   */
  if (widget_was_visible)
    gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
gtk_bin_forall (GtkContainer *container,
		gboolean      include_internals,
		GtkCallback   callback,
		gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  GtkBinPrivate *priv = bin->priv;

  if (priv->child)
    (* callback) (priv->child, callback_data);
}


/* GtkBin widgets define the padding and borders independantly so
 * we cannot provide a generic get_size() for the same reason
 * we never implemented size_request() here.
 *
 * But for cases where the GtkBin class's padding is constant and
 * does not vary based on allocation (most cases), we can at least 
 * deduce a common code path for the get_width_for_height()/get_height_for_width()
 * cases by using the delta of the base size requsts.
 */
static void
get_child_padding_delta (GtkBin *bin,
			 gint   *delta_h,
			 gint   *delta_v)
{
  GtkBinPrivate *priv = bin->priv;
  gint hmin, vmin, hnat, vnat, child_hmin, child_vmin;

  /* we can't use gtk_widget_get_preferred_width() wrapper 
   * because we want our "original" request, not any external
   * adjustments from set_size_request() or whatever.  we have
   * to ask for natural also because NULL isn't allowed for the
   * direct vfuncs
   */
  GTK_WIDGET_GET_CLASS (bin)->get_preferred_width (GTK_WIDGET (bin), &hmin, &hnat);
  GTK_WIDGET_GET_CLASS (bin)->adjust_size_request (GTK_WIDGET (bin), 
						   GTK_ORIENTATION_HORIZONTAL, &hmin, &hnat);

  GTK_WIDGET_GET_CLASS (bin)->get_preferred_height (GTK_WIDGET (bin), &vmin, &vnat);
  GTK_WIDGET_GET_CLASS (bin)->adjust_size_request (GTK_WIDGET (bin), 
						   GTK_ORIENTATION_VERTICAL, &vmin, &vnat);

  gtk_widget_get_preferred_width (priv->child, &child_hmin, NULL);
  gtk_widget_get_preferred_height (priv->child, &child_vmin, NULL);

  *delta_h = hmin - child_hmin;
  *delta_v = vmin - child_vmin;
}

static void 
gtk_bin_get_preferred_width_for_height (GtkWidget *widget,
                                        gint       height,
                                        gint      *minimum_width,
                                        gint      *natural_width)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkBinPrivate *priv = bin->priv;
  gint    hdelta, vdelta, child_min, child_nat;

  if (priv->child)
    {
      get_child_padding_delta (bin, &hdelta, &vdelta);

      gtk_widget_get_preferred_width_for_height (priv->child,
                                                 height - vdelta,
                                                 &child_min, &child_nat);

      if (minimum_width)
	*minimum_width = child_min + hdelta;
      
      if (natural_width)
	*natural_width = child_nat + hdelta;
    }
  else
    GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_width, natural_width);
}

static void
gtk_bin_get_preferred_height_for_width  (GtkWidget *widget,
                                         gint       width,
                                         gint      *minimum_height,
                                         gint      *natural_height)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkBinPrivate *priv = bin->priv;
  gint    hdelta, vdelta, child_min, child_nat;

  if (priv->child)
    {
      get_child_padding_delta (bin, &hdelta, &vdelta);

      gtk_widget_get_preferred_height_for_width (priv->child,
                                                 width - hdelta,
                                                 &child_min, &child_nat);

      if (minimum_height)
	*minimum_height = child_min + vdelta;
      
      if (natural_height)
	*natural_height = child_nat + vdelta;
    }
  else
    GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, minimum_height, natural_height);
}


/**
 * gtk_bin_get_child:
 * @bin: a #GtkBin
 * 
 * Gets the child of the #GtkBin, or %NULL if the bin contains
 * no child widget. The returned widget does not have a reference
 * added, so you do not need to unref it.
 *
 * Return value: (transfer none): pointer to child of the #GtkBin
 **/
GtkWidget*
gtk_bin_get_child (GtkBin *bin)
{
  g_return_val_if_fail (GTK_IS_BIN (bin), NULL);

  return bin->priv->child;
}

void
_gtk_bin_set_child (GtkBin    *bin,
                    GtkWidget *widget)
{
  bin->priv->child = widget;
}
