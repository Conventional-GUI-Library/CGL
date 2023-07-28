/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include "config.h"
#include "gtkrevealer.h"
#include <gdk/gdk.h>
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include <math.h>

/**
 * SECTION:gtkrevealer
 * @Short_description: Hide and show with animation
 * @Title: GtkRevealer
 * @See_also: #GtkExpander
 *
 * The GtkRevealer widget is a container which animates
 * the transition of its child from invisible to visible.
 *
 * The style of transition can be controlled with
 * gtk_revealer_set_transition_type().
 *
 * The GtkRevealer widget was added in GTK+ 3.10.
 */

/**
 * GtkRevealerTransitionType:
 * @GTK_REVEALER_TRANSITION_TYPE_NONE: No transition
 * @GTK_REVEALER_TRANSITION_TYPE_CROSSFADE: Fade in
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT: Slide in from the left
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT: Slide in from the right
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP: Slide in from the bottom
 * @GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN: Slide in from the top
 *
 * These enumeration values describe the possible transitions
 * when the child of a #GtkRevealer widget is shown or hidden.
 */

enum  {
  PROP_0,
  PROP_TRANSITION_TYPE,
  PROP_TRANSITION_DURATION,
  PROP_REVEAL_CHILD,
  PROP_CHILD_REVEALED
};

struct _GtkRevealerPrivate {
  GtkRevealerTransitionType transition_type;
  guint transition_duration;

  GdkWindow* bin_window;
  GdkWindow* view_window;

  gdouble current_pos;
  gdouble source_pos;
  gdouble target_pos;

  guint tick_id;
  gint64 start_time;
  gint64 end_time;
};


static void     gtk_revealer_real_realize                        (GtkWidget     *widget);
static void     gtk_revealer_real_unrealize                      (GtkWidget     *widget);
static void     gtk_revealer_real_add                            (GtkContainer  *widget,
                                                                  GtkWidget     *child);
static void     gtk_revealer_real_style_updated                  (GtkWidget     *widget);
static void     gtk_revealer_real_size_allocate                  (GtkWidget     *widget,
                                                                  GtkAllocation *allocation);
static void     gtk_revealer_real_map                            (GtkWidget     *widget);
static void     gtk_revealer_real_unmap                          (GtkWidget     *widget);
static gboolean gtk_revealer_real_draw                           (GtkWidget     *widget,
                                                                  cairo_t       *cr);
static void     gtk_revealer_real_get_preferred_height           (GtkWidget     *widget,
                                                                  gint          *minimum_height,
                                                                  gint          *natural_height);
static void     gtk_revealer_real_get_preferred_height_for_width (GtkWidget     *widget,
                                                                  gint           width,
                                                                  gint          *minimum_height,
                                                                  gint          *natural_height);
static void     gtk_revealer_real_get_preferred_width            (GtkWidget     *widget,
                                                                  gint          *minimum_width,
                                                                  gint          *natural_width);
static void     gtk_revealer_real_get_preferred_width_for_height (GtkWidget     *widget,
                                                                  gint           height,
                                                                  gint          *minimum_width,
                                                                  gint          *natural_width);

G_DEFINE_TYPE (GtkRevealer, gtk_revealer, GTK_TYPE_BIN);


static void
gtk_revealer_init (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv;

  priv = G_TYPE_INSTANCE_GET_PRIVATE (revealer, GTK_TYPE_REVEALER, GtkRevealerPrivate);
  revealer->priv = priv;

  priv->transition_type = GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN;
  priv->transition_duration = 250;
  priv->current_pos = 0.0;
  priv->target_pos = 0.0;

  gtk_widget_set_has_window ((GtkWidget*) revealer, TRUE);
  gtk_widget_set_redraw_on_allocate ((GtkWidget*) revealer, FALSE);
}

static void
gtk_revealer_finalize (GObject *obj)
{
  GtkRevealer *revealer = GTK_REVEALER (obj);
  GtkRevealerPrivate *priv = revealer->priv;

  if (priv->tick_id != 0)
    gtk_widget_remove_tick_callback (GTK_WIDGET (revealer), priv->tick_id);
  priv->tick_id = 0;

  G_OBJECT_CLASS (gtk_revealer_parent_class)->finalize (obj);
}

