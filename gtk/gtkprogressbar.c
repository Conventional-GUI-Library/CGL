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

#include <string.h>

#include "gtkprogressbar.h"
#include "gtkorientableprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#include "a11y/gtkprogressbaraccessible.h"

/**
 * SECTION:gtkprogressbar
 * @Short_description: A widget which indicates progress visually
 * @Title: GtkProgressBar
 *
 * The #GtkProgressBar is typically used to display the progress of a long
 * running operation.  It provides a visual clue that processing
 * is underway.  The #GtkProgressBar can be used in two different
 * modes: percentage mode and activity mode.
 *
 * When an application can determine how much work needs to take place
 * (e.g. read a fixed number of bytes from a file) and can monitor its
 * progress, it can use the #GtkProgressBar in percentage mode and the user
 * sees a growing bar indicating the percentage of the work that has
 * been completed.  In this mode, the application is required to call
 * gtk_progress_bar_set_fraction() periodically to update the progress bar.
 *
 * When an application has no accurate way of knowing the amount of work
 * to do, it can use the #GtkProgressBar in activity mode, which shows
 * activity by a block moving back and forth within the progress area. In
 * this mode, the application is required to call gtk_progress_bar_pulse()
 * periodically to update the progress bar.
 *
 * There is quite a bit of flexibility provided to control the appearance
 * of the #GtkProgressBar.  Functions are provided to control the
 * orientation of the bar, optional text can be displayed along with
 * the bar, and the step size used in activity mode can be set.
 */

#define MIN_HORIZONTAL_BAR_WIDTH   150
#define MIN_HORIZONTAL_BAR_HEIGHT  20
#define MIN_VERTICAL_BAR_WIDTH     22
#define MIN_VERTICAL_BAR_HEIGHT    80


struct _GtkProgressBarPrivate
{
  gchar         *text;

  gdouble        fraction;
  gdouble        pulse_fraction;

  gint           activity_pos;
  guint          activity_blocks;
  guint          activity_step;

  GtkOrientation orientation;

  guint          activity_dir  : 1;
  guint          activity_mode : 1;
  guint          ellipsize     : 3;
  guint          show_text     : 1;
  guint          inverted      : 1;
};

enum {
  PROP_0,
  PROP_FRACTION,
  PROP_PULSE_STEP,
  PROP_ORIENTATION,
  PROP_INVERTED,
  PROP_TEXT,
  PROP_SHOW_TEXT,
  PROP_ELLIPSIZE
};

static void gtk_progress_bar_set_property         (GObject        *object,
                                                   guint           prop_id,
                                                   const GValue   *value,
                                                   GParamSpec     *pspec);
static void gtk_progress_bar_get_property         (GObject        *object,
                                                   guint           prop_id,
                                                   GValue         *value,
                                                   GParamSpec     *pspec);
static void gtk_progress_bar_get_preferred_width  (GtkWidget      *widget,
                                                   gint           *minimum,
                                                   gint           *natural);
static void gtk_progress_bar_get_preferred_height (GtkWidget      *widget,
                                                   gint           *minimum,
                                                   gint           *natural);

static void     gtk_progress_bar_real_update      (GtkProgressBar *progress);
static gboolean gtk_progress_bar_draw             (GtkWidget      *widget,
                                                   cairo_t        *cr);
static void     gtk_progress_bar_act_mode_enter   (GtkProgressBar *progress);
static void     gtk_progress_bar_finalize         (GObject        *object);
static void     gtk_progress_bar_set_orientation  (GtkProgressBar *progress,
                                                   GtkOrientation  orientation);

