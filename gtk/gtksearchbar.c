/* GTK - The GIMP Toolkit
 * Copyright (C) 2013 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 2013.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkentry.h"
#include "gtkentryprivate.h"
#include "gtkintl.h"
#include "gtktoolbar.h"
#include "gtktoolitem.h"
#include "gtkstylecontext.h"
#include "gtksearchbar.h"

/**
 * SECTION:gtksearchbar
 * @Short_description: An toolbar to integrate a search entry with
 * @Title: GtkSearchBar
 *
 * #GtkSearchBar is a container made to have a search entry (possibly
 * with additional connex widgets, such as drop-down menus, or buttons) built-in.
 * The search bar would appear when a search is started through typing on the
 * keyboard, or the application's search mode is toggled on.
 *
 * For keyboard presses to start a search, events will need to be forwarded
 * from the top-level window that contains the search bar. See
 * gtk_search_bar_handle_event() for example code.
 *
 * You will also need to tell the search bar about which entry you are
 * using as your search entry using gtk_search_bar_connect_entry().
 * The following example shows you how to create a more complex
 * search entry.
 *
 * <example>
 * <title>Creating a search bar</title>
 * <programlisting><![CDATA[
 * bar = gtk_search_bar_new ();
 *
 * /<!---->* Create a box for the search entry and related widgets *<---->/
 * entry = gtk_search_entry_new ();
 * box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
 * gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
 * /<!---->* Add a menu button to select the category of the search *<---->/
 * menu_button = gtk_menu_button_new ();
 * gtk_box_pack_start (GTK_BOX (box), menu_button, FALSE, FALSE, 0);
 * gtk_container_add (GTK_CONTAINER (searchbar), box);
 *
 * /<!---->* And tell the search bar about the search entry *<---->/
 * gtk_search_bar_set_search_entry (GTK_SEARCH_BAR (bar), entry);
 * ]]></programlisting>
 * </example>
 *
 * Since: 3.10
 */

G_DEFINE_TYPE (GtkSearchBar, gtk_search_bar, GTK_TYPE_BIN)

struct _GtkSearchBarPrivate {
  /* Template widgets */
  GtkWidget   *revealer;
  GtkWidget   *toolbar;
  GtkWidget   *box_center;
  GtkWidget   *close_button;

  GtkWidget   *entry;
  gboolean     reveal_child;
};

enum {
  PROP_0,
  PROP_SEARCH_MODE_ENABLED,
  PROP_SHOW_CLOSE_BUTTON,
  LAST_PROPERTY
};

static GParamSpec *widget_props[LAST_PROPERTY] = { NULL, };

static gboolean
is_keynav_event (GdkEvent *event,
                 guint     keyval)
{
  GdkModifierType state = 0;

  gdk_event_get_state (event, &state);

  if (keyval == GDK_KEY_Tab ||
      keyval == GDK_KEY_KP_Tab ||
      keyval == GDK_KEY_Up ||
      keyval == GDK_KEY_KP_Up ||
      keyval == GDK_KEY_Down ||
      keyval == GDK_KEY_KP_Down ||
      keyval == GDK_KEY_Left ||
      keyval == GDK_KEY_KP_Left ||
      keyval == GDK_KEY_Right ||
      keyval == GDK_KEY_KP_Right ||
      keyval == GDK_KEY_Home ||
      keyval == GDK_KEY_KP_Home ||
      keyval == GDK_KEY_End ||
      keyval == GDK_KEY_KP_End ||
      keyval == GDK_KEY_Page_Up ||
      keyval == GDK_KEY_KP_Page_Up ||
      keyval == GDK_KEY_Page_Down ||
      keyval == GDK_KEY_KP_Page_Down ||
      ((state & (GDK_CONTROL_MASK | GDK_MOD1_MASK)) != 0))
        return TRUE;

  /* Other navigation events should get automatically
   * ignored as they will not change the content of the entry
   */

  return FALSE;
}

static gboolean
entry_key_pressed_event_cb (GtkWidget    *widget,
                            GdkEvent     *event,
                            GtkSearchBar *bar)
{
  guint keyval;

  if (!gdk_event_get_keyval (event, &keyval) ||
      keyval != GDK_KEY_Escape)
    return GDK_EVENT_PROPAGATE;

  gtk_revealer_set_reveal_child (GTK_REVEALER (bar->priv->revealer), FALSE);
  return GDK_EVENT_STOP;
}

