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

#include "gtkviewport.h"

#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkscrollable.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"


/**
 * SECTION:gtkviewport
 * @Short_description: An adapter which makes widgets scrollable
 * @Title: GtkViewport
 * @See_also:#GtkScrolledWindow, #GtkAdjustment
 *
 * The #GtkViewport widget acts as an adaptor class, implementing
 * scrollability for child widgets that lack their own scrolling
 * capabilities. Use #GtkViewport to scroll child widgets such as
 * #GtkTable, #GtkBox, and so on.
 *
 * If a widget has native scrolling abilities, such as #GtkTextView,
 * #GtkTreeView or #GtkIconview, it can be added to a #GtkScrolledWindow
 * with gtk_container_add(). If a widget does not, you must first add the
 * widget to a #GtkViewport, then add the viewport to the scrolled window.
 * The convenience function gtk_scrolled_window_add_with_viewport() does
 * exactly this, so you can ignore the presence of the viewport.
 *
 * The #GtkViewport will start scrolling content only if allocated less
 * than the child widget's minimum size in a given orientation.
 */

struct _GtkViewportPrivate
{
  GtkAdjustment  *hadjustment;
  GtkAdjustment  *vadjustment;
  GtkShadowType   shadow_type;

  GdkWindow      *bin_window;
  GdkWindow      *view_window;

  /* GtkScrollablePolicy needs to be checked when
   * driving the scrollable adjustment values */
  guint hscroll_policy : 1;
  guint vscroll_policy : 1;
};

enum {
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  PROP_SHADOW_TYPE
};


static void gtk_viewport_finalize                 (GObject          *object);
static void gtk_viewport_set_property             (GObject         *object,
						   guint            prop_id,
						   const GValue    *value,
						   GParamSpec      *pspec);
static void gtk_viewport_get_property             (GObject         *object,
						   guint            prop_id,
						   GValue          *value,
						   GParamSpec      *pspec);
static void gtk_viewport_destroy                  (GtkWidget        *widget);
static void gtk_viewport_realize                  (GtkWidget        *widget);
static void gtk_viewport_unrealize                (GtkWidget        *widget);
static gint gtk_viewport_draw                     (GtkWidget        *widget,
						   cairo_t          *cr);
static void gtk_viewport_add                      (GtkContainer     *container,
						   GtkWidget        *widget);
static void gtk_viewport_size_allocate            (GtkWidget        *widget,
						   GtkAllocation    *allocation);
static void gtk_viewport_adjustment_value_changed (GtkAdjustment    *adjustment,
						   gpointer          data);
static void gtk_viewport_style_updated            (GtkWidget        *widget);

static void gtk_viewport_get_preferred_width      (GtkWidget        *widget,
						   gint             *minimum_size,
						   gint             *natural_size);
static void gtk_viewport_get_preferred_height     (GtkWidget        *widget,
						   gint             *minimum_size,
						   gint             *natural_size);


G_DEFINE_TYPE_WITH_CODE (GtkViewport, gtk_viewport, GTK_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

static void
gtk_viewport_class_init (GtkViewportClass *class)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass*) class;
  container_class = (GtkContainerClass*) class;

  gobject_class->finalize = gtk_viewport_finalize;
  gobject_class->set_property = gtk_viewport_set_property;
  gobject_class->get_property = gtk_viewport_get_property;

  widget_class->destroy = gtk_viewport_destroy;
  widget_class->realize = gtk_viewport_realize;
  widget_class->unrealize = gtk_viewport_unrealize;
  widget_class->draw = gtk_viewport_draw;
  widget_class->size_allocate = gtk_viewport_size_allocate;
  widget_class->style_updated = gtk_viewport_style_updated;
  widget_class->get_preferred_width = gtk_viewport_get_preferred_width;
  widget_class->get_preferred_height = gtk_viewport_get_preferred_height;
  
  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_VIEWPORT);

  container_class->add = gtk_viewport_add;

  /* GtkScrollable implementation */
  g_object_class_override_property (gobject_class, PROP_HADJUSTMENT,    "hadjustment");
  g_object_class_override_property (gobject_class, PROP_VADJUSTMENT,    "vadjustment");
  g_object_class_override_property (gobject_class, PROP_HSCROLL_POLICY, "hscroll-policy");
  g_object_class_override_property (gobject_class, PROP_VSCROLL_POLICY, "vscroll-policy");

  g_object_class_install_property (gobject_class,
                                   PROP_SHADOW_TYPE,
                                   g_param_spec_enum ("shadow-type",
						      P_("Shadow type"),
						      P_("Determines how the shadowed box around the viewport is drawn"),
						      GTK_TYPE_SHADOW_TYPE,
						      GTK_SHADOW_IN,
						      GTK_PARAM_READWRITE));

  g_type_class_add_private (class, sizeof (GtkViewportPrivate));
}