static void
gtk_revealer_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkRevealer *revealer = GTK_REVEALER (object);

  switch (property_id)
   {
    case PROP_TRANSITION_TYPE:
      g_value_set_enum (value, gtk_revealer_get_transition_type (revealer));
      break;
    case PROP_TRANSITION_DURATION:
      g_value_set_uint (value, gtk_revealer_get_transition_duration (revealer));
      break;
    case PROP_REVEAL_CHILD:
      g_value_set_boolean (value, gtk_revealer_get_reveal_child (revealer));
      break;
    case PROP_CHILD_REVEALED:
      g_value_set_boolean (value, gtk_revealer_get_child_revealed (revealer));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_revealer_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkRevealer *revealer = GTK_REVEALER (object);

  switch (property_id)
    {
    case PROP_TRANSITION_TYPE:
      gtk_revealer_set_transition_type (revealer, g_value_get_enum (value));
      break;
    case PROP_TRANSITION_DURATION:
      gtk_revealer_set_transition_duration (revealer, g_value_get_uint (value));
      break;
    case PROP_REVEAL_CHILD:
      gtk_revealer_set_reveal_child (revealer, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_revealer_class_init (GtkRevealerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->get_property = gtk_revealer_get_property;
  object_class->set_property = gtk_revealer_set_property;
  object_class->finalize = gtk_revealer_finalize;

  widget_class->realize = gtk_revealer_real_realize;
  widget_class->unrealize = gtk_revealer_real_unrealize;
  widget_class->style_updated = gtk_revealer_real_style_updated;
  widget_class->size_allocate = gtk_revealer_real_size_allocate;
  widget_class->map = gtk_revealer_real_map;
  widget_class->unmap = gtk_revealer_real_unmap;
  widget_class->draw = gtk_revealer_real_draw;
  widget_class->get_preferred_height = gtk_revealer_real_get_preferred_height;
  widget_class->get_preferred_height_for_width = gtk_revealer_real_get_preferred_height_for_width;
  widget_class->get_preferred_width = gtk_revealer_real_get_preferred_width;
  widget_class->get_preferred_width_for_height = gtk_revealer_real_get_preferred_width_for_height;

  container_class->add = gtk_revealer_real_add;

  g_object_class_install_property (object_class,
                                   PROP_TRANSITION_TYPE,
                                   g_param_spec_enum ("transition-type",
                                                      P_("Transition type"),
                                                      P_("The type of animation used to transition"),
                                                      GTK_TYPE_REVEALER_TRANSITION_TYPE,
                                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN,
                                                      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_TRANSITION_DURATION,
                                   g_param_spec_uint ("transition-duration",
                                                      P_("Transition duration"),
                                                      P_("The animation duration, in milliseconds"),
                                                      0, G_MAXUINT,
                                                      250,
                                                      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property (object_class,
                                   PROP_REVEAL_CHILD,
                                   g_param_spec_boolean ("reveal-child",
                                                         P_("Reveal Child"),
                                                         P_("Whether the container should reveal the child"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class,
                                   PROP_CHILD_REVEALED,
                                   g_param_spec_boolean ("child-revealed",
                                                         P_("Child Revealed"),
                                                         P_("Whether the child is revealed and the animation target reached"),
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_type_class_add_private (klass, sizeof (GtkRevealerPrivate));
}

GtkWidget *
gtk_revealer_new (void)
{
  return g_object_new (GTK_TYPE_REVEALER, NULL);
}

static GtkRevealerTransitionType
effective_transition (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv = revealer->priv;

  if (gtk_widget_get_direction (GTK_WIDGET (revealer)) == GTK_TEXT_DIR_RTL)
    {
      if (priv->transition_type == GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT)
        return GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT;
      else if (priv->transition_type == GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT)
        return GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT;
    }

  return priv->transition_type;
}

static void
gtk_revealer_get_child_allocation (GtkRevealer   *revealer,
                                   GtkAllocation *allocation,
                                   GtkAllocation *child_allocation)
{
  GtkWidget *child;
  GtkRevealerTransitionType transition;

  g_return_if_fail (revealer != NULL);
  g_return_if_fail (allocation != NULL);

  child_allocation->x = 0;
  child_allocation->y = 0;
  child_allocation->width = allocation->width;
  child_allocation->height = allocation->height;

  child = gtk_bin_get_child (GTK_BIN (revealer));
  if (child != NULL && gtk_widget_get_visible (child))
    {
      transition = effective_transition (revealer);
      if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT ||
          transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT)
        gtk_widget_get_preferred_width_for_height (child, child_allocation->height, NULL,
                                                   &child_allocation->width);
      else
        gtk_widget_get_preferred_height_for_width (child, child_allocation->width, NULL,
                                                   &child_allocation->height);
    }
}

static void
gtk_revealer_real_realize (GtkWidget *widget)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;
  GtkAllocation allocation;
  GdkWindowAttr attributes = { 0 };
  GdkWindowAttributesType attributes_mask;
  GtkAllocation child_allocation;
  GtkWidget *child;
  GtkStyleContext *context;
  GtkRevealerTransitionType transition;

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.event_mask =
    gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes_mask = (GDK_WA_X | GDK_WA_Y) | GDK_WA_VISUAL;

  priv->view_window =
    gdk_window_new (gtk_widget_get_parent_window ((GtkWidget*) revealer),
                    &attributes, attributes_mask);
  gtk_widget_set_window (widget, priv->view_window);
  gtk_widget_register_window (widget, priv->view_window);

  gtk_revealer_get_child_allocation (revealer, &allocation, &child_allocation);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = child_allocation.width;
  attributes.height = child_allocation.height;

  transition = effective_transition (revealer);
  if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN)
    attributes.y = allocation.height - child_allocation.height;
  else if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT)
    attributes.x = allocation.width - child_allocation.width;

  priv->bin_window =
    gdk_window_new (priv->view_window, &attributes, attributes_mask);
  gtk_widget_register_window (widget, priv->bin_window);

  child = gtk_bin_get_child (GTK_BIN (revealer));
  if (child != NULL)
    gtk_widget_set_parent_window (child, priv->bin_window);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_background (context, priv->view_window);
  gtk_style_context_set_background (context, priv->bin_window);
  gdk_window_show (priv->bin_window);
}

static void
gtk_revealer_real_unrealize (GtkWidget *widget)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;

  gtk_widget_unregister_window (widget, priv->bin_window);
  gdk_window_destroy (priv->bin_window);
  priv->view_window = NULL;

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->unrealize (widget);
}

static void
gtk_revealer_real_add (GtkContainer *container,
                       GtkWidget    *child)
{
  GtkRevealer *revealer = GTK_REVEALER (container);
  GtkRevealerPrivate *priv = revealer->priv;

  g_return_if_fail (child != NULL);

  gtk_widget_set_parent_window (child, priv->bin_window);
  gtk_widget_set_child_visible (child, priv->current_pos != 0.0);

  GTK_CONTAINER_CLASS (gtk_revealer_parent_class)->add (container, child);
}

static void
gtk_revealer_real_style_updated (GtkWidget *widget)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;
  GtkStyleContext* context;

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->style_updated (widget);

  if (gtk_widget_get_realized (widget))
    {
      context = gtk_widget_get_style_context (widget);
      gtk_style_context_set_background (context, priv->bin_window);
      gtk_style_context_set_background (context, priv->view_window);
    }
}

static void
gtk_revealer_real_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;
  GtkAllocation child_allocation;
  GtkWidget *child;
  gboolean window_visible;
  int bin_x, bin_y;
  GtkRevealerTransitionType transition;

  g_return_if_fail (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);
  gtk_revealer_get_child_allocation (revealer, allocation, &child_allocation);

  child = gtk_bin_get_child (GTK_BIN (revealer));
  if (child != NULL && gtk_widget_get_visible (child))
    gtk_widget_size_allocate (child, &child_allocation);

  if (gtk_widget_get_realized (widget))
    {
      if (gtk_widget_get_mapped (widget))
        {
          window_visible = allocation->width > 0 && allocation->height > 0;

          if (!window_visible && gdk_window_is_visible (priv->view_window))
            gdk_window_hide (priv->view_window);

          if (window_visible && !gdk_window_is_visible (priv->view_window))
            gdk_window_show (priv->view_window);
        }

      gdk_window_move_resize (priv->view_window,
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);

      bin_x = 0;
      bin_y = 0;
      transition = effective_transition (revealer);
      if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN)
        bin_y = allocation->height - child_allocation.height;
      else if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT)
        bin_x = allocation->width - child_allocation.width;

      gdk_window_move_resize (priv->bin_window,
                              bin_x, bin_y,
                              child_allocation.width, child_allocation.height);
    }
}

static void
gtk_revealer_set_position (GtkRevealer *revealer,
                           gdouble      pos)
{
  GtkRevealerPrivate *priv = revealer->priv;
  gboolean new_visible;
  GtkWidget *child;
  GtkRevealerTransitionType transition;

  priv->current_pos = pos;

  /* We check target_pos here too, because we want to ensure we set
   * child_visible immediately when starting a reveal operation
   * otherwise the child widgets will not be properly realized
   * after the reveal returns.
   */
  new_visible = priv->current_pos != 0.0 || priv->target_pos != 0.0;

  child = gtk_bin_get_child (GTK_BIN (revealer));
  if (child != NULL &&
      new_visible != gtk_widget_get_child_visible (child))
    gtk_widget_set_child_visible (child, new_visible);

  transition = effective_transition (revealer);
  if (transition == GTK_REVEALER_TRANSITION_TYPE_CROSSFADE)
    {
      gtk_widget_set_opacity (GTK_WIDGET (revealer), priv->current_pos);
      gtk_widget_queue_draw (GTK_WIDGET (revealer));
    }
  else
    {
      gtk_widget_queue_resize (GTK_WIDGET (revealer));
    }

  if (priv->current_pos == priv->target_pos)
    g_object_notify (G_OBJECT (revealer), "child-revealed");
}

static gdouble
ease_out_quad (gdouble t, gdouble d)
{
  gdouble p = t / d;
  return  ((-1.0) * p) * (p - 2);
}

static void
gtk_revealer_animate_step (GtkRevealer *revealer,
                           gint64       now)
{
  GtkRevealerPrivate *priv = revealer->priv;
  gdouble t;

  t = 1.0;
  if (now < priv->end_time)
      t = (now - priv->start_time) / (gdouble) (priv->end_time - priv->start_time);
  t = ease_out_quad (t, 1.0);

  gtk_revealer_set_position (revealer,
                            priv->source_pos + (t * (priv->target_pos - priv->source_pos)));
}

static gboolean
gtk_revealer_animate_cb (GtkRevealer   *revealer,
                         GdkFrameClock *frame_clock,
                         gpointer       user_data)
{
  GtkRevealerPrivate *priv = revealer->priv;
  gint64 now;

  now = gdk_frame_clock_get_frame_time (frame_clock);
  gtk_revealer_animate_step (revealer, now);
  if (priv->current_pos == priv->target_pos)
    {
      priv->tick_id = 0;
      return FALSE;
    }

  return TRUE;
}

static void
gtk_revealer_start_animation (GtkRevealer *revealer,
                              gdouble      target)
{
  GtkRevealerPrivate *priv = revealer->priv;
  GtkWidget *widget = GTK_WIDGET (revealer);
  GtkRevealerTransitionType transition;

  if (priv->target_pos == target)
    return;

  priv->target_pos = target;
  g_object_notify (G_OBJECT (revealer), "reveal-child");

  transition = effective_transition (revealer);
  if (gtk_widget_get_mapped (widget) &&
      priv->transition_duration != 0 &&
      transition != GTK_REVEALER_TRANSITION_TYPE_NONE)
    {
      priv->source_pos = priv->current_pos;
      priv->start_time = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));
      priv->end_time = priv->start_time + (priv->transition_duration * 1000);
      if (priv->tick_id == 0)
        priv->tick_id =
          gtk_widget_add_tick_callback (widget, (GtkTickCallback)gtk_revealer_animate_cb, revealer, NULL);
      gtk_revealer_animate_step (revealer, priv->start_time);
    }
  else
    {
      gtk_revealer_set_position (revealer, target);
    }
}

