/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_STYLE_PROPERTIES_PRIVATE_H__
#define __GTK_STYLE_PROPERTIES_PRIVATE_H__

#include "gtkstyleproperties.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstylecontextprivate.h"

G_BEGIN_DECLS

void           _gtk_style_properties_get                      (GtkStyleProperties      *props,
							       GtkStateFlags            state,
							       GtkStylePropertyContext *context,
							       ...);
void           _gtk_style_properties_get_valist               (GtkStyleProperties      *props,
							       GtkStateFlags            state,
							       GtkStylePropertyContext *context,
							       va_list                  args);

const GValue * _gtk_style_properties_resolve_property         (GtkStyleProperties      *props,
							       const gchar             *prop_name,
							       GtkStateFlags            state,
							       const GtkStyleProperty **property,
							       GtkStylePropertyContext *context,
							       GValue                  *value);

void           _gtk_style_properties_set_property_by_property (GtkStyleProperties      *props,
                                                               const GtkStyleProperty  *property,
                                                               GtkStateFlags            state,
                                                               const GValue            *value);

G_END_DECLS

#endif /* __GTK_STYLE_PROPERTIES_PRIVATE_H__ */
