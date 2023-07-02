/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2010 Javier Jardón
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
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_BUTTON_PRIVATE_H__
#define __GTK_BUTTON_PRIVATE_H__

#include "gsimpleactionobserver.h"
#include "gtkaction.h"

G_BEGIN_DECLS


struct _GtkButtonPrivate
{
  GtkAction             *action;
  GtkWidget             *image;

  gchar                 *action_name;
  GVariant              *action_target;
  GSimpleActionObserver *action_observer;

  GdkDevice             *grab_keyboard;
  GdkWindow             *event_window;

  gchar                 *label_text;

  gfloat                 xalign;
  gfloat                 yalign;

  guint                  activate_timeout;
  guint32                grab_time;

  GtkPositionType        image_position;

  guint          align_set             : 1;
  guint          button_down           : 1;
  guint          constructed           : 1;
  guint          depressed             : 1;
  guint          depress_on_activate   : 1;
  guint          focus_on_click        : 1;
  guint          image_is_stock        : 1;
  guint          in_button             : 1;
  guint          relief                : 2;
  guint          use_action_appearance : 1;
  guint          use_stock             : 1;
  guint          use_underline         : 1;
};

void _gtk_button_set_depressed             (GtkButton          *button,
                                            gboolean            depressed);
void _gtk_button_paint                     (GtkButton          *button,
                                            cairo_t            *cr,
                                            int                 width,
                                            int                 height,
                                            GtkStateFlags       state);


G_END_DECLS

#endif /* __GTK_BUTTON_PRIVATE_H__ */