G_DEFINE_TYPE_WITH_CODE (GtkProgressBar, gtk_progress_bar, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_progress_bar_class_init (GtkProgressBarClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass *) class;

  gobject_class->set_property = gtk_progress_bar_set_property;
  gobject_class->get_property = gtk_progress_bar_get_property;
  gobject_class->finalize = gtk_progress_bar_finalize;

  widget_class->draw = gtk_progress_bar_draw;
  widget_class->get_preferred_width = gtk_progress_bar_get_preferred_width;
  widget_class->get_preferred_height = gtk_progress_bar_get_preferred_height;

  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");

  g_object_class_install_property (gobject_class,
                                   PROP_INVERTED,
                                   g_param_spec_boolean ("inverted",
                                                         P_("Inverted"),
                                                         P_("Invert the direction in which the progress bar grows"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_FRACTION,
                                   g_param_spec_double ("fraction",
                                                        P_("Fraction"),
                                                        P_("The fraction of total work that has been completed"),
                                                        0.0, 1.0, 0.0,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_PULSE_STEP,
                                   g_param_spec_double ("pulse-step",
                                                        P_("Pulse Step"),
                                                        P_("The fraction of total progress to move the bouncing block when pulsed"),
                                                        0.0, 1.0, 0.1,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        P_("Text"),
                                                        P_("Text to be displayed in the progress bar"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkProgressBar:show_text:
   *
   * Sets whether the progressbar will show text superimposed
   * over the bar. The shown text is either the value of
   * the #GtkProgressBar::text property or, if that is %NULL,
   * the #GtkProgressBar::fraction value, as a percentage.
   *
   * To make a progress bar that is styled and sized suitably for containing
   * text (even if the actual text is blank), set #GtkProgressBar:show-text to
   * %TRUE and #GtkProgressBar:text to the empty string (not %NULL).
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SHOW_TEXT,
                                   g_param_spec_boolean ("show-text",
                                                         P_("Show text"),
                                                         P_("Whether the progress is shown as text."),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkProgressBar:ellipsize:
   *
   * The preferred place to ellipsize the string, if the progressbar does
   * not have enough room to display the entire string, specified as a
   * #PangoEllisizeMode.
   *
   * Note that setting this property to a value other than
   * %PANGO_ELLIPSIZE_NONE has the side-effect that the progressbar requests
   * only enough space to display the ellipsis "...". Another means to set a
   * progressbar's width is gtk_widget_set_size_request().
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ELLIPSIZE,
                                   g_param_spec_enum ("ellipsize",
                                                      P_("Ellipsize"),
                                                      P_("The preferred place to ellipsize the string, if the progress bar "
                                                         "does not have enough room to display the entire string, if at all."),
                                                      PANGO_TYPE_ELLIPSIZE_MODE,
                                                      PANGO_ELLIPSIZE_NONE,
                                                      GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("xspacing",
                                                             P_("X spacing"),
                                                             P_("Extra spacing applied to the width of a progress bar."),
                                                             0, G_MAXINT, 7,
                                                             G_PARAM_READWRITE));
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("yspacing",
                                                             P_("Y spacing"),
                                                             P_("Extra spacing applied to the height of a progress bar."),
                                                             0, G_MAXINT, 7,
                                                             G_PARAM_READWRITE));

  /**
   * GtkProgressBar:min-horizontal-bar-width:
   *
   * The minimum horizontal width of the progress bar.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-horizontal-bar-width",
                                                             P_("Minimum horizontal bar width"),
                                                             P_("The minimum horizontal width of the progress bar"),
                                                             1, G_MAXINT, MIN_HORIZONTAL_BAR_WIDTH,
                                                             G_PARAM_READWRITE));
  /**
   * GtkProgressBar:min-horizontal-bar-height:
   *
   * Minimum horizontal height of the progress bar.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-horizontal-bar-height",
                                                             P_("Minimum horizontal bar height"),
                                                             P_("Minimum horizontal height of the progress bar"),
                                                             1, G_MAXINT, MIN_HORIZONTAL_BAR_HEIGHT,
                                                             G_PARAM_READWRITE));
  /**
   * GtkProgressBar:min-vertical-bar-width:
   *
   * The minimum vertical width of the progress bar.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-vertical-bar-width",
                                                             P_("Minimum vertical bar width"),
                                                             P_("The minimum vertical width of the progress bar"),
                                                             1, G_MAXINT, MIN_VERTICAL_BAR_WIDTH,
                                                             G_PARAM_READWRITE));
  /**
   * GtkProgressBar:min-vertical-bar-height:
   *
   * The minimum vertical height of the progress bar.
   *
   * Since: 2.14
   */
  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_int ("min-vertical-bar-height",
                                                             P_("Minimum vertical bar height"),
                                                             P_("The minimum vertical height of the progress bar"),
                                                             1, G_MAXINT, MIN_VERTICAL_BAR_HEIGHT,
                                                             G_PARAM_READWRITE));

  g_type_class_add_private (class, sizeof (GtkProgressBarPrivate));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_PROGRESS_BAR_ACCESSIBLE);
}

static void
gtk_progress_bar_init (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv;

  pbar->priv = G_TYPE_INSTANCE_GET_PRIVATE (pbar,
                                            GTK_TYPE_PROGRESS_BAR,
                                            GtkProgressBarPrivate);
  priv = pbar->priv;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->inverted = FALSE;
  priv->pulse_fraction = 0.1;
  priv->activity_pos = 0;
  priv->activity_dir = 1;
  priv->activity_step = 3;
  priv->activity_blocks = 5;
  priv->ellipsize = PANGO_ELLIPSIZE_NONE;
  priv->show_text = FALSE;

  priv->text = NULL;
  priv->fraction = 0.0;

  gtk_widget_set_has_window (GTK_WIDGET (pbar), FALSE);
}

static void
gtk_progress_bar_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkProgressBar *pbar;

  pbar = GTK_PROGRESS_BAR (object);

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      gtk_progress_bar_set_orientation (pbar, g_value_get_enum (value));
      break;
    case PROP_INVERTED:
      gtk_progress_bar_set_inverted (pbar, g_value_get_boolean (value));
      break;
    case PROP_FRACTION:
      gtk_progress_bar_set_fraction (pbar, g_value_get_double (value));
      break;
    case PROP_PULSE_STEP:
      gtk_progress_bar_set_pulse_step (pbar, g_value_get_double (value));
      break;
    case PROP_TEXT:
      gtk_progress_bar_set_text (pbar, g_value_get_string (value));
      break;
    case PROP_SHOW_TEXT:
      gtk_progress_bar_set_show_text (pbar, g_value_get_boolean (value));
      break;
    case PROP_ELLIPSIZE:
      gtk_progress_bar_set_ellipsize (pbar, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_progress_bar_get_property (GObject      *object,
                               guint         prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (object);
  GtkProgressBarPrivate* priv = pbar->priv;

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, priv->inverted);
      break;
    case PROP_FRACTION:
      g_value_set_double (value, priv->fraction);
      break;
    case PROP_PULSE_STEP:
      g_value_set_double (value, priv->pulse_fraction);
      break;
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;
    case PROP_SHOW_TEXT:
      g_value_set_boolean (value, priv->show_text);
      break;
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, priv->ellipsize);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_progress_bar_new:
 *
 * Creates a new #GtkProgressBar.
 *
 * Returns: a #GtkProgressBar.
 */
GtkWidget*
gtk_progress_bar_new (void)
{
  GtkWidget *pbar;

  pbar = g_object_new (GTK_TYPE_PROGRESS_BAR, NULL);

  return pbar;
}

static void
gtk_progress_bar_real_update (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv;
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;
  widget = GTK_WIDGET (pbar);

  if (priv->activity_mode)
    {
      GtkAllocation allocation;
      GtkStyleContext *context;
      GtkStateFlags state;
      GtkBorder padding;
      gint size;

      gtk_widget_get_allocation (widget, &allocation);
      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_padding (context, state, &padding);

      /* advance the block */
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          /* Update our activity step. */
          priv->activity_step = allocation.width * priv->pulse_fraction;

          size = MAX (2, allocation.width / priv->activity_blocks);

          if (priv->activity_dir == 0)
            {
              priv->activity_pos += priv->activity_step;
              if (priv->activity_pos + size >= allocation.width - padding.left)
                {
                  priv->activity_pos = allocation.width - padding.left - size;
                  priv->activity_dir = 1;
                }
            }
          else
            {
              priv->activity_pos -= priv->activity_step;
              if (priv->activity_pos <= padding.left)
                {
                  priv->activity_pos = padding.left;
                  priv->activity_dir = 0;
                }
            }
        }
      else
        {
          /* Update our activity step. */
          priv->activity_step = allocation.height * priv->pulse_fraction;

          size = MAX (2, allocation.height / priv->activity_blocks);

          if (priv->activity_dir == 0)
            {
              priv->activity_pos += priv->activity_step;
              if (priv->activity_pos + size >= allocation.height - padding.top)
                {
                  priv->activity_pos = allocation.height - padding.top - size;
                  priv->activity_dir = 1;
                }
            }
          else
            {
              priv->activity_pos -= priv->activity_step;
              if (priv->activity_pos <= padding.top)
                {
                  priv->activity_pos = padding.top;
                  priv->activity_dir = 0;
                }
            }
        }
    }
  gtk_widget_queue_draw (widget);
}

static void
gtk_progress_bar_finalize (GObject *object)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (object);
  GtkProgressBarPrivate *priv = pbar->priv;

  g_free (priv->text);

  G_OBJECT_CLASS (gtk_progress_bar_parent_class)->finalize (object);
}

static gchar *
get_current_text (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;

  if (priv->text)
    return g_strdup (priv->text);
  else
    return g_strdup_printf ("%.0f %%", priv->fraction * 100.0);
}

static void
gtk_progress_bar_get_preferred_width (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  GtkProgressBar *pbar;
  GtkProgressBarPrivate *priv;
  GtkStyleContext *style_context;
  GtkStateFlags state;
  GtkBorder padding;
  gchar *buf;
  PangoRectangle logical_rect;
  PangoLayout *layout;
  gint width;
  gint xspacing;
  gint min_width;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (widget));

  style_context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (style_context, state, &padding);

  gtk_widget_style_get (widget,
                        "xspacing", &xspacing,
                        NULL);

  pbar = GTK_PROGRESS_BAR (widget);
  priv = pbar->priv;

  width = padding.left + padding.right + xspacing;

  if (priv->show_text)
    {
      buf = get_current_text (pbar);
      layout = gtk_widget_create_pango_layout (widget, buf);

      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      if (priv->ellipsize)
        {
	  const PangoFontDescription *font_desc;
          PangoContext *context;
          PangoFontMetrics *metrics;
          gint char_width;

          /* The minimum size for ellipsized text is ~ 3 chars */
          context = pango_layout_get_context (layout);
          font_desc = gtk_style_context_get_font (style_context, state);
          metrics = pango_context_get_metrics (context, font_desc, pango_context_get_language (context));

          char_width = pango_font_metrics_get_approximate_char_width (metrics);
          pango_font_metrics_unref (metrics);

          width += PANGO_PIXELS (char_width) * 3;
        }
      else
        width += logical_rect.width;

      g_object_unref (layout);
      g_free (buf);
    }

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_style_get (widget,
                          "min-horizontal-bar-width", &min_width,
                          NULL);
  else
    gtk_widget_style_get (widget,
                          "min-vertical-bar-width", &min_width,
                          NULL);

  *minimum = *natural = MAX (min_width, width);
}

static void
gtk_progress_bar_get_preferred_height (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  GtkProgressBar *pbar;
  GtkProgressBarPrivate *priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  gchar *buf;
  PangoRectangle logical_rect;
  PangoLayout *layout;
  gint height;
  gint yspacing;
  gint min_height;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (widget));

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  gtk_widget_style_get (widget,
                        "yspacing", &yspacing,
                        NULL);

  pbar = GTK_PROGRESS_BAR (widget);
  priv = pbar->priv;

  height = padding.top + padding.bottom + yspacing;

  if (priv->show_text)
    {
      buf = get_current_text (pbar);
      layout = gtk_widget_create_pango_layout (widget, buf);

      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      height += logical_rect.height;

      g_object_unref (layout);
      g_free (buf);
    }

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    gtk_widget_style_get (widget,
                          "min-horizontal-bar-height", &min_height,
                          NULL);
  else
    gtk_widget_style_get (widget,
                          "min-vertical-bar-height", &min_height,
                          NULL);

  *minimum = *natural = MAX (min_height, height);
}

