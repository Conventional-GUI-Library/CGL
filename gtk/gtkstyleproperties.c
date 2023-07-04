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

#include "gtkstylepropertiesprivate.h"

#include <stdlib.h>
#include <gobject/gvaluecollector.h>
#include <cairo-gobject.h>

#include "gtkstyleprovider.h"
#include "gtksymboliccolor.h"
#include "gtkthemingengine.h"
#include "gtkanimationdescription.h"
#include "gtkgradient.h"
#include "gtkshadowprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkborderimageprivate.h"

#include "gtkprivatetypebuiltins.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtkintl.h"

#include "gtkwin32themeprivate.h"

/**
 * SECTION:gtkstyleproperties
 * @Short_description: Store for style property information
 * @Title: GtkStyleProperties
 *
 * GtkStyleProperties provides the storage for style information
 * that is used by #GtkStyleContext and other #GtkStyleProvider
 * implementations.
 *
 * Before style properties can be stored in GtkStyleProperties, they
 * must be registered with gtk_style_properties_register_property().
 *
 * Unless you are writing a #GtkStyleProvider implementation, you
 * are unlikely to use this API directly, as gtk_style_context_get()
 * and its variants are the preferred way to access styling information
 * from widget implementations and theming engine implementations
 * should use the APIs provided by #GtkThemingEngine instead.
 */

typedef struct PropertyData PropertyData;
typedef struct ValueData ValueData;

struct ValueData
{
  GtkStateFlags state;
  GValue value;
};

struct PropertyData
{
  GArray *values;
};

struct _GtkStylePropertiesPrivate
{
  GHashTable *color_map;
  GHashTable *properties;
  GtkSymbolicColorLookupFunc color_lookup_func;
  gpointer color_lookup_data;
};

static void gtk_style_properties_provider_init         (GtkStyleProviderIface            *iface);
static void gtk_style_properties_provider_private_init (GtkStyleProviderPrivateInterface *iface);
static void gtk_style_properties_finalize              (GObject                          *object);


G_DEFINE_TYPE_EXTENDED (GtkStyleProperties, gtk_style_properties, G_TYPE_OBJECT, 0,
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER,
                                               gtk_style_properties_provider_init)
                        G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLE_PROVIDER_PRIVATE,
                                               gtk_style_properties_provider_private_init));

static void
gtk_style_properties_class_init (GtkStylePropertiesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_properties_finalize;

  g_type_class_add_private (object_class, sizeof (GtkStylePropertiesPrivate));
}

static PropertyData *
property_data_new (void)
{
  PropertyData *data;

  data = g_slice_new0 (PropertyData);
  data->values = g_array_new (FALSE, FALSE, sizeof (ValueData));

  return data;
}

static void
property_data_remove_values (PropertyData *data)
{
  guint i;

  for (i = 0; i < data->values->len; i++)
    {
      ValueData *value_data;

      value_data = &g_array_index (data->values, ValueData, i);

      if (G_IS_VALUE (&value_data->value))
        g_value_unset (&value_data->value);
    }

  if (data->values->len > 0)
    g_array_remove_range (data->values, 0, data->values->len);
}

static void
property_data_free (PropertyData *data)
{
  property_data_remove_values (data);
  g_array_free (data->values, TRUE);
  g_slice_free (PropertyData, data);
}

static gboolean
property_data_find_position (PropertyData  *data,
                             GtkStateFlags  state,
                             guint         *pos)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint position;

  if (pos)
    *pos = 0;

  if (data->values->len == 0)
    return FALSE;

  /* Find position for the given state, or the position where
   * it would be if not found, the array is ordered by the
   * state flags.
   */
  min = 0;
  max = data->values->len - 1;

  do
    {
      ValueData *value_data;

      mid = (min + max) / 2;
      value_data = &g_array_index (data->values, ValueData, mid);

      if (value_data->state == state)
        {
          found = TRUE;
          position = mid;
        }
      else if (value_data->state < state)
          position = min = mid + 1;
      else
        {
          max = mid - 1;
          position = mid;
        }
    }
  while (!found && min <= max);

  if (pos)
    *pos = position;

  return found;
}

static GValue *
property_data_get_value (PropertyData  *data,
                         GtkStateFlags  state)
{
  ValueData *val_data;
  guint pos;

  if (!property_data_find_position (data, state, &pos))
    {
      ValueData new = { 0 };

      new.state = state;
      g_array_insert_val (data->values, pos, new);
    }

  val_data = &g_array_index (data->values, ValueData, pos);

  return &val_data->value;
}

