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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef __GTK_LIST_BOX_H__
#define __GTK_LIST_BOX_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkbin.h>

G_BEGIN_DECLS


#define GTK_TYPE_LIST_BOX (gtk_list_box_get_type ())
#define GTK_LIST_BOX(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LIST_BOX, GtkListBox))
#define GTK_LIST_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LIST_BOX, GtkListBoxClass))
#define GTK_IS_LIST_BOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LIST_BOX))
#define GTK_IS_LIST_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LIST_BOX))
#define GTK_LIST_BOX_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_LIST_BOX, GtkListBoxClass))

typedef struct _GtkListBox        GtkListBox;
typedef struct _GtkListBoxClass   GtkListBoxClass;

typedef struct _GtkListBoxRow        GtkListBoxRow;
typedef struct _GtkListBoxRowClass   GtkListBoxRowClass;

struct _GtkListBox
{
  GtkContainer parent_instance;
};

/**
 * GtkListBoxClass:
 * @parent_class: The parent class.
 * @row_selected: Signal emitted when a new row is selected.
 * @row_activated: Signal emitted when a row has been activated by the user.
 * @activate_cursor_row: 
 * @toggle_cursor_row: 
 * @move_cursor: 
 */
struct _GtkListBoxClass
{
  GtkContainerClass parent_class;

  /*< public >*/

  void (*row_selected)        (GtkListBox      *list_box,
                               GtkListBoxRow   *row);
  void (*row_activated)       (GtkListBox      *list_box,
                               GtkListBoxRow   *row);
  void (*activate_cursor_row) (GtkListBox      *list_box);
  void (*toggle_cursor_row)   (GtkListBox      *list_box);
  void (*move_cursor)         (GtkListBox      *list_box,
                               GtkMovementStep  step,
                               gint             count);

  /*< private >*/

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
};

#define GTK_TYPE_LIST_BOX_ROW            (gtk_list_box_row_get_type ())
#define GTK_LIST_BOX_ROW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_LIST_BOX_ROW, GtkListBoxRow))
#define GTK_LIST_BOX_ROW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_LIST_BOX_ROW, GtkListBoxRowClass))
#define GTK_IS_LIST_BOX_ROW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_LIST_BOX_ROW))
#define GTK_IS_LIST_BOX_ROW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LIST_BOX_ROW))
#define GTK_LIST_BOX_ROW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_LIST_BOX_ROW, GtkListBoxRowClass))

struct _GtkListBoxRow
{
  GtkBin parent_instance;
};

/**
 * GtkListBoxRowClass:
 * @parent_class: The parent class.
 * @activate: 
 */
struct _GtkListBoxRowClass
{
  GtkBinClass parent_class;

  /*< public >*/

  void (* activate) (GtkListBoxRow *row);

  /*< private >*/

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
};

/**
 * GtkListBoxFilterFunc:
 * @row: the row that may be filtered
 * @user_data: (closure): user data
 *
 * Will be called whenever the row changes or is added and lets you control
 * if the row should be visible or not.
 *
 * Returns: %TRUE if the row should be visible, %FALSE otherwise
 *
 * Since: 3.10
 */
typedef gboolean (*GtkListBoxFilterFunc) (GtkListBoxRow *row,
                                          gpointer       user_data);

/**
 * GtkListBoxSortFunc:
 * @row1: the first row
 * @row2: the second row
 * @user_data: (closure): user data
 *
 * Compare two rows to determine which should be first.
 *
 * Returns: < 0 if @row1 should be before @row2, 0 if they are
 *     equal and > 0 otherwise
 *
 * Since: 3.10
 */
typedef gint (*GtkListBoxSortFunc) (GtkListBoxRow *row1,
                                    GtkListBoxRow *row2,
                                    gpointer       user_data);

/**
 * GtkListBoxUpdateHeaderFunc:
 * @row: the row to update
 * @before: the row before @row, or %NULL if it is first
 * @user_data: (closure): user data
 *
 * Whenever @row changes or which row is before @row changes this
 * is called, which lets you update the header on @row. You may
 * remove or set a new one via gtk_list_box_row_set_header() or
 * just change the state of the current header widget.
 *
 * Since: 3.10
 */
typedef void (*GtkListBoxUpdateHeaderFunc) (GtkListBoxRow *row,
                                            GtkListBoxRow *before,
                                            gpointer       user_data);

GType      gtk_list_box_row_get_type      (void) G_GNUC_CONST;
GtkWidget* gtk_list_box_row_new           (void);
GtkWidget* gtk_list_box_row_get_header (GtkListBoxRow *row);
void       gtk_list_box_row_set_header (GtkListBoxRow *row,
                                           GtkWidget     *header);
void       gtk_list_box_row_changed       (GtkListBoxRow *row);


GType          gtk_list_box_get_type                     (void) G_GNUC_CONST;
GtkListBoxRow* gtk_list_box_get_selected_row             (GtkListBox                    *list_box);
GtkListBoxRow* gtk_list_box_get_row_at_index             (GtkListBox                    *list_box,
                                                          gint                           index);
GtkListBoxRow* gtk_list_box_get_row_at_y                 (GtkListBox                    *list_box,
                                                          gint                           y);
void           gtk_list_box_select_row                   (GtkListBox                    *list_box,
                                                          GtkListBoxRow                 *row);
void           gtk_list_box_set_adjustment               (GtkListBox                    *list_box,
                                                          GtkAdjustment                 *adjustment);
GtkAdjustment *gtk_list_box_get_adjustment               (GtkListBox                    *list_box);
void           gtk_list_box_set_selection_mode           (GtkListBox                    *list_box,
                                                          GtkSelectionMode               mode);
GtkSelectionMode gtk_list_box_get_selection_mode         (GtkListBox                    *list_box);
void           gtk_list_box_set_filter_func              (GtkListBox                    *list_box,
                                                          GtkListBoxFilterFunc           filter_func,
                                                          gpointer                       user_data,
                                                          GDestroyNotify                 destroy);
void           gtk_list_box_set_header_func           (GtkListBox                    *list_box,
                                                          GtkListBoxUpdateHeaderFunc  update_header,
                                                          gpointer                       user_data,
                                                          GDestroyNotify                 destroy);
void           gtk_list_box_invalidate_filter                     (GtkListBox                    *list_box);
void           gtk_list_box_invalidate_sort                       (GtkListBox                    *list_box);
void           gtk_list_box_invalidate_headers                   (GtkListBox                    *list_box);
void           gtk_list_box_set_sort_func                (GtkListBox                    *list_box,
                                                          GtkListBoxSortFunc             sort_func,
                                                          gpointer                       user_data,
                                                          GDestroyNotify                 destroy);
void           gtk_list_box_set_activate_on_single_click (GtkListBox                    *list_box,
                                                          gboolean                       single);
void           gtk_list_box_drag_unhighlight_row         (GtkListBox                    *list_box);
void           gtk_list_box_drag_highlight_row           (GtkListBox                    *list_box,
                                                          GtkListBoxRow                 *row);
GtkWidget*     gtk_list_box_new                          (void);
void           gtk_list_box_set_placeholder              (GtkListBox                    *list_box, GtkWidget                     *placeholder);
gboolean       gtk_list_box_get_activate_on_single_click (GtkListBox                    *list_box);
gint       gtk_list_box_row_get_index     (GtkListBoxRow *row);
void           gtk_list_box_prepend                      (GtkListBox                    *list_box,      GtkWidget                     *child);                            
void           gtk_list_box_insert                       (GtkListBox                    *list_box,
                                                          GtkWidget                     *child,
                                                          gint                           position);                                                                         

G_END_DECLS

#endif
