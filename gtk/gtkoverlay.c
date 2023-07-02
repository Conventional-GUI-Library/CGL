/*
 * gtkoverlay.c
 * This file is part of gtk
 *
 * Copyright (C) 2011 - Ignacio Casal Quinteiro, Mike Krüger
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

#include "gtkoverlay.h"
#include "gtkbuildable.h"
#include "gtkscrolledwindow.h"
#include "gtkmarshalers.h"

#include "gtkprivate.h"
#include "gtkintl.h"

/**
 * SECTION:gtkoverlay
 * @short_description: A container which overlays widgets on top of each other
 * @title: GtkOverlay
 *
 * GtkOverlay is a container which contains a single main child, on top
 * of which it can place <firstterm>overlay</firstterm> widgets. The
 * position of each overlay widget is determined by its #GtkWidget::halign
 * and #GtkWidget::valign properties. E.g. a widget with both alignments
 * set to %GTK_ALIGN_START will be placed at the top left corner of the
 * main widget, whereas an overlay with halign set to %GTK_ALIGN_CENTER
 * and valign set to %GTK_ALIGN_END will be placed a the bottom edge of
 * the main widget, horizontally centered. The position can be adjusted
 * by setting the margin properties of the child to non-zero values.
 *
 * More complicated placement of overlays is possible by connecting
 * to the #GtkOverlay::get-child-position signal.
 *
 * <refsect2 id="GtkOverlay-BUILDER-UI">
 * <title>GtkOverlay as GtkBuildable</title>
 * <para>
 * The GtkOverlay implementation of the GtkBuildable interface
 * supports placing a child as an overlay by specifying "overlay" as
 * the "type" attribute of a <tag class="starttag">child</tag> element.
 * </para>
 * </refsect2>
 */

struct _GtkOverlayPrivate
{
  GSList *children;
};

typedef struct _GtkOverlayChild GtkOverlayChild;

struct _GtkOverlayChild
{
  GtkWidget *widget;
  GdkWindow *window;
};

enum {
  GET_CHILD_POSITION,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void gtk_overlay_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkOverlay, gtk_overlay, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_overlay_buildable_init))

static GdkWindow *
gtk_overlay_create_child_window (GtkOverlay *overlay,
                                 GtkWidget  *child)
{
  GtkWidget *widget = GTK_WIDGET (overlay);
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_get_allocation (child, &allocation);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes_mask = GDK_WA_X | GDK_WA_Y;
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

  window = gdk_window_new (gtk_widget_get_window (widget),
                           &attributes, attributes_mask);
  gdk_window_set_user_data (window, overlay);
  gtk_style_context_set_background (gtk_widget_get_style_context (widget), window);

  gtk_widget_set_parent_window (child, window);

  return window;
}

static void
gtk_overlay_child_allocate (GtkOverlay      *overlay,
                            GtkOverlayChild *child)
{
  gint left, right, top, bottom;
  GtkAllocation allocation, child_allocation, overlay_allocation;
  gboolean result;

  if (gtk_widget_get_mapped (GTK_WIDGET (overlay)))
    {
      if (gtk_widget_get_visible (child->widget))
        gdk_window_show (child->window);
      else if (gdk_window_is_visible (child->window))
        gdk_window_hide (child->window);
    }

  if (!gtk_widget_get_visible (child->widget))
    return;

  g_signal_emit (overlay, signals[GET_CHILD_POSITION],
                 0, child->widget, &allocation, &result);

  gtk_widget_get_allocation (GTK_WIDGET (overlay), &overlay_allocation);

  allocation.x += overlay_allocation.x;
  allocation.y += overlay_allocation.y;

  /* put the margins outside the window; also arrange things
   * so that the adjusted child allocation still ends up at 0, 0
   */
  left = gtk_widget_get_margin_left (child->widget);
  right = gtk_widget_get_margin_right (child->widget);
  top = gtk_widget_get_margin_top (child->widget);
  bottom = gtk_widget_get_margin_bottom (child->widget);

  child_allocation.x = - left;
  child_allocation.y = - top;
  child_allocation.width = allocation.width;
  child_allocation.height = allocation.height;

  allocation.x += left;
  allocation.y += top;
  allocation.width -= left + right;
  allocation.height -= top + bottom;

  if (child->window)
    gdk_window_move_resize (child->window,
                            allocation.x, allocation.y,
                            allocation.width, allocation.height);

  gtk_widget_size_allocate (child->widget, &child_allocation);
}