static void
gtk_viewport_set_property (GObject         *object,
			   guint            prop_id,
			   const GValue    *value,
			   GParamSpec      *pspec)
{
  GtkViewport *viewport;

  viewport = GTK_VIEWPORT (object);

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      gtk_viewport_set_hadjustment (viewport, g_value_get_object (value));
      break;
    case PROP_VADJUSTMENT:
      gtk_viewport_set_vadjustment (viewport, g_value_get_object (value));
      break;
    case PROP_HSCROLL_POLICY:
      viewport->priv->hscroll_policy = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (viewport));
      break;
    case PROP_VSCROLL_POLICY:
      viewport->priv->vscroll_policy = g_value_get_enum (value);
      gtk_widget_queue_resize (GTK_WIDGET (viewport));
      break;
    case PROP_SHADOW_TYPE:
      gtk_viewport_set_shadow_type (viewport, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_viewport_get_property (GObject         *object,
			   guint            prop_id,
			   GValue          *value,
			   GParamSpec      *pspec)
{
  GtkViewport *viewport = GTK_VIEWPORT (object);
  GtkViewportPrivate *priv = viewport->priv;

  switch (prop_id)
    {
    case PROP_HADJUSTMENT:
      g_value_set_object (value, priv->hadjustment);
      break;
    case PROP_VADJUSTMENT:
      g_value_set_object (value, priv->vadjustment);
      break;
    case PROP_HSCROLL_POLICY:
      g_value_set_enum (value, priv->hscroll_policy);
      break;
    case PROP_VSCROLL_POLICY:
      g_value_set_enum (value, priv->vscroll_policy);
      break;
    case PROP_SHADOW_TYPE:
      g_value_set_enum (value, priv->shadow_type);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_viewport_init (GtkViewport *viewport)
{
  GtkViewportPrivate *priv;

  viewport->priv = G_TYPE_INSTANCE_GET_PRIVATE (viewport,
                                                GTK_TYPE_VIEWPORT,
                                                GtkViewportPrivate);
  priv = viewport->priv;

  gtk_widget_set_has_window (GTK_WIDGET (viewport), TRUE);

  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (viewport), FALSE);
  gtk_container_set_resize_mode (GTK_CONTAINER (viewport), GTK_RESIZE_QUEUE);

  priv->shadow_type = GTK_SHADOW_IN;
  priv->view_window = NULL;
  priv->bin_window = NULL;
  priv->hadjustment = NULL;
  priv->vadjustment = NULL;
}

/**
 * gtk_viewport_new:
 * @hadjustment: horizontal adjustment
 * @vadjustment: vertical adjustment
 *
 * Creates a new #GtkViewport with the given adjustments.
 *
 * Returns: a new #GtkViewport
 */
GtkWidget*
gtk_viewport_new (GtkAdjustment *hadjustment,
		  GtkAdjustment *vadjustment)
{
  GtkWidget *viewport;

  viewport = g_object_new (GTK_TYPE_VIEWPORT,
			     "hadjustment", hadjustment,
			     "vadjustment", vadjustment,
			     NULL);

  return viewport;
}

#define ADJUSTMENT_POINTER(viewport, orientation)         \
  (((orientation) == GTK_ORIENTATION_HORIZONTAL) ?        \
     &(viewport)->priv->hadjustment : &(viewport)->priv->vadjustment)

static void
viewport_disconnect_adjustment (GtkViewport    *viewport,
				GtkOrientation  orientation)
{
  GtkAdjustment **adjustmentp = ADJUSTMENT_POINTER (viewport, orientation);

  if (*adjustmentp)
    {
      g_signal_handlers_disconnect_by_func (*adjustmentp,
					    gtk_viewport_adjustment_value_changed,
					    viewport);
      g_object_unref (*adjustmentp);
      *adjustmentp = NULL;
    }
}

static void
gtk_viewport_finalize (GObject *object)
{
  GtkViewport *viewport = GTK_VIEWPORT (object);

  viewport_disconnect_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL);
  viewport_disconnect_adjustment (viewport, GTK_ORIENTATION_VERTICAL);

  G_OBJECT_CLASS (gtk_viewport_parent_class)->finalize (object);
}

static void
gtk_viewport_destroy (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);

  viewport_disconnect_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL);
  viewport_disconnect_adjustment (viewport, GTK_ORIENTATION_VERTICAL);

  GTK_WIDGET_CLASS (gtk_viewport_parent_class)->destroy (widget);
}

