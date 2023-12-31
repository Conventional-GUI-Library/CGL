/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_PROGRESS_BAR_H__
#define __GTK_PROGRESS_BAR_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_PROGRESS_BAR            (gtk_progress_bar_get_type ())
#define GTK_PROGRESS_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PROGRESS_BAR, GtkProgressBar))
#define GTK_PROGRESS_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PROGRESS_BAR, GtkProgressBarClass))
#define GTK_IS_PROGRESS_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PROGRESS_BAR))
#define GTK_IS_PROGRESS_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PROGRESS_BAR))
#define GTK_PROGRESS_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PROGRESS_BAR, GtkProgressBarClass))


typedef struct _GtkProgressBar              GtkProgressBar;
typedef struct _GtkProgressBarPrivate       GtkProgressBarPrivate;
typedef struct _GtkProgressBarClass         GtkProgressBarClass;

struct _GtkProgressBar
{
  GtkWidget parent;

  /*< private >*/
  GtkProgressBarPrivate *priv;
};

struct _GtkProgressBarClass
{
  GtkWidgetClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType      gtk_progress_bar_get_type             (void) G_GNUC_CONST;
GtkWidget* gtk_progress_bar_new                  (void);

void       gtk_progress_bar_pulse                (GtkProgressBar *pbar);
void       gtk_progress_bar_set_text             (GtkProgressBar *pbar,
                                                  const gchar    *text);
void       gtk_progress_bar_set_fraction         (GtkProgressBar *pbar,
                                                  gdouble         fraction);

void       gtk_progress_bar_set_pulse_step       (GtkProgressBar *pbar,
                                                  gdouble         fraction);
void       gtk_progress_bar_set_inverted         (GtkProgressBar *pbar,
                                                  gboolean        inverted);

const gchar *      gtk_progress_bar_get_text       (GtkProgressBar *pbar);
gdouble            gtk_progress_bar_get_fraction   (GtkProgressBar *pbar);
gdouble            gtk_progress_bar_get_pulse_step (GtkProgressBar *pbar);

gboolean           gtk_progress_bar_get_inverted    (GtkProgressBar *pbar);
void               gtk_progress_bar_set_ellipsize (GtkProgressBar     *pbar,
                                                   PangoEllipsizeMode  mode);
PangoEllipsizeMode gtk_progress_bar_get_ellipsize (GtkProgressBar     *pbar);

void               gtk_progress_bar_set_show_text (GtkProgressBar     *pbar,
                                                   gboolean            show_text);
gboolean           gtk_progress_bar_get_show_text (GtkProgressBar     *pbar);

G_END_DECLS

#endif /* __GTK_PROGRESS_BAR_H__ */