static void
gtk_progress_bar_act_mode_enter (GtkProgressBar *pbar)
{
  GtkProgressBarPrivate *priv = pbar->priv;
  GtkAllocation allocation;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkWidget *widget = GTK_WIDGET (pbar);
  GtkOrientation orientation;
  gboolean inverted;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  orientation = priv->orientation;
  inverted = priv->inverted;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        inverted = !inverted;
    }

  /* calculate start pos */

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (!inverted)
        {
          priv->activity_pos = padding.left;
          priv->activity_dir = 0;
        }
      else
        {
          gtk_widget_get_allocation (widget, &allocation);
          priv->activity_pos = allocation.width - padding.left -
                               (allocation.height - padding.top - padding.bottom);
          priv->activity_dir = 1;
        }
    }
  else
    {
      if (!inverted)
        {
          priv->activity_pos = padding.top;
          priv->activity_dir = 0;
        }
      else
        {
          gtk_widget_get_allocation (widget, &allocation);
          priv->activity_pos = allocation.height - padding.top -
                               (allocation.width - padding.left - padding.right);
          priv->activity_dir = 1;
        }
    }
}

static void
gtk_progress_bar_get_activity (GtkProgressBar *pbar,
                               GtkOrientation  orientation,
                               gint           *offset,
                               gint           *amount)
{
  GtkProgressBarPrivate *priv = pbar->priv;
  GtkAllocation allocation;
  GtkWidget *widget = GTK_WIDGET (pbar);

  *offset = priv->activity_pos;

  gtk_widget_get_allocation (widget, &allocation);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *amount = MAX (2, allocation.width / priv->activity_blocks);
  else
    *amount = MAX (2, allocation.height / priv->activity_blocks);
}

