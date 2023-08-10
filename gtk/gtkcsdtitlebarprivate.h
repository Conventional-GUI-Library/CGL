/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __GTK_CSD_TITLE_BAR_PRIVATE_H__
#define __GTK_CSD_TITLE_BAR_PRIVATE_H__

GtkWidget * _gtk_csd_title_bar_create_title_box (const char *title,
                                              const char *subtitle,
                                              GtkWidget **ret_title_label,
                                              GtkWidget **ret_subtitle_label);

gboolean     _gtk_csd_title_bar_get_shows_app_menu    (GtkCSDTitleBar *bar);
void         _gtk_csd_title_bar_update_window_buttons (GtkCSDTitleBar *bar);
gboolean     _gtk_csd_title_bar_update_window_icon    (GtkCSDTitleBar *bar,
                                                    GtkWindow    *window);

#endif /* __GTK_CSD_TITLE_BAR_PRIVATE_H__ */
