/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Alberto Ruiz <aruiz@gnome.org>
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

#include <stdlib.h>
#include <glib/gprintf.h>
#include <string.h>

#include <atk/atk.h>

#undef GTK_DISABLE_DEPRECATED
#include "gtkfontchooserwidget.h"
#include "gtkfontchooser.h"
#include "gtkfontchooserutils.h"
#include "gtkfontsel.h"
#include "gtkbox.h"



/**
 * SECTION:gtkfontchooser
 * @Short_description: A widget for selecting fonts
 * @Title: GtkFontChooserWidget
 * @See_also: #GtkFontChooserDialog
 *
 * The #GtkFontChooserWidget widget lists the available fonts,
 * styles and sizes, allowing the user to select a font. It is
 * used in the #GtkFontChooserDialog widget to provide a
 * dialog box for selecting fonts.
 *
 * To set the font which is initially selected, use
 * gtk_font_chooser_set_font() or gtk_font_chooser_set_font_desc().
 *
 * To get the selected font use gtk_font_chooser_get_font() or
 * gtk_font_chooser_get_font_desc().
 *
 * To change the text which is shown in the preview area, use
 * gtk_font_chooser_set_preview_text().
 *
 * Since: 3.2
 */


struct _GtkFontChooserWidgetPrivate
{
  GtkWidget    *font_sel_widget;
};

static void gtk_font_chooser_widget_set_property         (GObject         *object,
                                                          guint            prop_id,
                                                          const GValue    *value,
                                                          GParamSpec      *pspec);
                                                           
static void gtk_font_chooser_widget_get_property         (GObject         *object,
                                                          guint            prop_id,
                                                          GValue          *value,
                                                          GParamSpec      *pspec);

static void gtk_font_chooser_widget_finalize             (GObject         *object);


static gchar   *gtk_font_chooser_widget_get_font         (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_font         (GtkFontChooserWidget *fontchooser,
                                                          const gchar          *fontname);
                                                          
static PangoFontDescription *gtk_font_chooser_widget_get_font_desc  (GtkFontChooserWidget *fontchooser);
static void                  gtk_font_chooser_widget_take_font_desc (GtkFontChooserWidget *fontchooser,
                                                                     PangoFontDescription *font_desc);       
                                                                     
static const gchar *gtk_font_chooser_widget_get_preview_text (GtkFontChooserWidget *fontchooser);
static void         gtk_font_chooser_widget_set_preview_text (GtkFontChooserWidget *fontchooser,
                                                              const gchar          *text);                            
                                                              
static gboolean gtk_font_chooser_widget_get_show_preview_entry (GtkFontChooserWidget *fontchooser);
static void     gtk_font_chooser_widget_set_show_preview_entry (GtkFontChooserWidget *fontchooser,
                                                                gboolean              show_preview_entry);
                                                                                             
static void gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkFontChooserWidget, gtk_font_chooser_widget, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_FONT_CHOOSER,
                                                gtk_font_chooser_widget_iface_init))

static void
gtk_font_chooser_widget_class_init (GtkFontChooserWidgetClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->finalize = gtk_font_chooser_widget_finalize;
  gobject_class->set_property = gtk_font_chooser_widget_set_property;
  gobject_class->get_property = gtk_font_chooser_widget_get_property;

  _gtk_font_chooser_install_properties (gobject_class);

  g_type_class_add_private (klass, sizeof (GtkFontChooserWidgetPrivate));
}

