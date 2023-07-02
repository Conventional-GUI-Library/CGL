/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2003 Ricardo Fernandez Pascual
 * Copyright (C) 2004 Paolo Borelli
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

#include "gtkmenutoolbutton.h"

#include "gtktogglebutton.h"
#include "gtkarrow.h"
#include "gtkbox.h"
#include "gtkmenu.h"
#include "gtkmain.h"
#include "gtksizerequest.h"
#include "gtkbuildable.h"

#include "gtkprivate.h"
#include "gtkintl.h"


/**
 * SECTION:gtkmenutoolbutton
 * @Short_description: A GtkToolItem containing a button with an additional dropdown menu
 * @Title: GtkMenuToolButton
 * @See_also: #GtkToolbar, #GtkToolButton
 *
 * A #GtkMenuToolButton is a #GtkToolItem that contains a button and
 * a small additional button with an arrow. When clicked, the arrow
 * button pops up a dropdown menu.
 *
 * Use gtk_menu_tool_button_new() to create a new
 * #GtkMenuToolButton. Use gtk_menu_tool_button_new_from_stock() to
 * create a new #GtkMenuToolButton containing a stock item.
 *
 * <refsect2 id="GtkMenuToolButton-BUILDER-UI">
 * <title>GtkMenuToolButton as GtkBuildable</title>
 * <para>
 * The GtkMenuToolButton implementation of the GtkBuildable interface
 * supports adding a menu by specifying "menu" as the "type"
 * attribute of a &lt;child&gt; element.
 *
 * <example>
 * <title>A UI definition fragment with menus</title>
 * <programlisting><![CDATA[
 * <object class="GtkMenuToolButton">
 *   <child type="menu">
 *     <object class="GtkMenu"/>
 *   </child>
 * </object>
 * ]]></programlisting>
 * </example>
 * </para>
 * </refsect2>
 */


struct _GtkMenuToolButtonPrivate
{
  GtkWidget *button;
  GtkWidget *arrow;
  GtkWidget *arrow_button;
  GtkWidget *box;
  GtkMenu   *menu;
};

static void gtk_menu_tool_button_destroy    (GtkWidget              *widget);

static int  menu_deactivate_cb              (GtkMenuShell           *menu_shell,
					     GtkMenuToolButton      *button);

static void gtk_menu_tool_button_buildable_interface_init (GtkBuildableIface   *iface);
static void gtk_menu_tool_button_buildable_add_child      (GtkBuildable        *buildable,
							   GtkBuilder          *builder,
							   GObject             *child,
							   const gchar         *type);

enum
{
  SHOW_MENU,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MENU
};

static gint signals[LAST_SIGNAL];

static GtkBuildableIface *parent_buildable_iface;

G_DEFINE_TYPE_WITH_CODE (GtkMenuToolButton, gtk_menu_tool_button, GTK_TYPE_TOOL_BUTTON,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_menu_tool_button_buildable_interface_init))

static void
gtk_menu_tool_button_construct_contents (GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv = button->priv;
  GtkWidget *box;
  GtkWidget *parent;
  GtkOrientation orientation;

  orientation = gtk_tool_item_get_orientation (GTK_TOOL_ITEM (button));

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_DOWN, GTK_SHADOW_NONE);
    }
  else
    {
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_arrow_set (GTK_ARROW (priv->arrow), GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
    }

  parent = gtk_widget_get_parent (priv->button);
  if (priv->button && parent)
    {
      g_object_ref (priv->button);
      gtk_container_remove (GTK_CONTAINER (parent),
                            priv->button);
      gtk_container_add (GTK_CONTAINER (box), priv->button);
      g_object_unref (priv->button);
    }

  parent = gtk_widget_get_parent (priv->arrow_button);
  if (priv->arrow_button && parent)
    {
      g_object_ref (priv->arrow_button);
      gtk_container_remove (GTK_CONTAINER (parent),
                            priv->arrow_button);
      gtk_box_pack_end (GTK_BOX (box), priv->arrow_button,
                        FALSE, FALSE, 0);
      g_object_unref (priv->arrow_button);
    }

  if (priv->box)
    {
      gchar *tmp;

      /* Transfer a possible tooltip to the new box */
      g_object_get (priv->box, "tooltip-markup", &tmp, NULL);

      if (tmp)
        {
	  g_object_set (box, "tooltip-markup", tmp, NULL);
	  g_free (tmp);
	}

      /* Note: we are not destroying the button and the arrow_button
       * here because they were removed from their container above
       */
      gtk_widget_destroy (priv->box);
    }

  priv->box = box;

  gtk_container_add (GTK_CONTAINER (button), priv->box);
  gtk_widget_show_all (priv->box);

  gtk_button_set_relief (GTK_BUTTON (priv->arrow_button),
			 gtk_tool_item_get_relief_style (GTK_TOOL_ITEM (button)));
  
  gtk_widget_queue_resize (GTK_WIDGET (button));
}