static void
gtk_overlay_get_preferred_width (GtkWidget *widget,
                                 gint      *minimum,
                                 gint      *natural)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;

  if (minimum)
    *minimum = 0;

  if (natural)
    *natural = 0;

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    gtk_widget_get_preferred_width (child, minimum, natural);
}

static void
gtk_overlay_get_preferred_height (GtkWidget *widget,
                                  gint      *minimum,
                                  gint      *natural)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;

  if (minimum)
    *minimum = 0;

  if (natural)
    *natural = 0;

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    gtk_widget_get_preferred_height (child, minimum, natural);
}

static void
gtk_overlay_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkOverlay *overlay = GTK_OVERLAY (widget);
  GtkOverlayPrivate *priv = overlay->priv;
  GSList *children;
  GtkWidget *main_widget;

  GTK_WIDGET_CLASS (gtk_overlay_parent_class)->size_allocate (widget, allocation);

  main_widget = gtk_bin_get_child (GTK_BIN (overlay));
  if (!main_widget || !gtk_widget_get_visible (main_widget))
    return;

  gtk_widget_size_allocate (main_widget, allocation);

  for (children = priv->children; children; children = children->next)
    {
      gtk_overlay_child_allocate (overlay, children->data);
    }
}

static GtkAlign
effective_align (GtkAlign         align,
                 GtkTextDirection direction)
{
  switch (align)
    {
    case GTK_ALIGN_START:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_END : GTK_ALIGN_START;
    case GTK_ALIGN_END:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_START : GTK_ALIGN_END;
    default:
      return align;
    }
}

static gboolean
gtk_overlay_get_child_position (GtkOverlay    *overlay,
                                GtkWidget     *widget,
                                GtkAllocation *alloc)
{
  GtkWidget *main_widget;
  GtkAllocation main_alloc;
  GtkRequisition req;
  GtkAlign halign;
  GtkTextDirection direction;

  main_widget = gtk_bin_get_child (GTK_BIN (overlay));

  /* special-case scrolled windows */
  if (GTK_IS_SCROLLED_WINDOW (main_widget))
    {
      GtkWidget *grandchild;
      gint x, y;

      grandchild = gtk_bin_get_child (GTK_BIN (main_widget));
      gtk_widget_translate_coordinates (grandchild, main_widget, 0, 0, &x, &y);

      main_alloc.x = x;
      main_alloc.y = y;
      main_alloc.width = gtk_widget_get_allocated_width (grandchild);
      main_alloc.height = gtk_widget_get_allocated_height (grandchild);
    }
  else
    {
      main_alloc.x = 0;
      main_alloc.y = 0;
      main_alloc.width = gtk_widget_get_allocated_width (main_widget);
      main_alloc.height = gtk_widget_get_allocated_height (main_widget);
    }

  gtk_widget_get_preferred_size (widget, NULL, &req);

  alloc->x = main_alloc.x;
  alloc->width = MIN (main_alloc.width, req.width);

  direction = gtk_widget_get_direction (widget);

  halign = gtk_widget_get_halign (widget);
  switch (effective_align (halign, direction))
    {
    case GTK_ALIGN_START:
      /* nothing to do */
      break;
    case GTK_ALIGN_FILL:
      alloc->width = main_alloc.width;
      break;
    case GTK_ALIGN_CENTER:
      alloc->x += main_alloc.width / 2 - req.width / 2;
      break;
    case GTK_ALIGN_END:
      alloc->x += main_alloc.width - req.width;
      break;
    }

  alloc->y = main_alloc.y;
  alloc->height = MIN (main_alloc.height, req.height);

  switch (gtk_widget_get_valign (widget))
    {
    case GTK_ALIGN_START:
      /* nothing to do */
      break;
    case GTK_ALIGN_FILL:
      alloc->height = main_alloc.height;
      break;
    case GTK_ALIGN_CENTER:
      alloc->y += main_alloc.height / 2 - req.height / 2;
      break;
    case GTK_ALIGN_END:
      alloc->y += main_alloc.height - req.height;
      break;
    }

  return TRUE;
}

