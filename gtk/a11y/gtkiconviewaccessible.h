/* gtkiconview.c
 * Copyright (C) 2002, 2004  Anders Carlsson <andersca@gnu.org>
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

#ifndef __GTK_ICON_VIEW_ACCESSIBLE_H__
#define __GTK_ICON_VIEW_ACCESSIBLE_H__

#include "gtkcontaineraccessible.h"
#include "gtk/gtkiconview.h"

G_BEGIN_DECLS

#define GTK_TYPE_ICON_VIEW_ACCESSIBLE                  (_gtk_icon_view_accessible_get_type ())
#define GTK_ICON_VIEW_ACCESSIBLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_ICON_VIEW_ACCESSIBLE, GtkIconViewAccessible))
#define GTK_ICON_VIEW_ACCESSIBLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_ICON_VIEW_ACCESSIBLE, GtkIconViewAccessibleClass))
#define GTK_IS_ICON_VIEW_ACCESSIBLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_ICON_VIEW_ACCESSIBLE))
#define GTK_IS_ICON_VIEW_ACCESSIBLE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ICON_VIEW_ACCESSIBLE))
#define GTK_ICON_VIEW_ACCESSIBLE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_ICON_VIEW_ACCESSIBLE, GtkIconViewAccessibleClass))

typedef struct _GtkIconViewAccessible      GtkIconViewAccessible;
typedef struct _GtkIconViewAccessibleClass GtkIconViewAccessibleClass;

struct _GtkIconViewAccessible
{
  GtkContainerAccessible parent;

  GList *items;
  GtkTreeModel *model;
};

struct _GtkIconViewAccessibleClass
{
  GtkContainerAccessibleClass parent_class;
};

GType           _gtk_icon_view_accessible_get_type            (void);

void            _gtk_icon_view_accessible_adjustment_changed  (GtkIconView            *icon_view);

G_END_DECLS

#endif /* __GTK_ICON_VIEW_ACCESSIBLE_H__ */
