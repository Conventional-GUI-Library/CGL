/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
#include "gtkmodifierstyle.h"
#include "gtkintl.h"

typedef struct GtkModifierStylePrivate GtkModifierStylePrivate;
typedef struct StylePropertyValue StylePropertyValue;

struct GtkModifierStylePrivate
{
  GtkStyleProperties *style;
  GHashTable *color_properties;
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static void gtk_modifier_style_provider_init (GtkStyleProviderIface *iface);
static void gtk_modifier_style_finalize      (GObject      *object);

G_DEFINE_TYPE_EXTENDED (GtkModifierStyle, _gtk_modifier_style, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_modifier_style_provider_init));

static void
_gtk_modifier_style_class_init (GtkModifierStyleClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_modifier_style_finalize;

  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_type_class_add_private (object_class, sizeof (GtkModifierStylePrivate));
}

static void
_gtk_modifier_style_init (GtkModifierStyle *modifier_style)
{
  GtkModifierStylePrivate *priv;

  priv = modifier_style->priv = G_TYPE_INSTANCE_GET_PRIVATE (modifier_style,
                                                             GTK_TYPE_MODIFIER_STYLE,
                                                             GtkModifierStylePrivate);

  priv->color_properties = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  (GDestroyNotify) g_free,
                                                  (GDestroyNotify) gdk_rgba_free);
  priv->style = gtk_style_properties_new ();
}

static GtkStyleProperties *
gtk_modifier_style_get_style (GtkStyleProvider *provider,
                              GtkWidgetPath    *path)
{
  GtkModifierStylePrivate *priv;

  priv = GTK_MODIFIER_STYLE (provider)->priv;
  return g_object_ref (priv->style);
}

static gboolean
gtk_modifier_style_get_style_property (GtkStyleProvider *provider,
                                       GtkWidgetPath    *path,
                                       GtkStateFlags     state,
                                       GParamSpec       *pspec,
                                       GValue           *value)
{
  GtkModifierStylePrivate *priv;
  GdkRGBA *rgba;
  GdkColor color;
  gchar *str;

  /* Reject non-color types for now */
  if (pspec->value_type != GDK_TYPE_COLOR)
    return FALSE;

  priv = GTK_MODIFIER_STYLE (provider)->priv;
  str = g_strdup_printf ("-%s-%s",
                         g_type_name (pspec->owner_type),
                         pspec->name);

  rgba = g_hash_table_lookup (priv->color_properties, str);
  g_free (str);

  if (!rgba)
    return FALSE;

  color.red = (guint) (rgba->red * 65535.) + 0.5;
  color.green = (guint) (rgba->green * 65535.) + 0.5;
  color.blue = (guint) (rgba->blue * 65535.) + 0.5;

  g_value_set_boxed (value, &color);
  return TRUE;
}

static void
gtk_modifier_style_provider_init (GtkStyleProviderIface *iface)
{
  iface->get_style = gtk_modifier_style_get_style;
  iface->get_style_property = gtk_modifier_style_get_style_property;
}

static void
gtk_modifier_style_finalize (GObject *object)
{
  GtkModifierStylePrivate *priv;

  priv = GTK_MODIFIER_STYLE (object)->priv;
  g_hash_table_destroy (priv->color_properties);
  g_object_unref (priv->style);

  G_OBJECT_CLASS (_gtk_modifier_style_parent_class)->finalize (object);
}

GtkModifierStyle *
_gtk_modifier_style_new (void)
{
  return g_object_new (GTK_TYPE_MODIFIER_STYLE, NULL);
}

static void
modifier_style_set_color (GtkModifierStyle *style,
                          const gchar      *prop,
                          GtkStateFlags     state,
                          const GdkRGBA    *color)
{
  GtkModifierStylePrivate *priv;
  GdkRGBA *old_color;

  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));

  priv = style->priv;
  gtk_style_properties_get (priv->style, state,
                            prop, &old_color,
                            NULL);

  if ((!color && !old_color) ||
      (color && old_color && gdk_rgba_equal (color, old_color)))
    {
      gdk_rgba_free (old_color);
      return;
    }

  if (color)
    gtk_style_properties_set (priv->style, state,
                              prop, color,
                              NULL);
  else
    gtk_style_properties_unset_property (priv->style, prop, state);

  g_signal_emit (style, signals[CHANGED], 0);
  gdk_rgba_free (old_color);
}

void
_gtk_modifier_style_set_background_color (GtkModifierStyle *style,
                                          GtkStateFlags     state,
                                          const GdkRGBA    *color)
{
  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));

  modifier_style_set_color (style, "background-color", state, color);
}

void
_gtk_modifier_style_set_color (GtkModifierStyle *style,
                               GtkStateFlags     state,
                               const GdkRGBA    *color)
{
  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));

  modifier_style_set_color (style, "color", state, color);
}

void
_gtk_modifier_style_set_font (GtkModifierStyle           *style,
                              const PangoFontDescription *font_desc)
{
  GtkModifierStylePrivate *priv;
  PangoFontDescription *old_font;

  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));

  priv = style->priv;
  gtk_style_properties_get (priv->style, 0,
                            "font", &old_font,
                            NULL);

  if ((!old_font && !font_desc) ||
      (old_font && font_desc &&
       pango_font_description_equal (old_font, font_desc)))
    {
      if (old_font)
        pango_font_description_free (old_font);

      return;
    }

  if (font_desc)
    gtk_style_properties_set (priv->style, 0,
                              "font", font_desc,
                              NULL);
  else
    gtk_style_properties_unset_property (priv->style, "font", 0);

  if (old_font)
    pango_font_description_free (old_font);

  g_signal_emit (style, signals[CHANGED], 0);
}

void
_gtk_modifier_style_map_color (GtkModifierStyle *style,
                               const gchar      *name,
                               const GdkRGBA    *color)
{
  GtkModifierStylePrivate *priv;
  GtkSymbolicColor *symbolic_color = NULL;

  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));
  g_return_if_fail (name != NULL);

  priv = style->priv;

  if (color)
    symbolic_color = gtk_symbolic_color_new_literal (color);

  gtk_style_properties_map_color (priv->style,
                                  name, symbolic_color);

  g_signal_emit (style, signals[CHANGED], 0);
}

void
_gtk_modifier_style_set_color_property (GtkModifierStyle *style,
                                        GType             widget_type,
                                        const gchar      *prop_name,
                                        const GdkRGBA    *color)
{
  GtkModifierStylePrivate *priv;
  const GdkRGBA *old_color;
  gchar *str;

  g_return_if_fail (GTK_IS_MODIFIER_STYLE (style));
  g_return_if_fail (g_type_is_a (widget_type, GTK_TYPE_WIDGET));
  g_return_if_fail (prop_name != NULL);

  priv = style->priv;
  str = g_strdup_printf ("-%s-%s", g_type_name (widget_type), prop_name);

  old_color = g_hash_table_lookup (priv->color_properties, str);

  if ((!color && !old_color) ||
      (color && old_color && gdk_rgba_equal (color, old_color)))
    {
      g_free (str);
      return;
    }

  if (color)
    {
      g_hash_table_insert (priv->color_properties, str,
                           gdk_rgba_copy (color));
    }
  else
    {
      g_hash_table_remove (priv->color_properties, str);
      g_free (str);
    }

  g_signal_emit (style, signals[CHANGED], 0);
}
