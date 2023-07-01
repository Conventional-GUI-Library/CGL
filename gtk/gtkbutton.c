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
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/**
 * SECTION:gtkbutton
 * @Short_description: A widget that creates a signal when clicked on
 * @Title: GtkButton
 *
 * The #GtkButton widget is generally used to attach a function to that is
 * called when the button is pressed.  The various signals and how to use them
 * are outlined below.
 *
 * The #GtkButton widget can hold any valid child widget.  That is it can hold
 * most any other standard #GtkWidget.  The most commonly used child is the
 * #GtkLabel.
 */

#include "config.h"

#include "gtkbutton.h"
#include "gtkbuttonprivate.h"

#include <string.h>
#include "gtkalignment.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkmarshalers.h"
#include "gtkimage.h"
#include "gtkbox.h"
#include "gtkstock.h"
#include "gtkiconfactory.h"
#include "gtkactivatable.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "a11y/gtkbuttonaccessible.h"


static const GtkBorder default_default_border = { 1, 1, 1, 1 };
static const GtkBorder default_default_outside_border = { 0, 0, 0, 0 };
static const GtkBorder default_inner_border = { 1, 1, 1, 1 };

/* Time out before giving up on getting a key release when animating
 * the close button.
 */
#define ACTIVATE_TIMEOUT 250


enum {
  PRESSED,
  RELEASED,
  CLICKED,
  ENTER,
  LEAVE,
  ACTIVATE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_LABEL,
  PROP_IMAGE,
  PROP_RELIEF,
  PROP_USE_UNDERLINE,
  PROP_USE_STOCK,
  PROP_FOCUS_ON_CLICK,
  PROP_XALIGN,
  PROP_YALIGN,
  PROP_IMAGE_POSITION,

  /* activatable properties */
  PROP_ACTIVATABLE_RELATED_ACTION,
  PROP_ACTIVATABLE_USE_ACTION_APPEARANCE
};


static void gtk_button_destroy        (GtkWidget          *widget);
static void gtk_button_dispose        (GObject            *object);
static void gtk_button_set_property   (GObject            *object,
                                       guint               prop_id,
                                       const GValue       *value,
                                       GParamSpec         *pspec);
static void gtk_button_get_property   (GObject            *object,
                                       guint               prop_id,
                                       GValue             *value,
                                       GParamSpec         *pspec);
static void gtk_button_screen_changed (GtkWidget          *widget,
				       GdkScreen          *previous_screen);
static void gtk_button_realize (GtkWidget * widget);
static void gtk_button_unrealize (GtkWidget * widget);
static void gtk_button_map (GtkWidget * widget);
static void gtk_button_unmap (GtkWidget * widget);
static void gtk_button_style_updated (GtkWidget * widget);
static void gtk_button_size_allocate (GtkWidget * widget,
				      GtkAllocation * allocation);
static gint gtk_button_draw (GtkWidget * widget, cairo_t *cr);
static gint gtk_button_button_press (GtkWidget * widget,
				     GdkEventButton * event);
static gint gtk_button_button_release (GtkWidget * widget,
				       GdkEventButton * event);
static gint gtk_button_grab_broken (GtkWidget * widget,
				    GdkEventGrabBroken * event);
static gint gtk_button_key_release (GtkWidget * widget, GdkEventKey * event);
static gint gtk_button_enter_notify (GtkWidget * widget,
				     GdkEventCrossing * event);
static gint gtk_button_leave_notify (GtkWidget * widget,
				     GdkEventCrossing * event);
static void gtk_real_button_pressed (GtkButton * button);
static void gtk_real_button_released (GtkButton * button);
static void gtk_real_button_clicked (GtkButton * button);
static void gtk_real_button_activate  (GtkButton          *button);
static void gtk_button_update_state   (GtkButton          *button);
static void gtk_button_add            (GtkContainer       *container,
			               GtkWidget          *widget);
static GType gtk_button_child_type    (GtkContainer       *container);
static void gtk_button_finish_activate (GtkButton         *button,
					gboolean           do_it);

static GObject*	gtk_button_constructor (GType                  type,
					guint                  n_construct_properties,
					GObjectConstructParam *construct_params);
static void gtk_button_construct_child (GtkButton             *button);
static void gtk_button_state_changed   (GtkWidget             *widget,
					GtkStateType           previous_state);
static void gtk_button_grab_notify     (GtkWidget             *widget,
					gboolean               was_grabbed);


static void gtk_button_activatable_interface_init(GtkActivatableIface  *iface);
static void gtk_button_update                    (GtkActivatable       *activatable,
				                  GtkAction            *action,
			                          const gchar          *property_name);
static void gtk_button_sync_action_properties    (GtkActivatable       *activatable,
                                                  GtkAction            *action);
static void gtk_button_set_related_action        (GtkButton            *button,
					          GtkAction            *action);
static void gtk_button_set_use_action_appearance (GtkButton            *button,
						  gboolean              use_appearance);

static void gtk_button_get_preferred_width       (GtkWidget           *widget,
						  gint                *minimum_size,
						  gint                *natural_size);
static void gtk_button_get_preferred_height      (GtkWidget           *widget,
						  gint                *minimum_size,
						  gint                *natural_size);
  
static guint button_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_CODE (GtkButton, gtk_button, GTK_TYPE_BIN,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
						gtk_button_activatable_interface_init))

