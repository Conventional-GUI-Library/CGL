/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include "gtkcontainercellaccessible.h"
#include "gtkcellaccessible.h"
#include "gtkcellaccessibleparent.h"

static const struct {
  AtkState atk_state;
  GtkCellRendererState renderer_state;
  gboolean invert;
} state_map[] = {
  { ATK_STATE_SENSITIVE, GTK_CELL_RENDERER_INSENSITIVE, TRUE },
  { ATK_STATE_ENABLED,   GTK_CELL_RENDERER_INSENSITIVE, TRUE },
  { ATK_STATE_SELECTED,  GTK_CELL_RENDERER_SELECTED,    FALSE },
  /* XXX: why do we map ACTIVE here? */
  { ATK_STATE_ACTIVE,    GTK_CELL_RENDERER_FOCUSED,     FALSE },
  { ATK_STATE_FOCUSED,   GTK_CELL_RENDERER_FOCUSED,     FALSE },
  { ATK_STATE_EXPANDABLE,GTK_CELL_RENDERER_EXPANDABLE,  FALSE },
  { ATK_STATE_EXPANDED,  GTK_CELL_RENDERER_EXPANDED,    FALSE },
};

static void atk_action_interface_init    (AtkActionIface    *iface);
static void atk_component_interface_init (AtkComponentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkCellAccessible, _gtk_cell_accessible, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init))

static void
gtk_cell_accessible_object_finalize (GObject *obj)
{
  AtkRelationSet *relation_set;
  AtkRelation *relation;
  GPtrArray *target;
  gpointer target_object;
  gint i;

  relation_set = atk_object_ref_relation_set (ATK_OBJECT (obj));
  if (ATK_IS_RELATION_SET (relation_set))
    {
      relation = atk_relation_set_get_relation_by_type (relation_set,
                                                        ATK_RELATION_NODE_CHILD_OF);
      if (relation)
        {
          target = atk_relation_get_target (relation);
          for (i = 0; i < target->len; i++)
            {
              target_object = g_ptr_array_index (target, i);
              if (GTK_IS_CELL_ACCESSIBLE (target_object))
                g_object_unref (target_object);
            }
        }
      g_object_unref (relation_set);
    }
  G_OBJECT_CLASS (_gtk_cell_accessible_parent_class)->finalize (obj);
}

static gint
gtk_cell_accessible_get_index_in_parent (AtkObject *obj)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (obj);

  parent = atk_object_get_parent (obj);
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    return g_list_index (GTK_CONTAINER_CELL_ACCESSIBLE (parent)->children, obj);

  parent = gtk_widget_get_accessible (cell->widget);
  if (parent == NULL)
    return -1;

  return _gtk_cell_accessible_parent_get_child_index (GTK_CELL_ACCESSIBLE_PARENT (parent), cell);
}

static AtkStateSet *
gtk_cell_accessible_ref_state_set (AtkObject *accessible)
{
  GtkCellAccessible *cell_accessible;
  AtkStateSet *state_set;
  GtkCellRendererState flags;
  guint i;

  cell_accessible = GTK_CELL_ACCESSIBLE (accessible);

  state_set = atk_state_set_new ();

  if (cell_accessible->widget == NULL)
    {
      atk_state_set_add_state (state_set, ATK_STATE_DEFUNCT);
      return state_set;
    }

  flags = _gtk_cell_accessible_get_state (cell_accessible);

  atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);
  atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);
  atk_state_set_add_state (state_set, ATK_STATE_TRANSIENT);
  atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);

  for (i = 0; i < G_N_ELEMENTS (state_map); i++)
    {
      if (flags & state_map[i].renderer_state)
        {
          if (!state_map[i].invert)
            atk_state_set_add_state (state_set, state_map[i].atk_state);
        }
      else
        {
          if (state_map[i].invert)
            atk_state_set_add_state (state_set, state_map[i].atk_state);
        }
    }

  if (gtk_widget_get_mapped (cell_accessible->widget))
    atk_state_set_add_state (state_set, ATK_STATE_SHOWING);

  return state_set;
}