static GValue *
property_data_match_state (PropertyData  *data,
                           GtkStateFlags  state)
{
  guint pos;
  gint i;

  if (property_data_find_position (data, state, &pos))
    {
      ValueData *val_data;

      /* Exact match */
      val_data = &g_array_index (data->values, ValueData, pos);
      return &val_data->value;
    }

  if (pos >= data->values->len)
    pos = data->values->len - 1;

  /* No exact match, go downwards the list to find
   * the closest match to the given state flags, as
   * a side effect, there is an implicit precedence
   * of higher flags over the smaller ones.
   */
  for (i = pos; i >= 0; i--)
    {
      ValueData *val_data;

      val_data = &g_array_index (data->values, ValueData, i);

       /* Check whether any of the requested
        * flags are set, and no other flags are.
        *
        * Also, no flags acts as a wildcard, such
        * value should be always in the first position
        * in the array (if present) anyways.
        */
      if (val_data->state == 0 ||
          ((val_data->state & state) != 0 &&
           (val_data->state & ~state) == 0))
        return &val_data->value;
    }

  return NULL;
}

static void
gtk_style_properties_init (GtkStyleProperties *props)
{
  GtkStylePropertiesPrivate *priv;

  priv = props->priv = G_TYPE_INSTANCE_GET_PRIVATE (props,
                                                    GTK_TYPE_STYLE_PROPERTIES,
                                                    GtkStylePropertiesPrivate);

  priv->properties = g_hash_table_new_full (NULL, NULL, NULL,
                                            (GDestroyNotify) property_data_free);
}

static void
gtk_style_properties_finalize (GObject *object)
{
  GtkStylePropertiesPrivate *priv;
  GtkStyleProperties *props;

  props = GTK_STYLE_PROPERTIES (object);
  priv = props->priv;
  g_hash_table_destroy (priv->properties);

  if (priv->color_map)
    g_hash_table_destroy (priv->color_map);

  G_OBJECT_CLASS (gtk_style_properties_parent_class)->finalize (object);
}

GtkStyleProperties *
gtk_style_properties_get_style (GtkStyleProvider *provider,
                                GtkWidgetPath    *path)
{
  /* Return style set itself */
  return g_object_ref (provider);
}

static void
gtk_style_properties_provider_init (GtkStyleProviderIface *iface)
{
  iface->get_style = gtk_style_properties_get_style;
}

static GtkSymbolicColor *
gtk_style_properties_provider_get_color (GtkStyleProviderPrivate *provider,
                                         const char              *name)
{
  return gtk_style_properties_lookup_color (GTK_STYLE_PROPERTIES (provider), name);
}

static void
gtk_style_properties_provider_lookup (GtkStyleProviderPrivate *provider,
                                      GtkWidgetPath           *path,
                                      GtkStateFlags            state,
                                      GtkCssLookup            *lookup)
{
  GtkStyleProperties *props;
  GtkStylePropertiesPrivate *priv;
  GHashTableIter iter;
  gpointer key, value;

  props = GTK_STYLE_PROPERTIES (provider);
  priv = props->priv;

  /* Merge symbolic style properties */
  g_hash_table_iter_init (&iter, priv->properties);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GtkCssStyleProperty *prop = key;
      PropertyData *data = value;
      const GValue *value;
      guint id;

      id = _gtk_css_style_property_get_id (prop);

      if (!_gtk_css_lookup_is_missing (lookup, id))
          continue;

      value = property_data_match_state (data, state);
      if (value == NULL)
        continue;

      _gtk_css_lookup_set (lookup, id, NULL, value);
    }
}

static void
gtk_style_properties_provider_private_init (GtkStyleProviderPrivateInterface *iface)
{
  iface->get_color = gtk_style_properties_provider_get_color;
  iface->lookup = gtk_style_properties_provider_lookup;
}

/* GtkStyleProperties methods */

/**
 * gtk_style_properties_new:
 *
 * Returns a newly created #GtkStyleProperties
 *
 * Returns: a new #GtkStyleProperties
 **/
GtkStyleProperties *
gtk_style_properties_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_PROPERTIES, NULL);
}

void
_gtk_style_properties_set_color_lookup_func (GtkStyleProperties         *props,
                                             GtkSymbolicColorLookupFunc  func,
                                             gpointer                    data)
{
  GtkStylePropertiesPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (func != NULL);

  priv = props->priv;
  g_return_if_fail (priv->color_map == NULL);

  priv->color_lookup_func = func;
  priv->color_lookup_data = data;
}

