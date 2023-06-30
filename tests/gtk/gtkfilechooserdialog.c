/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/* GTK - The GIMP Toolkit
 * gtkfilechooserdialog.c: File selector dialog
 * Copyright (C) 2003, Red Hat, Inc.
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

#include "gtkfilechooserdialog.h"

#include "gtkfilechooserprivate.h"
#include "gtkfilechooserwidget.h"
#include "gtkfilechooserutils.h"
#include "gtkfilechooserembed.h"
#include "gtkfilesystem.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkintl.h"

#include <stdarg.h>


/**
 * SECTION:gtkfilechooserdialog
 * @Short_description: A file chooser dialog, suitable for "File/Open" or "File/Save" commands
 * @Title: GtkFileChooserDialog
 * @See_also: #GtkFileChooser, #GtkDialog
 *
 * #GtkFileChooserDialog is a dialog box suitable for use with
 * "File/Open" or "File/Save as" commands.  This widget works by
 * putting a #GtkFileChooserWidget inside a #GtkDialog.  It exposes
 * the #GtkFileChooserIface interface, so you can use all of the
 * #GtkFileChooser functions on the file chooser dialog as well as
 * those for #GtkDialog.
 *
 * Note that #GtkFileChooserDialog does not have any methods of its
 * own.  Instead, you should use the functions that work on a
 * #GtkFileChooser.
 *
 * <example id="gtkfilechooser-typical-usage">
 * <title>Typical usage</title>
 * In the simplest of cases, you can the following code to use
 * #GtkFileChooserDialog to select a file for opening:
 * <para>
 * <informalexample><programlisting>
 * GtkWidget *dialog;
 *
 * dialog = gtk_file_chooser_dialog_new ("Open File",
 *                                       parent_window,
 *                                       GTK_FILE_CHOOSER_ACTION_OPEN,
 *                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
 *                                       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
 *                                       NULL);
 *
 * if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
 *   {
 *     char *filename;
 *
 *     filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
 *     open_file (filename);
 *     g_free (filename);
 *   }
 *
 * gtk_widget_destroy (dialog);
 * </programlisting></informalexample>
 * </para>
 * To use a dialog for saving, you can use this:
 * <para>
 * <informalexample><programlisting>
 * GtkWidget *dialog;
 *
 * dialog = gtk_file_chooser_dialog_new ("Save File",
 *                                       parent_window,
 *                                       GTK_FILE_CHOOSER_ACTION_SAVE,
 *                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
 *                                       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
 *                                       NULL);
 * gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
 *
 * if (user_edited_a_new_document)
 *   gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "Untitled document");
 * else
 *   gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), filename_for_existing_document);
 *
 * if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
 *   {
 *     char *filename;
 *
 *     filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
 *     save_to_file (filename);
 *     g_free (filename);
 *   }
 *
 * gtk_widget_destroy (dialog);
 * </programlisting></informalexample>
 * </para>
 * </example>
 * <section id="gtkfilechooserdialog-setting-up">
 * <title>Setting up a file chooser dialog</title>
 * There are various cases in which you may need to use a #GtkFileChooserDialog:
 * <itemizedlist><listitem>To select a file for opening, as for a
 *   <guimenuitem>File/Open</guimenuitem> command.  Use
 *   #GTK_FILE_CHOOSER_ACTION_OPEN.
 * </listitem>
 * <listitem>To save a file for the first time, as for a
 *   <guimenuitem>File/Save</guimenuitem> command.  Use
 *   #GTK_FILE_CHOOSER_ACTION_SAVE, and suggest a name such as
 *   "Untitled" with gtk_file_chooser_set_current_name().
 * </listitem>
 * <listitem>To save a file under a different name, as for a
 *   <guimenuitem>File/Save As</guimenuitem> command.  Use
 *   #GTK_FILE_CHOOSER_ACTION_SAVE, and set the existing filename
 *   with gtk_file_chooser_set_filename().
 * </listitem>
 * <listitem>To choose a folder instead of a file.  Use
 *   #GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER.
 * </listitem></itemizedlist>
 * <note>
 * <para>
 * Old versions of the file chooser's documentation suggested
 * using gtk_file_chooser_set_current_folder() in various
 * situations, with the intention of letting the application
 * suggest a reasonable default folder.  This is no longer
 * considered to be a good policy, as now the file chooser is
 * able to make good suggestions on its own.  In general, you
 * should only cause the file chooser to show a specific folder
 * when it is appropriate to use gtk_file_chooser_set_filename(),
 * i.e. when you are doing a <guimenuitem>File/Save
 * As</guimenuitem> command <emphasis>and</emphasis> you already
 * have a file saved somewhere.
 * </para>
 * </note>
 * </section>
 * <section id="gtkfilechooserdialog-response-codes">
 * <title>Response Codes</title>
 * #GtkFileChooserDialog inherits from #GtkDialog, so buttons that
 * go in its action area have response codes such as
 * #GTK_RESPONSE_ACCEPT and #GTK_RESPONSE_CANCEL.  For example, you
 * could call gtk_file_chooser_dialog_new() as follows:
 * <para>
 * <informalexample><programlisting>
 * GtkWidget *dialog;
 *
 * dialog = gtk_file_chooser_dialog_new ("Open File",
 *                                       parent_window,
 *                                       GTK_FILE_CHOOSER_ACTION_OPEN,
 *                                       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
 *                                       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
 *                                       NULL);
 * </programlisting></informalexample>
 * </para>
 * This will create buttons for "Cancel" and "Open" that use stock
 * response identifiers from #GtkResponseType.  For most dialog
 * boxes you can use your own custom response codes rather than the
 * ones in #GtkResponseType, but #GtkFileChooserDialog assumes that
 * its "accept"-type action, e.g. an "Open" or "Save" button,
 * <emphasis>will</emphasis> have one of the following response
 * codes:
 * <para>
 * <simplelist id="gtkfilechooserdialog-responses">
 * <member>#GTK_RESPONSE_ACCEPT</member>
 * <member>#GTK_RESPONSE_OK</member>
 * <member>#GTK_RESPONSE_YES</member>
 * <member>#GTK_RESPONSE_APPLY</member>
 * </simplelist>
 * </para>
 * This is because #GtkFileChooserDialog must intercept responses
 * and switch to folders if appropriate, rather than letting the
 * dialog terminate &mdash; the implementation uses these known
 * response codes to know which responses can be blocked if
 * appropriate.
 * <para>
 * <note>
 * To summarize, make sure you use a
 * <link linkend="gtkfilechooserdialog-responses">stock response code</link>
 * when you use #GtkFileChooserDialog to ensure proper operation.
 * </note>
 * </para>
 * </section>
 */


