/* GAIL - The GNOME Accessibility Implementation Library
 *
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

#ifndef __GTK_CELL_ACCESSIBLE_PARENT_H__
#define __GTK_CELL_ACCESSIBLE_PARENT_H__

#include <atk/atk.h>
#include "gtkcellaccessible.h"

G_BEGIN_DECLS

/*
 * The GtkCellAccessibleParent interface should be supported by any object
 * which contains children which are flyweights, i.e. do not have corresponding
 * widgets and the children need help from their parent to provide
 * functionality. One example is GtkTreeViewAccessible where the children
 * GtkCellAccessible need help from the GtkTreeViewAccessible in order to
 * implement atk_component_get_extents().
 */

#define GTK_TYPE_CELL_ACCESSIBLE_PARENT            (_gtk_cell_accessible_parent_get_type ())
#define GTK_IS_CELL_ACCESSIBLE_PARENT(obj)         G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_ACCESSIBLE_PARENT)
#define GTK_CELL_ACCESSIBLE_PARENT(obj)            G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_ACCESSIBLE_PARENT, GtkCellAccessibleParent)
#define GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GTK_TYPE_CELL_ACCESSIBLE_PARENT, GtkCellAccessibleParentIface))

typedef struct _GtkCellAccessibleParent GtkCellAccessibleParent;
typedef struct _GtkCellAccessibleParentIface GtkCellAccessibleParentIface;

struct _GtkCellAccessibleParentIface
{
  GTypeInterface parent;
  void     ( *get_cell_extents) (GtkCellAccessibleParent *parent,
                                 GtkCellAccessible       *cell,
                                 gint                    *x,
                                 gint                    *y,
                                 gint                    *width,
                                 gint                    *height,
                                 AtkCoordType             coord_type);
  void     ( *get_cell_area)    (GtkCellAccessibleParent *parent,
                                 GtkCellAccessible       *cell,
                                 GdkRectangle            *cell_rect);
  gboolean ( *grab_focus)       (GtkCellAccessibleParent *parent,
                                 GtkCellAccessible       *cell);
};

GType    _gtk_cell_accessible_parent_get_type         (void);

void     _gtk_cell_accessible_parent_get_cell_extents (GtkCellAccessibleParent *parent,
                                                       GtkCellAccessible       *cell,
                                                       gint                    *x,
                                                       gint                    *y,
                                                       gint                    *width,
                                                       gint                    *height,
                                                       AtkCoordType             coord_type);
void     _gtk_cell_accessible_parent_get_cell_area    (GtkCellAccessibleParent *parent,
                                                       GtkCellAccessible       *cell,
                                                       GdkRectangle            *cell_rect);
gboolean _gtk_cell_accessible_parent_grab_focus       (GtkCellAccessibleParent *parent,
                                                       GtkCellAccessible       *cell);

G_END_DECLS

#endif /* __GTK_CELL_ACCESSIBLE_PARENT_H__ */