/**
 * gtk_style_properties_map_color:
 * @props: a #GtkStyleProperties
 * @name: color name
 * @color: #GtkSymbolicColor to map @name to
 *
 * Maps @color so it can be referenced by @name. See
 * gtk_style_properties_lookup_color()
 *
 * Since: 3.0
 **/
void
gtk_style_properties_map_color (GtkStyleProperties *props,
                                const gchar        *name,
                                GtkSymbolicColor   *color)
{
  GtkStylePropertiesPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (name != NULL);
  g_return_if_fail (color != NULL);

  priv = props->priv;
  g_return_if_fail (priv->color_lookup_func == NULL);

  if (G_UNLIKELY (!priv->color_map))
    priv->color_map = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             (GDestroyNotify) g_free,
                                             (GDestroyNotify) gtk_symbolic_color_unref);

  g_hash_table_replace (priv->color_map,
                        g_strdup (name),
                        gtk_symbolic_color_ref (color));
}

/**
 * gtk_style_properties_lookup_color:
 * @props: a #GtkStyleProperties
 * @name: color name to lookup
 *
 * Returns the symbolic color that is mapped
 * to @name.
 *
 * Returns: (transfer none): The mapped color
 *
 * Since: 3.0
 **/
GtkSymbolicColor *
gtk_style_properties_lookup_color (GtkStyleProperties *props,
                                   const gchar        *name)
{
  GtkStylePropertiesPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_PROPERTIES (props), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  priv = props->priv;

  if (priv->color_lookup_func)
    return priv->color_lookup_func (priv->color_lookup_data, name);

  if (!priv->color_map)
    return NULL;

  return g_hash_table_lookup (priv->color_map, name);
}

void
_gtk_style_properties_set_property_by_property (GtkStyleProperties  *props,
                                                GtkCssStyleProperty *style_prop,
                                                GtkStateFlags        state,
                                                const GValue        *value)
{
  GtkStylePropertiesPrivate *priv;
  PropertyData *prop;
  GValue *val;

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties, style_prop);

  if (!prop)
    {
      prop = property_data_new ();
      g_hash_table_insert (priv->properties, (gpointer) style_prop, prop);
    }

  val = property_data_get_value (prop, state);

  if (G_VALUE_TYPE (val) == G_VALUE_TYPE (value))
    g_value_reset (val);
  else
    {
      if (G_IS_VALUE (val))
        g_value_unset (val);

      g_value_init (val, G_VALUE_TYPE (value));
    }

  g_value_copy (value, val);
}

/**
 * gtk_style_properties_set_property:
 * @props: a #GtkStyleProperties
 * @property: styling property to set
 * @state: state to set the value for
 * @value: new value for the property
 *
 * Sets a styling property in @props.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_set_property (GtkStyleProperties *props,
                                   const gchar        *property,
                                   GtkStateFlags       state,
                                   const GValue       *value)
{
  GtkStyleProperty *node;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  node = _gtk_style_property_lookup (property);

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }
  if (_gtk_style_property_get_value_type (node) == G_TYPE_NONE)
    {
      g_warning ("Style property \"%s\" is not settable", property);
      return;
    }
  
  _gtk_style_property_assign (node, props, state, value);
}

/**
 * gtk_style_properties_set_valist:
 * @props: a #GtkStyleProperties
 * @state: state to set the values for
 * @args: va_list of property name/value pairs, followed by %NULL
 *
 * Sets several style properties on @props.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_set_valist (GtkStyleProperties *props,
                                 GtkStateFlags       state,
                                 va_list             args)
{
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      GtkStyleProperty *node;
      gchar *error = NULL;
      GType val_type;
      GValue val = { 0 };

      node = _gtk_style_property_lookup (property_name);

      if (!node)
        {
          g_warning ("Style property \"%s\" is not registered", property_name);
          break;
        }

      val_type = _gtk_style_property_get_value_type (node);
      if (val_type == G_TYPE_NONE)
        {
          g_warning ("Style property \"%s\" is not settable", property_name);
          break;
        }

      G_VALUE_COLLECT_INIT (&val, _gtk_style_property_get_value_type (node),
                            args, 0, &error);
      if (error)
        {
          g_warning ("Could not set style property \"%s\": %s", property_name, error);
          g_value_unset (&val);
          g_free (error);
          break;
        }

      _gtk_style_property_assign (node, props, state, &val);
      g_value_unset (&val);

      property_name = va_arg (args, const gchar *);
    }
}

/**
 * gtk_style_properties_set:
 * @props: a #GtkStyleProperties
 * @state: state to set the values for
 * @...: property name/value pairs, followed by %NULL
 *
 * Sets several style properties on @props.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_set (GtkStyleProperties *props,
                          GtkStateFlags       state,
                          ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  va_start (args, state);
  gtk_style_properties_set_valist (props, state, args);
  va_end (args);
}

const GValue *
_gtk_style_properties_peek_property (GtkStyleProperties  *props,
                                     GtkCssStyleProperty *property,
                                     GtkStateFlags        state)
{
  GtkStylePropertiesPrivate *priv;
  PropertyData *prop;

  g_return_val_if_fail (GTK_IS_STYLE_PROPERTIES (props), FALSE);
  g_return_val_if_fail (property != NULL, FALSE);

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties, property);
  if (prop == NULL)
    return NULL;

  return property_data_match_state (prop, state);
}

/**
 * gtk_style_properties_get_property:
 * @props: a #GtkStyleProperties
 * @property: style property name
 * @state: state to retrieve the property value for
 * @value: (out) (transfer full):  return location for the style property value.
 *
 * Gets a style property from @props for the given state. When done with @value,
 * g_value_unset() needs to be called to free any allocated memory.
 *
 * Returns: %TRUE if the property exists in @props, %FALSE otherwise
 *
 * Since: 3.0
 **/