static void
viewport_get_view_allocation (GtkViewport   *viewport,
			      GtkAllocation *view_allocation)
{
  GtkViewportPrivate *priv = viewport->priv;
  GtkWidget *widget = GTK_WIDGET (viewport);
  GtkAllocation allocation;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding, border;
  guint border_width;

  gtk_widget_get_allocation (widget, &allocation);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (viewport));

  view_allocation->x = 0;
  view_allocation->y = 0;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);
  gtk_style_context_get_border (context, state, &border);

  if (priv->shadow_type != GTK_SHADOW_NONE)
    {
      view_allocation->x = border.left;
      view_allocation->y = border.top;
    }

  view_allocation->x += padding.left;
  view_allocation->y += padding.right;
  view_allocation->width = MAX (1, allocation.width - padding.left - padding.right - border_width * 2);
  view_allocation->height = MAX (1, allocation.height - padding.top - padding.bottom - border_width * 2);

  if (priv->shadow_type != GTK_SHADOW_NONE)
    {
      view_allocation->width = MAX (1, view_allocation->width - border.left - border.right);
      view_allocation->height = MAX (1, view_allocation->height - border.top - border.bottom);
    }
}

/**
 * gtk_viewport_get_hadjustment:
 * @viewport: a #GtkViewport.
 *
 * Returns the horizontal adjustment of the viewport.
 *
 * Return value: (transfer none): the horizontal adjustment of @viewport.
 *
 * Deprecated: 3.0: Use gtk_scrollable_get_hadjustment()
 **/
GtkAdjustment*
gtk_viewport_get_hadjustment (GtkViewport *viewport)
{
  GtkViewportPrivate *priv;

  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  priv = viewport->priv;

  if (!priv->hadjustment)
    gtk_viewport_set_hadjustment (viewport, NULL);

  return priv->hadjustment;
}

/**
 * gtk_viewport_get_vadjustment:
 * @viewport: a #GtkViewport.
 * 
 * Returns the vertical adjustment of the viewport.
 *
 * Return value: (transfer none): the vertical adjustment of @viewport.
 *
 * Deprecated: 3.0: Use gtk_scrollable_get_vadjustment()
 **/
GtkAdjustment*
gtk_viewport_get_vadjustment (GtkViewport *viewport)
{
  GtkViewportPrivate *priv;

  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  priv = viewport->priv;

  if (!priv->vadjustment)
    gtk_viewport_set_vadjustment (viewport, NULL);

  return priv->vadjustment;
}

static void
viewport_set_hadjustment_values (GtkViewport *viewport)
{
  GtkBin *bin = GTK_BIN (viewport);
  GtkAllocation view_allocation;
  GtkAdjustment *hadjustment = gtk_viewport_get_hadjustment (viewport);
  GtkWidget *child;
  gdouble upper, value;
  
  viewport_get_view_allocation (viewport, &view_allocation);  

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    {
      gint minimum_width, natural_width;
      gint scroll_height;
      
      if (viewport->priv->vscroll_policy == GTK_SCROLL_MINIMUM)
	gtk_widget_get_preferred_height (child, &scroll_height, NULL);
      else
	gtk_widget_get_preferred_height (child, NULL, &scroll_height);

      gtk_widget_get_preferred_width_for_height (child,
                                                 MAX (view_allocation.height, scroll_height),
                                                 &minimum_width,
                                                 &natural_width);

      if (viewport->priv->hscroll_policy == GTK_SCROLL_MINIMUM)
	upper = MAX (minimum_width, view_allocation.width);
      else
	upper = MAX (natural_width, view_allocation.width);
    }
  else
    upper = view_allocation.width;

  value = gtk_adjustment_get_value (hadjustment);
  /* We clamp to the left in RTL mode */
  if (gtk_widget_get_direction (GTK_WIDGET (viewport)) == GTK_TEXT_DIR_RTL)
    {
      gdouble dist = gtk_adjustment_get_upper (hadjustment)
                     - value
                     - gtk_adjustment_get_page_size (hadjustment);
      value = upper - dist - view_allocation.width;
    }

  gtk_adjustment_configure (hadjustment,
                            value,
                            0,
                            upper,
                            view_allocation.width * 0.1,
                            view_allocation.width * 0.9,
                            view_allocation.width);
}