static void
gtk_progress_bar_paint_activity (GtkProgressBar *pbar,
                                 cairo_t        *cr,
                                 GtkOrientation  orientation,
                                 gboolean        inverted,
                                 int             width,
                                 int             height)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkWidget *widget = GTK_WIDGET (pbar);
  GdkRectangle area;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_progress_bar_get_activity (pbar, orientation, &area.x, &area.width);
      area.y = padding.top;
      area.height = height - padding.top - padding.bottom;
    }
  else
    {
      gtk_progress_bar_get_activity (pbar, orientation, &area.y, &area.height);
      area.x = padding.left;
      area.width = width - padding.left - padding.right;
    }

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_PROGRESSBAR);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_PULSE);

  gtk_render_activity (context, cr, area.x, area.y, area.width, area.height);

  gtk_style_context_restore (context);
}

static void
gtk_progress_bar_paint_continuous (GtkProgressBar *pbar,
                                   cairo_t        *cr,
                                   gint            amount,
                                   GtkOrientation  orientation,
                                   gboolean        inverted,
                                   int             width,
                                   int             height)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkWidget *widget = GTK_WIDGET (pbar);
  GdkRectangle area;

  if (amount <= 0)
    return;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      area.width = amount;
      area.height = height - padding.top - padding.bottom;
      area.y = padding.top;

      if (!inverted)
        area.x = padding.left;
      else
        area.x = width - amount - padding.right;
    }
  else
    {
      area.width = width - padding.left - padding.right;
      area.height = amount;
      area.x = padding.left;

      if (!inverted)
        area.y = padding.top;
      else
        area.y = height - amount - padding.bottom;
    }

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_PROGRESSBAR);

  gtk_render_activity (context, cr, area.x, area.y, area.width, area.height);

  gtk_style_context_restore (context);
}

