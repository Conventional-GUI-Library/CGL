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

#include <string.h>
#include <gtk/gtk.h>
#include "gtkframeaccessible.h"


G_DEFINE_TYPE (GtkFrameAccessible, _gtk_frame_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE)

static void
gtk_frame_accessible_initialize (AtkObject *accessible,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (_gtk_frame_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_PANEL;
}

static const gchar *
gtk_frame_accessible_get_name (AtkObject *obj)
{
  const gchar *name;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
      return NULL;

  name = ATK_OBJECT_CLASS (_gtk_frame_accessible_parent_class)->get_name (obj);
  if (name != NULL)
    return name;

  return gtk_frame_get_label (GTK_FRAME (widget));
}

static void
_gtk_frame_accessible_class_init (GtkFrameAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gtk_frame_accessible_initialize;
  class->get_name = gtk_frame_accessible_get_name;
}

static void
_gtk_frame_accessible_init (GtkFrameAccessible *frame)
{
}
