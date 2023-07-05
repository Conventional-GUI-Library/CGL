/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcolorchooserprivate.h"
#include "gtkcolorchooserwidget.h"
#include "gtkcolorsel.h"
#include "gtkbox.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkorientable.h"

/**
 * SECTION:gtkcolorchooserwidget
 * @Short_description: A widget for choosing colors
 * @Title: GtkColorChooserWidget
 * @See_also: #GtkColorChooserDialog
 *
 * The #GtkColorChooserWidget widget lets the user select a
 * color. By default, the chooser presents a prefined palette
 * of colors, plus a small number of settable custom colors.
 * It is also possible to select a different color with the
 * single-color editor. To enter the single-color editing mode,
 * use the context menu of any color of the palette, or use the
 * '+' button to add a new custom color.
 *
 * The chooser automatically remembers the last selection, as well
 * as custom colors.
 *
 * To change the initially selected color, use gtk_color_chooser_set_rgba().
 * To get the selected font use gtk_color_chooser_get_rgba().
 *
 * The #GtkColorChooserWidget is used in the #GtkColorChooserDialog
 * to provide a dialog for selecting colors.
 *
 * Since: 3.4
 */

struct _GtkColorChooserWidgetPrivate
{
  GtkWidget *color_sel_widget;
};

enum
{
  PROP_ZERO,
  PROP_RGBA,
  PROP_USE_ALPHA,
  PROP_SHOW_EDITOR
};

static void gtk_color_chooser_widget_iface_init (GtkColorChooserInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorChooserWidget, gtk_color_chooser_widget, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_COLOR_CHOOSER,
                                                gtk_color_chooser_widget_iface_init))
                                                
static void
gtk_color_chooser_widget_set_use_alpha (GtkColorChooserWidget *cc,
                                        gboolean               use_alpha)
{
 gtk_color_selection_set_has_opacity_control(GTK_COLOR_SELECTION(cc->priv->color_sel_widget), use_alpha);
}

static void
gtk_color_chooser_widget_set_show_editor (GtkColorChooserWidget *cc,
                                          gboolean               show_editor)
{
  // again, this is unimplemented
}

static void
gtk_color_chooser_widget_init (GtkColorChooserWidget *cc)
{
  cc->priv = G_TYPE_INSTANCE_GET_PRIVATE (cc, GTK_TYPE_COLOR_CHOOSER_WIDGET, GtkColorChooserWidgetPrivate);

  cc->priv->color_sel_widget = gtk_color_selection_new();

  gtk_box_pack_start(GTK_BOX(cc), cc->priv->color_sel_widget, TRUE, TRUE, 0);
  
  gtk_widget_show_all (GTK_WIDGET (cc));

  gtk_orientable_set_orientation(GTK_ORIENTABLE(cc), gtk_orientable_get_orientation(GTK_ORIENTABLE(cc->priv->color_sel_widget)));
}

static void
gtk_color_chooser_widget_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GtkColorChooserWidget *cw = GTK_COLOR_CHOOSER_WIDGET (object);
  GtkColorChooser *cc = GTK_COLOR_CHOOSER (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      {
        GdkRGBA color;

        gtk_color_selection_get_current_rgba(GTK_COLOR_SELECTION(cw->priv->color_sel_widget), &color);
        g_value_set_boxed (value, &color);
      }
      break;
    case PROP_USE_ALPHA:
      g_value_set_boolean (value, gtk_color_selection_get_has_opacity_control(GTK_COLOR_SELECTION(cw->priv->color_sel_widget)));
      break;
    case PROP_SHOW_EDITOR:
	  // this is also unimplemented
      g_value_set_boolean (value, TRUE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_widget_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case PROP_RGBA:
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (cc),
                                  g_value_get_boxed (value));
      break;
    case PROP_USE_ALPHA:
      gtk_color_chooser_widget_set_use_alpha (cc,
                                              g_value_get_boolean (value));
      break;
    case PROP_SHOW_EDITOR:
      gtk_color_chooser_widget_set_show_editor (cc,
                                                g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_color_chooser_widget_finalize (GObject *object)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (object);
  gtk_widget_destroy(cc->priv->color_sel_widget);
  G_OBJECT_CLASS (gtk_color_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_color_chooser_widget_class_init (GtkColorChooserWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->get_property = gtk_color_chooser_widget_get_property;
  object_class->set_property = gtk_color_chooser_widget_set_property;
  object_class->finalize = gtk_color_chooser_widget_finalize;

  g_object_class_override_property (object_class, PROP_RGBA, "rgba");
  g_object_class_override_property (object_class, PROP_USE_ALPHA, "use-alpha");

  // this property currently does nothing
  g_object_class_install_property (object_class, PROP_SHOW_EDITOR,
      g_param_spec_boolean ("show-editor", P_("Show editor"), P_("Show editor"),
                            FALSE, GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkColorChooserWidgetPrivate));
}

static void
gtk_color_chooser_widget_get_rgba (GtkColorChooser *chooser,
                                   GdkRGBA         *color)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);
  gtk_color_selection_get_current_rgba(GTK_COLOR_SELECTION(cc->priv->color_sel_widget), color);
}



static void
gtk_color_chooser_widget_set_rgba (GtkColorChooser *chooser,
                                   const GdkRGBA   *color)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);
  gtk_color_selection_set_current_rgba(GTK_COLOR_SELECTION(cc->priv->color_sel_widget), color);
}

static void
gtk_color_chooser_widget_add_palette (GtkColorChooser *chooser,
                                      GtkOrientation   orientation,
                                      gint             colors_per_line,
                                      gint             n_colors,
                                      GdkRGBA         *colors)
{
  GtkColorChooserWidget *cc = GTK_COLOR_CHOOSER_WIDGET (chooser);

  //implement this
}

static void
gtk_color_chooser_widget_iface_init (GtkColorChooserInterface *iface)
{
  iface->get_rgba = gtk_color_chooser_widget_get_rgba;
  iface->set_rgba = gtk_color_chooser_widget_set_rgba;
  iface->add_palette = gtk_color_chooser_widget_add_palette;
}


/**
 * gtk_color_chooser_widget_new:
 *
 * Creates a new #GtkColorChooserWidget.
 *
 * Returns: a new #GtkColorChooserWidget
 *
 * Since: 3.4
 */
GtkWidget *
gtk_color_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_COLOR_CHOOSER_WIDGET, NULL);
}