static void
preedit_changed_cb (GtkEntry  *entry,
                    GtkWidget *popup,
                    gboolean  *preedit_changed)
{
  *preedit_changed = TRUE;
}

/**
 * gtk_search_bar_handle_event:
 * @bar: a #GtkSearchBar
 * @event: a #GdkEvent containing key press events
 *
 * This function should be called when the top-level
 * window which contains the search bar received a key event.
 *
 * If the key event is handled by the search bar, the bar will
 * be shown, the entry populated with the entered text and %GDK_EVENT_STOP
 * will be returned. The caller should ensure that events are
 * not propagated further.
 *
 * If no entry has been connected to the search bar, using
 * gtk_search_bar_connect_entry(), this function will return immediately with
 * a warning.
 *
 * <example>
 * <title>Showing the search bar on key presses</title>
 * <programlisting><![CDATA[
 * static gboolean
 * window_key_press_event_cb (GtkWidget *widget,
 *                            GdkEvent  *event,
 *                            gpointer   user_data)
 * {
 *   return gtk_search_bar_handle_event (GTK_SEARCH_BAR (user_data), event);
 * }
 *
 * g_signal_connect (window, "key-press-event",
 *                   G_CALLBACK (window_key_press_event_cb), search_bar);
 * ]]></programlisting>
 * </example>
 *
 * Return value: %GDK_EVENT_STOP if the key press event resulted in text
 * being * entered in the search entry (and revealing the search bar if
 * necessary), %GDK_EVENT_PROPAGATE otherwise.
 *
 * Since: 3.10
 **/
gboolean
gtk_search_bar_handle_event (GtkSearchBar *bar,
                             GdkEvent     *event)
{
  guint keyval;
  gboolean handled;
  gboolean preedit_changed;
  guint preedit_change_id;
  gboolean res;
  char *old_text, *new_text;

  if (!bar->priv->entry)
    {
      g_warning ("The search bar does not have an entry connected to it. Call gtk_search_bar_connect_entry() to connect one.");
      return GDK_EVENT_PROPAGATE;
    }

  /* Exit early if the search bar is already shown,
   * the event doesn't contain a key press,
   * or the event is a navigation or space bar key press
   */
  if (bar->priv->reveal_child ||
      !gdk_event_get_keyval (event, &keyval) ||
      is_keynav_event (event, keyval) ||
      keyval == GDK_KEY_space)
    return GDK_EVENT_PROPAGATE;

  if (!gtk_widget_get_realized (bar->priv->entry))
    gtk_widget_realize (bar->priv->entry);

  handled = GDK_EVENT_PROPAGATE;
  preedit_changed = FALSE;
  preedit_change_id = g_signal_connect (bar->priv->entry, "preedit-changed",
                                        G_CALLBACK (preedit_changed_cb), &preedit_changed);

  old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (bar->priv->entry)));
  res = gtk_widget_event (bar->priv->entry, event);
  new_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (bar->priv->entry)));

  g_signal_handler_disconnect (bar->priv->entry, preedit_change_id);

  if ((res && g_strcmp0 (new_text, old_text) != 0) || preedit_changed)
    {
      handled = GDK_EVENT_STOP;
      gtk_revealer_set_reveal_child (GTK_REVEALER (bar->priv->revealer), TRUE);
    }

  g_free (old_text);
  g_free (new_text);

  return handled;
}

static void
reveal_child_changed_cb (GObject      *object,
                         GParamSpec   *pspec,
                         GtkSearchBar *bar)
{
  gboolean reveal_child;

  g_object_get (object, "reveal-child", &reveal_child, NULL);
  if (reveal_child == bar->priv->reveal_child)
    return;

  bar->priv->reveal_child = reveal_child;

  if (reveal_child)
    _gtk_entry_grab_focus (GTK_ENTRY (bar->priv->entry), FALSE);
  else
    gtk_entry_set_text (GTK_ENTRY (bar->priv->entry), "");

  g_object_notify (G_OBJECT (bar), "search-mode-enabled");
}

