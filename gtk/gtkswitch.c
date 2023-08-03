/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2010  Intel Corporation
 * Copyright (C) 2010  RedHat, Inc.
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
 * Boston, MA  02111-1307, USA.
 *
 * Author:
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * Based on similar code from Mx.
 */

/**
 * SECTION:gtkswitch
 * @Short_Description: A "light switch" style toggle
 * @Title: GtkSwitch
 * @See_Also: #GtkToggleButton
 *
 * #GtkSwitch is a widget that has two states: on or off. The user can control
 * which state should be active by clicking the empty area, or by dragging the
 * handle.
 */

#include "config.h"

#include "gtkswitch.h"

#include "gtkactivatable.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtktoggleaction.h"
#include "gtkwidget.h"
#include "gtkmarshalers.h"
#include "gtkapplicationprivate.h"
#include "gtkactionable.h"
#include "gtkactionhelper.h"
#include "gtktogglebutton.h"
#include "gtkbox.h"
#include "gtkstock.h"

#include <math.h>

struct _GtkSwitchPrivate
{
  GtkWidget *togglebutton;
  GtkWidget *onimg;
  GtkWidget *offimg;
};

enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_RELATED_ACTION,
  PROP_USE_ACTION_APPEARANCE,
  LAST_PROP,
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET
};

enum
{
  ACTIVATE,
  LAST_SIGNAL
};

gboolean toggling = FALSE;

static guint signals[LAST_SIGNAL] = { 0 };

static GParamSpec *switch_props[LAST_PROP] = { NULL, };

static void gtk_switch_actionable_iface_init (GtkActionableInterface *iface);
static void gtk_switch_activatable_interface_init (GtkActivatableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSwitch, gtk_switch, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE,
                                                gtk_switch_actionable_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
                                                gtk_switch_activatable_interface_init));
static void
gtk_switch_activate (GtkSwitch *sw)
{
  GtkSwitchPrivate *priv = sw->priv;

  gtk_switch_set_active (sw, !gtk_switch_get_active(sw));
}

static const gchar *
gtk_switch_get_action_name (GtkActionable *actionable)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);

  return gtk_actionable_get_action_name(GTK_ACTIONABLE(sw->priv->togglebutton));
}

static GVariant *
gtk_switch_get_action_target_value (GtkActionable *actionable)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);

  return gtk_actionable_get_action_target_value(GTK_ACTIONABLE(sw->priv->togglebutton));
}

static void
gtk_switch_set_action_name (GtkActionable *actionable,
                            const gchar   *action_name)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);

  return gtk_actionable_set_action_name(GTK_ACTIONABLE(sw->priv->togglebutton), action_name);
}

static void
gtk_switch_set_action_target_value (GtkActionable *actionable,
                                    GVariant      *action_target)
{
  GtkSwitch *sw = GTK_SWITCH (actionable);

  return gtk_actionable_set_action_target_value(GTK_ACTIONABLE(sw->priv->togglebutton), action_target);
}

static void
gtk_switch_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_switch_get_action_name;
  iface->set_action_name = gtk_switch_set_action_name;
  iface->get_action_target_value = gtk_switch_get_action_target_value;
  iface->set_action_target_value = gtk_switch_set_action_target_value;
}