gboolean
gtk_style_properties_get_property (GtkStyleProperties *props,
                                   const gchar        *property,
                                   GtkStateFlags       state,
                                   GValue             *value)
{
  GtkStyleProperty *node;

  g_return_val_if_fail (GTK_IS_STYLE_PROPERTIES (props), FALSE);
  g_return_val_if_fail (property != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  node = _gtk_style_property_lookup (property);
  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return FALSE;
    }
  if (_gtk_style_property_get_value_type (node) == G_TYPE_NONE)
    {
      g_warning ("Style property \"%s\" is not gettable", property);
      return FALSE;
    }

  _gtk_style_property_query (node, props, state, value);
  return TRUE;
}

/**
 * gtk_style_properties_get_valist:
 * @props: a #GtkStyleProperties
 * @state: state to retrieve the property values for
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several style property values from @props for a given state.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_get_valist (GtkStyleProperties *props,
                                 GtkStateFlags       state,
                                 va_list             args)
{
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      gchar *error = NULL;
      GValue value = G_VALUE_INIT;

      if (!gtk_style_properties_get_property (props,
					      property_name,
					      state,
					      &value))
	break;

      G_VALUE_LCOPY (&value, args, 0, &error);
      g_value_unset (&value);

      if (error)
        {
          g_warning ("Could not get style property \"%s\": %s", property_name, error);
          g_free (error);
          break;
        }

      property_name = va_arg (args, const gchar *);
    }
}

/**
 * gtk_style_properties_get:
 * @props: a #GtkStyleProperties
 * @state: state to retrieve the property values for
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several style property values from @props for a
 * given state.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_get (GtkStyleProperties *props,
                          GtkStateFlags       state,
                          ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  va_start (args, state);
  gtk_style_properties_get_valist (props, state, args);
  va_end (args);
}

/**
 * gtk_style_properties_unset_property:
 * @props: a #GtkStyleProperties
 * @property: property to unset
 * @state: state to unset
 *
 * Unsets a style property in @props.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_unset_property (GtkStyleProperties *props,
                                     const gchar        *property,
                                     GtkStateFlags       state)
{
  GtkStylePropertiesPrivate *priv;
  GtkStyleProperty *node;
  PropertyData *prop;
  guint pos;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (property != NULL);

  node = _gtk_style_property_lookup (property);

  if (!node)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }
  if (_gtk_style_property_get_value_type (node) == G_TYPE_NONE)
    {
      g_warning ("Style property \"%s\" is not settable", property);
      return;
    }

  if (GTK_IS_CSS_SHORTHAND_PROPERTY (node))
    {
      GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (node);

      for (pos = 0; pos < _gtk_css_shorthand_property_get_n_subproperties (shorthand); pos++)
        {
          GtkCssStyleProperty *sub = _gtk_css_shorthand_property_get_subproperty (shorthand, pos);
          gtk_style_properties_unset_property (props,
                                               _gtk_style_property_get_name (GTK_STYLE_PROPERTY (sub)),
                                               state);
        }
      return;
    }

  priv = props->priv;
  prop = g_hash_table_lookup (priv->properties, node);

  if (!prop)
    return;

  if (property_data_find_position (prop, state, &pos))
    {
      ValueData *data;

      data = &g_array_index (prop->values, ValueData, pos);

      if (G_IS_VALUE (&data->value))
        g_value_unset (&data->value);

      g_array_remove_index (prop->values, pos);
    }
}

/**
 * gtk_style_properties_clear:
 * @props: a #GtkStyleProperties
 *
 * Clears all style information from @props.
 **/