static void
gtk_revealer_stop_animation (GtkRevealer *revealer)
{
  GtkRevealerPrivate *priv = revealer->priv;

  priv->current_pos = priv->target_pos;
  if (priv->tick_id != 0)
    {
      gtk_widget_remove_tick_callback (GTK_WIDGET (revealer), priv->tick_id);
      priv->tick_id = 0;
    }
}

static void
gtk_revealer_real_map (GtkWidget *widget)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;
  GtkAllocation allocation;

  if (!gtk_widget_get_mapped (widget))
    {
      gtk_widget_get_allocation (widget, &allocation);

      if (allocation.width > 0 && allocation.height > 0)
        gdk_window_show (priv->view_window);

      gtk_revealer_start_animation (revealer, priv->target_pos);
    }

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->map (widget);
}

static void
gtk_revealer_real_unmap (GtkWidget *widget)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->unmap (widget);

  gtk_revealer_stop_animation (revealer);
}

static gboolean
gtk_revealer_real_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;

  if (gtk_cairo_should_draw_window (cr, priv->bin_window))
    GTK_WIDGET_CLASS (gtk_revealer_parent_class)->draw (widget, cr);

  return TRUE;
}

/**
 * gtk_revealer_set_reveal_child:
 * @revealer: a #GtkRevealer
 * @reveal_child: %TRUE to reveal the child
 *
 * Tells the #GtkRevealer to reveal or conceal its child.
 *
 * The transition will be animated with the current
 * transition type of @revealer.
 *
 * Since: 3.10
 */