static void
gtk_progress_bar_paint_text (GtkProgressBar *pbar,
                             cairo_t        *cr,
                             gint            offset,
                             gint            amount,
                             GtkOrientation  orientation,
                             gboolean        inverted,
                             int             width,
                             int             height)
{
  GtkProgressBarPrivate *priv = pbar->priv;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  GtkWidget *widget = GTK_WIDGET (pbar);
  gint x;
  gint y;
  gchar *buf;
  GdkRectangle rect;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  GdkRectangle prelight_clip, start_clip, end_clip;
  gfloat text_xalign = 0.5;
  gfloat text_yalign = 0.5;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  if (gtk_widget_get_direction (widget) != GTK_TEXT_DIR_LTR)
    text_xalign = 1.0 - text_xalign;

  buf = get_current_text (pbar);

  layout = gtk_widget_create_pango_layout (widget, buf);
  pango_layout_set_ellipsize (layout, priv->ellipsize);
  if (priv->ellipsize)
    pango_layout_set_width (layout, width * PANGO_SCALE);

  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

  x = padding.left + 1 + text_xalign * (width - padding.left - padding.right - 2 - logical_rect.width);
  y = padding.top + 1 + text_yalign * (height - padding.top - padding.bottom - 2 - logical_rect.height);

  rect.x = padding.left;
  rect.y = padding.top;
  rect.width = width - padding.left - padding.right;
  rect.height = height - padding.top - padding.bottom;

  prelight_clip = start_clip = end_clip = rect;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (!inverted)
        {
          if (offset != -1)
            prelight_clip.x = offset;
          prelight_clip.width = amount;
          start_clip.width = prelight_clip.x - start_clip.x;
          end_clip.x = start_clip.x + start_clip.width + prelight_clip.width;
          end_clip.width -= prelight_clip.width + start_clip.width;
        }
      else
        {
          if (offset != -1)
            prelight_clip.x = offset;
          else
            prelight_clip.x = rect.x + rect.width - amount;
          prelight_clip.width = amount;
          start_clip.width = prelight_clip.x - start_clip.x;
          end_clip.x = start_clip.x + start_clip.width + prelight_clip.width;
          end_clip.width -= prelight_clip.width + start_clip.width;
        }
    }
  else
    {
      if (!inverted)
        {
          if (offset != -1)
            prelight_clip.y = offset;
          prelight_clip.height = amount;
          start_clip.height = prelight_clip.y - start_clip.y;
          end_clip.y = start_clip.y + start_clip.height + prelight_clip.height;
          end_clip.height -= prelight_clip.height + start_clip.height;
        }
      else
        {
          if (offset != -1)
            prelight_clip.y = offset;
          else
            prelight_clip.y = rect.y + rect.height - amount;
          prelight_clip.height = amount;
          start_clip.height = prelight_clip.y - start_clip.y;
          end_clip.y = start_clip.y + start_clip.height + prelight_clip.height;
          end_clip.height -= prelight_clip.height + start_clip.height;
        }
    }

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);

  if (start_clip.width > 0 && start_clip.height > 0)
    {
      cairo_save (cr);
      gdk_cairo_rectangle (cr, &start_clip);
      cairo_clip (cr);

      gtk_render_layout (context, cr, x, y, layout);
      cairo_restore (cr);
    }

  if (end_clip.width > 0 && end_clip.height > 0)
    {
      cairo_save (cr);
      gdk_cairo_rectangle (cr, &end_clip);
      cairo_clip (cr);

      gtk_render_layout (context, cr, x, y, layout);
      cairo_restore (cr);
    }

  gtk_style_context_restore (context);

  cairo_save (cr);
  gdk_cairo_rectangle (cr, &prelight_clip);
  cairo_clip (cr);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_PROGRESSBAR);

  gtk_render_layout (context, cr, x, y, layout);

  gtk_style_context_restore (context);
  cairo_restore (cr);

  g_object_unref (layout);
  g_free (buf);
}