static void
viewport_set_vadjustment_values (GtkViewport *viewport)
{
  GtkBin *bin = GTK_BIN (viewport);
  GtkAllocation view_allocation;
  GtkAdjustment *vadjustment = gtk_viewport_get_vadjustment (viewport);
  GtkWidget *child;
  gdouble upper;

  viewport_get_view_allocation (viewport, &view_allocation);  

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    {
      gint minimum_height, natural_height;
      gint scroll_width;

      if (viewport->priv->hscroll_policy == GTK_SCROLL_MINIMUM)
	gtk_widget_get_preferred_width (child, &scroll_width, NULL);
      else
	gtk_widget_get_preferred_width (child, NULL, &scroll_width);

      gtk_widget_get_preferred_height_for_width (child,
                                                 MAX (view_allocation.width, scroll_width),
                                                 &minimum_height,
                                                 &natural_height);

      if (viewport->priv->vscroll_policy == GTK_SCROLL_MINIMUM)
	upper = MAX (minimum_height, view_allocation.height);
      else
	upper = MAX (natural_height, view_allocation.height);
    }
  else
    upper = view_allocation.height;

  gtk_adjustment_configure (vadjustment,
                            gtk_adjustment_get_value (vadjustment),
                            0,
                            upper,
                            view_allocation.height * 0.1,
                            view_allocation.height * 0.9,
                            view_allocation.height);
}

static void
viewport_set_adjustment (GtkViewport    *viewport,
			 GtkOrientation  orientation,
			 GtkAdjustment  *adjustment)
{
  GtkAdjustment **adjustmentp = ADJUSTMENT_POINTER (viewport, orientation);

  if (adjustment && adjustment == *adjustmentp)
    return;

  if (!adjustment)
    adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
  viewport_disconnect_adjustment (viewport, orientation);
  *adjustmentp = adjustment;
  g_object_ref_sink (adjustment);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    viewport_set_hadjustment_values (viewport);
  else
    viewport_set_vadjustment_values (viewport);

  g_signal_connect (adjustment, "value-changed",
		    G_CALLBACK (gtk_viewport_adjustment_value_changed),
		    viewport);

  gtk_viewport_adjustment_value_changed (adjustment, viewport);
}

/**
 * gtk_viewport_set_hadjustment:
 * @viewport: a #GtkViewport.
 * @adjustment: (allow-none): a #GtkAdjustment.
 *
 * Sets the horizontal adjustment of the viewport.
 *
 * Deprecated: 3.0: Use gtk_scrollable_set_hadjustment()
 **/
void
gtk_viewport_set_hadjustment (GtkViewport   *viewport,
			      GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  viewport_set_adjustment (viewport, GTK_ORIENTATION_HORIZONTAL, adjustment);

  g_object_notify (G_OBJECT (viewport), "hadjustment");
}

/**
 * gtk_viewport_set_vadjustment:
 * @viewport: a #GtkViewport.
 * @adjustment: (allow-none): a #GtkAdjustment.
 *
 * Sets the vertical adjustment of the viewport.
 *
 * Deprecated: 3.0: Use gtk_scrollable_set_vadjustment()
 **/
void
gtk_viewport_set_vadjustment (GtkViewport   *viewport,
			      GtkAdjustment *adjustment)
{
  g_return_if_fail (GTK_IS_VIEWPORT (viewport));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  viewport_set_adjustment (viewport, GTK_ORIENTATION_VERTICAL, adjustment);

  g_object_notify (G_OBJECT (viewport), "vadjustment");
}