static void
gtk_button_class_init (GtkButtonClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = (GtkWidgetClass*) klass;
  container_class = (GtkContainerClass*) klass;
  
  gobject_class->constructor  = gtk_button_constructor;
  gobject_class->dispose      = gtk_button_dispose;
  gobject_class->set_property = gtk_button_set_property;
  gobject_class->get_property = gtk_button_get_property;

  widget_class->get_preferred_width  = gtk_button_get_preferred_width;
  widget_class->get_preferred_height = gtk_button_get_preferred_height;
  widget_class->destroy = gtk_button_destroy;
  widget_class->screen_changed = gtk_button_screen_changed;
  widget_class->realize = gtk_button_realize;
  widget_class->unrealize = gtk_button_unrealize;
  widget_class->map = gtk_button_map;
  widget_class->unmap = gtk_button_unmap;
  widget_class->style_updated = gtk_button_style_updated;
  widget_class->size_allocate = gtk_button_size_allocate;
  widget_class->draw = gtk_button_draw;
  widget_class->button_press_event = gtk_button_button_press;
  widget_class->button_release_event = gtk_button_button_release;
  widget_class->grab_broken_event = gtk_button_grab_broken;
  widget_class->key_release_event = gtk_button_key_release;
  widget_class->enter_notify_event = gtk_button_enter_notify;
  widget_class->leave_notify_event = gtk_button_leave_notify;
  widget_class->state_changed = gtk_button_state_changed;
  widget_class->grab_notify = gtk_button_grab_notify;

  container_class->child_type = gtk_button_child_type;
  container_class->add = gtk_button_add;
  gtk_container_class_handle_border_width (container_class);

  klass->pressed = gtk_real_button_pressed;
  klass->released = gtk_real_button_released;
  klass->clicked = NULL;
  klass->enter = gtk_button_update_state;
  klass->leave = gtk_button_update_state;
  klass->activate = gtk_real_button_activate;

  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        P_("Label"),
                                                        P_("Text of the label widget inside the button, if the button contains a label widget"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  
  g_object_class_install_property (gobject_class,
                                   PROP_USE_UNDERLINE,
                                   g_param_spec_boolean ("use-underline",
							 P_("Use underline"),
							 P_("If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  
  g_object_class_install_property (gobject_class,
                                   PROP_USE_STOCK,
                                   g_param_spec_boolean ("use-stock",
							 P_("Use stock"),
							 P_("If set, the label is used to pick a stock item instead of being displayed"),
                                                        FALSE,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  
  g_object_class_install_property (gobject_class,
                                   PROP_FOCUS_ON_CLICK,
                                   g_param_spec_boolean ("focus-on-click",
							 P_("Focus on click"),
							 P_("Whether the button grabs focus when it is clicked with the mouse"),
							 TRUE,
							 GTK_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_RELIEF,
                                   g_param_spec_enum ("relief",
                                                      P_("Border relief"),
                                                      P_("The border relief style"),
                                                      GTK_TYPE_RELIEF_STYLE,
                                                      GTK_RELIEF_NORMAL,
                                                      GTK_PARAM_READWRITE));
  
  /**
   * GtkButton:xalign:
   *
   * If the child of the button is a #GtkMisc or #GtkAlignment, this property 
   * can be used to control its horizontal alignment. 0.0 is left aligned, 
   * 1.0 is right aligned.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_XALIGN,
                                   g_param_spec_float("xalign",
                                                      P_("Horizontal alignment for child"),
                                                      P_("Horizontal position of child in available space. 0.0 is left aligned, 1.0 is right aligned"),
                                                      0.0,
                                                      1.0,
                                                      0.5,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkButton:yalign:
   *
   * If the child of the button is a #GtkMisc or #GtkAlignment, this property 
   * can be used to control its vertical alignment. 0.0 is top aligned, 
   * 1.0 is bottom aligned.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_YALIGN,
                                   g_param_spec_float("yalign",
                                                      P_("Vertical alignment for child"),
                                                      P_("Vertical position of child in available space. 0.0 is top aligned, 1.0 is bottom aligned"),
                                                      0.0,
                                                      1.0,
                                                      0.5,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkButton:image:
   *
   * The child widget to appear next to the button text.
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
                                   PROP_IMAGE,
                                   g_param_spec_object ("image",
                                                        P_("Image widget"),
                                                        P_("Child widget to appear next to the button text"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));

  /**
   * GtkButton:image-position:
   *
   * The position of the image relative to the text inside the button.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
                                   PROP_IMAGE_POSITION,
                                   g_param_spec_enum ("image-position",
                                            P_("Image position"),
                                                      P_("The position of the image relative to the text"),
                                                      GTK_TYPE_POSITION_TYPE,
                                                      GTK_POS_LEFT,
                                                      GTK_PARAM_READWRITE));

  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_RELATED_ACTION, "related-action");
  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_USE_ACTION_APPEARANCE, "use-action-appearance");

  /**
   * GtkButton::pressed:
   * @button: the object that received the signal
   *
   * Emitted when the button is pressed.
   *
   * Deprecated: 2.8: Use the #GtkWidget::button-press-event signal.
   */ 
  button_signals[PRESSED] =
    g_signal_new (I_("pressed"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkButtonClass, pressed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::released:
   * @button: the object that received the signal
   *
   * Emitted when the button is released.
   *
   * Deprecated: 2.8: Use the #GtkWidget::button-release-event signal.
   */ 
  button_signals[RELEASED] =
    g_signal_new (I_("released"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkButtonClass, released),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::clicked:
   * @button: the object that received the signal
   *
   * Emitted when the button has been activated (pressed and released).
   */ 
  button_signals[CLICKED] =
    g_signal_new (I_("clicked"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkButtonClass, clicked),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::enter:
   * @button: the object that received the signal
   *
   * Emitted when the pointer enters the button.
   *
   * Deprecated: 2.8: Use the #GtkWidget::enter-notify-event signal.
   */ 
  button_signals[ENTER] =
    g_signal_new (I_("enter"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkButtonClass, enter),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::leave:
   * @button: the object that received the signal
   *
   * Emitted when the pointer leaves the button.
   *
   * Deprecated: 2.8: Use the #GtkWidget::leave-notify-event signal.
   */ 
  button_signals[LEAVE] =
    g_signal_new (I_("leave"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkButtonClass, leave),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkButton::activate:
   * @widget: the object which received the signal.
   *
   * The ::activate signal on GtkButton is an action signal and
   * emitting it causes the button to animate press then release. 
   * Applications should never connect to this signal, but use the
   * #GtkButton::clicked signal.
   */
  button_signals[ACTIVATE] =
    g_signal_new (I_("activate"),
		  G_OBJECT_CLASS_TYPE (gobject_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkButtonClass, activate),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  widget_class->activate_signal = button_signals[ACTIVATE];

  /**
   * GtkButton:default-border:
   *
   * The "default-border" style property defines the extra space to add
   * around a button that can become the default widget of its window.
   * For more information about default widgets, see gtk_widget_grab_default().
   */

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("default-border",
							       P_("Default Spacing"),
							       P_("Extra space to add for GTK_CAN_DEFAULT buttons"),
							       GTK_TYPE_BORDER,
							       GTK_PARAM_READABLE));

  /**
   * GtkButton:default-outside-border:
   *
   * The "default-outside-border" style property defines the extra outside
   * space to add around a button that can become the default widget of its
   * window. Extra outside space is always drawn outside the button border.
   * For more information about default widgets, see gtk_widget_grab_default().
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("default-outside-border",
							       P_("Default Outside Spacing"),
							       P_("Extra space to add for GTK_CAN_DEFAULT buttons that is always drawn outside the border"),
							       GTK_TYPE_BORDER,
							       GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-displacement-x",
							     P_("Child X Displacement"),
							     P_("How far in the x direction to move the child when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("child-displacement-y",
							     P_("Child Y Displacement"),
							     P_("How far in the y direction to move the child when the button is depressed"),
							     G_MININT,
							     G_MAXINT,
							     0,
							     GTK_PARAM_READABLE));

  /**
   * GtkButton:displace-focus:
   *
   * Whether the child_displacement_x/child_displacement_y properties 
   * should also affect the focus rectangle.
   *
   * Since: 2.6
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boolean ("displace-focus",
								 P_("Displace focus"),
								 P_("Whether the child_displacement_x/_y properties should also affect the focus rectangle"),
								 FALSE,
								 GTK_PARAM_READABLE));

  /**
   * GtkButton:inner-border:
   *
   * Sets the border between the button edges and child.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_boxed ("inner-border",
                                                               P_("Inner Border"),
                                                               P_("Border between button edges and child."),
                                                               GTK_TYPE_BORDER,
                                                               GTK_PARAM_READABLE));

  /**
   * GtkButton::image-spacing:
   *
   * Spacing in pixels between the image and label.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("image-spacing",
							     P_("Image spacing"),
							     P_("Spacing in pixels between the image and label"),
							     0,
							     G_MAXINT,
							     2,
							     GTK_PARAM_READABLE));

  g_type_class_add_private (gobject_class, sizeof (GtkButtonPrivate));

  gtk_widget_class_set_accessible_type (widget_class, GTK_TYPE_BUTTON_ACCESSIBLE);
}

static void
gtk_button_init (GtkButton *button)
{
  GtkButtonPrivate *priv;
  GtkStyleContext *context;

  button->priv = G_TYPE_INSTANCE_GET_PRIVATE (button,
                                              GTK_TYPE_BUTTON,
                                              GtkButtonPrivate);
  priv = button->priv;

  gtk_widget_set_can_focus (GTK_WIDGET (button), TRUE);
  gtk_widget_set_receives_default (GTK_WIDGET (button), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (button), FALSE);

  priv->label_text = NULL;

  priv->constructed = FALSE;
  priv->in_button = FALSE;
  priv->button_down = FALSE;
  priv->relief = GTK_RELIEF_NORMAL;
  priv->use_stock = FALSE;
  priv->use_underline = FALSE;
  priv->depressed = FALSE;
  priv->depress_on_activate = TRUE;
  priv->focus_on_click = TRUE;

  priv->xalign = 0.5;
  priv->yalign = 0.5;
  priv->align_set = 0;
  priv->image_is_stock = TRUE;
  priv->image_position = GTK_POS_LEFT;
  priv->use_action_appearance = TRUE;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_BUTTON);
}

static void
gtk_button_destroy (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->label_text)
    {
      g_free (priv->label_text);
      priv->label_text = NULL;
    }

  GTK_WIDGET_CLASS (gtk_button_parent_class)->destroy (widget);
}

static GObject*
gtk_button_constructor (GType                  type,
			guint                  n_construct_properties,
			GObjectConstructParam *construct_params)
{
  GObject *object;
  GtkButton *button;
  GtkButtonPrivate *priv;

  object = G_OBJECT_CLASS (gtk_button_parent_class)->constructor (type,
                                                                  n_construct_properties,
                                                                  construct_params);

  button = GTK_BUTTON (object);
  priv = button->priv;

  priv->constructed = TRUE;

  if (priv->label_text != NULL)
    gtk_button_construct_child (button);
  
  return object;
}


static GType
gtk_button_child_type  (GtkContainer     *container)
{
  if (!gtk_bin_get_child (GTK_BIN (container)))
    return GTK_TYPE_WIDGET;
  else
    return G_TYPE_NONE;
}

static void
maybe_set_alignment (GtkButton *button,
		     GtkWidget *widget)
{
  GtkButtonPrivate *priv = button->priv;

  if (GTK_IS_MISC (widget))
    {
      GtkMisc *misc = GTK_MISC (widget);
      
      if (priv->align_set)
	gtk_misc_set_alignment (misc, priv->xalign, priv->yalign);
    }
  else if (GTK_IS_ALIGNMENT (widget))
    {
      GtkAlignment *alignment = GTK_ALIGNMENT (widget);
      gfloat xscale, yscale;

      g_object_get (alignment,
                    "xscale", &xscale,
                    "yscale", &yscale,
                    NULL);

      if (priv->align_set)
        gtk_alignment_set (alignment,
                           priv->xalign, priv->yalign,
                           xscale, yscale);
    }
}

static void
gtk_button_add (GtkContainer *container,
		GtkWidget    *widget)
{
  maybe_set_alignment (GTK_BUTTON (container), widget);

  GTK_CONTAINER_CLASS (gtk_button_parent_class)->add (container, widget);
}

static void 
gtk_button_dispose (GObject *object)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = button->priv;

  if (priv->action)
    {
      gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (button), NULL);
      priv->action = NULL;
    }
  G_OBJECT_CLASS (gtk_button_parent_class)->dispose (object);
}

static void
gtk_button_set_property (GObject         *object,
                         guint            prop_id,
                         const GValue    *value,
                         GParamSpec      *pspec)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = button->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_button_set_label (button, g_value_get_string (value));
      break;
    case PROP_IMAGE:
      gtk_button_set_image (button, (GtkWidget *) g_value_get_object (value));
      break;
    case PROP_RELIEF:
      gtk_button_set_relief (button, g_value_get_enum (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_button_set_use_underline (button, g_value_get_boolean (value));
      break;
    case PROP_USE_STOCK:
      gtk_button_set_use_stock (button, g_value_get_boolean (value));
      break;
    case PROP_FOCUS_ON_CLICK:
      gtk_button_set_focus_on_click (button, g_value_get_boolean (value));
      break;
    case PROP_XALIGN:
      gtk_button_set_alignment (button, g_value_get_float (value), priv->yalign);
      break;
    case PROP_YALIGN:
      gtk_button_set_alignment (button, priv->xalign, g_value_get_float (value));
      break;
    case PROP_IMAGE_POSITION:
      gtk_button_set_image_position (button, g_value_get_enum (value));
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      gtk_button_set_related_action (button, g_value_get_object (value));
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      gtk_button_set_use_action_appearance (button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_button_get_property (GObject         *object,
                         guint            prop_id,
                         GValue          *value,
                         GParamSpec      *pspec)
{
  GtkButton *button = GTK_BUTTON (object);
  GtkButtonPrivate *priv = button->priv;

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, priv->label_text);
      break;
    case PROP_IMAGE:
      g_value_set_object (value, (GObject *)priv->image);
      break;
    case PROP_RELIEF:
      g_value_set_enum (value, priv->relief);
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, priv->use_underline);
      break;
    case PROP_USE_STOCK:
      g_value_set_boolean (value, priv->use_stock);
      break;
    case PROP_FOCUS_ON_CLICK:
      g_value_set_boolean (value, priv->focus_on_click);
      break;
    case PROP_XALIGN:
      g_value_set_float (value, priv->xalign);
      break;
    case PROP_YALIGN:
      g_value_set_float (value, priv->yalign);
      break;
    case PROP_IMAGE_POSITION:
      g_value_set_enum (value, priv->image_position);
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      g_value_set_object (value, priv->action);
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, priv->use_action_appearance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_button_activatable_interface_init (GtkActivatableIface  *iface)
{
  iface->update = gtk_button_update;
  iface->sync_action_properties = gtk_button_sync_action_properties;
}

static void
activatable_update_stock_id (GtkButton *button,
			     GtkAction *action)
{
  if (!gtk_button_get_use_stock (button))
    return;

  gtk_button_set_label (button, gtk_action_get_stock_id (action));
}

static void
activatable_update_short_label (GtkButton *button,
				GtkAction *action)
{
  GtkWidget *child;
  GtkWidget *image;

  if (gtk_button_get_use_stock (button))
    return;

  image = gtk_button_get_image (button);

  /* Dont touch custom child... */
  child = gtk_bin_get_child (GTK_BIN (button));
  if (GTK_IS_IMAGE (image) ||
      child == NULL ||
      GTK_IS_LABEL (child))
    {
      gtk_button_set_label (button, gtk_action_get_short_label (action));
      gtk_button_set_use_underline (button, TRUE);
    }
}

static void
activatable_update_icon_name (GtkButton *button,
			      GtkAction *action)
{
  GtkWidget *image;
	      
  if (gtk_button_get_use_stock (button))
    return;

  image = gtk_button_get_image (button);

  if (GTK_IS_IMAGE (image) &&
      (gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_EMPTY ||
       gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_ICON_NAME))
    gtk_image_set_from_icon_name (GTK_IMAGE (image),
				  gtk_action_get_icon_name (action), GTK_ICON_SIZE_MENU);
}

static void
activatable_update_gicon (GtkButton *button,
			  GtkAction *action)
{
  GtkWidget *image = gtk_button_get_image (button);
  GIcon *icon = gtk_action_get_gicon (action);
  
  if (GTK_IS_IMAGE (image) &&
      (gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_EMPTY ||
       gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_GICON))
    gtk_image_set_from_gicon (GTK_IMAGE (image), icon, GTK_ICON_SIZE_BUTTON);
}

static void 
gtk_button_update (GtkActivatable *activatable,
		   GtkAction      *action,
	           const gchar    *property_name)
{
  GtkButton *button = GTK_BUTTON (activatable);
  GtkButtonPrivate *priv = button->priv;

  if (strcmp (property_name, "visible") == 0)
    {
      if (gtk_action_is_visible (action))
	gtk_widget_show (GTK_WIDGET (activatable));
      else
	gtk_widget_hide (GTK_WIDGET (activatable));
    }
  else if (strcmp (property_name, "sensitive") == 0)
    gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));

  if (!priv->use_action_appearance)
    return;

  if (strcmp (property_name, "stock-id") == 0)
    activatable_update_stock_id (GTK_BUTTON (activatable), action);
  else if (strcmp (property_name, "gicon") == 0)
    activatable_update_gicon (GTK_BUTTON (activatable), action);
  else if (strcmp (property_name, "short-label") == 0)
    activatable_update_short_label (GTK_BUTTON (activatable), action);
  else if (strcmp (property_name, "icon-name") == 0)
    activatable_update_icon_name (GTK_BUTTON (activatable), action);
}

static void
gtk_button_sync_action_properties (GtkActivatable *activatable,
			           GtkAction      *action)
{
  GtkButton *button = GTK_BUTTON (activatable);
  GtkButtonPrivate *priv = button->priv;

  if (!action)
    return;

  if (gtk_action_is_visible (action))
    gtk_widget_show (GTK_WIDGET (activatable));
  else
    gtk_widget_hide (GTK_WIDGET (activatable));
  
  gtk_widget_set_sensitive (GTK_WIDGET (activatable), gtk_action_is_sensitive (action));
  
  if (priv->use_action_appearance)
    {
      activatable_update_stock_id (GTK_BUTTON (activatable), action);
      activatable_update_short_label (GTK_BUTTON (activatable), action);
      activatable_update_gicon (GTK_BUTTON (activatable), action);
      activatable_update_icon_name (GTK_BUTTON (activatable), action);
    }
}

static void
gtk_button_set_related_action (GtkButton *button,
			       GtkAction *action)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->action == action)
    return;

  /* This should be a default handler, but for compatibility reasons
   * we need to support derived classes that don't chain up their
   * clicked handler.
   */
  g_signal_handlers_disconnect_by_func (button, gtk_real_button_clicked, NULL);
  if (action)
    g_signal_connect_after (button, "clicked",
                            G_CALLBACK (gtk_real_button_clicked), NULL);

  gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (button), action);

  priv->action = action;
}

static void
gtk_button_set_use_action_appearance (GtkButton *button,
				      gboolean   use_appearance)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->use_action_appearance != use_appearance)
    {
      priv->use_action_appearance = use_appearance;

      gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (button), priv->action);
    }
}

/**
 * gtk_button_new:
 *
 * Creates a new #GtkButton widget. To add a child widget to the button,
 * use gtk_container_add().
 *
 * Returns: The newly created #GtkButton widget.
 */
GtkWidget*
gtk_button_new (void)
{
  return g_object_new (GTK_TYPE_BUTTON, NULL);
}

static gboolean
show_image (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;
  gboolean show;

  if (priv->label_text)
    {
      GtkSettings *settings;

      settings = gtk_widget_get_settings (GTK_WIDGET (button));        
      g_object_get (settings, "gtk-button-images", &show, NULL);
    }
  else
    show = TRUE;

  return show;
}

static void
gtk_button_construct_child (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;
  GtkStyleContext *context;
  GtkStockItem item;
  GtkWidget *child;
  GtkWidget *label;
  GtkWidget *box;
  GtkWidget *align;
  GtkWidget *image = NULL;
  gchar *label_text = NULL;
  gint image_spacing;

  if (!priv->constructed)
    return;

  if (!priv->label_text && !priv->image)
    return;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));

  gtk_style_context_get_style (context,
                               "image-spacing", &image_spacing,
                               NULL);

  if (priv->image && !priv->image_is_stock)
    {
      GtkWidget *parent;

      image = g_object_ref (priv->image);

      parent = gtk_widget_get_parent (image);
      if (parent)
	gtk_container_remove (GTK_CONTAINER (parent), image);
    }

  priv->image = NULL;

  child = gtk_bin_get_child (GTK_BIN (button));
  if (child)
    gtk_container_remove (GTK_CONTAINER (button), child);

  if (priv->use_stock &&
      priv->label_text &&
      gtk_stock_lookup (priv->label_text, &item))
    {
      if (!image)
	image = g_object_ref (gtk_image_new_from_stock (priv->label_text, GTK_ICON_SIZE_BUTTON));

      label_text = item.label;
    }
  else
    label_text = priv->label_text;

  if (image)
    {
      priv->image = image;
      g_object_set (priv->image,
		    "visible", show_image (button),
		    "no-show-all", TRUE,
		    NULL);

      if (priv->image_position == GTK_POS_LEFT ||
	  priv->image_position == GTK_POS_RIGHT)
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, image_spacing);
      else
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, image_spacing);

      if (priv->align_set)
	align = gtk_alignment_new (priv->xalign, priv->yalign, 0.0, 0.0);
      else
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

      if (priv->image_position == GTK_POS_LEFT ||
	  priv->image_position == GTK_POS_TOP)
	gtk_box_pack_start (GTK_BOX (box), priv->image, FALSE, FALSE, 0);
      else
	gtk_box_pack_end (GTK_BOX (box), priv->image, FALSE, FALSE, 0);

      if (label_text)
	{
          if (priv->use_underline || priv->use_stock)
            {
	      label = gtk_label_new_with_mnemonic (label_text);
	      gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                             GTK_WIDGET (button));
            }
          else
            label = gtk_label_new (label_text);

	  if (priv->image_position == GTK_POS_RIGHT ||
	      priv->image_position == GTK_POS_BOTTOM)
	    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
	  else
	    gtk_box_pack_end (GTK_BOX (box), label, FALSE, FALSE, 0);
	}

      gtk_container_add (GTK_CONTAINER (button), align);
      gtk_container_add (GTK_CONTAINER (align), box);
      gtk_widget_show_all (align);

      g_object_unref (image);

      return;
    }

  if (priv->use_underline || priv->use_stock)
    {
      label = gtk_label_new_with_mnemonic (priv->label_text);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (button));
    }
  else
    label = gtk_label_new (priv->label_text);

  if (priv->align_set)
    gtk_misc_set_alignment (GTK_MISC (label), priv->xalign, priv->yalign);

  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (button), label);
}