static gboolean
gtk_progress_bar_draw (GtkWidget      *widget,
                       cairo_t        *cr)
{
  GtkProgressBar *pbar = GTK_PROGRESS_BAR (widget);
  GtkProgressBarPrivate *priv = pbar->priv;
  GtkOrientation orientation;
  gboolean inverted;
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder padding;
  int width, height;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_padding (context, state, &padding);

  orientation = priv->orientation;
  inverted = priv->inverted;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
        inverted = !inverted;
    }
  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);

  gtk_render_background (context, cr, 0, 0, width, height);
  gtk_render_frame (context, cr, 0, 0, width, height);

  gtk_style_context_restore (context);

  if (priv->activity_mode)
    {
      gtk_progress_bar_paint_activity (pbar, cr,
                                       orientation, inverted,
                                       width, height);

      if (priv->show_text)
        {
          gint offset;
          gint amount;

          gtk_progress_bar_get_activity (pbar, orientation, &offset, &amount);
          gtk_progress_bar_paint_text (pbar, cr,
                                       offset, amount,
                                       orientation, inverted,
                                       width, height);
        }
    }
  else
    {
      gint amount;
      gint space;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        space = width - padding.left - padding.right;
      else
        space = height - padding.top - padding.bottom;

      amount = space * gtk_progress_bar_get_fraction (pbar);

      gtk_progress_bar_paint_continuous (pbar, cr, amount, orientation, inverted, width, height);

      if (priv->show_text)
        gtk_progress_bar_paint_text (pbar, cr, -1, amount, orientation, inverted, width, height);
    }

  return FALSE;
}