/** 
 * gtk_viewport_set_shadow_type:
 * @viewport: a #GtkViewport.
 * @type: the new shadow type.
 *
 * Sets the shadow type of the viewport.
 **/ 
void
gtk_viewport_set_shadow_type (GtkViewport   *viewport,
			      GtkShadowType  type)
{
  GtkViewportPrivate *priv;
  GtkAllocation allocation;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_VIEWPORT (viewport));

  widget = GTK_WIDGET (viewport);
  priv = viewport->priv;

  if ((GtkShadowType) priv->shadow_type != type)
    {
      priv->shadow_type = type;

      if (gtk_widget_get_visible (widget))
	{
          gtk_widget_get_allocation (widget, &allocation);
          gtk_widget_size_allocate (widget, &allocation);
          gtk_widget_set_allocation (widget, &allocation);
          gtk_widget_queue_draw (widget);
	}

      g_object_notify (G_OBJECT (viewport), "shadow-type");
    }
}

/**
 * gtk_viewport_get_shadow_type:
 * @viewport: a #GtkViewport
 *
 * Gets the shadow type of the #GtkViewport. See
 * gtk_viewport_set_shadow_type().
 *
 * Return value: the shadow type 
 **/
GtkShadowType
gtk_viewport_get_shadow_type (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), GTK_SHADOW_NONE);

  return viewport->priv->shadow_type;
}

/**
 * gtk_viewport_get_bin_window:
 * @viewport: a #GtkViewport
 *
 * Gets the bin window of the #GtkViewport.
 *
 * Return value: (transfer none): a #GdkWindow
 *
 * Since: 2.20
 **/
GdkWindow*
gtk_viewport_get_bin_window (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  return viewport->priv->bin_window;
}

/**
 * gtk_viewport_get_view_window:
 * @viewport: a #GtkViewport
 *
 * Gets the view window of the #GtkViewport.
 *
 * Return value: (transfer none): a #GdkWindow
 *
 * Since: 2.22
 **/
GdkWindow*
gtk_viewport_get_view_window (GtkViewport *viewport)
{
  g_return_val_if_fail (GTK_IS_VIEWPORT (viewport), NULL);

  return viewport->priv->view_window;
}

static void
gtk_viewport_realize (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;
  GtkBin *bin = GTK_BIN (widget);
  GtkAdjustment *hadjustment = gtk_viewport_get_hadjustment (viewport);
  GtkAdjustment *vadjustment = gtk_viewport_get_vadjustment (viewport);
  GtkAllocation allocation;
  GtkAllocation view_allocation;
  GtkStyleContext *context;
  GtkWidget *child;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint event_mask;
  guint border_width;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x + border_width;
  attributes.y = allocation.y + border_width;
  attributes.width = allocation.width - border_width * 2;
  attributes.height = allocation.height - border_width * 2;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);

  event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  /* We select on button_press_mask so that button 4-5 scrolls are trapped.
   */
  attributes.event_mask = event_mask | GDK_BUTTON_PRESS_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, viewport);

  viewport_get_view_allocation (viewport, &view_allocation);
  
  attributes.x = view_allocation.x;
  attributes.y = view_allocation.y;
  attributes.width = view_allocation.width;
  attributes.height = view_allocation.height;
  attributes.event_mask = 0;

  priv->view_window = gdk_window_new (window,
                                      &attributes, attributes_mask);
  gdk_window_set_user_data (priv->view_window, viewport);

  attributes.x = - gtk_adjustment_get_value (hadjustment);
  attributes.y = - gtk_adjustment_get_value (vadjustment);
  attributes.width = gtk_adjustment_get_upper (hadjustment);
  attributes.height = gtk_adjustment_get_upper (vadjustment);
  
  attributes.event_mask = event_mask;

  priv->bin_window = gdk_window_new (priv->view_window, &attributes, attributes_mask);
  gdk_window_set_user_data (priv->bin_window, viewport);

  child = gtk_bin_get_child (bin);
  if (child)
    gtk_widget_set_parent_window (child, priv->bin_window);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_background (context, window);
  gtk_style_context_set_background (context, priv->bin_window);

  gdk_window_show (priv->bin_window);
  gdk_window_show (priv->view_window);
}