/**
 * gtk_button_new_with_label:
 * @label: The text you want the #GtkLabel to hold.
 *
 * Creates a #GtkButton widget with a #GtkLabel child containing the given
 * text.
 *
 * Returns: The newly created #GtkButton widget.
 */
GtkWidget*
gtk_button_new_with_label (const gchar *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, NULL);
}

/**
 * gtk_button_new_from_stock:
 * @stock_id: the name of the stock item 
 *
 * Creates a new #GtkButton containing the image and text from a stock item.
 * Some stock ids have preprocessor macros like #GTK_STOCK_OK and
 * #GTK_STOCK_APPLY.
 *
 * If @stock_id is unknown, then it will be treated as a mnemonic
 * label (as for gtk_button_new_with_mnemonic()).
 *
 * Returns: a new #GtkButton
 **/
GtkWidget*
gtk_button_new_from_stock (const gchar *stock_id)
{
  return g_object_new (GTK_TYPE_BUTTON,
                       "label", stock_id,
                       "use-stock", TRUE,
                       "use-underline", TRUE,
                       NULL);
}

/**
 * gtk_button_new_with_mnemonic:
 * @label: The text of the button, with an underscore in front of the
 *         mnemonic character
 *
 * Creates a new #GtkButton containing a label.
 * If characters in @label are preceded by an underscore, they are underlined.
 * If you need a literal underscore character in a label, use '__' (two
 * underscores). The first underlined character represents a keyboard
 * accelerator called a mnemonic.
 * Pressing Alt and that key activates the button.
 *
 * Returns: a new #GtkButton
 **/