static void
gtk_progress_bar_set_activity_mode (GtkProgressBar *pbar,
                                    gboolean        activity_mode)
{
  GtkProgressBarPrivate *priv = pbar->priv;

  activity_mode = !!activity_mode;

  if (priv->activity_mode != activity_mode)
    {
      priv->activity_mode = activity_mode;

      if (priv->activity_mode)
        gtk_progress_bar_act_mode_enter (pbar);

      gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

/**
 * gtk_progress_bar_set_fraction:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction of the task that's been completed
 *
 * Causes the progress bar to "fill in" the given fraction
 * of the bar. The fraction should be between 0.0 and 1.0,
 * inclusive.
 *
 */
void
gtk_progress_bar_set_fraction (GtkProgressBar *pbar,
                               gdouble         fraction)
{
  GtkProgressBarPrivate* priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  priv->fraction = CLAMP(fraction, 0.0, 1.0);
  gtk_progress_bar_set_activity_mode (pbar, FALSE);
  gtk_progress_bar_real_update (pbar);

  g_object_notify (G_OBJECT (pbar), "fraction");
}

/**
 * gtk_progress_bar_pulse:
 * @pbar: a #GtkProgressBar
 *
 * Indicates that some progress is made, but you don't know how much.
 * Causes the progress bar to enter "activity mode," where a block
 * bounces back and forth. Each call to gtk_progress_bar_pulse()
 * causes the block to move by a little bit (the amount of movement
 * per pulse is determined by gtk_progress_bar_set_pulse_step()).
 **/
void
gtk_progress_bar_pulse (GtkProgressBar *pbar)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  gtk_progress_bar_set_activity_mode (pbar, TRUE);
  gtk_progress_bar_real_update (pbar);
}

/**
 * gtk_progress_bar_set_text:
 * @pbar: a #GtkProgressBar
 * @text: (allow-none): a UTF-8 string, or %NULL
 *
 * Causes the given @text to appear superimposed on the progress bar.
 *
 * If @text is %NULL and #GtkProgressBar:show-text is %TRUE, the current
 * value of #GtkProgressBar:fraction will be displayed as a percentage.
 *
 * If @text is non-%NULL and #GtkProgressBar:show-text is %TRUE, the text will
 * be displayed. In this case, it will not display the progress percentage.
 * If @text is the empty string, the progress bar will still be styled and sized
 * suitably for containing text, as long as #GtkProgressBar:show-text is %TRUE.
 **/
void
gtk_progress_bar_set_text (GtkProgressBar *pbar,
                           const gchar    *text)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  /* Don't notify again if nothing's changed. */
  if (g_strcmp0 (priv->text, text) == 0)
    return;

  g_free (priv->text);
  priv->text = g_strdup (text);

  gtk_widget_queue_resize (GTK_WIDGET (pbar));

  g_object_notify (G_OBJECT (pbar), "text");
}

/**
 * gtk_progress_bar_set_show_text:
 * @pbar: a #GtkProgressBar
 * @show_text: whether to show superimposed text
 *
 * Sets whether the progressbar will show text superimposed
 * over the bar. The shown text is either the value of
 * the #GtkProgressBar::text property or, if that is %NULL,
 * the #GtkProgressBar::fraction value, as a percentage.
 *
 * To make a progress bar that is styled and sized suitably for containing
 * text (even if the actual text is blank), set #GtkProgressBar:show-text to
 * %TRUE and #GtkProgressBar:text to the empty string (not %NULL).
 *
 * Since: 3.0
 */
void
gtk_progress_bar_set_show_text (GtkProgressBar *pbar,
                                gboolean        show_text)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  show_text = !!show_text;

  if (priv->show_text != show_text)
    {
      priv->show_text = show_text;

      gtk_widget_queue_resize (GTK_WIDGET (pbar));

      g_object_notify (G_OBJECT (pbar), "show-text");
    }
}

/**
 * gtk_progress_bar_get_show_text:
 * @pbar: a #GtkProgressBar
 *
 * Gets the value of the #GtkProgressBar::show-text property.
 * See gtk_progress_bar_set_show_text().
 *
 * Returns: %TRUE if text is shown in the progress bar
 *
 * Since: 3.0
 */
gboolean
gtk_progress_bar_get_show_text (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), FALSE);

  return pbar->priv->show_text;
}