void
gtk_revealer_set_reveal_child (GtkRevealer *revealer,
                               gboolean     reveal_child)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  if (reveal_child)
    gtk_revealer_start_animation (revealer, 1.0);
  else
    gtk_revealer_start_animation (revealer, 0.0);
}

/**
 * gtk_revealer_get_reveal_child:
 * @revealer:
 *
 * Returns whether the child is currently
 * revealed. See gtk_revealer_set_reveal_child().
 *
 * This function returns %TRUE as soon as the transition
 * is to the revealed state is started. To learn whether
 * the child is fully revealed (ie the transition is completed),
 * use gtk_revealer_get_child_revealed().
 *
 * Return value: %TRUE if the child is revealed.
 *
 * Since: 3.10
 */
gboolean
gtk_revealer_get_reveal_child (GtkRevealer *revealer)
{
  g_return_val_if_fail (GTK_IS_REVEALER (revealer), FALSE);

  return revealer->priv->target_pos != 0.0;
}

/**
 * gtk_revealer_get_child_revealed:
 * @revealer: a #GtkRevealer
 *
 * Returns whether the child is fully revealed, ie wether
 * the transition to the revealed state is completed.
 *
 * Return value: %TRUE if the child is fully revealed
 *
 * Since: 3.10
 */
gboolean
gtk_revealer_get_child_revealed (GtkRevealer *revealer)
{
  gboolean animation_finished = (revealer->priv->target_pos == revealer->priv->current_pos);
  gboolean reveal_child = gtk_revealer_get_reveal_child (revealer);

  if (animation_finished)
    return reveal_child;
  else
    return !reveal_child;
}