GtkWidget*
gtk_button_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GTK_TYPE_BUTTON, "label", label, "use-underline", TRUE,  NULL);
}

/**
 * gtk_button_pressed:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::pressed signal to the given #GtkButton.
 *
 * Deprecated: 2.20: Use the #GtkWidget::button-press-event signal.
 */
void
gtk_button_pressed (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[PRESSED], 0);
}

/**
 * gtk_button_released:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::released signal to the given #GtkButton.
 *
 * Deprecated: 2.20: Use the #GtkWidget::button-release-event signal.
 */
void
gtk_button_released (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[RELEASED], 0);
}

/**
 * gtk_button_clicked:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::clicked signal to the given #GtkButton.
 */
void
gtk_button_clicked (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[CLICKED], 0);
}

/**
 * gtk_button_enter:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::enter signal to the given #GtkButton.
 *
 * Deprecated: 2.20: Use the #GtkWidget::enter-notify-event signal.
 */
void
gtk_button_enter (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[ENTER], 0);
}

/**
 * gtk_button_leave:
 * @button: The #GtkButton you want to send the signal to.
 *
 * Emits a #GtkButton::leave signal to the given #GtkButton.
 *
 * Deprecated: 2.20: Use the #GtkWidget::leave-notify-event signal.
 */
void
gtk_button_leave (GtkButton *button)
{
  g_return_if_fail (GTK_IS_BUTTON (button));

  g_signal_emit (button, button_signals[LEAVE], 0);
}