#define GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE(o)  (GTK_FILE_CHOOSER_DIALOG (o)->priv)

static void gtk_file_chooser_dialog_finalize   (GObject                   *object);

static GObject* gtk_file_chooser_dialog_constructor  (GType                  type,
						      guint                  n_construct_properties,
						      GObjectConstructParam *construct_params);
static void     gtk_file_chooser_dialog_set_property (GObject               *object,
						      guint                  prop_id,
						      const GValue          *value,
						      GParamSpec            *pspec);
static void     gtk_file_chooser_dialog_get_property (GObject               *object,
						      guint                  prop_id,
						      GValue                *value,
						      GParamSpec            *pspec);

static void     gtk_file_chooser_dialog_map          (GtkWidget             *widget);

static void response_cb (GtkDialog *dialog,
			 gint       response_id);

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserDialog, gtk_file_chooser_dialog, GTK_TYPE_DIALOG,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_FILE_CHOOSER,
						_gtk_file_chooser_delegate_iface_init))

static void
gtk_file_chooser_dialog_class_init (GtkFileChooserDialogClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  gobject_class->constructor = gtk_file_chooser_dialog_constructor;
  gobject_class->set_property = gtk_file_chooser_dialog_set_property;
  gobject_class->get_property = gtk_file_chooser_dialog_get_property;
  gobject_class->finalize = gtk_file_chooser_dialog_finalize;

  widget_class->map       = gtk_file_chooser_dialog_map;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_FILE_CHOOSER);

  _gtk_file_chooser_install_properties (gobject_class);

  g_type_class_add_private (class, sizeof (GtkFileChooserDialogPrivate));
}

