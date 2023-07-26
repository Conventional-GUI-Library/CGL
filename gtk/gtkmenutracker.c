/*
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkmenutracker.h"

typedef struct _GtkMenuTrackerSection GtkMenuTrackerSection;

struct _GtkMenuTracker
{
  GtkMenuTrackerInsertFunc  insert_func;
  GtkMenuTrackerRemoveFunc  remove_func;
  gpointer                  user_data;

  GtkMenuTrackerSection    *toplevel;
};

struct _GtkMenuTrackerSection
{
  GMenuModel *model;
  GSList     *items;
  gchar      *action_namespace;

  guint       with_separators : 1;
  guint       has_separator   : 1;

  gulong      handler;
};

static GtkMenuTrackerSection *  gtk_menu_tracker_section_new    (GtkMenuTracker        *tracker,
                                                                 GMenuModel            *model,
                                                                 gboolean               with_separators,
                                                                 gint                   offset,
                                                                 const gchar           *action_namespace);
static void                    gtk_menu_tracker_section_free    (GtkMenuTrackerSection *section);

static GtkMenuTrackerSection *
gtk_menu_tracker_section_find_model (GtkMenuTrackerSection *section,
                                     GMenuModel            *model,
                                     gint                  *offset)
{
  GSList *item;

  if (section->has_separator)
    (*offset)++;

  if (section->model == model)
    return section;

  for (item = section->items; item; item = item->next)
    {
      GtkMenuTrackerSection *subsection = item->data;

      if (subsection)
        {
          GtkMenuTrackerSection *found_section;

          found_section = gtk_menu_tracker_section_find_model (subsection, model, offset);

          if (found_section)
            return found_section;
        }
      else
        (*offset)++;
    }

  return FALSE;
}

/* this is responsible for syncing the showing of a separator for a
 * single subsection (and its children).
 *
 * we only ever show separators if we have _actual_ children (ie: we do
 * not show a separator if the section contains only empty child
 * sections).  it's difficult to determine this on-the-fly, so we have
 * this separate function to come back later and figure it out.
 *
 * 'section' is that section.
 *
 * 'tracker' is passed in so that we can emit callbacks when we decide
 * to add/remove separators.
 *
 * 'offset' is passed in so we know which position to emit in our
 * callbacks.  ie: if we add a separator right at the top of this
 * section then we would emit it with this offset.  deeper inside, we
 * adjust accordingly.
 *
 * could_have_separator is true in two situations:
 *
 *  - our parent section had with_separators defined and we are not the
 *    first section (ie: we should add a separator if we have content in
 *    order to divide us from the items above)
 *
 *  - if we had a 'label' attribute set for this section
 *
 * parent_model and parent_index are passed in so that we can give them
 * to the insertion callback so that it can see the label (and anything
 * else that happens to be defined on the section).
 *
 * we iterate each item in ourselves.  for subsections, we recursively
 * run ourselves to sync separators.  after we are done, we notice if we
 * have any items in us or if we are completely empty and sync if our
 * separator is shown or not.
 */
static gint
gtk_menu_tracker_section_sync_separators (GtkMenuTrackerSection *section,
                                          GtkMenuTracker        *tracker,
                                          gint                   offset,
                                          gboolean               could_have_separator,
                                          GMenuModel            *parent_model,
                                          gint                   parent_index)
{
  gboolean should_have_separator;
  gint n_items = 0;
  GSList *item;
  gint i = 0;

  for (item = section->items; item; item = item->next)
    {
      GtkMenuTrackerSection *subsection = item->data;

      if (subsection)
        {
          gboolean could_have_separator;

          could_have_separator = (section->with_separators && i > 0) ||
                                 g_menu_model_get_item_attribute (section->model, i, "label", "s", NULL);

          n_items += gtk_menu_tracker_section_sync_separators (subsection, tracker, offset + n_items,
                                                               could_have_separator, section->model, i);
        }
      else
        n_items++;

      i++;
    }

  should_have_separator = could_have_separator && n_items != 0;

  if (should_have_separator > section->has_separator)
    {
      /* Add a separator */
      (* tracker->insert_func) (offset, parent_model, parent_index, NULL, TRUE, tracker->user_data);
      section->has_separator = TRUE;
    }
  else if (should_have_separator < section->has_separator)
    {
      /* Remove a separator */
      (* tracker->remove_func) (offset, tracker->user_data);
      section->has_separator = FALSE;
    }

  n_items += section->has_separator;

  return n_items;
}