/**
 * gtk_button_set_relief:
 * @button: The #GtkButton you want to set relief styles of.
 * @newstyle: The GtkReliefStyle as described above.
 *
 * Sets the relief style of the edges of the given #GtkButton widget.
 * Three styles exist, GTK_RELIEF_NORMAL, GTK_RELIEF_HALF, GTK_RELIEF_NONE.
 * The default style is, as one can guess, GTK_RELIEF_NORMAL.
 *
 * <!-- FIXME: put pictures of each style -->
 */
void
gtk_button_set_relief (GtkButton *button,
		       GtkReliefStyle newrelief)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  if (newrelief != priv->relief)
    {
       priv->relief = newrelief;
       g_object_notify (G_OBJECT (button), "relief");
       gtk_widget_queue_draw (GTK_WIDGET (button));
    }
}

/**
 * gtk_button_get_relief:
 * @button: The #GtkButton you want the #GtkReliefStyle from.
 *
 * Returns the current relief style of the given #GtkButton.
 *
 * Returns: The current #GtkReliefStyle
 */
GtkReliefStyle
gtk_button_get_relief (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), GTK_RELIEF_NORMAL);

  return button->priv->relief;
}

static void
gtk_button_realize (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_get_allocation (widget, &allocation);

  gtk_widget_set_realized (widget, TRUE);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, window);
  g_object_ref (window);

  priv->event_window = gdk_window_new (window,
                                       &attributes, attributes_mask);
  gdk_window_set_user_data (priv->event_window, button);
}

static void
gtk_button_unrealize (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->activate_timeout)
    gtk_button_finish_activate (button, FALSE);

  if (priv->event_window)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (gtk_button_parent_class)->unrealize (widget);
}

static void
gtk_button_map (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  GTK_WIDGET_CLASS (gtk_button_parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show (priv->event_window);
}

static void
gtk_button_unmap (GtkWidget *widget)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (gtk_button_parent_class)->unmap (widget);
}

static void
gtk_button_update_image_spacing (GtkButton       *button,
                                 GtkStyleContext *context)
{
  GtkButtonPrivate *priv = button->priv;
  GtkWidget *child; 
  gint spacing;

  /* Keep in sync with gtk_button_construct_child,
   * we only want to update the spacing if the box 
   * was constructed there.
   */
  if (!priv->constructed || !priv->image)
    return;

  child = gtk_bin_get_child (GTK_BIN (button));
  if (GTK_IS_ALIGNMENT (child))
    {
      child = gtk_bin_get_child (GTK_BIN (child));
      if (GTK_IS_BOX (child))
        {
          gtk_style_context_get_style (context,
                                       "image-spacing", &spacing,
                                       NULL);

          gtk_box_set_spacing (GTK_BOX (child), spacing);
        }
    }
}

static void
gtk_button_style_updated (GtkWidget *widget)
{
  GtkStyleContext *context;

  GTK_WIDGET_CLASS (gtk_button_parent_class)->style_updated (widget);

  context = gtk_widget_get_style_context (widget);

  gtk_button_update_image_spacing (GTK_BUTTON (widget), context);
}

static void
gtk_button_get_props (GtkButton *button,
		      GtkBorder *default_border,
		      GtkBorder *default_outside_border,
                      GtkBorder *inner_border,
                      GtkBorder *padding,
		      gboolean  *interior_focus)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder *tmp_border;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  state = gtk_style_context_get_state (context);

  if (default_border)
    {
      gtk_style_context_get_style (context,
                                   "default-border", &tmp_border,
                                   NULL);

      if (tmp_border)
	{
	  *default_border = *tmp_border;
	  gtk_border_free (tmp_border);
	}
      else
	*default_border = default_default_border;
    }

  if (default_outside_border)
    {
      gtk_style_context_get_style (context,
                                   "default-outside-border", &tmp_border,
                                   NULL);

      if (tmp_border)
	{
	  *default_outside_border = *tmp_border;
	  gtk_border_free (tmp_border);
	}
      else
	*default_outside_border = default_default_outside_border;
    }

  if (inner_border)
    {
      gtk_style_context_get_style (context,
                                   "inner-border", &tmp_border,
                                   NULL);

      if (tmp_border)
	{
	  *inner_border = *tmp_border;
	  gtk_border_free (tmp_border);
	}
      else
	*inner_border = default_inner_border;
    }

  if (interior_focus)
    {
      gtk_style_context_get_style (context,
                                   "interior-focus", interior_focus,
                                   NULL);
    }

  if (padding)
    gtk_style_context_get_padding (context, state, padding);
}

