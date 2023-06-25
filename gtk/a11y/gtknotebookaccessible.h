/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_NOTEBOOK_ACCESSIBLE_H__
#define __GTK_NOTEBOOK_ACCESSIBLE_H__

#include "gtkcontaineraccessible.h"

G_BEGIN_DECLS

#define GTK_TYPE_NOTEBOOK_ACCESSIBLE                   (_gtk_notebook_accessible_get_type ())
#define GTK_NOTEBOOK_ACCESSIBLE(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_NOTEBOOK_ACCESSIBLE, GtkNotebookAccessible))
#define GTK_NOTEBOOK_ACCESSIBLE_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_NOTEBOOK_ACCESSIBLE, GtkNotebookAccessibleClass))
#define GTK_IS_NOTEBOOK_ACCESSIBLE(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_NOTEBOOK_ACCESSIBLE))
#define GTK_IS_NOTEBOOK_ACCESSIBLE_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_NOTEBOOK_ACCESSIBLE))
#define GTK_NOTEBOOK_ACCESSIBLE_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_NOTEBOOK_ACCESSIBLE, GtkNotebookAccessibleClass))

typedef struct _GtkNotebookAccessible      GtkNotebookAccessible;
typedef struct _GtkNotebookAccessibleClass GtkNotebookAccessibleClass;

struct _GtkNotebookAccessible
{
  GtkContainerAccessible parent;

  /*
   * page_cache maintains a list of pre-ref'd Notebook Pages.
   * This cache is queried by gtk_notebook_accessible_ref_child().
   * If the page is found in the list then a new page does not
   * need to be created
   */
  GHashTable * pages;
  gint         selected_page;
  gint         focus_tab_page;
  guint        idle_focus_id;
};

struct _GtkNotebookAccessibleClass
{
  GtkContainerAccessibleClass parent_class;
};

GType _gtk_notebook_accessible_get_type (void);

G_END_DECLS

#endif /* __GTK_NOTEBOOK_ACCESSIBLE_H__ */