static void
close_button_clicked_cb (GtkWidget    *button,
                         GtkSearchBar *bar)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (bar->priv->revealer), FALSE);
}

static void
gtk_search_bar_add (GtkContainer *container,
                    GtkWidget    *child)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (container);

  /* When constructing the widget, we want the revealer to be added
   * as the first child of the search bar, as an implementation detail.
   * After that, the child added by the application should be added
   * to the toolbar's box_center.
   */
  if (bar->priv->box_center == NULL)
    {
      GTK_CONTAINER_CLASS (gtk_search_bar_parent_class)->add (container, child);
      /* If an entry is the only child, save the developer a couple of
       * lines of code */
      if (GTK_IS_ENTRY (child))
        gtk_search_bar_connect_entry (bar, GTK_ENTRY (child));
    }
  else
    {
      gtk_container_add (GTK_CONTAINER (bar->priv->box_center), child);
    }
}

static void
gtk_search_bar_set_property (GObject         *object,
                             guint            prop_id,
                             const GValue    *value,
                             GParamSpec      *pspec)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      gtk_search_bar_set_search_mode (bar, g_value_get_boolean (value));
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      gtk_search_bar_set_show_close_button (bar, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_search_bar_get_property (GObject         *object,
                             guint            prop_id,
                             GValue          *value,
                             GParamSpec      *pspec)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE_ENABLED:
      g_value_set_boolean (value, bar->priv->reveal_child);
      break;
    case PROP_SHOW_CLOSE_BUTTON:
      g_value_set_boolean (value, gtk_search_bar_get_show_close_button (bar));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_search_bar_dispose (GObject *object)
{
  GtkSearchBar *bar = GTK_SEARCH_BAR (object);

  if (bar->priv->entry)
    {
      g_signal_handlers_disconnect_by_func (bar->priv->entry, entry_key_pressed_event_cb, bar);
      g_object_remove_weak_pointer (G_OBJECT (bar->priv->entry), (gpointer *) &bar->priv->entry);
      bar->priv->entry = NULL;
    }

  G_OBJECT_CLASS (gtk_search_bar_parent_class)->dispose (object);
}

static void
gtk_search_bar_class_init (GtkSearchBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

  object_class->dispose = gtk_search_bar_dispose;
  object_class->set_property = gtk_search_bar_set_property;
  object_class->get_property = gtk_search_bar_get_property;

  container_class->add = gtk_search_bar_add;

  /**
   * GtkEntry:search-mode-enabled:
   *
   * Whether the search mode is on and the search bar shown.
   *
   * See gtk_search_bar_set_search_mode() for details.
   *
   **/
  widget_props[PROP_SEARCH_MODE_ENABLED] = g_param_spec_boolean ("search-mode-enabled",
                                                                 P_("Search Mode Enabled"),
                                                                 P_("Whether the search mode is on and the search bar shown"),
                                                                 FALSE,
                                                                 G_PARAM_READWRITE);
  /**
   * GtkEntry:show-close-button:
   *
   * Whether to show the close button in the toolbar.
   *
   **/
  widget_props[PROP_SHOW_CLOSE_BUTTON] = g_param_spec_boolean ("show-close-button",
                                                               P_("Show Close Button"),
                                                               P_("Whether to show the close button in the toolbar"),
                                                               TRUE,
                                                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, LAST_PROPERTY, widget_props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/gtksearchbar.ui");
  gtk_widget_class_bind_child_internal (widget_class, GtkSearchBarPrivate, toolbar);
  gtk_widget_class_bind_child_internal (widget_class, GtkSearchBarPrivate, revealer);
  gtk_widget_class_bind_child_internal (widget_class, GtkSearchBarPrivate, box_center);
  gtk_widget_class_bind_child_internal (widget_class, GtkSearchBarPrivate, close_button);

  g_type_class_add_private (klass, sizeof (GtkSearchBarPrivate));
}

static void
gtk_search_bar_init (GtkSearchBar *bar)
{
  bar->priv = G_TYPE_INSTANCE_GET_PRIVATE (bar, GTK_TYPE_SEARCH_BAR, GtkSearchBarPrivate);

  gtk_widget_init_template (GTK_WIDGET (bar));

  gtk_widget_show_all (bar->priv->toolbar);

  g_signal_connect (bar->priv->revealer, "notify::reveal-child",
                    G_CALLBACK (reveal_child_changed_cb), bar);

  gtk_widget_set_no_show_all (bar->priv->close_button, TRUE);
  g_signal_connect (bar->priv->close_button, "clicked",
                    G_CALLBACK (close_button_clicked_cb), bar);
};

/**
 * gtk_search_bar_new:
 *
 * Creates a #GtkSearchBar. You will need to tell it about
 * which widget is going to be your text entry using
 * gtk_search_bar_set_entry().
 *
 * Return value: a new #GtkSearchBar
 *
 * Since: 3.10
 */
GtkWidget *
gtk_search_bar_new (void)
{
  return g_object_new (GTK_TYPE_SEARCH_BAR, NULL);
}

/**
 * gtk_search_bar_connect_entry:
 * @bar: a #GtkSearchBar
 * @entry: a #GtkEntry
 *
 * Connects the #GtkEntry widget passed as the one to be used in
 * this search bar. The entry should be a descendant of the search bar.
 * This is only required if the entry isn't the direct child of the
 * search bar (as in our main example).
 *
 * Since: 3.10
 **/
void
gtk_search_bar_connect_entry (GtkSearchBar *bar,
                              GtkEntry     *entry)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));
  g_return_if_fail (entry == NULL || GTK_IS_ENTRY (entry));

  if (bar->priv->entry != NULL)
    {
      g_signal_handlers_disconnect_by_func (bar->priv->entry, entry_key_pressed_event_cb, bar);
      g_object_remove_weak_pointer (G_OBJECT (bar->priv->entry), (gpointer *) &bar->priv->entry);
      bar->priv->entry = NULL;
    }

  if (entry != NULL)
    {
      bar->priv->entry = GTK_WIDGET (entry);
      g_object_add_weak_pointer (G_OBJECT (bar->priv->entry), (gpointer *) &bar->priv->entry);
      g_signal_connect (bar->priv->entry, "key-press-event",
                        G_CALLBACK (entry_key_pressed_event_cb), bar);
    }
}