static void
gtk_button_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;
  GtkAllocation child_allocation;
  GtkStyleContext *context;
  GtkWidget *child;
  GtkBorder default_border;
  GtkBorder inner_border;
  GtkBorder padding;
  gint focus_width;
  gint focus_pad;

  context = gtk_widget_get_style_context (widget);

  gtk_button_get_props (button, &default_border, NULL, &inner_border, &padding, NULL);
  gtk_style_context_get_style (context,
                              "focus-line-width", &focus_width,
                              "focus-padding", &focus_pad,
                              NULL);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (priv->event_window,
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  child = gtk_bin_get_child (GTK_BIN (button));
  if (child && gtk_widget_get_visible (child))
    {
      child_allocation.x = allocation->x + inner_border.left + padding.left;
      child_allocation.y = allocation->y + inner_border.top + padding.top;

      child_allocation.width =
	allocation->width -
        (padding.left + padding.right) -
	inner_border.left -
	inner_border.right;

      child_allocation.height = 
	allocation->height -
        (padding.top + padding.bottom) -
	inner_border.top -
	inner_border.bottom;

      if (gtk_widget_get_can_default (GTK_WIDGET (button)))
	{
	  child_allocation.x += default_border.left;
	  child_allocation.y += default_border.top;
	  child_allocation.width =  child_allocation.width - default_border.left - default_border.right;
	  child_allocation.height = child_allocation.height - default_border.top - default_border.bottom;
	}

      if (gtk_widget_get_can_focus (GTK_WIDGET (button)))
	{
	  child_allocation.x += focus_width + focus_pad;
	  child_allocation.y += focus_width + focus_pad;
	  child_allocation.width =  child_allocation.width - (focus_width + focus_pad) * 2;
	  child_allocation.height = child_allocation.height - (focus_width + focus_pad) * 2;
	}

      if (priv->depressed)
	{
	  gint child_displacement_x;
	  gint child_displacement_y;

          gtk_style_context_get_style (context,
                                       "child-displacement-x", &child_displacement_x,
                                       "child-displacement-y", &child_displacement_y,
                                       NULL);
	  child_allocation.x += child_displacement_x;
	  child_allocation.y += child_displacement_y;
	}

      child_allocation.width  = MAX (1, child_allocation.width);
      child_allocation.height = MAX (1, child_allocation.height);

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

void
_gtk_button_paint (GtkButton          *button,
		   cairo_t            *cr,
                   int                 width,
                   int                 height,
		   GtkStateFlags       state)
{
  GtkButtonPrivate *priv = button->priv;
  GtkWidget *widget;
  gint x, y;
  GtkBorder default_border;
  GtkBorder default_outside_border;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  GtkAllocation allocation;
  GtkStyleContext *context;
  gboolean draw_focus;

  widget = GTK_WIDGET (button);
  context = gtk_widget_get_style_context (widget);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);

  gtk_button_get_props (button, &default_border, &default_outside_border, NULL, NULL, &interior_focus);
  gtk_style_context_get_style (context,
                               "focus-line-width", &focus_width,
                               "focus-padding", &focus_pad,
                               NULL);

  gtk_widget_get_allocation (widget, &allocation);

  x = 0;
  y = 0;

  if (gtk_widget_has_default (widget) &&
      priv->relief == GTK_RELIEF_NORMAL)
    {
      x += default_border.left;
      y += default_border.top;
      width -= default_border.left + default_border.right;
      height -= default_border.top + default_border.bottom;

      gtk_style_context_add_class (context, GTK_STYLE_CLASS_DEFAULT);
    }
  else if (gtk_widget_get_can_default (widget))
    {
      x += default_outside_border.left;
      y += default_outside_border.top;
      width -= default_outside_border.left + default_outside_border.right;
      height -= default_outside_border.top + default_outside_border.bottom;
    }

  draw_focus = gtk_widget_has_visible_focus (widget);


  if (!interior_focus && draw_focus)
    {
      x += focus_width + focus_pad;
      y += focus_width + focus_pad;
      width -= 2 * (focus_width + focus_pad);
      height -= 2 * (focus_width + focus_pad);
    }

  if (priv->relief != GTK_RELIEF_NONE || priv->depressed ||
      state & GTK_STATE_FLAG_PRELIGHT)
    {
      gtk_render_background (context, cr,
			     x, y, width, height);
      gtk_render_frame (context, cr,
			x, y, width, height);
    }

  if (draw_focus)
    {
      gint child_displacement_x;
      gint child_displacement_y;
      gboolean displace_focus;
      GtkBorder border;

      gtk_style_context_get_style (context,
                                   "child-displacement-y", &child_displacement_y,
                                   "child-displacement-x", &child_displacement_x,
                                   "displace-focus", &displace_focus,
                                   NULL);
      gtk_style_context_get_border (context, state, &border);

      if (interior_focus)
        {
          x += border.left + focus_pad;
          y += border.top + focus_pad;
          width -= (2 * focus_pad) + border.left + border.right;
          height -=  (2 * focus_pad) + border.top + border.bottom;
        }
      else
        {
          x -= focus_width + focus_pad;
          y -= focus_width + focus_pad;
          width += 2 * (focus_width + focus_pad);
          height += 2 * (focus_width + focus_pad);
        }

      if (priv->depressed && displace_focus)
        {
          x += child_displacement_x;
          y += child_displacement_y;
        }

      gtk_render_focus (context, cr, x, y, width, height);
    }

  gtk_style_context_restore (context);
}

static gboolean
gtk_button_draw (GtkWidget *widget,
		 cairo_t   *cr)
{
  GtkButton *button = GTK_BUTTON (widget);

  _gtk_button_paint (button, cr, 
                     gtk_widget_get_allocated_width (widget),
                     gtk_widget_get_allocated_height (widget),
                     gtk_widget_get_state_flags (widget));

  GTK_WIDGET_CLASS (gtk_button_parent_class)->draw (widget, cr);

  return FALSE;
}

static gboolean
gtk_button_button_press (GtkWidget      *widget,
			 GdkEventButton *event)
{
  GtkButton *button;
  GtkButtonPrivate *priv;

  if (event->type == GDK_BUTTON_PRESS)
    {
      button = GTK_BUTTON (widget);
      priv = button->priv;

      if (priv->focus_on_click && !gtk_widget_has_focus (widget))
	gtk_widget_grab_focus (widget);

      if (event->button == 1)
	gtk_button_pressed (button);
    }

  return TRUE;
}

static gboolean
gtk_button_button_release (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GtkButton *button;

  if (event->button == 1)
    {
      button = GTK_BUTTON (widget);
      gtk_button_released (button);
    }

  return TRUE;
}

static gboolean
gtk_button_grab_broken (GtkWidget          *widget,
			GdkEventGrabBroken *event)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;
  gboolean save_in;
  
  /* Simulate a button release without the pointer in the button */
  if (priv->button_down)
    {
      save_in = priv->in_button;
      priv->in_button = FALSE;
      gtk_button_released (button);
      if (save_in != priv->in_button)
	{
	  priv->in_button = save_in;
	  gtk_button_update_state (button);
	}
    }

  return TRUE;
}

static gboolean
gtk_button_key_release (GtkWidget   *widget,
			GdkEventKey *event)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (priv->activate_timeout)
    {
      gtk_button_finish_activate (button, TRUE);
      return TRUE;
    }
  else if (GTK_WIDGET_CLASS (gtk_button_parent_class)->key_release_event)
    return GTK_WIDGET_CLASS (gtk_button_parent_class)->key_release_event (widget, event);
  else
    return FALSE;
}

static gboolean
gtk_button_enter_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if ((event->window == button->priv->event_window) &&
      (event->detail != GDK_NOTIFY_INFERIOR))
    {
      priv->in_button = TRUE;
      gtk_button_enter (button);
    }

  return FALSE;
}

static gboolean
gtk_button_leave_notify (GtkWidget        *widget,
			 GdkEventCrossing *event)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if ((event->window == button->priv->event_window) &&
      (event->detail != GDK_NOTIFY_INFERIOR) &&
      (gtk_widget_get_sensitive (widget)))
    {
      priv->in_button = FALSE;
      gtk_button_leave (button);
    }

  return FALSE;
}

static void
gtk_real_button_pressed (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->activate_timeout)
    return;

  priv->button_down = TRUE;
  gtk_button_update_state (button);
}

static void
gtk_real_button_released (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->button_down)
    {
      priv->button_down = FALSE;

      if (priv->activate_timeout)
	return;

      if (priv->in_button)
	gtk_button_clicked (button);

      gtk_button_update_state (button);
    }
}

static void 
gtk_real_button_clicked (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->action)
    gtk_action_activate (priv->action);
}

static gboolean
button_activate_timeout (gpointer data)
{
  gtk_button_finish_activate (data, TRUE);

  return FALSE;
}

