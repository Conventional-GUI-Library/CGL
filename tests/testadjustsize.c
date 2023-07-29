/* testadjustsize.c
 * Copyright (C) 2010 Havoc Pennington
 *
 * Author:
 *      Havoc Pennington <hp@pobox.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

static GtkWidget *test_window;

enum {
  TEST_WIDGET_LABEL,
  TEST_WIDGET_VERTICAL_LABEL,
  TEST_WIDGET_WRAP_LABEL,
  TEST_WIDGET_ALIGNMENT,
  TEST_WIDGET_IMAGE,
  TEST_WIDGET_BUTTON,
  TEST_WIDGET_LAST
};

static GtkWidget *test_widgets[TEST_WIDGET_LAST];

static GtkWidget*
create_image (void)
{
  return gtk_image_new_from_stock (GTK_STOCK_OPEN,
                                   GTK_ICON_SIZE_BUTTON);
}

static GtkWidget*
create_label (gboolean vertical,
              gboolean wrap)
{
  GtkWidget *widget;

  widget = gtk_label_new ("This is a label, label label label");

  if (vertical)
    gtk_label_set_angle (GTK_LABEL (widget), 90);

  if (wrap)
    gtk_label_set_line_wrap (GTK_LABEL (widget), TRUE);

  return widget;
}

static GtkWidget*
create_button (void)
{
  return gtk_button_new_with_label ("BUTTON!");
}

static gboolean
on_draw_alignment (GtkWidget      *widget,
                   cairo_t        *cr,
                   void           *data)
{
  cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
  cairo_paint (cr);

  return FALSE;
}

static GtkWidget*
create_alignment (void)
{
  GtkWidget *alignment;

  alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);

  /* make the alignment visible */
  gtk_widget_set_redraw_on_allocate (alignment, TRUE);
  g_signal_connect (G_OBJECT (alignment),
                    "draw",
                    G_CALLBACK (on_draw_alignment),
                    NULL);

  return alignment;
}

static void
open_test_window (void)
{
  GtkWidget *table;
  int i;

  test_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (test_window), "Tests");

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  gtk_window_set_resizable (GTK_WINDOW (test_window), FALSE);

  test_widgets[TEST_WIDGET_IMAGE] = create_image ();
  test_widgets[TEST_WIDGET_LABEL] = create_label (FALSE, FALSE);
  test_widgets[TEST_WIDGET_VERTICAL_LABEL] = create_label (TRUE, FALSE);
  test_widgets[TEST_WIDGET_WRAP_LABEL] = create_label (FALSE, TRUE);
  test_widgets[TEST_WIDGET_BUTTON] = create_button ();
  test_widgets[TEST_WIDGET_ALIGNMENT] = create_alignment ();

  table = gtk_table_new (2, 3, FALSE);

  gtk_container_add (GTK_CONTAINER (test_window), table);

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      gtk_table_attach (GTK_TABLE (table),
                        test_widgets[i],
                        i % 3,
                        i % 3 + 1,
                        i / 3,
                        i / 3 + 1,
                        0, 0, 0, 0);
    }

  gtk_widget_show_all (test_window);
}

static void
on_toggle_border_widths (GtkToggleButton *button,
                         void            *data)
{
  gboolean has_border;
  int i;

  has_border = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      if (GTK_IS_CONTAINER (test_widgets[i]))
        {
          gtk_container_set_border_width (GTK_CONTAINER (test_widgets[i]),
                                          has_border ? 50 : 0);
        }
    }
}

static void
on_set_small_size_requests (GtkToggleButton *button,
                            void            *data)
{
  gboolean has_small_size_requests;
  int i;

  has_small_size_requests = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      gtk_widget_set_size_request (test_widgets[i],
                                   has_small_size_requests ? 5 : -1,
                                   has_small_size_requests ? 5 : -1);
    }
}

static void
on_set_large_size_requests (GtkToggleButton *button,
                            void            *data)
{
  gboolean has_large_size_requests;
  int i;

  has_large_size_requests = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  for (i = 0; i < TEST_WIDGET_LAST; ++i)
    {
      gtk_widget_set_size_request (test_widgets[i],
                                   has_large_size_requests ? 200 : -1,
                                   has_large_size_requests ? 200 : -1);
    }
}

static void
open_control_window (void)
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *toggle;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Controls");

  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  toggle =
    gtk_toggle_button_new_with_label ("Containers have borders");
  g_signal_connect (G_OBJECT (toggle),
                    "toggled", G_CALLBACK (on_toggle_border_widths),
                    NULL);
  gtk_container_add (GTK_CONTAINER (box), toggle);

  toggle =
    gtk_toggle_button_new_with_label ("Set small size requests");
  g_signal_connect (G_OBJECT (toggle),
                    "toggled", G_CALLBACK (on_set_small_size_requests),
                    NULL);
  gtk_container_add (GTK_CONTAINER (box), toggle);

  toggle =
    gtk_toggle_button_new_with_label ("Set large size requests");
  g_signal_connect (G_OBJECT (toggle),
                    "toggled", G_CALLBACK (on_set_large_size_requests),
                    NULL);
  gtk_container_add (GTK_CONTAINER (box), toggle);


  gtk_widget_show_all (window);
}