static void
_gtk_cell_accessible_class_init (GtkCellAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize = gtk_cell_accessible_object_finalize;

  class->get_index_in_parent = gtk_cell_accessible_get_index_in_parent;
  class->ref_state_set = gtk_cell_accessible_ref_state_set;
}

static void
_gtk_cell_accessible_init (GtkCellAccessible *cell)
{
  cell->widget = NULL;
}

static void
widget_destroyed (GtkWidget         *widget,
                  GtkCellAccessible *cell)
{
  cell->widget = NULL;
}

void
_gtk_cell_accessible_initialise (GtkCellAccessible *cell,
                                 GtkWidget         *widget,
                                 AtkObject         *parent)
{
  cell->widget = widget;
  atk_object_set_parent (ATK_OBJECT (cell), parent);

  g_signal_connect_object (G_OBJECT (widget), "destroy",
                           G_CALLBACK (widget_destroyed), cell, 0);
}

gboolean
_gtk_cell_accessible_add_state (GtkCellAccessible *cell,
                                AtkStateType       state_type,
                                gboolean           emit_signal)
{
  AtkObject *parent;

  /* The signal should only be generated if the value changed,
   * not when the cell is set up. So states that are set
   * initially should pass FALSE as the emit_signal argument.
   */
  if (emit_signal)
    {
      atk_object_notify_state_change (ATK_OBJECT (cell), state_type, TRUE);
      /* If state_type is ATK_STATE_VISIBLE, additional notification */
      if (state_type == ATK_STATE_VISIBLE)
        g_signal_emit_by_name (cell, "visible-data-changed");
    }

  /* If the parent is a flyweight container cell, propagate the state
   * change to it also
   */
  parent = atk_object_get_parent (ATK_OBJECT (cell));
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (parent), state_type, emit_signal);

  return TRUE;
}

gboolean
_gtk_cell_accessible_remove_state (GtkCellAccessible *cell,
                                   AtkStateType       state_type,
                                   gboolean           emit_signal)
{
  AtkObject *parent;

  parent = atk_object_get_parent (ATK_OBJECT (cell));

  /* The signal should only be generated if the value changed,
   * not when the cell is set up.  So states that are set
   * initially should pass FALSE as the emit_signal argument.
   */
  if (emit_signal)
    {
      atk_object_notify_state_change (ATK_OBJECT (cell), state_type, FALSE);
      /* If state_type is ATK_STATE_VISIBLE, additional notification */
      if (state_type == ATK_STATE_VISIBLE)
        g_signal_emit_by_name (cell, "visible-data-changed");
    }

  /* If the parent is a flyweight container cell, propagate the state
   * change to it also
   */
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (parent), state_type, emit_signal);

  return TRUE;
}

static gint
gtk_cell_accessible_action_get_n_actions (AtkAction *action)
{
  return 3;
}

static const gchar *
gtk_cell_accessible_action_get_name (AtkAction *action,
                                     gint       index)
{
  switch (index)
    {
    case 0:
      return "expand or contract";
    case 1:
      return "edit";
    case 2:
      return "activate";
    default:
      return NULL;
    }
}

static const gchar *
gtk_cell_accessible_action_get_description (AtkAction *action,
                                            gint       index)
{
  switch (index)
    {
    case 0:
      return "expands or contracts the row in the tree view containing this cell";
    case 1:
      return "creates a widget in which the contents of the cell can be edited";
    case 2:
      return "activate the cell";
    default:
      return NULL;
    }
}

static const gchar *
gtk_cell_accessible_action_get_keybinding (AtkAction *action,
                                           gint       index)
{
  return NULL;
}

static gboolean
gtk_cell_accessible_action_do_action (AtkAction *action,
                                      gint       index)
{
  GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (action);
  GtkCellAccessibleParent *parent;

  cell = GTK_CELL_ACCESSIBLE (action);
  if (cell->widget == NULL)
    return FALSE;

  parent = GTK_CELL_ACCESSIBLE_PARENT (gtk_widget_get_accessible (cell->widget));

  switch (index)
    {
    case 0:
      _gtk_cell_accessible_parent_expand_collapse (parent, cell);
    case 1:
      _gtk_cell_accessible_parent_edit (parent, cell);
    case 2:
      _gtk_cell_accessible_parent_activate (parent, cell);
    default:
      return FALSE;
    }

  return TRUE;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->get_n_actions = gtk_cell_accessible_action_get_n_actions;
  iface->do_action = gtk_cell_accessible_action_do_action;
  iface->get_name = gtk_cell_accessible_action_get_name;
  iface->get_description = gtk_cell_accessible_action_get_description;
  iface->get_keybinding = gtk_cell_accessible_action_get_keybinding;
}