static void
gtk_menu_tool_button_toolbar_reconfigured (GtkToolItem *toolitem)
{
  gtk_menu_tool_button_construct_contents (GTK_MENU_TOOL_BUTTON (toolitem));

  /* chain up */
  GTK_TOOL_ITEM_CLASS (gtk_menu_tool_button_parent_class)->toolbar_reconfigured (toolitem);
}

static void
gtk_menu_tool_button_state_changed (GtkWidget    *widget,
				    GtkStateType  previous_state)
{
  GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (widget);
  GtkMenuToolButtonPrivate *priv = button->priv;

  if (!gtk_widget_is_sensitive (widget) && priv->menu)
    {
      gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));
    }
}

static void
gtk_menu_tool_button_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MENU:
      gtk_menu_tool_button_set_menu (button, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_tool_button_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (object);

  switch (prop_id)
    {
    case PROP_MENU:
      g_value_set_object (value, button->priv->menu);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_menu_tool_button_class_init (GtkMenuToolButtonClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkToolItemClass *toolitem_class;

  object_class = (GObjectClass *)klass;
  widget_class = (GtkWidgetClass *)klass;
  toolitem_class = (GtkToolItemClass *)klass;

  object_class->set_property = gtk_menu_tool_button_set_property;
  object_class->get_property = gtk_menu_tool_button_get_property;

  widget_class->destroy = gtk_menu_tool_button_destroy;
  widget_class->state_changed = gtk_menu_tool_button_state_changed;

  toolitem_class->toolbar_reconfigured = gtk_menu_tool_button_toolbar_reconfigured;

  /**
   * GtkMenuToolButton::show-menu:
   * @button: the object on which the signal is emitted
   *
   * The ::show-menu signal is emitted before the menu is shown.
   *
   * It can be used to populate the menu on demand, using 
   * gtk_menu_tool_button_get_menu(). 

   * Note that even if you populate the menu dynamically in this way, 
   * you must set an empty menu on the #GtkMenuToolButton beforehand,
   * since the arrow is made insensitive if the menu is not set.
   */
  signals[SHOW_MENU] =
    g_signal_new (I_("show-menu"),
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkMenuToolButtonClass, show_menu),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_MENU,
                                   g_param_spec_object ("menu",
                                                        P_("Menu"),
                                                        P_("The dropdown menu"),
                                                        GTK_TYPE_MENU,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkMenuToolButtonPrivate));
}

static void
menu_position_func (GtkMenu           *menu,
                    int               *x,
                    int               *y,
                    gboolean          *push_in,
                    GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv = button->priv;
  GtkAllocation arrow_allocation;
  GtkWidget *widget = GTK_WIDGET (button);
  GtkRequisition req;
  GtkRequisition menu_req;
  GtkOrientation orientation;
  GtkTextDirection direction;
  GdkRectangle monitor;
  gint monitor_num;
  GdkScreen *screen;
  GdkWindow *window;

  gtk_widget_get_preferred_size (GTK_WIDGET (priv->menu),
                                 &menu_req, NULL);

  orientation = gtk_tool_item_get_orientation (GTK_TOOL_ITEM (button));
  direction = gtk_widget_get_direction (widget);
  window = gtk_widget_get_window (widget);

  screen = gtk_widget_get_screen (GTK_WIDGET (menu));
  monitor_num = gdk_screen_get_monitor_at_window (screen, window);
  if (monitor_num < 0)
    monitor_num = 0;
  gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      GtkAllocation allocation;

      gtk_widget_get_allocation (widget, &allocation);
      gtk_widget_get_allocation (priv->arrow_button, &arrow_allocation);

      gdk_window_get_origin (window, x, y);
      *x += allocation.x;
      *y += allocation.y;

      if (direction == GTK_TEXT_DIR_LTR)
	*x += MAX (allocation.width - menu_req.width, 0);
      else if (menu_req.width > allocation.width)
        *x -= menu_req.width - allocation.width;

      if ((*y + arrow_allocation.height + menu_req.height) <= monitor.y + monitor.height)
	*y += arrow_allocation.height;
      else if ((*y - menu_req.height) >= monitor.y)
	*y -= menu_req.height;
      else if (monitor.y + monitor.height - (*y + arrow_allocation.height) > *y)
	*y += arrow_allocation.height;
      else
	*y -= menu_req.height;
    }
  else 
    {
      gdk_window_get_origin (gtk_button_get_event_window (GTK_BUTTON (priv->arrow_button)), x, y);
      gtk_widget_get_preferred_size (priv->arrow_button,
                                     &req, NULL);

      gtk_widget_get_allocation (priv->arrow_button, &arrow_allocation);

      if (direction == GTK_TEXT_DIR_LTR)
        *x += arrow_allocation.width;
      else 
        *x -= menu_req.width;

      if (*y + menu_req.height > monitor.y + monitor.height &&
	  *y + arrow_allocation.height - monitor.y > monitor.y + monitor.height - *y)
	*y += arrow_allocation.height - menu_req.height;
    }

  *push_in = FALSE;
}