/**
 * gtk_search_bar_get_search_mode:
 * @bar: a #GtkSearchBar
 *
 * Returns whether the search mode is on or off.
 *
 * Return value: whether search mode is toggled on
 *
 * Since: 3.10
 **/
gboolean
gtk_search_bar_get_search_mode (GtkSearchBar *bar)
{
  g_return_val_if_fail (GTK_IS_SEARCH_BAR (bar), FALSE);

  return bar->priv->reveal_child;
}

/**
 * gtk_search_bar_set_search_mode:
 * @bar: a #GtkSearchBar
 * @search_mode: the new state of the search mode
 *
 * Switches the search mode on or off.
 *
 * Since: 3.10
 **/
void
gtk_search_bar_set_search_mode (GtkSearchBar *bar,
                                gboolean      search_mode)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));

  gtk_revealer_set_reveal_child (GTK_REVEALER (bar->priv->revealer), search_mode);
}

/**
 * gtk_search_bar_get_show_close_button:
 * @bar: a #GtkSearchBar
 *
 * Returns whether the close button is shown.
 *
 * Return value: whether the close button is shown
 *
 * Since: 3.10
 **/
gboolean
gtk_search_bar_get_show_close_button (GtkSearchBar *bar)
{
  g_return_val_if_fail (GTK_IS_SEARCH_BAR (bar), FALSE);

  return gtk_widget_get_visible (bar->priv->close_button);
}

/**
 * gtk_search_bar_set_show_close_button:
 * @bar: a #GtkSearchBar
 * @shown: whether the close button will be shown or not.
 *
 * Shows or hides the close button.
 *
 * Since: 3.10
 **/
void
gtk_search_bar_set_show_close_button (GtkSearchBar *bar,
                                      gboolean      visible)
{
  g_return_if_fail (GTK_IS_SEARCH_BAR (bar));

  gtk_widget_set_visible (bar->priv->close_button, visible);
}
