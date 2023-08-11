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

#ifndef __GTK_CSD_TITLE_BAR_H__
#define __GTK_CSD_TITLE_BAR_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

#define GTK_TYPE_CSD_TITLE_BAR            (gtk_csd_title_bar_get_type ())
#define GTK_CSD_TITLE_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CSD_TITLE_BAR, GtkCSDTitleBar))
#define GTK_CSD_TITLE_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CSD_TITLE_BAR, GtkCSDTitleBarClass))
#define GTK_IS_CSD_TITLE_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CSD_TITLE_BAR))
#define GTK_IS_CSD_TITLE_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CSD_TITLE_BAR))
#define GTK_CSD_TITLE_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSD_TITLE_BAR, GtkCSDTitleBarClass))

typedef struct _GtkCSDTitleBar              GtkCSDTitleBar;
typedef struct _GtkCSDTitleBarPrivate       GtkCSDTitleBarPrivate;
typedef struct _GtkCSDTitleBarClass         GtkCSDTitleBarClass;

struct _GtkCSDTitleBar
{
  GtkContainer container;

  /*< private >*/
  GtkCSDTitleBarPrivate *priv;
};

struct _GtkCSDTitleBarClass
{
  GtkContainerClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType        gtk_csd_title_bar_get_type          (void) G_GNUC_CONST;

GtkWidget   *gtk_csd_title_bar_new               (void);

void         gtk_csd_title_bar_set_title         (GtkCSDTitleBar *bar,
                                               const gchar  *title);

const gchar *gtk_csd_title_bar_get_title         (GtkCSDTitleBar *bar);

void         gtk_csd_title_bar_set_custom_title  (GtkCSDTitleBar *bar,
                                               GtkWidget    *title_widget);

GtkWidget   *gtk_csd_title_bar_get_custom_title  (GtkCSDTitleBar *bar);

void         gtk_csd_title_bar_pack_start        (GtkCSDTitleBar *bar,
                                               GtkWidget    *child);

void         gtk_csd_title_bar_pack_end          (GtkCSDTitleBar *bar,
                                               GtkWidget    *child);
void         gtk_csd_title_bar_set_subtitle      (GtkCSDTitleBar *bar,
                                               const gchar  *subtitle);
const gchar *gtk_csd_title_bar_get_subtitle      (GtkCSDTitleBar *bar);
void
gtk_csd_title_bar_set_show_close_button (GtkCSDTitleBar *bar,
                                      gboolean      setting);

gboolean
gtk_csd_title_bar_get_show_close_button (GtkCSDTitleBar *bar);

GDK_AVAILABLE_IN_3_12
void         gtk_csd_title_bar_set_has_subtitle (GtkCSDTitleBar *bar,
                                              gboolean      setting);
GDK_AVAILABLE_IN_3_12
gboolean     gtk_csd_title_bar_get_has_subtitle (GtkCSDTitleBar *bar);

GDK_AVAILABLE_IN_3_12
void         gtk_csd_title_bar_set_decoration_layout (GtkCSDTitleBar *bar,
                                                   const gchar  *layout);
GDK_AVAILABLE_IN_3_12
const gchar *gtk_csd_title_bar_get_decoration_layout (GtkCSDTitleBar *bar);

G_END_DECLS

#endif /* __GTK_CSD_TITLE_BAR_H__ */