static gint
gtk_menu_tracker_section_measure (GtkMenuTrackerSection *section)
{
  GSList *item;
  gint n_items;

  if (section == NULL)
    return 1;

  n_items = 0;

  if (section->has_separator)
    n_items++;

  for (item = section->items; item; item = item->next)
    n_items += gtk_menu_tracker_section_measure (item->data);

  return n_items;
}

static void
gtk_menu_tracker_remove_items (GtkMenuTracker  *tracker,
                               GSList         **change_point,
                               gint             offset,
                               gint             n_items)
{
  gint i;

  for (i = 0; i < n_items; i++)
    {
      GtkMenuTrackerSection *subsection;
      gint n;

      subsection = (*change_point)->data;
      *change_point = g_slist_delete_link (*change_point, *change_point);

      n = gtk_menu_tracker_section_measure (subsection);
      gtk_menu_tracker_section_free (subsection);

      while (n--)
        (* tracker->remove_func) (offset, tracker->user_data);
    }
}

static void
gtk_menu_tracker_add_items (GtkMenuTracker         *tracker,
                            GtkMenuTrackerSection  *section,
                            GSList                **change_point,
                            gint                    offset,
                            GMenuModel             *model,
                            gint                    position,
                            gint                    n_items)
{
  while (n_items--)
    {
      GMenuModel *submenu;

      submenu = g_menu_model_get_item_link (model, position + n_items, G_MENU_LINK_SECTION);
      g_assert (submenu != model);
      if (submenu != NULL)
        {
          GtkMenuTrackerSection *subsection;
          gchar *action_namespace = NULL;

          g_menu_model_get_item_attribute (model, position + n_items,
                                           G_MENU_ATTRIBUTE_ACTION_NAMESPACE, "s", &action_namespace);

          if (section->action_namespace)
            {
              gchar *namespace;

              namespace = g_strjoin (".", section->action_namespace, action_namespace, NULL);
              subsection = gtk_menu_tracker_section_new (tracker, submenu, FALSE, offset, namespace);
              g_free (namespace);
            }
          else
            subsection = gtk_menu_tracker_section_new (tracker, submenu, FALSE, offset, section->action_namespace);

          *change_point = g_slist_prepend (*change_point, subsection);
          g_free (action_namespace);
          g_object_unref (submenu);
        }
      else
        {
          (* tracker->insert_func) (offset, model, position + n_items,
                                    section->action_namespace, FALSE, tracker->user_data);
          *change_point = g_slist_prepend (*change_point, NULL);
        }
    }
}

static void
gtk_menu_tracker_model_changed (GMenuModel *model,
                                gint        position,
                                gint        removed,
                                gint        added,
                                gpointer    user_data)
{
  GtkMenuTracker *tracker = user_data;
  GtkMenuTrackerSection *section;
  GSList **change_point;
  gint offset = 0;
  gint i;

  /* First find which section the changed model corresponds to, and the
   * position of that section within the overall menu.
   */
  section = gtk_menu_tracker_section_find_model (tracker->toplevel, model, &offset);

  /* Next, seek through that section to the change point.  This gives us
   * the correct GSList** to make the change to and also finds the final
   * offset at which we will make the changes (by measuring the number
   * of items within each item of the section before the change point).
   */
  change_point = &section->items;
  for (i = 0; i < position; i++)
    {
      offset += gtk_menu_tracker_section_measure ((*change_point)->data);
      change_point = &(*change_point)->next;
    }

  /* We remove items in order and add items in reverse order.  This
   * means that the offset used for all inserts and removes caused by a
   * single change will be the same.
   *
   * This also has a performance advantage: GtkMenuShell stores the
   * menu items in a linked list.  In the case where we are creating a
   * menu for the first time, adding the items in reverse order means
   * that we only ever insert at index zero, prepending the list.  This
   * means that we can populate in O(n) time instead of O(n^2) that we
   * would do by appending.
   */
  gtk_menu_tracker_remove_items (tracker, change_point, offset, removed);
  gtk_menu_tracker_add_items (tracker, section, change_point, offset, model, position, added);

  /* The offsets for insertion/removal of separators will be all over
   * the place, however...
   */
  gtk_menu_tracker_section_sync_separators (tracker->toplevel, tracker, 0, FALSE, NULL, 0);
}