static void
gtk_font_chooser_widget_set_property (GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case GTK_FONT_CHOOSER_PROP_FONT:
      gtk_font_chooser_widget_set_font (fontchooser, g_value_get_string (value));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      gtk_font_chooser_widget_take_font_desc (fontchooser, g_value_dup_boxed (value));
      break;
    case GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      gtk_font_chooser_widget_set_preview_text (fontchooser, g_value_get_string (value));
      break;
    case GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      gtk_font_chooser_widget_set_show_preview_entry (fontchooser, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_font_chooser_widget_get_property (GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (object);

  switch (prop_id)
    {
    case GTK_FONT_CHOOSER_PROP_FONT:
      g_value_take_string (value, gtk_font_chooser_widget_get_font (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_FONT_DESC:
      g_value_set_boxed (value, gtk_font_chooser_widget_get_font_desc (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_PREVIEW_TEXT:
      g_value_set_string (value, gtk_font_chooser_widget_get_preview_text (fontchooser));
      break;
    case GTK_FONT_CHOOSER_PROP_SHOW_PREVIEW_ENTRY:
      g_value_set_boolean (value, gtk_font_chooser_widget_get_show_preview_entry (fontchooser));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


static void
gtk_font_chooser_widget_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_font_chooser_widget_parent_class)->finalize (object);
}

GtkWidget *
gtk_font_chooser_widget_new (void)
{
  return g_object_new (GTK_TYPE_FONT_CHOOSER_WIDGET, NULL);
}

static void
gtk_font_chooser_widget_init (GtkFontChooserWidget *fontchooser)
{
  GtkFontChooserWidgetPrivate *priv;

  fontchooser->priv = G_TYPE_INSTANCE_GET_PRIVATE (fontchooser,
                                                   GTK_TYPE_FONT_CHOOSER_WIDGET,
                                                   GtkFontChooserWidgetPrivate);
  priv = fontchooser->priv;

  priv->font_sel_widget = gtk_font_selection_new();

  gtk_box_pack_start (GTK_BOX (fontchooser), priv->font_sel_widget, TRUE, TRUE, 0);
  
  gtk_widget_show_all (GTK_WIDGET (fontchooser));

  gtk_font_chooser_widget_take_font_desc(fontchooser, pango_font_description_new());
}

static gchar *
gtk_font_chooser_widget_get_font (GtkFontChooserWidget *fontchooser)
{
  return gtk_font_selection_get_font_name(GTK_FONT_SELECTION(fontchooser->priv->font_sel_widget));
}

static void
gtk_font_chooser_widget_set_font (GtkFontChooserWidget *fontchooser,
                                  const gchar          *fontname)
{
  gtk_font_chooser_widget_take_font_desc (fontchooser, pango_font_description_from_string (fontname));
}

static PangoFontDescription *
gtk_font_chooser_widget_get_font_desc (GtkFontChooserWidget *fontchooser)
{
  return pango_font_description_from_string(gtk_font_selection_get_font_name(GTK_FONT_SELECTION(fontchooser->priv->font_sel_widget)));
}

static const gchar*
gtk_font_chooser_widget_get_preview_text (GtkFontChooserWidget *fontchooser)
{
	
  return gtk_font_selection_get_preview_text(GTK_FONT_SELECTION(fontchooser->priv->font_sel_widget));
}

static void
gtk_font_chooser_widget_set_preview_text (GtkFontChooserWidget *fontchooser,
                                          const gchar          *text)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;

  gtk_font_selection_set_preview_text(GTK_FONT_SELECTION(priv->font_sel_widget), text);

  g_object_notify (G_OBJECT (fontchooser), "preview-text");
}

static void
gtk_font_chooser_widget_take_font_desc (GtkFontChooserWidget *fontchooser,
                                        PangoFontDescription *font_desc)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  
  char* font_name = pango_font_description_to_string(font_desc);
  
  gtk_font_selection_set_font_name(GTK_FONT_SELECTION(priv->font_sel_widget), font_name);
  
  g_free(font_name);
}

static gboolean
gtk_font_chooser_widget_get_show_preview_entry (GtkFontChooserWidget *fontchooser)
{
  return gtk_font_selection_get_show_preview(GTK_FONT_SELECTION(fontchooser->priv->font_sel_widget));
}

static void
gtk_font_chooser_widget_set_show_preview_entry (GtkFontChooserWidget *fontchooser,
                                                gboolean              show_preview_entry)
{
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  
  gtk_font_selection_set_show_preview(priv->font_sel_widget, show_preview_entry);

  g_object_notify (G_OBJECT (fontchooser), "show-preview-entry");
}

static PangoFontFamily *
gtk_font_chooser_widget_get_family (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  return gtk_font_selection_get_family(GTK_FONT_SELECTION(priv->font_sel_widget));
}

static PangoFontFace *
gtk_font_chooser_widget_get_face (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  return gtk_font_selection_get_face(GTK_FONT_SELECTION(priv->font_sel_widget));
}

static gint
gtk_font_chooser_widget_get_size (GtkFontChooser *chooser)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  return gtk_font_selection_get_size (GTK_FONT_SELECTION(priv->font_sel_widget));
}

static void
gtk_font_chooser_widget_set_filter_func (GtkFontChooser  *chooser,
                                         GtkFontFilterFunc filter,
                                         gpointer          data,
                                         GDestroyNotify    destroy)
{
  GtkFontChooserWidget *fontchooser = GTK_FONT_CHOOSER_WIDGET (chooser);
  GtkFontChooserWidgetPrivate *priv = fontchooser->priv;
  gtk_font_selection_set_filter_func(GTK_FONT_SELECTION(priv->font_sel_widget), filter, data, destroy);
}

static void
gtk_font_chooser_widget_iface_init (GtkFontChooserIface *iface)
{
  iface->get_font_family = gtk_font_chooser_widget_get_family;
  iface->get_font_face = gtk_font_chooser_widget_get_face;
  iface->get_font_size = gtk_font_chooser_widget_get_size;
  iface->set_filter_func = gtk_font_chooser_widget_set_filter_func;
}