static void
gtk_viewport_unrealize (GtkWidget *widget)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;

  gdk_window_set_user_data (priv->view_window, NULL);
  gdk_window_destroy (priv->view_window);
  priv->view_window = NULL;

  gdk_window_set_user_data (priv->bin_window, NULL);
  gdk_window_destroy (priv->bin_window);
  priv->bin_window = NULL;

  GTK_WIDGET_CLASS (gtk_viewport_parent_class)->unrealize (widget);
}

static gint
gtk_viewport_draw (GtkWidget *widget,
                   cairo_t   *cr)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;
  GtkStyleContext *context;
  int x, y;

  context = gtk_widget_get_style_context (widget);

  if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget)))
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);

      gtk_render_frame (context, cr, 0, 0,
                        gdk_window_get_width (gtk_widget_get_window (widget)),
                        gdk_window_get_height (gtk_widget_get_window (widget)));

      gtk_style_context_restore (context);
    }
  
  if (gtk_cairo_should_draw_window (cr, priv->view_window))
    {
      /* This is a cute hack to ensure the contents of bin_window are
       * restricted to where they are visible. We only need to do this
       * clipping when called via gtk_widget_draw() and not in expose
       * events. And when that happens every window (including this one)
       * should be drawn.
       */
      gdk_window_get_position (priv->view_window, &x, &y);
      cairo_rectangle (cr, x, y, 
                       gdk_window_get_width (priv->view_window),
                       gdk_window_get_height (priv->view_window));
      cairo_clip (cr);
    }

  if (gtk_cairo_should_draw_window (cr, priv->bin_window))
    {
      gdk_window_get_position (priv->bin_window, &x, &y);
      gtk_render_background (context, cr, x, y,
                             gdk_window_get_width (priv->bin_window),
                             gdk_window_get_height (priv->bin_window));

      GTK_WIDGET_CLASS (gtk_viewport_parent_class)->draw (widget, cr);
    }

  return FALSE;
}

static void
gtk_viewport_add (GtkContainer *container,
		  GtkWidget    *child)
{
  GtkBin *bin = GTK_BIN (container);
  GtkViewport *viewport = GTK_VIEWPORT (bin);
  GtkViewportPrivate *priv = viewport->priv;

  g_return_if_fail (gtk_bin_get_child (bin) == NULL);

  gtk_widget_set_parent_window (child, priv->bin_window);

  GTK_CONTAINER_CLASS (gtk_viewport_parent_class)->add (container, child);
}

static void
gtk_viewport_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GtkAllocation widget_allocation;
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;
  GtkBin *bin = GTK_BIN (widget);
  guint border_width;
  GtkAdjustment *hadjustment = gtk_viewport_get_hadjustment (viewport);
  GtkAdjustment *vadjustment = gtk_viewport_get_vadjustment (viewport);
  GtkAllocation child_allocation;
  GtkWidget *child;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  /* If our size changed, and we have a shadow, queue a redraw on widget->window to
   * redraw the shadow correctly.
   */
  gtk_widget_get_allocation (widget, &widget_allocation);
  if (gtk_widget_get_mapped (widget) &&
      priv->shadow_type != GTK_SHADOW_NONE &&
      (widget_allocation.width != allocation->width ||
       widget_allocation.height != allocation->height))
    gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);

  gtk_widget_set_allocation (widget, allocation);

  g_object_freeze_notify (G_OBJECT (hadjustment));
  g_object_freeze_notify (G_OBJECT (vadjustment));

  viewport_set_hadjustment_values (viewport);
  viewport_set_vadjustment_values (viewport);
  
  child_allocation.x = 0;
  child_allocation.y = 0;
  child_allocation.width = gtk_adjustment_get_upper (hadjustment);
  child_allocation.height = gtk_adjustment_get_upper (vadjustment);
  if (gtk_widget_get_realized (widget))
    {
      GtkAllocation view_allocation;

      gdk_window_move_resize (gtk_widget_get_window (widget),
			      allocation->x + border_width,
			      allocation->y + border_width,
			      allocation->width - border_width * 2,
			      allocation->height - border_width * 2);
      
      viewport_get_view_allocation (viewport, &view_allocation);
      gdk_window_move_resize (priv->view_window,
			      view_allocation.x,
			      view_allocation.y,
			      view_allocation.width,
			      view_allocation.height);
      gdk_window_move_resize (priv->bin_window,
                              - gtk_adjustment_get_value (hadjustment),
                              - gtk_adjustment_get_value (vadjustment),
                              child_allocation.width,
                              child_allocation.height);
    }

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation);

  g_object_thaw_notify (G_OBJECT (hadjustment));
  g_object_thaw_notify (G_OBJECT (vadjustment));
}