static void
gtk_real_button_activate (GtkButton *button)
{
  GtkWidget *widget = GTK_WIDGET (button);
  GtkButtonPrivate *priv = button->priv;
  GdkDevice *device;
  guint32 time;

  device = gtk_get_current_event_device ();

  if (device && gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  if (gtk_widget_get_realized (widget) && !priv->activate_timeout)
    {
      time = gtk_get_current_event_time ();

      /* bgo#626336 - Only grab if we have a device (from an event), not if we
       * were activated programmatically when no event is available.
       */
      if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
	{
	  if (gdk_device_grab (device, priv->event_window,
			       GDK_OWNERSHIP_WINDOW, TRUE,
			       GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
			       NULL, time) == GDK_GRAB_SUCCESS)
	    {
	      gtk_device_grab_add (widget, device, TRUE);
	      priv->grab_keyboard = device;
	      priv->grab_time = time;
	    }
	}

      priv->activate_timeout = gdk_threads_add_timeout (ACTIVATE_TIMEOUT,
						button_activate_timeout,
						button);
      priv->button_down = TRUE;
      gtk_button_update_state (button);
      gtk_widget_queue_draw (GTK_WIDGET (button));
    }
}

static void
gtk_button_finish_activate (GtkButton *button,
			    gboolean   do_it)
{
  GtkWidget *widget = GTK_WIDGET (button);
  GtkButtonPrivate *priv = button->priv;

  g_source_remove (priv->activate_timeout);
  priv->activate_timeout = 0;

  if (priv->grab_keyboard)
    {
      gdk_device_ungrab (priv->grab_keyboard, priv->grab_time);
      gtk_device_grab_remove (widget, priv->grab_keyboard);
      priv->grab_keyboard = NULL;
    }

  priv->button_down = FALSE;

  gtk_button_update_state (button);
  gtk_widget_queue_draw (GTK_WIDGET (button));

  if (do_it)
    gtk_button_clicked (button);
}


static void
gtk_button_get_size (GtkWidget      *widget,
		     GtkOrientation  orientation,
		     gint           *minimum_size,
		     gint           *natural_size)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkStyleContext *context;
  GtkWidget *child;
  GtkBorder default_border;
  GtkBorder inner_border;
  GtkBorder padding;
  gint focus_width;
  gint focus_pad;
  gint minimum, natural;

  context = gtk_widget_get_style_context (widget);

  gtk_button_get_props (button, &default_border, NULL, &inner_border, &padding, NULL);
  gtk_style_context_get_style (context,
                               "focus-line-width", &focus_width,
                               "focus-padding", &focus_pad,
                               NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      minimum = inner_border.left + inner_border.right + padding.left + padding.right;

      if (gtk_widget_get_can_default (GTK_WIDGET (widget)))
	minimum += default_border.left + default_border.right;
    }
  else
    {
      minimum = inner_border.top + inner_border.bottom + padding.top + padding.bottom;

      if (gtk_widget_get_can_default (GTK_WIDGET (widget)))
	minimum += default_border.top + default_border.bottom;
    }  

  minimum += 2 * (focus_width + focus_pad);
  natural = minimum;

  if ((child = gtk_bin_get_child (GTK_BIN (button))) && 
      gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
	gtk_widget_get_preferred_width (child, &child_min, &child_nat);
      else
	gtk_widget_get_preferred_height (child, &child_min, &child_nat);

      minimum += child_min;
      natural += child_nat;
    }

  if (minimum_size)
    *minimum_size = minimum;

  if (natural_size)
    *natural_size = natural;
}

static void 
gtk_button_get_preferred_width (GtkWidget *widget,
                                gint      *minimum_size,
                                gint      *natural_size)
{
  gtk_button_get_size (widget, GTK_ORIENTATION_HORIZONTAL, minimum_size, natural_size);
}

static void 
gtk_button_get_preferred_height (GtkWidget *widget,
                                 gint      *minimum_size,
                                 gint      *natural_size)
{
  gtk_button_get_size (widget, GTK_ORIENTATION_VERTICAL, minimum_size, natural_size);
}

/**
 * gtk_button_set_label:
 * @button: a #GtkButton
 * @label: a string
 *
 * Sets the text of the label of the button to @str. This text is
 * also used to select the stock item if gtk_button_set_use_stock()
 * is used.
 *
 * This will also clear any previously set labels.
 **/
void
gtk_button_set_label (GtkButton   *button,
		      const gchar *label)
{
  GtkButtonPrivate *priv;
  gchar *new_label;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  new_label = g_strdup (label);
  g_free (priv->label_text);
  priv->label_text = new_label;

  gtk_button_construct_child (button);
  
  g_object_notify (G_OBJECT (button), "label");
}

/**
 * gtk_button_get_label:
 * @button: a #GtkButton
 *
 * Fetches the text from the label of the button, as set by
 * gtk_button_set_label(). If the label text has not 
 * been set the return value will be %NULL. This will be the 
 * case if you create an empty button with gtk_button_new() to 
 * use as a container.
 *
 * Return value: The text of the label widget. This string is owned
 * by the widget and must not be modified or freed.
 **/
const gchar *
gtk_button_get_label (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  return button->priv->label_text;
}

/**
 * gtk_button_set_use_underline:
 * @button: a #GtkButton
 * @use_underline: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text of the button label indicates
 * the next character should be used for the mnemonic accelerator key.
 */
void
gtk_button_set_use_underline (GtkButton *button,
			      gboolean   use_underline)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  use_underline = use_underline != FALSE;

  if (use_underline != priv->use_underline)
    {
      priv->use_underline = use_underline;

      gtk_button_construct_child (button);
      
      g_object_notify (G_OBJECT (button), "use-underline");
    }
}

/**
 * gtk_button_get_use_underline:
 * @button: a #GtkButton
 *
 * Returns whether an embedded underline in the button label indicates a
 * mnemonic. See gtk_button_set_use_underline ().
 *
 * Return value: %TRUE if an embedded underline in the button label
 *               indicates the mnemonic accelerator keys.
 **/
gboolean
gtk_button_get_use_underline (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return button->priv->use_underline;
}

/**
 * gtk_button_set_use_stock:
 * @button: a #GtkButton
 * @use_stock: %TRUE if the button should use a stock item
 *
 * If %TRUE, the label set on the button is used as a
 * stock id to select the stock item for the button.
 */
void
gtk_button_set_use_stock (GtkButton *button,
			  gboolean   use_stock)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  use_stock = use_stock != FALSE;

  if (use_stock != priv->use_stock)
    {
      priv->use_stock = use_stock;

      gtk_button_construct_child (button);
      
      g_object_notify (G_OBJECT (button), "use-stock");
    }
}

/**
 * gtk_button_get_use_stock:
 * @button: a #GtkButton
 *
 * Returns whether the button label is a stock item.
 *
 * Return value: %TRUE if the button label is used to
 *               select a stock item instead of being
 *               used directly as the label text.
 */
gboolean
gtk_button_get_use_stock (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);

  return button->priv->use_stock;
}

/**
 * gtk_button_set_focus_on_click:
 * @button: a #GtkButton
 * @focus_on_click: whether the button grabs focus when clicked with the mouse
 *
 * Sets whether the button will grab focus when it is clicked with the mouse.
 * Making mouse clicks not grab focus is useful in places like toolbars where
 * you don't want the keyboard focus removed from the main area of the
 * application.
 *
 * Since: 2.4
 **/
void
gtk_button_set_focus_on_click (GtkButton *button,
			       gboolean   focus_on_click)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));

  priv = button->priv;

  focus_on_click = focus_on_click != FALSE;

  if (priv->focus_on_click != focus_on_click)
    {
      priv->focus_on_click = focus_on_click;
      
      g_object_notify (G_OBJECT (button), "focus-on-click");
    }
}

/**
 * gtk_button_get_focus_on_click:
 * @button: a #GtkButton
 *
 * Returns whether the button grabs focus when it is clicked with the mouse.
 * See gtk_button_set_focus_on_click().
 *
 * Return value: %TRUE if the button grabs focus when it is clicked with
 *               the mouse.
 *
 * Since: 2.4
 **/
gboolean
gtk_button_get_focus_on_click (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
  
  return button->priv->focus_on_click;
}

/**
 * gtk_button_set_alignment:
 * @button: a #GtkButton
 * @xalign: the horizontal position of the child, 0.0 is left aligned, 
 *   1.0 is right aligned
 * @yalign: the vertical position of the child, 0.0 is top aligned, 
 *   1.0 is bottom aligned
 *
 * Sets the alignment of the child. This property has no effect unless 
 * the child is a #GtkMisc or a #GtkAlignment.
 *
 * Since: 2.4
 */
void
gtk_button_set_alignment (GtkButton *button,
			  gfloat     xalign,
			  gfloat     yalign)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));
  
  priv = button->priv;

  priv->xalign = xalign;
  priv->yalign = yalign;
  priv->align_set = 1;

  maybe_set_alignment (button, gtk_bin_get_child (GTK_BIN (button)));

  g_object_freeze_notify (G_OBJECT (button));
  g_object_notify (G_OBJECT (button), "xalign");
  g_object_notify (G_OBJECT (button), "yalign");
  g_object_thaw_notify (G_OBJECT (button));
}