static void
gtk_menu_tracker_section_free (GtkMenuTrackerSection *section)
{
  if (section == NULL)
    return;

  g_signal_handler_disconnect (section->model, section->handler);
  g_slist_free_full (section->items, (GDestroyNotify) gtk_menu_tracker_section_free);
  g_free (section->action_namespace);
  g_object_unref (section->model);
  g_slice_free (GtkMenuTrackerSection, section);
}

static GtkMenuTrackerSection *
gtk_menu_tracker_section_new (GtkMenuTracker *tracker,
                              GMenuModel     *model,
                              gboolean        with_separators,
                              gint            offset,
                              const gchar    *action_namespace)
{
  GtkMenuTrackerSection *section;

  section = g_slice_new0 (GtkMenuTrackerSection);
  section->model = g_object_ref (model);
  section->with_separators = with_separators;
  section->action_namespace = g_strdup (action_namespace);

  gtk_menu_tracker_add_items (tracker, section, &section->items, offset, model, 0, g_menu_model_get_n_items (model));
  section->handler = g_signal_connect (model, "items-changed", G_CALLBACK (gtk_menu_tracker_model_changed), tracker);

  return section;
}

/*< private >
 * gtk_menu_tracker_new:
 * @model: the model to flatten
 * @with_separators: if the toplevel should have separators (ie: TRUE
 *   for menus, FALSE for menubars)
 * @action_namespace: the passed-in action namespace
 * @insert_func: insert callback
 * @remove_func: remove callback
 * @user_data user data for callbacks
 *
 * Creates a GtkMenuTracker for @model, holding a ref on @model for as
 * long as the tracker is alive.
 *
 * This flattens out the model, merging sections and inserting
 * separators where appropriate.  It monitors for changes and performs
 * updates on the fly.  It also handles action_namespace for subsections
 * (but you will need to handle it yourself for submenus).
 *
 * When the tracker is first created, @insert_func will be called many
 * times to populate the menu with the initial contents of @model
 * (unless it is empty), before gtk_menu_tracker_new() returns.  For
 * this reason, the menu that is using the tracker ought to be empty
 * when it creates the tracker.
 *
 * Future changes to @model will result in more calls to @insert_func
 * and @remove_func.
 *
 * The position argument to both functions is the linear 0-based
 * position in the menu at which the item in question should be inserted
 * or removed.
 *
 * For @insert_func, @model and @item_index are used to get the
 * information about the menu item to insert.  @action_namespace is the
 * action namespace that actions referred to from that item should place
 * themselves in.  Note that if the item is a submenu and the
 * "action-namespace" attribute is defined on the item, it will _not_ be
 * applied to the @action_namespace argument as it is meant for the
 * items inside of the submenu, not the submenu item itself.
 *
 * @is_separator is set to %TRUE in case the item being added is a
 * separator.  @model and @item_index will still be meaningfully set in
 * this case -- to the section menu item corresponding to the separator.
 * This is useful if the section specifies a label, for example.  If
 * there is an "action-namespace" attribute on this menu item then it
 * should be ignored by the consumer because #GtkMenuTracker has already
 * handled it.
 *
 * When using #GtkMenuTracker there is no need to hold onto @model or
 * monitor it for changes.  The model will be unreffed when
 * gtk_menu_tracker_free() is called.
 */
GtkMenuTracker *
gtk_menu_tracker_new (GMenuModel               *model,
                      gboolean                  with_separators,
                      const gchar              *action_namespace,
                      GtkMenuTrackerInsertFunc  insert_func,
                      GtkMenuTrackerRemoveFunc  remove_func,
                      gpointer                  user_data)
{
  GtkMenuTracker *tracker;

  tracker = g_slice_new (GtkMenuTracker);
  tracker->insert_func = insert_func;
  tracker->remove_func = remove_func;
  tracker->user_data = user_data;

  tracker->toplevel = gtk_menu_tracker_section_new (tracker, model, with_separators, 0, action_namespace);
  gtk_menu_tracker_section_sync_separators (tracker->toplevel, tracker, 0, FALSE, NULL, 0);

  return tracker;
}

/*< private >
 * gtk_menu_tracker_free:
 * @tracker: a #GtkMenuTracker
 *
 * Frees the tracker, ...
 */
void
gtk_menu_tracker_free (GtkMenuTracker *tracker)
{
  gtk_menu_tracker_section_free (tracker->toplevel);
  g_slice_free (GtkMenuTracker, tracker);
}