static void
gtk_overlay_realize (GtkWidget *widget)
{
  GtkOverlay *overlay = GTK_OVERLAY (widget);
  GtkOverlayPrivate *priv = overlay->priv;
  GtkOverlayChild *child;
  GSList *children;

  GTK_WIDGET_CLASS (gtk_overlay_parent_class)->realize (widget);

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (child->window == NULL)
        child->window = gtk_overlay_create_child_window (overlay, child->widget);
    }
}

static void
gtk_overlay_unrealize (GtkWidget *widget)
{
  GtkOverlay *overlay = GTK_OVERLAY (widget);
  GtkOverlayPrivate *priv = overlay->priv;
  GtkOverlayChild *child;
  GSList *children;

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      gtk_widget_set_parent_window (child->widget, NULL);
      gdk_window_set_user_data (child->window, NULL);
      gdk_window_destroy (child->window);
      child->window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_overlay_parent_class)->unrealize (widget);
}

static void
gtk_overlay_map (GtkWidget *widget)
{
  GtkOverlay *overlay = GTK_OVERLAY (widget);
  GtkOverlayPrivate *priv = overlay->priv;
  GtkOverlayChild *child;
  GSList *children;

  GTK_WIDGET_CLASS (gtk_overlay_parent_class)->map (widget);

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (child->window != NULL &&
          gtk_widget_get_visible (child->widget) &&
          gtk_widget_get_child_visible (child->widget))
        gdk_window_show (child->window);
    }
}

static void
gtk_overlay_unmap (GtkWidget *widget)
{
  GtkOverlay *overlay = GTK_OVERLAY (widget);
  GtkOverlayPrivate *priv = overlay->priv;
  GtkOverlayChild *child;
  GSList *children;

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (child->window != NULL &&
          gdk_window_is_visible (child->window))
        gdk_window_hide (child->window);
    }

  GTK_WIDGET_CLASS (gtk_overlay_parent_class)->unmap (widget);
}

static gboolean
gtk_overlay_draw (GtkWidget *widget,
                  cairo_t   *cr)
{
  GtkOverlay *overlay = GTK_OVERLAY (widget);
  GtkOverlayPrivate *priv = overlay->priv;
  GtkOverlayChild *child;
  GSList *children;

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (gtk_cairo_should_draw_window (cr, child->window))
        {
          cairo_save (cr);
          gtk_cairo_transform_to_window (cr, widget, child->window);
          gtk_render_background (gtk_widget_get_style_context (widget),
                                 cr,
                                 0, 0,
                                 gdk_window_get_width (child->window),
                                 gdk_window_get_height (child->window));
          cairo_restore (cr);
        }
    }

  GTK_WIDGET_CLASS (gtk_overlay_parent_class)->draw (widget, cr);

  return FALSE;
}

static void
gtk_overlay_remove (GtkContainer *container,
                    GtkWidget    *widget)
{
  GtkOverlayPrivate *priv = GTK_OVERLAY (container)->priv;
  GtkOverlayChild *child;
  GSList *children;

  for (children = priv->children; children; children = children->next)
    {
      child = children->data;

      if (child->widget == widget)
        {
          if (child->window != NULL)
            {
              gdk_window_set_user_data (child->window, NULL);
              gdk_window_destroy (child->window);
            }

          gtk_widget_unparent (widget);

          priv->children = g_slist_delete_link (priv->children, children);
          g_slice_free (GtkOverlayChild, child);

          return;
        }
    }

  GTK_CONTAINER_CLASS (gtk_overlay_parent_class)->remove (container, widget);
}

static void
gtk_overlay_forall (GtkContainer *overlay,
                    gboolean      include_internals,
                    GtkCallback   callback,
                    gpointer      callback_data)
{
  GtkOverlayPrivate *priv = GTK_OVERLAY (overlay)->priv;
  GtkOverlayChild *child;
  GSList *children;
  GtkWidget *main_widget;

  main_widget = gtk_bin_get_child (GTK_BIN (overlay));
  if (main_widget)
    (* callback) (main_widget, callback_data);

  children = priv->children;
  while (children)
    {
      child = children->data;
      children = children->next;

      (* callback) (child->widget, callback_data);
    }
}