static void
gtk_file_chooser_dialog_init (GtkFileChooserDialog *dialog)
{
  GtkWidget *action_area, *content_area;
  GtkFileChooserDialogPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
								   GTK_TYPE_FILE_CHOOSER_DIALOG,
								   GtkFileChooserDialogPrivate);
  GtkDialog *fc_dialog = GTK_DIALOG (dialog);

  dialog->priv = priv;
  dialog->priv->response_requested = FALSE;

  content_area = gtk_dialog_get_content_area (fc_dialog);
  action_area = gtk_dialog_get_action_area (fc_dialog);

  gtk_container_set_border_width (GTK_CONTAINER (fc_dialog), 5);
  gtk_box_set_spacing (GTK_BOX (content_area), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);

  /* We do a signal connection here rather than overriding the method in
   * class_init because GtkDialog::response is a RUN_LAST signal.  We want *our*
   * handler to be run *first*, regardless of whether the user installs response
   * handlers of his own.
   */
  g_signal_connect (dialog, "response",
		    G_CALLBACK (response_cb), NULL);
}

static void
gtk_file_chooser_dialog_finalize (GObject *object)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (object);

  g_free (dialog->priv->file_system);

  G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->finalize (object);  
}

static gboolean
is_stock_accept_response_id (int response_id)
{
  return (response_id == GTK_RESPONSE_ACCEPT
	  || response_id == GTK_RESPONSE_OK
	  || response_id == GTK_RESPONSE_YES
	  || response_id == GTK_RESPONSE_APPLY);
}

/* Callback used when the user activates a file in the file chooser widget */
static void
file_chooser_widget_file_activated (GtkFileChooser       *chooser,
				    GtkFileChooserDialog *dialog)
{
  GtkDialog *fc_dialog = GTK_DIALOG (dialog);
  GtkWidget *action_area;
  GList *children, *l;

  if (gtk_window_activate_default (GTK_WINDOW (dialog)))
    return;

  /* There probably isn't a default widget, so make things easier for the
   * programmer by looking for a reasonable button on our own.
   */
  action_area = gtk_dialog_get_action_area (fc_dialog);
  children = gtk_container_get_children (GTK_CONTAINER (action_area));

  for (l = children; l; l = l->next)
    {
      GtkWidget *widget;
      int response_id;

      widget = GTK_WIDGET (l->data);
      response_id = gtk_dialog_get_response_for_widget (fc_dialog, widget);
      if (gtk_widget_is_sensitive (widget) &&
          is_stock_accept_response_id (response_id))
	{
	  gtk_widget_activate (widget); /* Should we gtk_dialog_response (dialog, response_id) instead? */
	  break;
	}
    }

  g_list_free (children);
}

#if 0
/* FIXME: to see why this function is ifdef-ed out, see the comment below in
 * file_chooser_widget_default_size_changed().
 */
static void
load_position (int *out_xpos, int *out_ypos)
{
  GtkFileChooserSettings *settings;
  int x, y, width, height;

  settings = _gtk_file_chooser_settings_new ();
  _gtk_file_chooser_settings_get_geometry (settings, &x, &y, &width, &height);
  g_object_unref (settings);

  *out_xpos = x;
  *out_ypos = y;
}
#endif