static void
gtk_switch_set_property (GObject      *gobject,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GtkSwitch *sw = GTK_SWITCH (gobject);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_switch_set_active (sw, g_value_get_boolean (value));
      break;

    case PROP_RELATED_ACTION:
      gtk_activatable_set_related_action(GTK_ACTIVATABLE(sw->priv->togglebutton), g_value_get_object (value));
      break;

    case PROP_USE_ACTION_APPEARANCE:
      gtk_activatable_set_use_action_appearance(GTK_ACTIVATABLE(sw->priv->togglebutton), g_value_get_boolean (value));
      break;

    case PROP_ACTION_NAME:
      gtk_actionable_set_action_name(GTK_ACTIONABLE(sw->priv->togglebutton), g_value_get_string (value));
      break;

    case PROP_ACTION_TARGET:
      gtk_actionable_set_action_target_value(GTK_ACTIONABLE(sw->priv->togglebutton), g_value_get_variant (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_switch_get_property (GObject    *gobject,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (gobject)->priv;

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, gtk_switch_get_active(GTK_SWITCH(gobject)));
      break;

    case PROP_RELATED_ACTION:
      g_value_set_object (value, gtk_activatable_get_related_action(GTK_ACTIVATABLE(priv->togglebutton)));
      break;

    case PROP_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, gtk_activatable_get_use_action_appearance(GTK_ACTIVATABLE(priv->togglebutton)));
      break;

    case PROP_ACTION_NAME:
      g_value_set_string (value, gtk_actionable_get_action_name(GTK_ACTIONABLE(priv->togglebutton)));
      break;

    case PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_actionable_get_action_target_value(GTK_ACTIONABLE(priv->togglebutton)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_switch_dispose (GObject *object)
{
  GtkSwitchPrivate *priv = GTK_SWITCH (object)->priv;
  
  if (priv->togglebutton) {
	  gtk_widget_destroy(priv->togglebutton);
  }
  
  if (priv->onimg) {
	  gtk_widget_destroy(priv->onimg);
  }
  
  if (priv->offimg) {
	  gtk_widget_destroy(priv->offimg);
  }
  G_OBJECT_CLASS (gtk_switch_parent_class)->dispose (object);
}

static void
gtk_switch_class_init (GtkSwitchClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gpointer activatable_iface;

  g_type_class_add_private (klass, sizeof (GtkSwitchPrivate));

  activatable_iface = g_type_default_interface_peek (GTK_TYPE_ACTIVATABLE);
  switch_props[PROP_RELATED_ACTION] =
    g_param_spec_override ("related-action",
                           g_object_interface_find_property (activatable_iface,
                                                             "related-action"));

  switch_props[PROP_USE_ACTION_APPEARANCE] =
    g_param_spec_override ("use-action-appearance",
                           g_object_interface_find_property (activatable_iface,
                                                             "use-action-appearance"));

  /**
   * GtkSwitch:active:
   *
   * Whether the #GtkSwitch widget is in its on or off state.
   */
  switch_props[PROP_ACTIVE] =
    g_param_spec_boolean ("active",
                          P_("Active"),
                          P_("Whether the switch is on or off"),
                          FALSE,
                          GTK_PARAM_READWRITE);

  gobject_class->set_property = gtk_switch_set_property;
  gobject_class->get_property = gtk_switch_get_property;
  gobject_class->dispose = gtk_switch_dispose;

  g_object_class_install_properties (gobject_class, LAST_PROP, switch_props);

  klass->activate = gtk_switch_activate;

  /**
   * GtkSwitch::activate:
   * @widget: the object which received the signal.
   *
   * The ::activate signal on GtkSwitch is an action signal and
   * emitting it causes the switch to animate.
   * Applications should never connect to this signal, but use the
   * notify::active signal.
   */
  signals[ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkSwitchClass, activate),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
                  
  widget_class->activate_signal = signals[ACTIVATE];

  g_object_class_override_property (gobject_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (gobject_class, PROP_ACTION_TARGET, "action-target");
}


void gtk_switch_btn_toggled(GtkWidget *widget, gpointer sw) {
  gtk_switch_set_active(GTK_SWITCH(sw), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}


static void
gtk_switch_init (GtkSwitch *self)
{
  if (toggling) {
	  return;
  }
  
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GTK_TYPE_SWITCH, GtkSwitchPrivate);
  self->priv->togglebutton = gtk_toggle_button_new_with_label(C_("switch", "OFF"));
  gtk_box_pack_start(GTK_BOX(self), self->priv->togglebutton, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(self->priv->togglebutton), "toggled", G_CALLBACK(gtk_switch_btn_toggled), (gpointer)self);
  
  self->priv->onimg = gtk_image_new_from_stock(GTK_STOCK_YES, GTK_ICON_SIZE_BUTTON);
  self->priv->offimg = gtk_image_new_from_stock(GTK_STOCK_NO, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image(GTK_BUTTON(self->priv->togglebutton), GTK_WIDGET(g_object_ref(self->priv->offimg))); 	  

  gtk_widget_show_all(GTK_WIDGET(self));
}

/**
 * gtk_switch_new:
 *
 * Creates a new #GtkSwitch widget.
 *
 * Return value: the newly created #GtkSwitch instance
 *
 * Since: 3.0
 */
GtkWidget *
gtk_switch_new (void)
{
  return g_object_new (GTK_TYPE_SWITCH, NULL);
}

/**
 * gtk_switch_set_active:
 * @sw: a #GtkSwitch
 * @is_active: %TRUE if @sw should be active, and %FALSE otherwise
 *
 * Changes the state of @sw to the desired one.
 *
 * Since: 3.0
 */
void
gtk_switch_set_active (GtkSwitch *sw,
                       gboolean   is_active)
{
  toggling = TRUE;
  GtkSwitchPrivate *priv;

  g_return_if_fail (GTK_IS_SWITCH (sw));

  priv = sw->priv;

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton), is_active);

  if (is_active)  {
	 gtk_button_set_label(GTK_BUTTON(priv->togglebutton), C_("switch", "ON")); 
	 gtk_button_set_image(GTK_BUTTON(priv->togglebutton), GTK_WIDGET(g_object_ref(priv->onimg))); 	  
  } else {
	 gtk_button_set_label(GTK_BUTTON(priv->togglebutton), C_("switch", "OFF")); 
	 gtk_button_set_image(GTK_BUTTON(priv->togglebutton), GTK_WIDGET(g_object_ref(priv->offimg))); 	   	  
  }
  toggling = FALSE;
}

/**
 * gtk_switch_get_active:
 * @sw: a #GtkSwitch
 *
 * Gets whether the #GtkSwitch is in its "on" or "off" state.
 *
 * Return value: %TRUE if the #GtkSwitch is active, and %FALSE otherwise
 *
 * Since: 3.0
 */
gboolean
gtk_switch_get_active (GtkSwitch *sw)
{
  g_return_val_if_fail (GTK_IS_SWITCH (sw), FALSE);

  return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sw->priv->togglebutton));
}

static void
gtk_switch_update (GtkActivatable *activatable,
                   GtkAction      *action,
                   const gchar    *property_name)
{
  if (strcmp (property_name, "visible") == 0)
    {
      if (gtk_action_is_visible (action))
        gtk_widget_show (GTK_WIDGET (activatable));
      else
        gtk_widget_hide (GTK_WIDGET (activatable));
    }
  else if (strcmp (property_name, "sensitive") == 0)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));
    }
  else if (strcmp (property_name, "active") == 0)
    {
      gtk_action_block_activate (action);
      gtk_switch_set_active (GTK_SWITCH (activatable), gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
      gtk_action_unblock_activate (action);
    }
}

static void
gtk_switch_sync_action_properties (GtkActivatable *activatable,
                                   GtkAction      *action)
{
  if (!action)
    return;

  if (gtk_action_is_visible (action))
    gtk_widget_show (GTK_WIDGET (activatable));
  else
    gtk_widget_hide (GTK_WIDGET (activatable));

  gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));

  gtk_action_block_activate (action);
  gtk_switch_set_active (GTK_SWITCH (activatable), gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
  gtk_action_unblock_activate (action);
}

static void
gtk_switch_activatable_interface_init (GtkActivatableIface *iface)
{
  iface->update = gtk_switch_update;
  iface->sync_action_properties = gtk_switch_sync_action_properties;
}