static void
gtk_viewport_adjustment_value_changed (GtkAdjustment *adjustment,
				       gpointer       data)
{
  GtkViewport *viewport = GTK_VIEWPORT (data);
  GtkViewportPrivate *priv = viewport->priv;
  GtkBin *bin = GTK_BIN (data);
  GtkWidget *child;

  child = gtk_bin_get_child (bin);
  if (child && gtk_widget_get_visible (child) &&
      gtk_widget_get_realized (GTK_WIDGET (viewport)))
    {
      GtkAdjustment *hadjustment = gtk_viewport_get_hadjustment (viewport);
      GtkAdjustment *vadjustment = gtk_viewport_get_vadjustment (viewport);
      gint old_x, old_y;
      gint new_x, new_y;

      gdk_window_get_position (priv->bin_window, &old_x, &old_y);
      new_x = - gtk_adjustment_get_value (hadjustment);
      new_y = - gtk_adjustment_get_value (vadjustment);

      if (new_x != old_x || new_y != old_y)
	{
	  gdk_window_move (priv->bin_window, new_x, new_y);
	  gdk_window_process_updates (priv->bin_window, TRUE);
	}
    }
}

static void
gtk_viewport_style_updated (GtkWidget *widget)
{
   GTK_WIDGET_CLASS (gtk_viewport_parent_class)->style_updated (widget);

   if (gtk_widget_get_realized (widget) &&
       gtk_widget_get_has_window (widget))
     {
        GtkStyleContext *context;
        GtkViewport *viewport = GTK_VIEWPORT (widget);
        GtkViewportPrivate *priv = viewport->priv;

        context = gtk_widget_get_style_context (widget);
        gtk_style_context_set_background (context, priv->bin_window);
        gtk_style_context_set_background (context, gtk_widget_get_window (widget));
     }
}


static void
gtk_viewport_get_preferred_size (GtkWidget      *widget,
                                 GtkOrientation  orientation,
                                 gint           *minimum_size,
                                 gint           *natural_size)
{
  GtkViewport *viewport = GTK_VIEWPORT (widget);
  GtkViewportPrivate *priv = viewport->priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding, border;
  GtkWidget *child;
  gint       child_min, child_nat;
  gint       minimum, natural;

  child = gtk_bin_get_child (GTK_BIN (widget));

  /* XXX This should probably be (border_width * 2); but GTK+ has
   * been doing this with a single border for a while now...
   */
  minimum = gtk_container_get_border_width (GTK_CONTAINER (widget));

  context = gtk_widget_get_style_context (GTK_WIDGET (widget));
  state = gtk_widget_get_state_flags (GTK_WIDGET (widget));
  gtk_style_context_get_padding (context, state, &padding);

  if (priv->shadow_type != GTK_SHADOW_NONE)
    {
      gtk_style_context_get_border (context, state, &border);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        minimum += border.left + border.right;
      else
        minimum += border.top + border.bottom;
    }

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    minimum += padding.left + padding.right;
  else
    minimum += padding.top + padding.bottom;

  natural = minimum;

  if (child && gtk_widget_get_visible (child))
    {
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	gtk_widget_get_preferred_width (child, &child_min, &child_nat);
      else
	gtk_widget_get_preferred_height (child, &child_min, &child_nat);

      minimum += child_min;
      natural += child_nat;
    }

  if (minimum_size)
    *minimum_size = minimum;

  if (natural_size)
    *natural_size = natural;
}

static void
gtk_viewport_get_preferred_width (GtkWidget *widget,
                                  gint      *minimum_size,
                                  gint      *natural_size)
{
  gtk_viewport_get_preferred_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum_size, natural_size);
}

static void
gtk_viewport_get_preferred_height (GtkWidget *widget,
                                   gint      *minimum_size,
                                   gint      *natural_size)
{
  gtk_viewport_get_preferred_size (widget, GTK_ORIENTATION_VERTICAL, minimum_size, natural_size);
}