static void
gtk_cell_accessible_get_extents (AtkComponent *component,
                                 gint         *x,
                                 gint         *y,
                                 gint         *width,
                                 gint         *height,
                                 AtkCoordType  coord_type)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (component);
  parent = gtk_widget_get_accessible (cell->widget);

  _gtk_cell_accessible_parent_get_cell_extents (GTK_CELL_ACCESSIBLE_PARENT (parent),
                                                cell,
                                                x, y, width, height, coord_type);
}

static gboolean
gtk_cell_accessible_grab_focus (AtkComponent *component)
{
  GtkCellAccessible *cell;
  AtkObject *parent;

  cell = GTK_CELL_ACCESSIBLE (component);
  parent = gtk_widget_get_accessible (cell->widget);

  return _gtk_cell_accessible_parent_grab_focus (GTK_CELL_ACCESSIBLE_PARENT (parent), cell);
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  iface->get_extents = gtk_cell_accessible_get_extents;
  iface->grab_focus = gtk_cell_accessible_grab_focus;
}

/**
 * _gtk_cell_accessible_get_state:
 * @cell: a #GtkCellAccessible
 *
 * Gets the state that would be used to render the area referenced by @cell.
 *
 * Returns: the #GtkCellRendererState for cell
 **/
GtkCellRendererState
_gtk_cell_accessible_get_state (GtkCellAccessible *cell)
{
  AtkObject *parent;

  g_return_val_if_fail (GTK_IS_CELL_ACCESSIBLE (cell), 0);

  parent = gtk_widget_get_accessible (cell->widget);
  if (parent == NULL)
    return 0;

  return _gtk_cell_accessible_parent_get_renderer_state (GTK_CELL_ACCESSIBLE_PARENT (parent), cell);
}

/**
 * _gtk_cell_accessible_state_changed:
 * @cell: a #GtkCellAccessible
 * @added: the flags that were added from @cell
 * @removed: the flags that were removed from @cell
 *
 * Notifies @cell of state changes. Multiple states may be added
 * or removed at the same time. A state that is @added may not be
 * @removed at the same time.
 **/
void
_gtk_cell_accessible_state_changed (GtkCellAccessible    *cell,
                                    GtkCellRendererState  added,
                                    GtkCellRendererState  removed)
{
  AtkObject *object;
  guint i;

  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (cell));
  g_return_if_fail ((added & removed) == 0);

  object = ATK_OBJECT (cell);

  for (i = 0; i < G_N_ELEMENTS (state_map); i++)
    {
      if (added & state_map[i].renderer_state)
        atk_object_notify_state_change (object, 
                                        state_map[i].atk_state,
                                        !state_map[i].invert);
      if (added & state_map[i].renderer_state)
        atk_object_notify_state_change (object, 
                                        state_map[i].atk_state,
                                        state_map[i].invert);
    }
}

/**
 * _gtk_cell_accessible_update_cache:
 * @cell: the cell that is changed
 *
 * Notifies the cell that the values in the data in the row that
 * is used to feed the cell renderer with has changed. The
 * cell_changed function of @cell is called to send update
 * notifications for the properties it takes from its cell
 * renderer.
 * 
 * Note that there is no higher granularity available about which
 * properties changed, so you will need to make do with this
 * function.
 **/
void
_gtk_cell_accessible_update_cache (GtkCellAccessible *cell)
{
  GtkCellAccessibleClass *klass;
  
  g_return_if_fail (GTK_CELL_ACCESSIBLE (cell));

  klass = GTK_CELL_ACCESSIBLE_GET_CLASS (cell);

  if (klass->update_cache)
    klass->update_cache (cell);
}