static void
file_chooser_widget_default_size_changed (GtkWidget            *widget,
					  GtkFileChooserDialog *dialog)
{
  GtkFileChooserDialogPrivate *priv;
  gint default_width, default_height;
  GtkRequisition req, widget_req;

  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  /* Unset any previously set size */
  gtk_widget_set_size_request (GTK_WIDGET (dialog), -1, -1);

  if (gtk_widget_is_drawable (widget))
    {
      /* Force a size request of everything before we start.  This will make sure
       * that widget->requisition is meaningful. */
      gtk_widget_get_preferred_size (GTK_WIDGET (dialog),
                                     &req, NULL);
      gtk_widget_get_preferred_size (widget,
                                     &widget_req, NULL);
    }

  _gtk_file_chooser_embed_get_default_size (GTK_FILE_CHOOSER_EMBED (priv->widget),
					    &default_width, &default_height);

  gtk_window_resize (GTK_WINDOW (dialog), default_width, default_height);

  if (!gtk_widget_get_mapped (GTK_WIDGET (dialog)))
    {
#if 0
      /* FIXME: the code to restore the position does not work yet.  It is not
       * clear whether it is actually desirable --- if enabled, applications
       * would not be able to say "center the file chooser on top of my toplevel
       * window".  So, we don't use this code at all.
       */
      load_position (&xpos, &ypos);
      if (xpos >= 0 && ypos >= 0)
	{
	  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_NONE);
	  gtk_window_move (GTK_WINDOW (dialog), xpos, ypos);
	}
#endif
    }
}

static void
file_chooser_widget_response_requested (GtkWidget            *widget,
					GtkFileChooserDialog *dialog)
{
  GtkDialog *fc_dialog = GTK_DIALOG (dialog);
  GtkWidget *action_area;
  GList *children, *l;

  dialog->priv->response_requested = TRUE;

  if (gtk_window_activate_default (GTK_WINDOW (dialog)))
    return;

  /* There probably isn't a default widget, so make things easier for the
   * programmer by looking for a reasonable button on our own.
   */
  action_area = gtk_dialog_get_action_area (fc_dialog);
  children = gtk_container_get_children (GTK_CONTAINER (action_area));

  for (l = children; l; l = l->next)
    {
      GtkWidget *widget;
      int response_id;

      widget = GTK_WIDGET (l->data);
      response_id = gtk_dialog_get_response_for_widget (fc_dialog, widget);
      if (gtk_widget_is_sensitive (widget) &&
          is_stock_accept_response_id (response_id))
	{
	  gtk_widget_activate (widget); /* Should we gtk_dialog_response (dialog, response_id) instead? */
	  break;
	}
    }

  if (l == NULL)
    dialog->priv->response_requested = FALSE;

  g_list_free (children);
}
  
static GObject*
gtk_file_chooser_dialog_constructor (GType                  type,
				     guint                  n_construct_properties,
				     GObjectConstructParam *construct_params)
{
  GtkFileChooserDialogPrivate *priv;
  GtkWidget *content_area;
  GObject *object;

  object = G_OBJECT_CLASS (gtk_file_chooser_dialog_parent_class)->constructor (type,
									       n_construct_properties,
									       construct_params);
  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  gtk_widget_push_composite_child ();

  if (priv->file_system)
    priv->widget = g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET,
				 NULL);
  else
    priv->widget = g_object_new (GTK_TYPE_FILE_CHOOSER_WIDGET, NULL);

  g_signal_connect (priv->widget, "file-activated",
		    G_CALLBACK (file_chooser_widget_file_activated), object);
  g_signal_connect (priv->widget, "default-size-changed",
		    G_CALLBACK (file_chooser_widget_default_size_changed), object);
  g_signal_connect (priv->widget, "response-requested",
		    G_CALLBACK (file_chooser_widget_response_requested), object);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (object));

  gtk_container_set_border_width (GTK_CONTAINER (priv->widget), 5);
  gtk_box_pack_start (GTK_BOX (content_area), priv->widget, TRUE, TRUE, 0);

  gtk_widget_show (priv->widget);

  _gtk_file_chooser_set_delegate (GTK_FILE_CHOOSER (object),
				  GTK_FILE_CHOOSER (priv->widget));

  gtk_widget_pop_composite_child ();

  return object;
}