static void
popup_menu_under_arrow (GtkMenuToolButton *button,
                        GdkEventButton    *event)
{
  GtkMenuToolButtonPrivate *priv = button->priv;

  g_signal_emit (button, signals[SHOW_MENU], 0);

  if (!priv->menu)
    return;

  gtk_menu_popup (priv->menu, NULL, NULL,
                  (GtkMenuPositionFunc) menu_position_func,
                  button,
                  event ? event->button : 0,
                  event ? event->time : gtk_get_current_event_time ());
}

static void
arrow_button_toggled_cb (GtkToggleButton   *togglebutton,
                         GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv = button->priv;

  if (!priv->menu)
    return;

  if (gtk_toggle_button_get_active (togglebutton) &&
      !gtk_widget_get_visible (GTK_WIDGET (priv->menu)))
    {
      /* we get here only when the menu is activated by a key
       * press, so that we can select the first menu item */
      popup_menu_under_arrow (button, NULL);
      gtk_menu_shell_select_first (GTK_MENU_SHELL (priv->menu), FALSE);
    }
}

static gboolean
arrow_button_button_press_event_cb (GtkWidget         *widget,
                                    GdkEventButton    *event,
                                    GtkMenuToolButton *button)
{
  if (event->button == 1)
    {
      popup_menu_under_arrow (button, event);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
gtk_menu_tool_button_init (GtkMenuToolButton *button)
{
  GtkWidget *box;
  GtkWidget *arrow;
  GtkWidget *arrow_button;
  GtkWidget *real_button;

  button->priv = G_TYPE_INSTANCE_GET_PRIVATE (button,
                                              GTK_TYPE_MENU_TOOL_BUTTON,
                                              GtkMenuToolButtonPrivate);

  gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (button), FALSE);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  real_button = gtk_bin_get_child (GTK_BIN (button));
  g_object_ref (real_button);
  gtk_container_remove (GTK_CONTAINER (button), real_button);
  gtk_container_add (GTK_CONTAINER (box), real_button);
  g_object_unref (real_button);

  arrow_button = gtk_toggle_button_new ();
  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (arrow_button), arrow);
  gtk_box_pack_end (GTK_BOX (box), arrow_button,
                    FALSE, FALSE, 0);

  /* the arrow button is insentive until we set a menu */
  gtk_widget_set_sensitive (arrow_button, FALSE);

  gtk_widget_show_all (box);

  gtk_container_add (GTK_CONTAINER (button), box);

  button->priv->button = real_button;
  button->priv->arrow = arrow;
  button->priv->arrow_button = arrow_button;
  button->priv->box = box;

  g_signal_connect (arrow_button, "toggled",
		    G_CALLBACK (arrow_button_toggled_cb), button);
  g_signal_connect (arrow_button, "button-press-event",
		    G_CALLBACK (arrow_button_button_press_event_cb), button);
}

