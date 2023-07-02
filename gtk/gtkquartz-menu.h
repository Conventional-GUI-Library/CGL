/*
 * Copyright © 2011 William Hua
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
 * Author: William Hua <william@attente.ca>
 */

#ifndef __GTK_QUARTZ_MENU_H__
#define __GTK_QUARTZ_MENU_H__

#include "gactionobservable.h"

void gtk_quartz_set_main_menu (GMenuModel        *model,
                               GActionObservable *observable);

#endif /* __GTK_QUARTZ_MENU_H__ */