/**
 * gtk_button_get_alignment:
 * @button: a #GtkButton
 * @xalign: (out): return location for horizontal alignment
 * @yalign: (out): return location for vertical alignment
 *
 * Gets the alignment of the child in the button.
 *
 * Since: 2.4
 */
void
gtk_button_get_alignment (GtkButton *button,
			  gfloat    *xalign,
			  gfloat    *yalign)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));
  
  priv = button->priv;
 
  if (xalign) 
    *xalign = priv->xalign;

  if (yalign)
    *yalign = priv->yalign;
}

/**
 * _gtk_button_set_depressed:
 * @button: a #GtkButton
 * @depressed: %TRUE if the button should be drawn with a recessed shadow.
 *
 * Sets whether the button is currently drawn as down or not. This is 
 * purely a visual setting, and is meant only for use by derived widgets
 * such as #GtkToggleButton.
 **/
void
_gtk_button_set_depressed (GtkButton *button,
			   gboolean   depressed)
{
  GtkWidget *widget = GTK_WIDGET (button);
  GtkButtonPrivate *priv = button->priv;

  depressed = depressed != FALSE;

  if (depressed != priv->depressed)
    {
      priv->depressed = depressed;
      gtk_widget_queue_resize (widget);
    }
}

static void
gtk_button_update_state (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;
  GtkStateFlags new_state;
  gboolean depressed;

  if (priv->activate_timeout)
    depressed = priv->depress_on_activate;
  else
    depressed = priv->in_button && priv->button_down;

  new_state = gtk_widget_get_state_flags (GTK_WIDGET (button)) &
    ~(GTK_STATE_FLAG_PRELIGHT | GTK_STATE_FLAG_ACTIVE);

  if (priv->in_button)
    new_state |= GTK_STATE_FLAG_PRELIGHT;

  if (priv->button_down || depressed)
    new_state |= GTK_STATE_FLAG_ACTIVE;

  _gtk_button_set_depressed (button, depressed);
  gtk_widget_set_state_flags (GTK_WIDGET (button), new_state, TRUE);
}

static void 
show_image_change_notify (GtkButton *button)
{
  GtkButtonPrivate *priv = button->priv;

  if (priv->image) 
    {
      if (show_image (button))
	gtk_widget_show (priv->image);
      else
	gtk_widget_hide (priv->image);
    }
}

static void
traverse_container (GtkWidget *widget,
		    gpointer   data)
{
  if (GTK_IS_BUTTON (widget))
    show_image_change_notify (GTK_BUTTON (widget));
  else if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), traverse_container, NULL);
}

static void
gtk_button_setting_changed (GtkSettings *settings)
{
  GList *list, *l;

  list = gtk_window_list_toplevels ();

  for (l = list; l; l = l->next)
    gtk_container_forall (GTK_CONTAINER (l->data), 
			  traverse_container, NULL);

  g_list_free (list);
}


static void
gtk_button_screen_changed (GtkWidget *widget,
			   GdkScreen *previous_screen)
{
  GtkButton *button;
  GtkButtonPrivate *priv;
  GtkSettings *settings;
  gulong show_image_connection;

  if (!gtk_widget_has_screen (widget))
    return;

  button = GTK_BUTTON (widget);
  priv = button->priv;

  /* If the button is being pressed while the screen changes the
    release might never occur, so we reset the state. */
  if (priv->button_down)
    {
      priv->button_down = FALSE;
      gtk_button_update_state (button);
    }

  settings = gtk_widget_get_settings (widget);

  show_image_connection = 
    g_signal_handler_find (settings, G_SIGNAL_MATCH_FUNC, 0, 0,
                           NULL, gtk_button_setting_changed, NULL);
  
  if (show_image_connection)
    return;

  g_signal_connect (settings, "notify::gtk-button-images",
                    G_CALLBACK (gtk_button_setting_changed), NULL);

  show_image_change_notify (button);
}

static void
gtk_button_state_changed (GtkWidget    *widget,
                          GtkStateType  previous_state)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;

  if (!gtk_widget_is_sensitive (widget))
    {
      priv->in_button = FALSE;
      gtk_real_button_released (button);
    }
}

static void
gtk_button_grab_notify (GtkWidget *widget,
			gboolean   was_grabbed)
{
  GtkButton *button = GTK_BUTTON (widget);
  GtkButtonPrivate *priv = button->priv;
  gboolean save_in;

  if (priv->activate_timeout &&
      priv->grab_keyboard &&
      gtk_widget_device_is_shadowed (widget, priv->grab_keyboard))
    gtk_button_finish_activate (button, FALSE);

  if (!was_grabbed)
    {
      save_in = priv->in_button;
      priv->in_button = FALSE;
      gtk_real_button_released (button);
      if (save_in != priv->in_button)
        {
          priv->in_button = save_in;
          gtk_button_update_state (button);
        }
    }
}

/**
 * gtk_button_set_image:
 * @button: a #GtkButton
 * @image: a widget to set as the image for the button
 *
 * Set the image of @button to the given widget. Note that
 * it depends on the #GtkSettings:gtk-button-images setting whether the
 * image will be displayed or not, you don't have to call
 * gtk_widget_show() on @image yourself.
 *
 * Since: 2.6
 */ 
void
gtk_button_set_image (GtkButton *button,
		      GtkWidget *image)
{
  GtkButtonPrivate *priv;
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (image == NULL || GTK_IS_WIDGET (image));

  priv = button->priv;

  if (priv->image)
    {
      parent = gtk_widget_get_parent (priv->image);
      if (parent)
        gtk_container_remove (GTK_CONTAINER (parent), priv->image);
    }

  priv->image = image;
  priv->image_is_stock = (image == NULL);

  gtk_button_construct_child (button);

  g_object_notify (G_OBJECT (button), "image");
}

/**
 * gtk_button_get_image:
 * @button: a #GtkButton
 *
 * Gets the widget that is currenty set as the image of @button.
 * This may have been explicitly set by gtk_button_set_image()
 * or constructed by gtk_button_new_from_stock().
 *
 * Return value: (transfer none): a #GtkWidget or %NULL in case there is no image
 *
 * Since: 2.6
 */
GtkWidget *
gtk_button_get_image (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);
  
  return button->priv->image;
}

/**
 * gtk_button_set_image_position:
 * @button: a #GtkButton
 * @position: the position
 *
 * Sets the position of the image relative to the text 
 * inside the button.
 *
 * Since: 2.10
 */ 
void
gtk_button_set_image_position (GtkButton       *button,
			       GtkPositionType  position)
{
  GtkButtonPrivate *priv;

  g_return_if_fail (GTK_IS_BUTTON (button));
  g_return_if_fail (position >= GTK_POS_LEFT && position <= GTK_POS_BOTTOM);
  
  priv = button->priv;

  if (priv->image_position != position)
    {
      priv->image_position = position;

      gtk_button_construct_child (button);

      g_object_notify (G_OBJECT (button), "image-position");
    }
}

/**
 * gtk_button_get_image_position:
 * @button: a #GtkButton
 *
 * Gets the position of the image relative to the text 
 * inside the button.
 *
 * Return value: the position
 *
 * Since: 2.10
 */
GtkPositionType
gtk_button_get_image_position (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), GTK_POS_LEFT);
  
  return button->priv->image_position;
}


/**
 * gtk_button_get_event_window:
 * @button: a #GtkButton
 *
 * Returns the button's event window if it is realized, %NULL otherwise.
 * This function should be rarely needed.
 *
 * Return value: (transfer none): @button's event window.
 *
 * Since: 2.22
 */
GdkWindow*
gtk_button_get_event_window (GtkButton *button)
{
  g_return_val_if_fail (GTK_IS_BUTTON (button), NULL);

  return button->priv->event_window;
}