static void
gtk_file_chooser_dialog_set_property (GObject         *object,
				      guint            prop_id,
				      const GValue    *value,
				      GParamSpec      *pspec)

{
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  switch (prop_id)
    {
    default:
      g_object_set_property (G_OBJECT (priv->widget), pspec->name, value);
      break;
    }
}

static void
gtk_file_chooser_dialog_get_property (GObject         *object,
				      guint            prop_id,
				      GValue          *value,
				      GParamSpec      *pspec)
{
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (object);

  g_object_get_property (G_OBJECT (priv->widget), pspec->name, value);
}

static void
foreach_ensure_default_response_cb (GtkWidget *widget,
				    gpointer   data)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (data);
  int response_id;

  response_id = gtk_dialog_get_response_for_widget (GTK_DIALOG (dialog), widget);
  if (is_stock_accept_response_id (response_id))
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), response_id);
}

static void
ensure_default_response (GtkFileChooserDialog *dialog)
{
  GtkWidget *action_area;

  action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
  gtk_container_foreach (GTK_CONTAINER (action_area),
			 foreach_ensure_default_response_cb,
			 dialog);
}

/* GtkWidget::map handler */
static void
gtk_file_chooser_dialog_map (GtkWidget *widget)
{
  GtkFileChooserDialog *dialog = GTK_FILE_CHOOSER_DIALOG (widget);
  GtkFileChooserDialogPrivate *priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  ensure_default_response (dialog);

  _gtk_file_chooser_embed_initial_focus (GTK_FILE_CHOOSER_EMBED (priv->widget));

  GTK_WIDGET_CLASS (gtk_file_chooser_dialog_parent_class)->map (widget);
}

/* GtkDialog::response handler */
static void
response_cb (GtkDialog *dialog,
	     gint       response_id)
{
  GtkFileChooserDialogPrivate *priv;

  priv = GTK_FILE_CHOOSER_DIALOG_GET_PRIVATE (dialog);

  /* Act only on response IDs we recognize */
  if (is_stock_accept_response_id (response_id)
      && !priv->response_requested
      && !_gtk_file_chooser_embed_should_respond (GTK_FILE_CHOOSER_EMBED (priv->widget)))
    {
      g_signal_stop_emission_by_name (dialog, "response");
    }

  priv->response_requested = FALSE;
}

static GtkWidget *
gtk_file_chooser_dialog_new_valist (const gchar          *title,
				    GtkWindow            *parent,
				    GtkFileChooserAction  action,
				    const gchar          *first_button_text,
				    va_list               varargs)
{
  GtkWidget *result;
  const char *button_text = first_button_text;
  gint response_id;

  result = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			 "title", title,
			 "action", action,
			 NULL);

  if (parent)
    gtk_window_set_transient_for (GTK_WINDOW (result), parent);

  while (button_text)
    {
      response_id = va_arg (varargs, gint);
      gtk_dialog_add_button (GTK_DIALOG (result), button_text, response_id);
      button_text = va_arg (varargs, const gchar *);
    }

  return result;
}

/**
 * gtk_file_chooser_dialog_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #GtkFileChooserDialog.  This function is analogous to
 * gtk_dialog_new_with_buttons().
 *
 * Return value: a new #GtkFileChooserDialog
 *
 * Since: 2.4
 **/
GtkWidget *
gtk_file_chooser_dialog_new (const gchar         *title,
			     GtkWindow           *parent,
			     GtkFileChooserAction action,
			     const gchar         *first_button_text,
			     ...)
{
  GtkWidget *result;
  va_list varargs;
  
  va_start (varargs, first_button_text);
  result = gtk_file_chooser_dialog_new_valist (title, parent, action,
					       first_button_text,
					       varargs);
  va_end (varargs);

  return result;
}