/* These all report only the natural size, ignoring the minimal size,
 * because its not really possible to allocate the right size during
 * animation if the child size can change (without the child
 * re-arranging itself during the animation).
 */

static void
gtk_revealer_real_get_preferred_height (GtkWidget *widget,
                                        gint      *minimum_height_out,
                                        gint      *natural_height_out)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;
  gint minimum_height;
  gint natural_height;
  GtkRevealerTransitionType transition;

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->get_preferred_height (widget, &minimum_height, &natural_height);

  transition = effective_transition (revealer);
  if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP ||
      transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN)
    natural_height = round (natural_height * priv->current_pos);

  minimum_height = natural_height;

  if (minimum_height_out)
    *minimum_height_out = minimum_height;
  if (natural_height_out)
    *natural_height_out = natural_height;
}

static void
gtk_revealer_real_get_preferred_height_for_width (GtkWidget *widget,
                                                  gint       width,
                                                  gint      *minimum_height_out,
                                                  gint      *natural_height_out)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;
  gint minimum_height;
  gint natural_height;
  GtkRevealerTransitionType transition;

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->get_preferred_height_for_width (widget, width, &minimum_height, &natural_height);

  transition = effective_transition (revealer);
  if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP ||
      transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN)
    natural_height = round (natural_height * priv->current_pos);

  minimum_height = natural_height;

  if (minimum_height_out)
    *minimum_height_out = minimum_height;
  if (natural_height_out)
    *natural_height_out = natural_height;
}