static void
gtk_menu_tool_button_destroy (GtkWidget *widget)
{
  GtkMenuToolButton *button = GTK_MENU_TOOL_BUTTON (widget);

  if (button->priv->menu)
    {
      g_signal_handlers_disconnect_by_func (button->priv->menu, 
					    menu_deactivate_cb, 
					    button);
      gtk_menu_detach (button->priv->menu);

      g_signal_handlers_disconnect_by_func (button->priv->arrow_button,
					    arrow_button_toggled_cb, 
					    button);
      g_signal_handlers_disconnect_by_func (button->priv->arrow_button, 
					    arrow_button_button_press_event_cb, 
					    button);
    }

  GTK_WIDGET_CLASS (gtk_menu_tool_button_parent_class)->destroy (widget);
}

static void
gtk_menu_tool_button_buildable_add_child (GtkBuildable *buildable,
					  GtkBuilder   *builder,
					  GObject      *child,
					  const gchar  *type)
{
  if (type && strcmp (type, "menu") == 0)
    gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (buildable),
                                   GTK_WIDGET (child));
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_menu_tool_button_buildable_interface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->add_child = gtk_menu_tool_button_buildable_add_child;
}

/**
 * gtk_menu_tool_button_new:
 * @icon_widget: (allow-none): a widget that will be used as icon widget, or %NULL
 * @label: (allow-none): a string that will be used as label, or %NULL
 *
 * Creates a new #GtkMenuToolButton using @icon_widget as icon and
 * @label as label.
 *
 * Return value: the new #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkToolItem *
gtk_menu_tool_button_new (GtkWidget   *icon_widget,
                          const gchar *label)
{
  GtkMenuToolButton *button;

  button = g_object_new (GTK_TYPE_MENU_TOOL_BUTTON, NULL);

  if (label)
    gtk_tool_button_set_label (GTK_TOOL_BUTTON (button), label);

  if (icon_widget)
    gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (button), icon_widget);

  return GTK_TOOL_ITEM (button);
}

/**
 * gtk_menu_tool_button_new_from_stock:
 * @stock_id: the name of a stock item
 *
 * Creates a new #GtkMenuToolButton.
 * The new #GtkMenuToolButton will contain an icon and label from
 * the stock item indicated by @stock_id.
 *
 * Return value: the new #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkToolItem *
gtk_menu_tool_button_new_from_stock (const gchar *stock_id)
{
  GtkMenuToolButton *button;

  g_return_val_if_fail (stock_id != NULL, NULL);

  button = g_object_new (GTK_TYPE_MENU_TOOL_BUTTON,
			 "stock-id", stock_id,
			 NULL);

  return GTK_TOOL_ITEM (button);
}

/* Callback for the "deactivate" signal on the pop-up menu.
 * This is used so that we unset the state of the toggle button
 * when the pop-up menu disappears. 
 */
static int
menu_deactivate_cb (GtkMenuShell      *menu_shell,
		    GtkMenuToolButton *button)
{
  GtkMenuToolButtonPrivate *priv = button->priv;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->arrow_button), FALSE);

  return TRUE;
}