#define TEST_WIDGET(outer) (gtk_bin_get_child (GTK_BIN (gtk_bin_get_child (GTK_BIN(outer)))))

static GtkWidget*
create_widget_visible_border (const char *text)
{
  GtkWidget *outer_box;
  GtkWidget *inner_box;
  GtkWidget *test_widget;
  GtkWidget *label;
  GdkRGBA color;

  outer_box = gtk_event_box_new ();
  gdk_rgba_parse (&color, "black");
  gtk_widget_override_background_color (outer_box, 0, &color);

  inner_box = gtk_event_box_new ();
  gtk_container_set_border_width (GTK_CONTAINER (inner_box), 5);
  gdk_rgba_parse (&color, "blue");
  gtk_widget_override_background_color (inner_box, 0, &color);

  gtk_container_add (GTK_CONTAINER (outer_box), inner_box);


  test_widget = gtk_event_box_new ();
  gdk_rgba_parse (&color, "red");
  gtk_widget_override_background_color (test_widget, 0, &color);

  gtk_container_add (GTK_CONTAINER (inner_box), test_widget);

  label = gtk_label_new (text);
  gtk_container_add (GTK_CONTAINER (test_widget), label);

  g_assert (TEST_WIDGET (outer_box) == test_widget);

  gtk_widget_show_all (outer_box);

  return outer_box;
}

static const char*
enum_to_string (GType enum_type,
                int   value)
{
  GEnumValue *v;

  v = g_enum_get_value (g_type_class_peek (enum_type), value);

  return v->value_nick;
}

static GtkWidget*
create_aligned (GtkAlign halign,
                GtkAlign valign)
{
  GtkWidget *widget;
  char *label;

  label = g_strdup_printf ("h=%s v=%s",
                           enum_to_string (GTK_TYPE_ALIGN, halign),
                           enum_to_string (GTK_TYPE_ALIGN, valign));

  widget = create_widget_visible_border (label);

  g_object_set (G_OBJECT (TEST_WIDGET (widget)),
                "halign", halign,
                "valign", valign,
                NULL);

  return widget;
}

static void
open_alignment_window (void)
{
  GtkWidget *table;
  int i;
  GEnumClass *align_class;

  test_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (test_window), "Alignment");

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  gtk_window_set_resizable (GTK_WINDOW (test_window), TRUE);
  gtk_window_set_default_size (GTK_WINDOW (test_window), 500, 500);

  align_class = g_type_class_peek (GTK_TYPE_ALIGN);

  table = gtk_table_new (align_class->n_values, align_class->n_values, TRUE);

  gtk_container_add (GTK_CONTAINER (test_window), table);

  for (i = 0; i < align_class->n_values; ++i)
    {
      int j;
      for (j = 0; j < align_class->n_values; ++j)
        {
          GtkWidget *child =
            create_aligned(align_class->values[i].value,
                           align_class->values[j].value);

          gtk_table_attach (GTK_TABLE (table),
                            child,
                            i, i + 1,
                            j, j + 1,
                            GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
        }
    }

  gtk_widget_show_all (test_window);
}

static GtkWidget*
create_margined (const char *propname)
{
  GtkWidget *widget;

  widget = create_widget_visible_border (propname);

  g_object_set (G_OBJECT (TEST_WIDGET (widget)),
                propname, 15,
                NULL);

  return widget;
}

static void
open_margin_window (void)
{
  GtkWidget *table;
  int i;
  const char * margins[] = {
    "margin-left",
    "margin-right",
    "margin-top",
    "margin-bottom",
    "margin"
  };

  test_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (test_window), "Margin");

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  gtk_window_set_resizable (GTK_WINDOW (test_window), TRUE);

  table = gtk_table_new (G_N_ELEMENTS (margins), 1, FALSE);

  gtk_container_add (GTK_CONTAINER (test_window), table);

  for (i = 0; i < (int) G_N_ELEMENTS (margins); ++i)
    {
      GtkWidget *child =
        create_margined(margins[i]);

      gtk_table_attach (GTK_TABLE (table),
                        child,
                        0, 1,
                        i, i + 1,
                        GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    }

  gtk_widget_show_all (test_window);
}

static void
open_valigned_label_window (void)
{
  GtkWidget *window, *box, *label, *frame;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (test_window, "delete-event",
                    G_CALLBACK (gtk_main_quit), test_window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (window), box);

  label = gtk_label_new ("Both labels expand");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  label = gtk_label_new ("Some wrapping text with width-chars = 15 and max-width-chars = 35");
  gtk_label_set_line_wrap  (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars  (GTK_LABEL (label), 15);
  gtk_label_set_max_width_chars  (GTK_LABEL (label), 35);

  gtk_widget_show (label);

  frame  = gtk_frame_new (NULL);
  gtk_widget_show (frame);
  gtk_container_add (GTK_CONTAINER (frame), label);

  gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);

  gtk_box_pack_start (GTK_BOX (box), frame, TRUE, TRUE, 0);

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  open_test_window ();
  open_control_window ();
  open_alignment_window ();
  open_margin_window ();
  open_valigned_label_window ();

  gtk_main ();

  return 0;
}