static void
gtk_revealer_real_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum_width_out,
                                       gint      *natural_width_out)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;
  gint minimum_width;
  gint natural_width;
  GtkRevealerTransitionType transition;

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->get_preferred_width (widget, &minimum_width, &natural_width);

  transition = effective_transition (revealer);
  if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT ||
      transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT)
    natural_width = round (natural_width * priv->current_pos);

  minimum_width = natural_width;

  if (minimum_width_out)
    *minimum_width_out = minimum_width;
  if (natural_width_out)
    *natural_width_out = natural_width;
}

static void
gtk_revealer_real_get_preferred_width_for_height (GtkWidget *widget,
                                                  gint       height,
                                                  gint      *minimum_width_out,
                                                  gint      *natural_width_out)
{
  GtkRevealer *revealer = GTK_REVEALER (widget);
  GtkRevealerPrivate *priv = revealer->priv;
  gint minimum_width;
  gint natural_width;
  GtkRevealerTransitionType transition;

  GTK_WIDGET_CLASS (gtk_revealer_parent_class)->get_preferred_width_for_height (widget, height, &minimum_width, &natural_width);

  transition = effective_transition (revealer);
  if (transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT ||
      transition == GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT)
    natural_width = round (natural_width * priv->current_pos);

  minimum_width = natural_width;

  if (minimum_width_out)
    *minimum_width_out = minimum_width;
  if (natural_width_out)
    *natural_width_out = natural_width;
}

/**
 * gtk_revealer_get_transition_duration:
 * @revealer: a #GtkRevealer
 *
 * Returns the amount of time (in milliseconds) that
 * transitions will take.
 *
 * Returns: the transition duration
 *
 * Since: 3.10
 */
guint
gtk_revealer_get_transition_duration (GtkRevealer *revealer)
{
  g_return_val_if_fail (GTK_IS_REVEALER (revealer), 0);

  return revealer->priv->transition_duration;
}

/**
 * gtk_revealer_set_transition_duration:
 * @revealer: a #GtkRevealer
 * @duration: the new duration, in milliseconds
 *
 * Sets the duration that transitions will take.
 *
 * Since: 3.10
 */
void
gtk_revealer_set_transition_duration (GtkRevealer *revealer,
                                      guint        value)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  revealer->priv->transition_duration = value;
  g_object_notify (G_OBJECT (revealer), "transition-duration");
}

/**
 * gtk_revealer_get_transition_type:
 * @revealer: a #GtkRevealer
 *
 * Gets the type of animation that will be used
 * for transitions in @revealer.
 *
 * Return value: the current transition type of @revealer
 *
 * Since: 3.10
 */
GtkRevealerTransitionType
gtk_revealer_get_transition_type (GtkRevealer *revealer)
{
  g_return_val_if_fail (GTK_IS_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);

  return revealer->priv->transition_type;
}

/**
 * gtk_stack_set_transition_type:
 * @stack: a #GtkStack
 * @transition: the new transition type
 *
 * Sets the type of animation that will be used for
 * transitions in @revealer. Available types include
 * various kinds of fades and slides.
 *
 * Since: 3.10
 */
void
gtk_revealer_set_transition_type (GtkRevealer               *revealer,
                                  GtkRevealerTransitionType  transition)
{
  g_return_if_fail (GTK_IS_REVEALER (revealer));

  revealer->priv->transition_type = transition;
  gtk_widget_queue_resize (GTK_WIDGET (revealer));
  g_object_notify (G_OBJECT (revealer), "transition-type");
}
