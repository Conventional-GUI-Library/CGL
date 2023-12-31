/* GTK - The GIMP Toolkit
 * Copyright © 2013 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_POPOVER_H__
#define __GTK_POPOVER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_POPOVER           (gtk_popover_get_type ())
#define GTK_POPOVER(o)             (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_POPOVER, GtkPopover))
#define GTK_POPOVER_CLASS(c)       (G_TYPE_CHECK_CLASS_CAST ((c), GTK_TYPE_POPOVER, GtkPopoverClass))
#define GTK_IS_POPOVER(o)          (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_POPOVER))
#define GTK_IS_POPOVER_CLASS(o)    (G_TYPE_CHECK_CLASS_TYPE ((o), GTK_TYPE_POPOVER))
#define GTK_POPOVER_GET_CLASS(o)   (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_POPOVER, GtkPopoverClass))

typedef struct _GtkPopover GtkPopover;
typedef struct _GtkPopoverClass GtkPopoverClass;

struct _GtkPopover
{
  GtkBin parent_instance;

  /*< private >*/
  gpointer priv;
};

struct _GtkPopoverClass
{
  GtkBinClass parent_class;
};

GDK_AVAILABLE_IN_3_12
GType           gtk_popover_get_type        (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_12
GtkWidget *     gtk_popover_new             (GtkWidget             *relative_to);

GDK_AVAILABLE_IN_3_12
void            gtk_popover_set_relative_to (GtkPopover            *popover,
                                             GtkWidget             *relative_to);
GDK_AVAILABLE_IN_3_12
GtkWidget *     gtk_popover_get_relative_to (GtkPopover            *popover);

GDK_AVAILABLE_IN_3_12
void            gtk_popover_set_pointing_to (GtkPopover            *popover,
                                             cairo_rectangle_int_t *rect);
GDK_AVAILABLE_IN_3_12
gboolean        gtk_popover_get_pointing_to (GtkPopover            *popover,
                                             cairo_rectangle_int_t *rect);
GDK_AVAILABLE_IN_3_12
void            gtk_popover_set_position    (GtkPopover            *popover,
                                             GtkPositionType        position);
GDK_AVAILABLE_IN_3_12
GtkPositionType gtk_popover_get_position    (GtkPopover            *popover);

GDK_AVAILABLE_IN_3_12
void            gtk_popover_set_modal       (GtkPopover            *popover,
                                             gboolean               modal);
GDK_AVAILABLE_IN_3_12
gboolean        gtk_popover_get_modal       (GtkPopover            *popover);

G_END_DECLS

#endif /* __GTK_POPOVER_H__ */