void
gtk_style_properties_clear (GtkStyleProperties *props)
{
  GtkStylePropertiesPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));

  priv = props->priv;
  g_hash_table_remove_all (priv->properties);
}

/**
 * gtk_style_properties_merge:
 * @props: a #GtkStyleProperties
 * @props_to_merge: a second #GtkStyleProperties
 * @replace: whether to replace values or not
 *
 * Merges into @props all the style information contained
 * in @props_to_merge. If @replace is %TRUE, the values
 * will be overwritten, if it is %FALSE, the older values
 * will prevail.
 *
 * Since: 3.0
 **/
void
gtk_style_properties_merge (GtkStyleProperties       *props,
                            const GtkStyleProperties *props_to_merge,
                            gboolean                  replace)
{
  GtkStylePropertiesPrivate *priv, *priv_to_merge;
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props));
  g_return_if_fail (GTK_IS_STYLE_PROPERTIES (props_to_merge));

  priv = props->priv;
  priv_to_merge = props_to_merge->priv;

  /* Merge symbolic color map */
  if (priv_to_merge->color_map)
    {
      g_hash_table_iter_init (&iter, priv_to_merge->color_map);

      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          const gchar *name;
          GtkSymbolicColor *color;

          name = key;
          color = value;

          if (!replace &&
              g_hash_table_lookup (priv->color_map, name))
            continue;

          gtk_style_properties_map_color (props, name, color);
        }
    }

  /* Merge symbolic style properties */
  g_hash_table_iter_init (&iter, priv_to_merge->properties);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      PropertyData *prop_to_merge = value;
      PropertyData *prop;
      guint i;

      prop = g_hash_table_lookup (priv->properties, key);

      if (!prop)
        {
          prop = property_data_new ();
          g_hash_table_insert (priv->properties, key, prop);
        }

      for (i = 0; i < prop_to_merge->values->len; i++)
        {
          ValueData *data;
          GValue *value;

          data = &g_array_index (prop_to_merge->values, ValueData, i);

          if (replace && data->state == GTK_STATE_FLAG_NORMAL &&
              G_VALUE_TYPE (&data->value) != PANGO_TYPE_FONT_DESCRIPTION)
            {
              /* Let normal state override all states
               * previously set in the original set
               */
              property_data_remove_values (prop);
            }

          value = property_data_get_value (prop, data->state);

          if (G_VALUE_TYPE (&data->value) == PANGO_TYPE_FONT_DESCRIPTION &&
              G_IS_VALUE (value))
            {
              PangoFontDescription *font_desc;
              PangoFontDescription *font_desc_to_merge;

              /* Handle merging of font descriptions */
              font_desc = g_value_get_boxed (value);
              font_desc_to_merge = g_value_get_boxed (&data->value);

              pango_font_description_merge (font_desc, font_desc_to_merge, replace);
            }
          else if (G_VALUE_TYPE (&data->value) == G_TYPE_PTR_ARRAY &&
                   G_IS_VALUE (value))
            {
              GPtrArray *array, *array_to_merge;
              gint i;

              /* Append the array, mainly thought
               * for the gtk-key-bindings property
               */
              array = g_value_get_boxed (value);
              array_to_merge = g_value_get_boxed (&data->value);

              for (i = 0; i < array_to_merge->len; i++)
                g_ptr_array_add (array, g_ptr_array_index (array_to_merge, i));
            }
          else if (replace || !G_IS_VALUE (value))
            {
              if (!G_IS_VALUE (value))
                g_value_init (value, G_VALUE_TYPE (&data->value));
              else if (G_VALUE_TYPE (value) != G_VALUE_TYPE (&data->value))
                {
                  g_value_unset (value);
                  g_value_init (value, G_VALUE_TYPE (&data->value));
                }

              g_value_copy (&data->value, value);
            }
        }
    }
}