static void
gtk_overlay_class_init (GtkOverlayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  widget_class->get_preferred_width = gtk_overlay_get_preferred_width;
  widget_class->get_preferred_height = gtk_overlay_get_preferred_height;
  widget_class->size_allocate = gtk_overlay_size_allocate;
  widget_class->realize = gtk_overlay_realize;
  widget_class->unrealize = gtk_overlay_unrealize;
  widget_class->map = gtk_overlay_map;
  widget_class->unmap = gtk_overlay_unmap;
  widget_class->draw = gtk_overlay_draw;

  container_class->remove = gtk_overlay_remove;
  container_class->forall = gtk_overlay_forall;

  klass->get_child_position = gtk_overlay_get_child_position;

  /**
   * GtkOverlay::get-child-position:
   * @overlay: the #GtkOverlay
   * @widget: the child widget to position
   * @allocation: (out): return location for the allocation
   *
   * The ::get-child-position signal is emitted to determine
   * the position and size of any overlay child widgets. A
   * handler for this signal should fill @allocation with
   * the desired position and size for @widget, relative to
   * the 'main' child of @overlay.
   *
   * The default handler for this signal uses the @widget's
   * halign and valign properties to determine the position
   * and gives the widget its natural size (except that an
   * alignment of %GTK_ALIGN_FILL will cause the overlay to
   * be full-width/height). If the main child is a
   * #GtkScrolledWindow, the overlays are placed relative
   * to its contents.
   *
   * Return: %TRUE if the @allocation has been filled
   */
  signals[GET_CHILD_POSITION] =
    g_signal_new (I_("get-child-position"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkOverlayClass, get_child_position),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__OBJECT_BOXED,
                  G_TYPE_BOOLEAN, 2,
                  GTK_TYPE_WIDGET,
                  GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);

  g_type_class_add_private (object_class, sizeof (GtkOverlayPrivate));
}

static void
gtk_overlay_init (GtkOverlay *overlay)
{
  overlay->priv = G_TYPE_INSTANCE_GET_PRIVATE (overlay, GTK_TYPE_OVERLAY, GtkOverlayPrivate);

  gtk_widget_set_has_window (GTK_WIDGET (overlay), FALSE);
}

static void
gtk_overlay_buildable_add_child (GtkBuildable *buildable,
                                 GtkBuilder   *builder,
                                 GObject      *child,
                                 const gchar  *type)
{
  if (type && strcmp (type, "overlay") == 0)
    gtk_overlay_add_overlay (GTK_OVERLAY (buildable), GTK_WIDGET (child));
  else if (!type)
    gtk_container_add (GTK_CONTAINER (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
}

static void
gtk_overlay_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_overlay_buildable_add_child;
}

/**
 * gtk_overlay_new:
 *
 * Creates a new #GtkOverlay.
 *
 * Returns: a new #GtkOverlay object.
 *
 * Since: 3.2
 */
GtkWidget *
gtk_overlay_new (void)
{
  return g_object_new (GTK_TYPE_OVERLAY, NULL);
}

/**
 * gtk_overlay_add_overlay:
 * @overlay: a #GtkOverlay
 * @widget: a #GtkWidget to be added to the container
 *
 * Adds @widget to @overlay.
 *
 * The widget will be stacked on top of the main widget
 * added with gtk_container_add().
 *
 * The position at which @widget is placed is determined
 * from its #GtkWidget::halign and #GtkWidget::valign properties.
 *
 * Since: 3.2
 */
void
gtk_overlay_add_overlay (GtkOverlay *overlay,
                         GtkWidget  *widget)
{
  GtkOverlayPrivate *priv = overlay->priv;
  GtkOverlayChild *child;

  g_return_if_fail (GTK_IS_OVERLAY (overlay));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  child = g_slice_new0 (GtkOverlayChild);
  child->widget = widget;

  priv->children = g_slist_append (priv->children, child);

  if (gtk_widget_get_realized (GTK_WIDGET (overlay)))
    {
      child->window = gtk_overlay_create_child_window (overlay, widget);
      gtk_widget_set_parent (widget, GTK_WIDGET (overlay));
      gtk_overlay_child_allocate (overlay, child);
    }
  else
    gtk_widget_set_parent (widget, GTK_WIDGET (overlay));

}