/**
 * gtk_progress_bar_set_pulse_step:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction between 0.0 and 1.0
 *
 * Sets the fraction of total progress bar length to move the
 * bouncing block for each call to gtk_progress_bar_pulse().
 **/
void
gtk_progress_bar_set_pulse_step (GtkProgressBar *pbar,
                                 gdouble         fraction)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  priv->pulse_fraction = fraction;

  g_object_notify (G_OBJECT (pbar), "pulse-step");
}

static void
gtk_progress_bar_set_orientation (GtkProgressBar *pbar,
                                  GtkOrientation  orientation)
{
  GtkProgressBarPrivate *priv = pbar->priv;

  if (priv->orientation != orientation)
    {
      priv->orientation = orientation;
      _gtk_orientable_set_style_classes (GTK_ORIENTABLE (pbar));

      gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

/**
 * gtk_progress_bar_set_inverted:
 * @pbar: a #GtkProgressBar
 * @inverted: %TRUE to invert the progress bar
 *
 * Progress bars normally grow from top to bottom or left to right.
 * Inverted progress bars grow in the opposite direction.
 */
void
gtk_progress_bar_set_inverted (GtkProgressBar *pbar,
                               gboolean        inverted)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  priv = pbar->priv;

  if (priv->inverted != inverted)
    {
      priv->inverted = inverted;

      gtk_widget_queue_resize (GTK_WIDGET (pbar));

      g_object_notify (G_OBJECT (pbar), "inverted");
    }
}

/**
 * gtk_progress_bar_get_text:
 * @pbar: a #GtkProgressBar
 *
 * Retrieves the text displayed superimposed on the progress bar,
 * if any, otherwise %NULL. The return value is a reference
 * to the text, not a copy of it, so will become invalid
 * if you change the text in the progress bar.
 *
 * Return value: text, or %NULL; this string is owned by the widget
 * and should not be modified or freed.
 **/
const gchar*
gtk_progress_bar_get_text (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), NULL);

  return pbar->priv->text;
}

/**
 * gtk_progress_bar_get_fraction:
 * @pbar: a #GtkProgressBar
 *
 * Returns the current fraction of the task that's been completed.
 *
 * Return value: a fraction from 0.0 to 1.0
 **/
gdouble
gtk_progress_bar_get_fraction (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->priv->fraction;
}

/**
 * gtk_progress_bar_get_pulse_step:
 * @pbar: a #GtkProgressBar
 *
 * Retrieves the pulse step set with gtk_progress_bar_set_pulse_step()
 *
 * Return value: a fraction from 0.0 to 1.0
 **/
gdouble
gtk_progress_bar_get_pulse_step (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->priv->pulse_fraction;
}

/**
 * gtk_progress_bar_get_inverted:
 * @pbar: a #GtkProgressBar
 *
 * Gets the value set by gtk_progress_bar_set_inverted()
 *
 * Return value: %TRUE if the progress bar is inverted
 */
gboolean
gtk_progress_bar_get_inverted (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), FALSE);

  return pbar->priv->inverted;
}

/**
 * gtk_progress_bar_set_ellipsize:
 * @pbar: a #GtkProgressBar
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") the text
 * if there is not enough space to render the entire string.
 *
 * Since: 2.6
 **/
void
gtk_progress_bar_set_ellipsize (GtkProgressBar     *pbar,
                                PangoEllipsizeMode  mode)
{
  GtkProgressBarPrivate *priv;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE &&
                    mode <= PANGO_ELLIPSIZE_END);

  priv = pbar->priv;

  if ((PangoEllipsizeMode)priv->ellipsize != mode)
    {
      priv->ellipsize = mode;

      g_object_notify (G_OBJECT (pbar), "ellipsize");
      gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

/**
 * gtk_progress_bar_get_ellipsize:
 * @pbar: a #GtkProgressBar
 *
 * Returns the ellipsizing position of the progressbar.
 * See gtk_progress_bar_set_ellipsize().
 *
 * Return value: #PangoEllipsizeMode
 *
 * Since: 2.6
 **/
PangoEllipsizeMode
gtk_progress_bar_get_ellipsize (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), PANGO_ELLIPSIZE_NONE);

  return pbar->priv->ellipsize;
}