static void
menu_detacher (GtkWidget *widget,
               GtkMenu   *menu)
{
  GtkMenuToolButtonPrivate *priv = GTK_MENU_TOOL_BUTTON (widget)->priv;

  g_return_if_fail (priv->menu == menu);

  priv->menu = NULL;
}

/**
 * gtk_menu_tool_button_set_menu:
 * @button: a #GtkMenuToolButton
 * @menu: the #GtkMenu associated with #GtkMenuToolButton
 *
 * Sets the #GtkMenu that is popped up when the user clicks on the arrow.
 * If @menu is NULL, the arrow button becomes insensitive.
 *
 * Since: 2.6
 **/
void
gtk_menu_tool_button_set_menu (GtkMenuToolButton *button,
                               GtkWidget         *menu)
{
  GtkMenuToolButtonPrivate *priv;

  g_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (button));
  g_return_if_fail (GTK_IS_MENU (menu) || menu == NULL);

  priv = button->priv;

  if (priv->menu != GTK_MENU (menu))
    {
      if (priv->menu && gtk_widget_get_visible (GTK_WIDGET (priv->menu)))
        gtk_menu_shell_deactivate (GTK_MENU_SHELL (priv->menu));

      if (priv->menu)
	{
          g_signal_handlers_disconnect_by_func (priv->menu, 
						menu_deactivate_cb, 
						button);
	  gtk_menu_detach (priv->menu);
	}

      priv->menu = GTK_MENU (menu);

      if (priv->menu)
        {
          gtk_menu_attach_to_widget (priv->menu, GTK_WIDGET (button),
                                     menu_detacher);

          gtk_widget_set_sensitive (priv->arrow_button, TRUE);

          g_signal_connect (priv->menu, "deactivate",
                            G_CALLBACK (menu_deactivate_cb), button);
        }
      else
       gtk_widget_set_sensitive (priv->arrow_button, FALSE);
    }

  g_object_notify (G_OBJECT (button), "menu");
}

/**
 * gtk_menu_tool_button_get_menu:
 * @button: a #GtkMenuToolButton
 *
 * Gets the #GtkMenu associated with #GtkMenuToolButton.
 *
 * Return value: (transfer none): the #GtkMenu associated
 *     with #GtkMenuToolButton
 *
 * Since: 2.6
 **/
GtkWidget *
gtk_menu_tool_button_get_menu (GtkMenuToolButton *button)
{
  g_return_val_if_fail (GTK_IS_MENU_TOOL_BUTTON (button), NULL);

  return GTK_WIDGET (button->priv->menu);
}

/**
 * gtk_menu_tool_button_set_arrow_tooltip_text:
 * @button: a #GtkMenuToolButton
 * @text: text to be used as tooltip text for button's arrow button
 *
 * Sets the tooltip text to be used as tooltip for the arrow button which
 * pops up the menu.  See gtk_tool_item_set_tooltip_text() for setting a tooltip
 * on the whole #GtkMenuToolButton.
 *
 * Since: 2.12
 **/
void
gtk_menu_tool_button_set_arrow_tooltip_text (GtkMenuToolButton *button,
					     const gchar       *text)
{
  g_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (button));

  gtk_widget_set_tooltip_text (button->priv->arrow_button, text);
}

/**
 * gtk_menu_tool_button_set_arrow_tooltip_markup:
 * @button: a #GtkMenuToolButton
 * @markup: markup text to be used as tooltip text for button's arrow button
 *
 * Sets the tooltip markup text to be used as tooltip for the arrow button
 * which pops up the menu.  See gtk_tool_item_set_tooltip_text() for setting a
 * tooltip on the whole #GtkMenuToolButton.
 *
 * Since: 2.12
 **/
void
gtk_menu_tool_button_set_arrow_tooltip_markup (GtkMenuToolButton *button,
					       const gchar       *markup)
{
  g_return_if_fail (GTK_IS_MENU_TOOL_BUTTON (button));

  gtk_widget_set_tooltip_markup (button->priv->arrow_button, markup);
}
