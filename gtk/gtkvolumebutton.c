/* GTK - The GIMP Toolkit
 * Copyright (C) 2007 Red Hat, Inc.
 *
 * Authors:
 * - Bastien Nocera <bnocera@redhat.com>
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
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkvolumebutton.h"
#include "gtkstock.h"
#include "gtktooltip.h"
#include "gtkintl.h"


/**
 * SECTION:gtkvolumebutton
 * @Short_description: A button which pops up a volume control
 * @Title: GtkVolumeButton
 *
 * #GtkVolumeButton is a subclass of #GtkScaleButton that has
 * been tailored for use as a volume control widget with suitable
 * icons, tooltips and accessible labels.
 */

#define EPSILON (1e-10)

static const gchar * const icons[] =
{
  "audio-volume-muted",
  "audio-volume-high",
  "audio-volume-low",
  "audio-volume-medium",
  NULL
};

static const gchar * const icons_symbolic[] =
{
  "audio-volume-muted-symbolic",
  "audio-volume-high-symbolic",
  "audio-volume-low-symbolic",
  "audio-volume-medium-symbolic",
  NULL
};

enum
{
  PROP_0,
  PROP_SYMBOLIC
};

static gboolean	cb_query_tooltip (GtkWidget       *button,
                                  gint             x,
                                  gint             y,
                                  gboolean         keyboard_mode,
                                  GtkTooltip      *tooltip,
                                  gpointer         user_data);
static void	cb_value_changed (GtkVolumeButton *button,
                                  gdouble          value,
                                  gpointer         user_data);

G_DEFINE_TYPE (GtkVolumeButton, gtk_volume_button, GTK_TYPE_SCALE_BUTTON)

static void
gtk_volume_button_set_property (GObject       *object,
				guint          prop_id,
				const GValue  *value,
				GParamSpec    *pspec)
{
  GtkScaleButton *button = GTK_SCALE_BUTTON (object);

  switch (prop_id)
    {
    case PROP_SYMBOLIC:
      if (g_value_get_boolean (value))
        gtk_scale_button_set_icons (button, (const char **) icons_symbolic);
      else
	gtk_scale_button_set_icons (button, (const char **) icons);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_volume_button_get_property (GObject     *object,
			        guint        prop_id,
			        GValue      *value,
			        GParamSpec  *pspec)
{
  switch (prop_id)
    {
    case PROP_SYMBOLIC: {
      char **icon_list;

      g_object_get (object, "icons", &icon_list, NULL);
      if (icon_list != NULL &&
	  icon_list[0] != NULL &&
      	  g_str_equal (icon_list[0], icons_symbolic[0]))
        g_value_set_boolean (value, TRUE);
      else
        g_value_set_boolean (value, FALSE);
      g_strfreev (icon_list);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_volume_button_class_init (GtkVolumeButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_volume_button_set_property;
  gobject_class->get_property = gtk_volume_button_get_property;

  /**
   * GtkVolumeButton:use-symbolic:
   *
   * Whether to use symbolic icons as the icons. Note that
   * if the symbolic icons are not available in your installed
   * theme, then the normal (potentially colorful) icons will
   * be used.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_SYMBOLIC,
                                   g_param_spec_boolean ("use-symbolic",
                                                         P_("Use symbolic icons"),
                                                         P_("Whether to use symbolic icons"),
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

static void
gtk_volume_button_init (GtkVolumeButton *button)
{
  GtkScaleButton *sbutton = GTK_SCALE_BUTTON (button);
  GtkAdjustment *adj;
  GtkWidget *minus_button, *plus_button;

  atk_object_set_name (gtk_widget_get_accessible (GTK_WIDGET (button)),
		       _("Volume"));
  atk_object_set_description (gtk_widget_get_accessible (GTK_WIDGET (button)),
		       _("Turns volume down or up"));
  atk_action_set_description (ATK_ACTION (gtk_widget_get_accessible (GTK_WIDGET (button))),
			      1,
			      _("Adjusts the volume"));

  minus_button = gtk_scale_button_get_minus_button (sbutton);
  plus_button = gtk_scale_button_get_plus_button (sbutton);

  atk_object_set_name (gtk_widget_get_accessible (minus_button),
		       _("Volume Down"));
  atk_object_set_description (gtk_widget_get_accessible (minus_button),
		       _("Decreases the volume"));
  gtk_widget_set_tooltip_text (minus_button, _("Volume Down"));

  atk_object_set_name (gtk_widget_get_accessible (plus_button),
		       _("Volume Up"));
  atk_object_set_description (gtk_widget_get_accessible (plus_button),
		       _("Increases the volume"));
  gtk_widget_set_tooltip_text (plus_button, _("Volume Up"));

  gtk_scale_button_set_icons (sbutton, (const char **) icons);

  adj = gtk_adjustment_new (0., 0., 1., 0.02, 0.2, 0.);
  g_object_set (G_OBJECT (button),
		"adjustment", adj,
		"size", GTK_ICON_SIZE_SMALL_TOOLBAR,
		"has-tooltip", TRUE,
		NULL);

  g_signal_connect (G_OBJECT (button), "query-tooltip",
		    G_CALLBACK (cb_query_tooltip), NULL);
  g_signal_connect (G_OBJECT (button), "value-changed",
		    G_CALLBACK (cb_value_changed), NULL);
}

/**
 * gtk_volume_button_new
 *
 * Creates a #GtkVolumeButton, with a range between 0.0 and 1.0, with
 * a stepping of 0.02. Volume values can be obtained and modified using
 * the functions from #GtkScaleButton.
 *
 * Return value: a new #GtkVolumeButton
 *
 * Since: 2.12
 */
GtkWidget *
gtk_volume_button_new (void)
{
  GObject *button;
  button = g_object_new (GTK_TYPE_VOLUME_BUTTON, NULL);
  return GTK_WIDGET (button);
}

static gboolean
cb_query_tooltip (GtkWidget  *button,
		  gint        x,
		  gint        y,
		  gboolean    keyboard_mode,
		  GtkTooltip *tooltip,
		  gpointer    user_data)
{
  GtkScaleButton *scale_button = GTK_SCALE_BUTTON (button);
  GtkAdjustment *adjustment;
  gdouble val;
  char *str;
  AtkImage *image;

  image = ATK_IMAGE (gtk_widget_get_accessible (button));

  adjustment = gtk_scale_button_get_adjustment (scale_button);
  val = gtk_scale_button_get_value (scale_button);

  if (val < (gtk_adjustment_get_lower (adjustment) + EPSILON))
    {
      str = g_strdup (_("Muted"));
    }
  else if (val >= (gtk_adjustment_get_upper (adjustment) - EPSILON))
    {
      str = g_strdup (_("Full Volume"));
    }
  else
    {
      int percent;

      percent = (int) (100. * val / (gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_lower (adjustment)) + .5);

      /* Translators: this is the percentage of the current volume,
       * as used in the tooltip, eg. "49 %".
       * Translate the "%d" to "%Id" if you want to use localised digits,
       * or otherwise translate the "%d" to "%d".
       */
      str = g_strdup_printf (C_("volume percentage", "%d %%"), percent);
    }

  gtk_tooltip_set_text (tooltip, str);
  atk_image_set_image_description (image, str);
  g_free (str);

  return TRUE;
}

static void
cb_value_changed (GtkVolumeButton *button, gdouble value, gpointer user_data)
{
  gtk_widget_trigger_tooltip_query (GTK_WIDGET (button));
}
