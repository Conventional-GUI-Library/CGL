/* gtkgladecatalog.c
 *
 * Copyright (C) 2013 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gtkpathbar.h"

#ifdef G_OS_UNIX
#  include "gtkprinteroptionwidget.h"
#endif

/* Some forward declarations of internal types */
GType _gtk_scale_button_scale_get_type (void);
GType _shortcuts_pane_model_filter_get_type (void);

/* This function is referred to in gtk/glade/gtk-private-widgets.xml
 * and is used to ensure the private types for use in Glade while
 * editing UI files that define GTK+'s various composite widget classes.
 */
void
gtk_glade_catalog_init (const gchar *catalog_name)
{
  g_type_ensure (GTK_TYPE_PATH_BAR);
  g_type_ensure (_gtk_scale_button_scale_get_type ());
  g_type_ensure (_shortcuts_pane_model_filter_get_type ());

#ifdef G_OS_UNIX
  g_type_ensure (GTK_TYPE_PRINTER_OPTION_WIDGET);
#endif
}
