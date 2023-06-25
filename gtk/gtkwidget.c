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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <locale.h>

#include <gobject/gvaluecollector.h>
#include <gobject/gobjectnotifyqueue.c>
#include <cairo-gobject.h>

#include "gtkcontainer.h"
#include "gtkaccelmap.h"
#include "gtkclipboard.h"
#include "gtkiconfactory.h"
#include "gtkintl.h"
#include "gtkmainprivate.h"
#include "gtkmarshalers.h"
#include "gtkselectionprivate.h"
#include "gtksettingsprivate.h"
#include "gtksizegroup-private.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkbindings.h"
#include "gtkprivate.h"
#include "gtkaccessible.h"
#include "gtktooltip.h"
#include "gtkinvisible.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtksymboliccolor.h"
#include "gtkcssprovider.h"
#include "gtkanimationdescription.h"
#include "gtkmodifierstyle.h"
#include "gtkversion.h"
#include "gtkdebug.h"
#include "gtkplug.h"
#include "gtktypebuiltins.h"
#include "a11y/gtkwidgetaccessible.h"

/**
 * SECTION:gtkwidget
 * @Short_description: Base class for all widgets
 * @Title: GtkWidget
 *
 * GtkWidget is the base class all widgets in GTK+ derive from. It manages the
 * widget lifecycle, states and style.
 *
 * <refsect2 id="geometry-management">
 * <title>Height-for-width Geometry Management</title>
 * <para>
 * GTK+ uses a height-for-width (and width-for-height) geometry management
 * system. Height-for-width means that a widget can change how much
 * vertical space it needs, depending on the amount of horizontal space
 * that it is given (and similar for width-for-height). The most common
 * example is a label that reflows to fill up the available width, wraps
 * to fewer lines, and therefore needs less height.
 *
 * Height-for-width geometry management is implemented in GTK+ by way
 * of five virtual methods:
 * <itemizedlist>
 *   <listitem>#GtkWidgetClass.get_request_mode()</listitem>
 *   <listitem>#GtkWidgetClass.get_preferred_width()</listitem>
 *   <listitem>#GtkWidgetClass.get_preferred_height()</listitem>
 *   <listitem>#GtkWidgetClass.get_preferred_height_for_width()</listitem>
 *   <listitem>#GtkWidgetClass.get_preferred_width_for_height()</listitem>
 * </itemizedlist>
 *
 * There are some important things to keep in mind when implementing
 * height-for-width and when using it in container implementations.
 *
 * The geometry management system will query a widget hierarchy in
 * only one orientation at a time. When widgets are initially queried
 * for their minimum sizes it is generally done in two initial passes
 * in the #GtkSizeRequestMode chosen by the toplevel.
 *
 * For example, when queried in the normal
 * %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH mode:
 * First, the default minimum and natural width for each widget
 * in the interface will be computed using gtk_widget_get_preferred_width().
 * Because the preferred widths for each container depend on the preferred
 * widths of their children, this information propagates up the hierarchy,
 * and finally a minimum and natural width is determined for the entire
 * toplevel. Next, the toplevel will use the minimum width to query for the
 * minimum height contextual to that width using
 * gtk_widget_get_preferred_height_for_width(), which will also be a highly
 * recursive operation. The minimum height for the minimum width is normally
 * used to set the minimum size constraint on the toplevel
 * (unless gtk_window_set_geometry_hints() is explicitly used instead).
 *
 * After the toplevel window has initially requested its size in both
 * dimensions it can go on to allocate itself a reasonable size (or a size
 * previously specified with gtk_window_set_default_size()). During the
 * recursive allocation process it's important to note that request cycles
 * will be recursively executed while container widgets allocate their children.
 * Each container widget, once allocated a size, will go on to first share the
 * space in one orientation among its children and then request each child's
 * height for its target allocated width or its width for allocated height,
 * depending. In this way a #GtkWidget will typically be requested its size
 * a number of times before actually being allocated a size. The size a
 * widget is finally allocated can of course differ from the size it has
 * requested. For this reason, #GtkWidget caches a  small number of results
 * to avoid re-querying for the same sizes in one allocation cycle.
 *
 * See <link linkend="container-geometry-management">GtkContainer's
 * geometry management section</link>
 * to learn more about how height-for-width allocations are performed
 * by container widgets.
 *
 * If a widget does move content around to intelligently use up the
 * allocated size then it must support the request in both
 * #GtkSizeRequestModes even if the widget in question only
 * trades sizes in a single orientation.
 *
 * For instance, a #GtkLabel that does height-for-width word wrapping
 * will not expect to have #GtkWidgetClass.get_preferred_height() called
 * because that call is specific to a width-for-height request. In this
 * case the label must return the height required for its own minimum
 * possible width. By following this rule any widget that handles
 * height-for-width or width-for-height requests will always be allocated
 * at least enough space to fit its own content.
 *
 * Here are some examples of how a %GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH widget
 * generally deals with width-for-height requests, for #GtkWidgetClass.get_preferred_height()
 * it will do:
 * <programlisting><![CDATA[
 * static void
 * foo_widget_get_preferred_height (GtkWidget *widget, gint *min_height, gint *nat_height)
 * {
 *    if (i_am_in_height_for_width_mode)
 *      {
 *        gint min_width;
 *
 *        GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_width, NULL);
 *        GTK_WIDGET_GET_CLASS (widget)->get_preferred_height_for_width (widget, min_width,
 *                                                                      min_height, nat_height);
 *      }
 *    else
 *      {
 *         ... some widgets do both. For instance, if a GtkLabel is rotated to 90 degrees
 *         it will return the minimum and natural height for the rotated label here.
 *      }
 * }
 * ]]></programlisting>
 *
 * And in #GtkWidgetClass.get_preferred_width_for_height() it will simply return
 * the minimum and natural width:
 *
 * <programlisting><![CDATA[
 * static void
 * foo_widget_get_preferred_width_for_height (GtkWidget *widget, gint for_height,
 *                                            gint *min_width, gint *nat_width)
 * {
 *    if (i_am_in_height_for_width_mode)
 *      {
 *        GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, min_width, nat_width);
 *      }
 *    else
 *      {
 *         ... again if a widget is sometimes operating in width-for-height mode
 *         (like a rotated GtkLabel) it can go ahead and do its real width for
 *         height calculation here.
 *      }
 * }
 * ]]></programlisting>
 *
 * Often a widget needs to get its own request during size request or
 * allocation. For example, when computing height it may need to also
 * compute width. Or when deciding how to use an allocation, the widget
 * may need to know its natural size. In these cases, the widget should
 * be careful to call its virtual methods directly, like this:
 * <example>
 *   <title>Widget calling its own size request method.</title>
 *   <programlisting>
 * GTK_WIDGET_GET_CLASS(widget)-&gt;get_preferred_width (widget),
 *                                  &min, &natural);
 *   </programlisting>
 * </example>
 *
 * It will not work to use the wrapper functions, such as
 * gtk_widget_get_preferred_width() inside your own size request
 * implementation. These return a request adjusted by #GtkSizeGroup
 * and by the #GtkWidgetClass.adjust_size_request() virtual method. If a
 * widget used the wrappers inside its virtual method implementations,
 * then the adjustments (such as widget margins) would be applied
 * twice. GTK+ therefore does not allow this and will warn if you try
 * to do it.
 *
 * Of course if you are getting the size request for
 * <emphasis>another</emphasis> widget, such as a child of a
 * container, you <emphasis>must</emphasis> use the wrapper APIs.
 * Otherwise, you would not properly consider widget margins,
 * #GtkSizeGroup, and so forth.
 * </para>
 * </refsect2>
 * <refsect2 id="style-properties">
 * <title>Style Properties</title>
 * <para>
 * <structname>GtkWidget</structname> introduces <firstterm>style
 * properties</firstterm> - these are basically object properties that are stored
 * not on the object, but in the style object associated to the widget. Style
 * properties are set in <link linkend="gtk-Resource-Files">resource files</link>.
 * This mechanism is used for configuring such things as the location of the
 * scrollbar arrows through the theme, giving theme authors more control over the
 * look of applications without the need to write a theme engine in C.
 * </para>
 * <para>
 * Use gtk_widget_class_install_style_property() to install style properties for
 * a widget class, gtk_widget_class_find_style_property() or
 * gtk_widget_class_list_style_properties() to get information about existing
 * style properties and gtk_widget_style_get_property(), gtk_widget_style_get() or
 * gtk_widget_style_get_valist() to obtain the value of a style property.
 * </para>
 * </refsect2>
 * <refsect2 id="GtkWidget-BUILDER-UI">
 * <title>GtkWidget as GtkBuildable</title>
 * <para>
 * The GtkWidget implementation of the GtkBuildable interface supports a
 * custom &lt;accelerator&gt; element, which has attributes named key,
 * modifiers and signal and allows to specify accelerators.
 * </para>
 * <example>
 * <title>A UI definition fragment specifying an accelerator</title>
 * <programlisting><![CDATA[
 * <object class="GtkButton">
 *   <accelerator key="q" modifiers="GDK_CONTROL_MASK" signal="clicked"/>
 * </object>
 * ]]></programlisting>
 * </example>
 * <para>
 * In addition to accelerators, <structname>GtkWidget</structname> also support a
 * custom &lt;accessible&gt; element, which supports actions and relations.
 * Properties on the accessible implementation of an object can be set by accessing the
 * internal child "accessible" of a <structname>GtkWidget</structname>.
 * </para>
 * <example>
 * <title>A UI definition fragment specifying an accessible</title>
 * <programlisting><![CDATA[
 * <object class="GtkButton" id="label1"/>
 *   <property name="label">I am a Label for a Button</property>
 * </object>
 * <object class="GtkButton" id="button1">
 *   <accessibility>
 *     <action action_name="click" translatable="yes">Click the button.</action>
 *     <relation target="label1" type="labelled-by"/>
 *   </accessibility>
 *   <child internal-child="accessible">
 *     <object class="AtkObject" id="a11y-button1">
 *       <property name="AtkObject::name">Clickable Button</property>
 *     </object>
 *   </child>
 * </object>
 * ]]></programlisting>
 * </example>
 * <para>
 * Finally, GtkWidget allows style information such as style classes to
 * be associated with widgets, using the custom &lt;style&gt; element:
 * <example>
 * <title>A UI definition fragment specifying an style class</title>
 * <programlisting><![CDATA[
 * <object class="GtkButton" id="button1">
 *   <style>
 *     <class name="my-special-button-class"/>
 *     <class name="dark-button"/>
 *   </style>
 * </object>
 * ]]></programlisting>
 * </example>
 * </para>
 * </refsect2>
 */

/* Add flags here that should not be propagated to children. By default,
 * all flags will be set on children (think prelight or active), but we
 * might want to not do this for some.
 */
#define GTK_STATE_FLAGS_DONT_PROPAGATE (GTK_STATE_FLAG_FOCUSED)
#define GTK_STATE_FLAGS_DO_PROPAGATE (~GTK_STATE_FLAGS_DONT_PROPAGATE)

#define WIDGET_CLASS(w)	 GTK_WIDGET_GET_CLASS (w)
#define	INIT_PATH_SIZE	(512)

struct _GtkWidgetPrivate
{
  /* The state of the widget. There are actually only
   * 5 widget states (defined in "gtkenums.h")
   * so 3 bits.
   */
  guint state_flags : 6;

  guint direction             : 2;

  guint in_destruction        : 1;
  guint toplevel              : 1;
  guint anchored              : 1;
  guint composite_child       : 1;
  guint no_window             : 1;
  guint realized              : 1;
  guint mapped                : 1;
  guint visible               : 1;
  guint sensitive             : 1;
  guint can_focus             : 1;
  guint has_focus             : 1;
  guint can_default           : 1;
  guint has_default           : 1;
  guint receives_default      : 1;
  guint has_grab              : 1;
  guint shadowed              : 1;
  guint rc_style              : 1;
  guint style_update_pending  : 1;
  guint app_paintable         : 1;
  guint double_buffered       : 1;
  guint redraw_on_alloc       : 1;
  guint no_show_all           : 1;
  guint child_visible         : 1;
  guint multidevice           : 1;
  guint has_shape_mask        : 1;
  guint in_reparent           : 1;

  /* Queue-resize related flags */
  guint resize_pending        : 1;
  guint alloc_needed          : 1;
  guint width_request_needed  : 1;
  guint height_request_needed : 1;

  /* Expand-related flags */
  guint need_compute_expand   : 1; /* Need to recompute computed_[hv]_expand */
  guint computed_hexpand      : 1; /* computed results (composite of child flags) */
  guint computed_vexpand      : 1;
  guint hexpand               : 1; /* application-forced expand */
  guint vexpand               : 1;
  guint hexpand_set           : 1; /* whether to use application-forced  */
  guint vexpand_set           : 1; /* instead of computing from children */

  /* SizeGroup related flags */
  guint sizegroup_visited     : 1;
  guint sizegroup_bumping     : 1;
  guint have_size_groups      : 1;

  /* The widget's name. If the widget does not have a name
   * (the name is NULL), then its name (as returned by
   * "gtk_widget_get_name") is its class's name.
   * Among other things, the widget name is used to determine
   * the style to use for a widget.
   */
  gchar *name;

  /* The style for the widget. The style contains the
   * colors the widget should be drawn in for each state
   * along with graphics contexts used to draw with and
   * the font to use for text.
   */
  GtkStyle *style;
  GtkStyleContext *context;

  /* Widget's path for styling */
  GtkWidgetPath *path;

  /* The widget's allocated size */
  GtkAllocation allocation;

  /* The widget's requested sizes */
  SizeRequestCache requests;

  /* The widget's window or its parent window if it does
   * not have a window. (Which will be indicated by the
   * GTK_NO_WINDOW flag being set).
   */
  GdkWindow *window;

  /* The widget's parent */
  GtkWidget *parent;

#ifdef G_ENABLE_DEBUG
  /* Number of gtk_widget_push_verify_invariants () */
  guint verifying_invariants_count;
#endif /* G_ENABLE_DEBUG */
};

struct _GtkWidgetClassPrivate
{
  GType accessible_type;
  AtkRole accessible_role;
};

enum {
  DESTROY,
  SHOW,
  HIDE,
  MAP,
  UNMAP,
  REALIZE,
  UNREALIZE,
  SIZE_ALLOCATE,
  STATE_FLAGS_CHANGED,
  STATE_CHANGED,
  PARENT_SET,
  HIERARCHY_CHANGED,
  STYLE_SET,
  DIRECTION_CHANGED,
  GRAB_NOTIFY,
  CHILD_NOTIFY,
  DRAW,
  MNEMONIC_ACTIVATE,
  GRAB_FOCUS,
  FOCUS,
  MOVE_FOCUS,
  KEYNAV_FAILED,
  EVENT,
  EVENT_AFTER,
  BUTTON_PRESS_EVENT,
  BUTTON_RELEASE_EVENT,
  SCROLL_EVENT,
  MOTION_NOTIFY_EVENT,
  DELETE_EVENT,
  DESTROY_EVENT,
  KEY_PRESS_EVENT,
  KEY_RELEASE_EVENT,
  ENTER_NOTIFY_EVENT,
  LEAVE_NOTIFY_EVENT,
  CONFIGURE_EVENT,
  FOCUS_IN_EVENT,
  FOCUS_OUT_EVENT,
  MAP_EVENT,
  UNMAP_EVENT,
  PROPERTY_NOTIFY_EVENT,
  SELECTION_CLEAR_EVENT,
  SELECTION_REQUEST_EVENT,
  SELECTION_NOTIFY_EVENT,
  SELECTION_GET,
  SELECTION_RECEIVED,
  PROXIMITY_IN_EVENT,
  PROXIMITY_OUT_EVENT,
  VISIBILITY_NOTIFY_EVENT,
  WINDOW_STATE_EVENT,
  DAMAGE_EVENT,
  GRAB_BROKEN_EVENT,
  DRAG_BEGIN,
  DRAG_END,
  DRAG_DATA_DELETE,
  DRAG_LEAVE,
  DRAG_MOTION,
  DRAG_DROP,
  DRAG_DATA_GET,
  DRAG_DATA_RECEIVED,
  POPUP_MENU,
  SHOW_HELP,
  ACCEL_CLOSURES_CHANGED,
  SCREEN_CHANGED,
  CAN_ACTIVATE_ACCEL,
  COMPOSITED_CHANGED,
  QUERY_TOOLTIP,
  DRAG_FAILED,
  STYLE_UPDATED,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_NAME,
  PROP_PARENT,
  PROP_WIDTH_REQUEST,
  PROP_HEIGHT_REQUEST,
  PROP_VISIBLE,
  PROP_SENSITIVE,
  PROP_APP_PAINTABLE,
  PROP_CAN_FOCUS,
  PROP_HAS_FOCUS,
  PROP_IS_FOCUS,
  PROP_CAN_DEFAULT,
  PROP_HAS_DEFAULT,
  PROP_RECEIVES_DEFAULT,
  PROP_COMPOSITE_CHILD,
  PROP_STYLE,
  PROP_EVENTS,
  PROP_NO_SHOW_ALL,
  PROP_HAS_TOOLTIP,
  PROP_TOOLTIP_MARKUP,
  PROP_TOOLTIP_TEXT,
  PROP_WINDOW,
  PROP_DOUBLE_BUFFERED,
  PROP_HALIGN,
  PROP_VALIGN,
  PROP_MARGIN_LEFT,
  PROP_MARGIN_RIGHT,
  PROP_MARGIN_TOP,
  PROP_MARGIN_BOTTOM,
  PROP_MARGIN,
  PROP_HEXPAND,
  PROP_VEXPAND,
  PROP_HEXPAND_SET,
  PROP_VEXPAND_SET,
  PROP_EXPAND
};

typedef	struct	_GtkStateData	 GtkStateData;

enum {
  STATE_CHANGE_REPLACE,
  STATE_CHANGE_SET,
  STATE_CHANGE_UNSET
};

struct _GtkStateData
{
  guint         flags : 6;
  guint         operation : 2;
  guint		use_forall : 1;
};

/* --- prototypes --- */
static void	gtk_widget_base_class_init	(gpointer            g_class);
static void	gtk_widget_class_init		(GtkWidgetClass     *klass);
static void	gtk_widget_base_class_finalize	(GtkWidgetClass     *klass);
static void	gtk_widget_init			(GtkWidget          *widget);
static void	gtk_widget_set_property		 (GObject           *object,
						  guint              prop_id,
						  const GValue      *value,
						  GParamSpec        *pspec);
static void	gtk_widget_get_property		 (GObject           *object,
						  guint              prop_id,
						  GValue            *value,
						  GParamSpec        *pspec);
static void	gtk_widget_dispose		 (GObject	    *object);
static void	gtk_widget_real_destroy		 (GtkWidget	    *object);
static void	gtk_widget_finalize		 (GObject	    *object);
static void	gtk_widget_real_show		 (GtkWidget	    *widget);
static void	gtk_widget_real_hide		 (GtkWidget	    *widget);
static void	gtk_widget_real_map		 (GtkWidget	    *widget);
static void	gtk_widget_real_unmap		 (GtkWidget	    *widget);
static void	gtk_widget_real_realize		 (GtkWidget	    *widget);
static void	gtk_widget_real_unrealize	 (GtkWidget	    *widget);
static void	gtk_widget_real_size_allocate	 (GtkWidget	    *widget,
                                                  GtkAllocation	    *allocation);
static void	gtk_widget_real_style_set        (GtkWidget         *widget,
                                                  GtkStyle          *previous_style);
static void	gtk_widget_real_direction_changed(GtkWidget         *widget,
                                                  GtkTextDirection   previous_direction);

static void	gtk_widget_real_grab_focus	 (GtkWidget         *focus_widget);
static gboolean gtk_widget_real_query_tooltip    (GtkWidget         *widget,
						  gint               x,
						  gint               y,
						  gboolean           keyboard_tip,
						  GtkTooltip        *tooltip);
static void     gtk_widget_real_style_updated    (GtkWidget         *widget);
static gboolean gtk_widget_real_show_help        (GtkWidget         *widget,
                                                  GtkWidgetHelpType  help_type);

static void	gtk_widget_dispatch_child_properties_changed	(GtkWidget        *object,
								 guint             n_pspecs,
								 GParamSpec      **pspecs);
static gboolean		gtk_widget_real_key_press_event   	(GtkWidget        *widget,
								 GdkEventKey      *event);
static gboolean		gtk_widget_real_key_release_event 	(GtkWidget        *widget,
								 GdkEventKey      *event);
static gboolean		gtk_widget_real_focus_in_event   	 (GtkWidget       *widget,
								  GdkEventFocus   *event);
static gboolean		gtk_widget_real_focus_out_event   	(GtkWidget        *widget,
								 GdkEventFocus    *event);
static gboolean		gtk_widget_real_focus			(GtkWidget        *widget,
								 GtkDirectionType  direction);
static void             gtk_widget_real_move_focus              (GtkWidget        *widget,
                                                                 GtkDirectionType  direction);
static gboolean		gtk_widget_real_keynav_failed		(GtkWidget        *widget,
								 GtkDirectionType  direction);
#ifdef G_ENABLE_DEBUG
static void             gtk_widget_verify_invariants            (GtkWidget        *widget);
static void             gtk_widget_push_verify_invariants       (GtkWidget        *widget);
static void             gtk_widget_pop_verify_invariants        (GtkWidget        *widget);
#else
#define                 gtk_widget_verify_invariants(widget)
#define                 gtk_widget_push_verify_invariants(widget)
#define                 gtk_widget_pop_verify_invariants(widget)
#endif
static PangoContext*	gtk_widget_peek_pango_context		(GtkWidget	  *widget);
static void     	gtk_widget_update_pango_context		(GtkWidget	  *widget);
static void		gtk_widget_propagate_state		(GtkWidget	  *widget,
								 GtkStateData 	  *data);
;
static gint		gtk_widget_event_internal		(GtkWidget	  *widget,
								 GdkEvent	  *event);
static gboolean		gtk_widget_real_mnemonic_activate	(GtkWidget	  *widget,
								 gboolean	   group_cycling);
static void             gtk_widget_real_get_width               (GtkWidget        *widget,
                                                                 gint             *minimum_size,
                                                                 gint             *natural_size);
static void             gtk_widget_real_get_height              (GtkWidget        *widget,
                                                                 gint             *minimum_size,
                                                                 gint             *natural_size);
static void             gtk_widget_real_get_height_for_width    (GtkWidget        *widget,
                                                                 gint              width,
                                                                 gint             *minimum_height,
                                                                 gint             *natural_height);
static void             gtk_widget_real_get_width_for_height    (GtkWidget        *widget,
                                                                 gint              height,
                                                                 gint             *minimum_width,
                                                                 gint             *natural_width);
static const GtkWidgetAuxInfo* _gtk_widget_get_aux_info_or_defaults (GtkWidget *widget);
static GtkWidgetAuxInfo* gtk_widget_get_aux_info                (GtkWidget        *widget,
                                                                 gboolean          create);
static void		gtk_widget_aux_info_destroy		(GtkWidgetAuxInfo *aux_info);
static AtkObject*	gtk_widget_real_get_accessible		(GtkWidget	  *widget);
static void		gtk_widget_accessible_interface_init	(AtkImplementorIface *iface);
static AtkObject*	gtk_widget_ref_accessible		(AtkImplementor *implementor);
static void             gtk_widget_invalidate_widget_windows    (GtkWidget        *widget,
								 cairo_region_t        *region);
static GdkScreen *      gtk_widget_get_screen_unchecked         (GtkWidget        *widget);
static void		gtk_widget_queue_shallow_draw		(GtkWidget        *widget);
static gboolean         gtk_widget_real_can_activate_accel      (GtkWidget *widget,
                                                                 guint      signal_id);

static void             gtk_widget_real_set_has_tooltip         (GtkWidget *widget,
								 gboolean   has_tooltip,
								 gboolean   force);
static void             gtk_widget_buildable_interface_init     (GtkBuildableIface *iface);
static void             gtk_widget_buildable_set_name           (GtkBuildable     *buildable,
                                                                 const gchar      *name);
static const gchar *    gtk_widget_buildable_get_name           (GtkBuildable     *buildable);
static GObject *        gtk_widget_buildable_get_internal_child (GtkBuildable *buildable,
								 GtkBuilder   *builder,
								 const gchar  *childname);
static void             gtk_widget_buildable_set_buildable_property (GtkBuildable     *buildable,
								     GtkBuilder       *builder,
								     const gchar      *name,
								     const GValue     *value);
static gboolean         gtk_widget_buildable_custom_tag_start   (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder,
                                                                 GObject          *child,
                                                                 const gchar      *tagname,
                                                                 GMarkupParser    *parser,
                                                                 gpointer         *data);
static void             gtk_widget_buildable_custom_finished    (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder,
                                                                 GObject          *child,
                                                                 const gchar      *tagname,
                                                                 gpointer          data);
static void             gtk_widget_buildable_parser_finished    (GtkBuildable     *buildable,
                                                                 GtkBuilder       *builder);

static GtkSizeRequestMode gtk_widget_real_get_request_mode      (GtkWidget         *widget);
static void             gtk_widget_real_get_width               (GtkWidget         *widget,
                                                                 gint              *minimum_size,
                                                                 gint              *natural_size);
static void             gtk_widget_real_get_height              (GtkWidget         *widget,
                                                                 gint              *minimum_size,
                                                                 gint              *natural_size);

static void             gtk_widget_queue_tooltip_query          (GtkWidget *widget);


static void             gtk_widget_real_adjust_size_request     (GtkWidget         *widget,
                                                                 GtkOrientation     orientation,
                                                                 gint              *minimum_size,
                                                                 gint              *natural_size);
static void             gtk_widget_real_adjust_size_allocation  (GtkWidget         *widget,
                                                                 GtkOrientation     orientation,
                                                                 gint              *minimum_size,
                                                                 gint              *natural_size,
                                                                 gint              *allocated_pos,
                                                                 gint              *allocated_size);

static void gtk_widget_set_usize_internal (GtkWidget          *widget,
					   gint                width,
					   gint                height,
					   GtkQueueResizeFlags flags);

static void gtk_widget_add_events_internal (GtkWidget *widget,
                                            GdkDevice *device,
                                            gint       events);
static void gtk_widget_set_device_enabled_internal (GtkWidget *widget,
                                                    GdkDevice *device,
                                                    gboolean   recurse,
                                                    gboolean   enabled);

/* --- variables --- */
static gpointer         gtk_widget_parent_class = NULL;
static guint            widget_signals[LAST_SIGNAL] = { 0 };
static GtkStyle        *gtk_default_style = NULL;
static guint            composite_child_stack = 0;
static GtkTextDirection gtk_default_direction = GTK_TEXT_DIR_LTR;
static GParamSpecPool  *style_property_spec_pool = NULL;

static GQuark		quark_property_parser = 0;
static GQuark		quark_aux_info = 0;
static GQuark		quark_accel_path = 0;
static GQuark		quark_accel_closures = 0;
static GQuark		quark_event_mask = 0;
static GQuark           quark_device_event_mask = 0;
static GQuark		quark_parent_window = 0;
static GQuark		quark_pointer_window = 0;
static GQuark		quark_shape_info = 0;
static GQuark		quark_input_shape_info = 0;
static GQuark		quark_pango_context = 0;
static GQuark		quark_rc_style = 0;
static GQuark		quark_accessible_object = 0;
static GQuark		quark_mnemonic_labels = 0;
static GQuark		quark_tooltip_markup = 0;
static GQuark		quark_has_tooltip = 0;
static GQuark		quark_tooltip_window = 0;
static GQuark		quark_visual = 0;
static GQuark           quark_modifier_style = 0;
static GQuark           quark_enabled_devices = 0;
static GQuark           quark_size_groups = 0;
GParamSpecPool         *_gtk_widget_child_property_pool = NULL;
GObjectNotifyContext   *_gtk_widget_child_property_notify_context = NULL;

/* --- functions --- */
GType
gtk_widget_get_type (void)
{
  static GType widget_type = 0;

  if (G_UNLIKELY (widget_type == 0))
    {
      const GTypeInfo widget_info =
      {
	sizeof (GtkWidgetClass),
	gtk_widget_base_class_init,
	(GBaseFinalizeFunc) gtk_widget_base_class_finalize,
	(GClassInitFunc) gtk_widget_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_init */
	sizeof (GtkWidget),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_widget_init,
	NULL,		/* value_table */
      };

      const GInterfaceInfo accessibility_info =
      {
	(GInterfaceInitFunc) gtk_widget_accessible_interface_init,
	(GInterfaceFinalizeFunc) NULL,
	NULL /* interface data */
      };

      const GInterfaceInfo buildable_info =
      {
	(GInterfaceInitFunc) gtk_widget_buildable_interface_init,
	(GInterfaceFinalizeFunc) NULL,
	NULL /* interface data */
      };

      widget_type = g_type_register_static (G_TYPE_INITIALLY_UNOWNED, "GtkWidget",
                                            &widget_info, G_TYPE_FLAG_ABSTRACT);

      g_type_add_class_private (widget_type, sizeof (GtkWidgetClassPrivate));

      g_type_add_interface_static (widget_type, ATK_TYPE_IMPLEMENTOR,
                                   &accessibility_info) ;
      g_type_add_interface_static (widget_type, GTK_TYPE_BUILDABLE,
                                   &buildable_info) ;
    }

  return widget_type;
}

static void
gtk_widget_base_class_init (gpointer g_class)
{
  GtkWidgetClass *klass = g_class;

  klass->priv = G_TYPE_CLASS_GET_PRIVATE (g_class, GTK_TYPE_WIDGET, GtkWidgetClassPrivate);
}

static void
child_property_notify_dispatcher (GObject     *object,
				  guint        n_pspecs,
				  GParamSpec **pspecs)
{
  GTK_WIDGET_GET_CLASS (object)->dispatch_child_properties_changed (GTK_WIDGET (object), n_pspecs, pspecs);
}

/* We guard against the draw signal callbacks modifying the state of the
 * cairo context by surounding it with save/restore.
 * Maybe we should also cairo_new_path() just to be sure?
 */
static void
gtk_widget_draw_marshaller (GClosure     *closure,
                            GValue       *return_value,
                            guint         n_param_values,
                            const GValue *param_values,
                            gpointer      invocation_hint,
                            gpointer      marshal_data)
{
  cairo_t *cr = g_value_get_boxed (&param_values[1]);

  cairo_save (cr);

  _gtk_marshal_BOOLEAN__BOXED (closure,
                               return_value,
                               n_param_values,
                               param_values,
                               invocation_hint,
                               marshal_data);

  cairo_restore (cr);
}

static void
gtk_widget_class_init (GtkWidgetClass *klass)
{
  static GObjectNotifyContext cpn_context = { 0, NULL, NULL };
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkBindingSet *binding_set;

  gtk_widget_parent_class = g_type_class_peek_parent (klass);

  quark_property_parser = g_quark_from_static_string ("gtk-rc-property-parser");
  quark_aux_info = g_quark_from_static_string ("gtk-aux-info");
  quark_accel_path = g_quark_from_static_string ("gtk-accel-path");
  quark_accel_closures = g_quark_from_static_string ("gtk-accel-closures");
  quark_event_mask = g_quark_from_static_string ("gtk-event-mask");
  quark_device_event_mask = g_quark_from_static_string ("gtk-device-event-mask");
  quark_parent_window = g_quark_from_static_string ("gtk-parent-window");
  quark_pointer_window = g_quark_from_static_string ("gtk-pointer-window");
  quark_shape_info = g_quark_from_static_string ("gtk-shape-info");
  quark_input_shape_info = g_quark_from_static_string ("gtk-input-shape-info");
  quark_pango_context = g_quark_from_static_string ("gtk-pango-context");
  quark_rc_style = g_quark_from_static_string ("gtk-rc-style");
  quark_accessible_object = g_quark_from_static_string ("gtk-accessible-object");
  quark_mnemonic_labels = g_quark_from_static_string ("gtk-mnemonic-labels");
  quark_tooltip_markup = g_quark_from_static_string ("gtk-tooltip-markup");
  quark_has_tooltip = g_quark_from_static_string ("gtk-has-tooltip");
  quark_tooltip_window = g_quark_from_static_string ("gtk-tooltip-window");
  quark_visual = g_quark_from_static_string ("gtk-widget-visual");
  quark_modifier_style = g_quark_from_static_string ("gtk-widget-modifier-style");
  quark_enabled_devices = g_quark_from_static_string ("gtk-widget-enabled-devices");
  quark_size_groups = g_quark_from_static_string ("gtk-widget-size-groups");

  style_property_spec_pool = g_param_spec_pool_new (FALSE);
  _gtk_widget_child_property_pool = g_param_spec_pool_new (TRUE);
  cpn_context.quark_notify_queue = g_quark_from_static_string ("GtkWidget-child-property-notify-queue");
  cpn_context.dispatcher = child_property_notify_dispatcher;
  _gtk_widget_child_property_notify_context = &cpn_context;

  gobject_class->dispose = gtk_widget_dispose;
  gobject_class->finalize = gtk_widget_finalize;
  gobject_class->set_property = gtk_widget_set_property;
  gobject_class->get_property = gtk_widget_get_property;

  klass->destroy = gtk_widget_real_destroy;

  klass->activate_signal = 0;
  klass->dispatch_child_properties_changed = gtk_widget_dispatch_child_properties_changed;
  klass->show = gtk_widget_real_show;
  klass->show_all = gtk_widget_show;
  klass->hide = gtk_widget_real_hide;
  klass->map = gtk_widget_real_map;
  klass->unmap = gtk_widget_real_unmap;
  klass->realize = gtk_widget_real_realize;
  klass->unrealize = gtk_widget_real_unrealize;
  klass->size_allocate = gtk_widget_real_size_allocate;
  klass->get_request_mode = gtk_widget_real_get_request_mode;
  klass->get_preferred_width = gtk_widget_real_get_width;
  klass->get_preferred_height = gtk_widget_real_get_height;
  klass->get_preferred_width_for_height = gtk_widget_real_get_width_for_height;
  klass->get_preferred_height_for_width = gtk_widget_real_get_height_for_width;
  klass->state_changed = NULL;
  klass->parent_set = NULL;
  klass->hierarchy_changed = NULL;
  klass->style_set = gtk_widget_real_style_set;
  klass->direction_changed = gtk_widget_real_direction_changed;
  klass->grab_notify = NULL;
  klass->child_notify = NULL;
  klass->draw = NULL;
  klass->mnemonic_activate = gtk_widget_real_mnemonic_activate;
  klass->grab_focus = gtk_widget_real_grab_focus;
  klass->focus = gtk_widget_real_focus;
  klass->move_focus = gtk_widget_real_move_focus;
  klass->keynav_failed = gtk_widget_real_keynav_failed;
  klass->event = NULL;
  klass->button_press_event = NULL;
  klass->button_release_event = NULL;
  klass->motion_notify_event = NULL;
  klass->delete_event = NULL;
  klass->destroy_event = NULL;
  klass->key_press_event = gtk_widget_real_key_press_event;
  klass->key_release_event = gtk_widget_real_key_release_event;
  klass->enter_notify_event = NULL;
  klass->leave_notify_event = NULL;
  klass->configure_event = NULL;
  klass->focus_in_event = gtk_widget_real_focus_in_event;
  klass->focus_out_event = gtk_widget_real_focus_out_event;
  klass->map_event = NULL;
  klass->unmap_event = NULL;
  klass->window_state_event = NULL;
  klass->property_notify_event = _gtk_selection_property_notify;
  klass->selection_clear_event = _gtk_selection_clear;
  klass->selection_request_event = _gtk_selection_request;
  klass->selection_notify_event = _gtk_selection_notify;
  klass->selection_received = NULL;
  klass->proximity_in_event = NULL;
  klass->proximity_out_event = NULL;
  klass->drag_begin = NULL;
  klass->drag_end = NULL;
  klass->drag_data_delete = NULL;
  klass->drag_leave = NULL;
  klass->drag_motion = NULL;
  klass->drag_drop = NULL;
  klass->drag_data_received = NULL;
  klass->screen_changed = NULL;
  klass->can_activate_accel = gtk_widget_real_can_activate_accel;
  klass->grab_broken_event = NULL;
  klass->query_tooltip = gtk_widget_real_query_tooltip;
  klass->style_updated = gtk_widget_real_style_updated;

  klass->show_help = gtk_widget_real_show_help;

  /* Accessibility support */
  klass->priv->accessible_type = GTK_TYPE_ACCESSIBLE;
  klass->priv->accessible_role = ATK_ROLE_INVALID;
  klass->get_accessible = gtk_widget_real_get_accessible;

  klass->adjust_size_request = gtk_widget_real_adjust_size_request;
  klass->adjust_size_allocation = gtk_widget_real_adjust_size_allocation;

  g_object_class_install_property (gobject_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
 							P_("Widget name"),
							P_("The name of the widget"),
							NULL,
							GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_PARENT,
				   g_param_spec_object ("parent",
							P_("Parent widget"),
							P_("The parent widget of this widget. Must be a Container widget"),
							GTK_TYPE_CONTAINER,
							GTK_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_WIDTH_REQUEST,
				   g_param_spec_int ("width-request",
 						     P_("Width request"),
 						     P_("Override for width request of the widget, or -1 if natural request should be used"),
 						     -1,
 						     G_MAXINT,
 						     -1,
 						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HEIGHT_REQUEST,
				   g_param_spec_int ("height-request",
 						     P_("Height request"),
 						     P_("Override for height request of the widget, or -1 if natural request should be used"),
 						     -1,
 						     G_MAXINT,
 						     -1,
 						     GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_VISIBLE,
				   g_param_spec_boolean ("visible",
 							 P_("Visible"),
 							 P_("Whether the widget is visible"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_SENSITIVE,
				   g_param_spec_boolean ("sensitive",
 							 P_("Sensitive"),
 							 P_("Whether the widget responds to input"),
 							 TRUE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_APP_PAINTABLE,
				   g_param_spec_boolean ("app-paintable",
 							 P_("Application paintable"),
 							 P_("Whether the application will paint directly on the widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_CAN_FOCUS,
				   g_param_spec_boolean ("can-focus",
 							 P_("Can focus"),
 							 P_("Whether the widget can accept the input focus"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HAS_FOCUS,
				   g_param_spec_boolean ("has-focus",
 							 P_("Has focus"),
 							 P_("Whether the widget has the input focus"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_IS_FOCUS,
				   g_param_spec_boolean ("is-focus",
 							 P_("Is focus"),
 							 P_("Whether the widget is the focus widget within the toplevel"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_CAN_DEFAULT,
				   g_param_spec_boolean ("can-default",
 							 P_("Can default"),
 							 P_("Whether the widget can be the default widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_HAS_DEFAULT,
				   g_param_spec_boolean ("has-default",
 							 P_("Has default"),
 							 P_("Whether the widget is the default widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_RECEIVES_DEFAULT,
				   g_param_spec_boolean ("receives-default",
 							 P_("Receives default"),
 							 P_("If TRUE, the widget will receive the default action when it is focused"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_COMPOSITE_CHILD,
				   g_param_spec_boolean ("composite-child",
 							 P_("Composite child"),
 							 P_("Whether the widget is part of a composite widget"),
 							 FALSE,
 							 GTK_PARAM_READABLE));
  g_object_class_install_property (gobject_class,
				   PROP_STYLE,
				   g_param_spec_object ("style",
 							P_("Style"),
 							P_("The style of the widget, which contains information about how it will look (colors etc)"),
 							GTK_TYPE_STYLE,
 							GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_EVENTS,
				   g_param_spec_flags ("events",
 						       P_("Events"),
 						       P_("The event mask that decides what kind of GdkEvents this widget gets"),
 						       GDK_TYPE_EVENT_MASK,
 						       GDK_STRUCTURE_MASK,
 						       GTK_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_NO_SHOW_ALL,
				   g_param_spec_boolean ("no-show-all",
 							 P_("No show all"),
 							 P_("Whether gtk_widget_show_all() should not affect this widget"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));

/**
 * GtkWidget:has-tooltip:
 *
 * Enables or disables the emission of #GtkWidget::query-tooltip on @widget.
 * A value of %TRUE indicates that @widget can have a tooltip, in this case
 * the widget will be queried using #GtkWidget::query-tooltip to determine
 * whether it will provide a tooltip or not.
 *
 * Note that setting this property to %TRUE for the first time will change
 * the event masks of the GdkWindows of this widget to include leave-notify
 * and motion-notify events.  This cannot and will not be undone when the
 * property is set to %FALSE again.
 *
 * Since: 2.12
 */
  g_object_class_install_property (gobject_class,
				   PROP_HAS_TOOLTIP,
				   g_param_spec_boolean ("has-tooltip",
 							 P_("Has tooltip"),
 							 P_("Whether this widget has a tooltip"),
 							 FALSE,
 							 GTK_PARAM_READWRITE));
  /**
   * GtkWidget:tooltip-text:
   *
   * Sets the text of tooltip to be the given string.
   *
   * Also see gtk_tooltip_set_text().
   *
   * This is a convenience property which will take care of getting the
   * tooltip shown if the given string is not %NULL: #GtkWidget:has-tooltip
   * will automatically be set to %TRUE and there will be taken care of
   * #GtkWidget::query-tooltip in the default signal handler.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
                                   PROP_TOOLTIP_TEXT,
                                   g_param_spec_string ("tooltip-text",
                                                        P_("Tooltip Text"),
                                                        P_("The contents of the tooltip for this widget"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkWidget:tooltip-markup:
   *
   * Sets the text of tooltip to be the given string, which is marked up
   * with the <link linkend="PangoMarkupFormat">Pango text markup language</link>.
   * Also see gtk_tooltip_set_markup().
   *
   * This is a convenience property which will take care of getting the
   * tooltip shown if the given string is not %NULL: #GtkWidget:has-tooltip
   * will automatically be set to %TRUE and there will be taken care of
   * #GtkWidget::query-tooltip in the default signal handler.
   *
   * Since: 2.12
   */
  g_object_class_install_property (gobject_class,
				   PROP_TOOLTIP_MARKUP,
				   g_param_spec_string ("tooltip-markup",
 							P_("Tooltip markup"),
							P_("The contents of the tooltip for this widget"),
							NULL,
							GTK_PARAM_READWRITE));

  /**
   * GtkWidget:window:
   *
   * The widget's window if it is realized, %NULL otherwise.
   *
   * Since: 2.14
   */
  g_object_class_install_property (gobject_class,
				   PROP_WINDOW,
				   g_param_spec_object ("window",
 							P_("Window"),
							P_("The widget's window if it is realized"),
							GDK_TYPE_WINDOW,
							GTK_PARAM_READABLE));

  /**
   * GtkWidget:double-buffered
   *
   * Whether the widget is double buffered.
   *
   * Since: 2.18
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DOUBLE_BUFFERED,
                                   g_param_spec_boolean ("double-buffered",
                                                         P_("Double Buffered"),
                                                         P_("Whether the widget is double buffered"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkWidget:halign:
   *
   * How to distribute horizontal space if widget gets extra space, see #GtkAlign
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HALIGN,
                                   g_param_spec_enum ("halign",
                                                      P_("Horizontal Alignment"),
                                                      P_("How to position in extra horizontal space"),
                                                      GTK_TYPE_ALIGN,
                                                      GTK_ALIGN_FILL,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkWidget:valign:
   *
   * How to distribute vertical space if widget gets extra space, see #GtkAlign
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VALIGN,
                                   g_param_spec_enum ("valign",
                                                      P_("Vertical Alignment"),
                                                      P_("How to position in extra vertical space"),
                                                      GTK_TYPE_ALIGN,
                                                      GTK_ALIGN_FILL,
                                                      GTK_PARAM_READWRITE));

  /**
   * GtkWidget:margin-left
   *
   * Margin on left side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_LEFT,
                                   g_param_spec_int ("margin-left",
                                                     P_("Margin on Left"),
                                                     P_("Pixels of extra space on the left side"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkWidget:margin-right
   *
   * Margin on right side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_RIGHT,
                                   g_param_spec_int ("margin-right",
                                                     P_("Margin on Right"),
                                                     P_("Pixels of extra space on the right side"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkWidget:margin-top
   *
   * Margin on top side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_TOP,
                                   g_param_spec_int ("margin-top",
                                                     P_("Margin on Top"),
                                                     P_("Pixels of extra space on the top side"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkWidget:margin-bottom
   *
   * Margin on bottom side of widget.
   *
   * This property adds margin outside of the widget's normal size
   * request, the margin will be added in addition to the size from
   * gtk_widget_set_size_request() for example.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN_BOTTOM,
                                   g_param_spec_int ("margin-bottom",
                                                     P_("Margin on Bottom"),
                                                     P_("Pixels of extra space on the bottom side"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkWidget:margin
   *
   * Sets all four sides' margin at once. If read, returns max
   * margin on any side.
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MARGIN,
                                   g_param_spec_int ("margin",
                                                     P_("All Margins"),
                                                     P_("Pixels of extra space on all four sides"),
                                                     0,
                                                     G_MAXINT16,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkWidget::destroy:
   * @object: the object which received the signal
   *
   * Signals that all holders of a reference to the widget should release
   * the reference that they hold. May result in finalization of the widget
   * if all references are released.
   */
  widget_signals[DESTROY] =
    g_signal_new (I_("destroy"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  G_STRUCT_OFFSET (GtkWidgetClass, destroy),
                  NULL, NULL,
                  _gtk_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget:hexpand
   *
   * Whether to expand horizontally. See gtk_widget_set_hexpand().
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HEXPAND,
                                   g_param_spec_boolean ("hexpand",
                                                         P_("Horizontal Expand"),
                                                         P_("Whether widget wants more horizontal space"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkWidget:hexpand-set
   *
   * Whether to use the #GtkWidget:hexpand property. See gtk_widget_get_hexpand_set().
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HEXPAND_SET,
                                   g_param_spec_boolean ("hexpand-set",
                                                         P_("Horizontal Expand Set"),
                                                         P_("Whether to use the hexpand property"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkWidget:vexpand
   *
   * Whether to expand vertically. See gtk_widget_set_vexpand().
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VEXPAND,
                                   g_param_spec_boolean ("vexpand",
                                                         P_("Vertical Expand"),
                                                         P_("Whether widget wants more vertical space"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkWidget:vexpand-set
   *
   * Whether to use the #GtkWidget:vexpand property. See gtk_widget_get_vexpand_set().
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VEXPAND_SET,
                                   g_param_spec_boolean ("vexpand-set",
                                                         P_("Vertical Expand Set"),
                                                         P_("Whether to use the vexpand property"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkWidget:expand
   *
   * Whether to expand in both directions. Setting this sets both #GtkWidget:hexpand and #GtkWidget:vexpand
   *
   * Since: 3.0
   */
  g_object_class_install_property (gobject_class,
                                   PROP_EXPAND,
                                   g_param_spec_boolean ("expand",
                                                         P_("Expand Both"),
                                                         P_("Whether widget wants to expand in both directions"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /**
   * GtkWidget::show:
   * @widget: the object which received the signal.
   */
  widget_signals[SHOW] =
    g_signal_new (I_("show"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, show),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::hide:
   * @widget: the object which received the signal.
   */
  widget_signals[HIDE] =
    g_signal_new (I_("hide"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, hide),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::map:
   * @widget: the object which received the signal.
   */
  widget_signals[MAP] =
    g_signal_new (I_("map"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, map),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::unmap:
   * @widget: the object which received the signal.
   */
  widget_signals[UNMAP] =
    g_signal_new (I_("unmap"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unmap),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::realize:
   * @widget: the object which received the signal.
   */
  widget_signals[REALIZE] =
    g_signal_new (I_("realize"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, realize),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::unrealize:
   * @widget: the object which received the signal.
   */
  widget_signals[UNREALIZE] =
    g_signal_new (I_("unrealize"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unrealize),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::size-allocate:
   * @widget: the object which received the signal.
   * @allocation:
   */
  widget_signals[SIZE_ALLOCATE] =
    g_signal_new (I_("size-allocate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, size_allocate),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_RECTANGLE | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::state-changed:
   * @widget: the object which received the signal.
   * @state: the previous state
   *
   * The ::state-changed signal is emitted when the widget state changes.
   * See gtk_widget_get_state().
   *
   * Deprecated: 3.0. Use #GtkWidget::state-flags-changed instead.
   */
  widget_signals[STATE_CHANGED] =
    g_signal_new (I_("state-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, state_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_STATE_TYPE);

  /**
   * GtkWidget::state-flags-changed:
   * @widget: the object which received the signal.
   * @flags: The previous state flags.
   *
   * The ::state-flags-changed signal is emitted when the widget state
   * changes, see gtk_widget_get_state_flags().
   *
   * Since: 3.0
   */
  widget_signals[STATE_FLAGS_CHANGED] =
    g_signal_new (I_("state-flags-changed"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, state_flags_changed),
                  NULL, NULL,
                  _gtk_marshal_VOID__FLAGS,
                  G_TYPE_NONE, 1,
                  GTK_TYPE_STATE_FLAGS);

  /**
   * GtkWidget::parent-set:
   * @widget: the object on which the signal is emitted
   * @old_parent: (allow-none): the previous parent, or %NULL if the widget
   *   just got its initial parent.
   *
   * The ::parent-set signal is emitted when a new parent
   * has been set on a widget.
   */
  widget_signals[PARENT_SET] =
    g_signal_new (I_("parent-set"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, parent_set),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);

  /**
   * GtkWidget::hierarchy-changed:
   * @widget: the object on which the signal is emitted
   * @previous_toplevel: (allow-none): the previous toplevel ancestor, or %NULL
   *   if the widget was previously unanchored
   *
   * The ::hierarchy-changed signal is emitted when the
   * anchored state of a widget changes. A widget is
   * <firstterm>anchored</firstterm> when its toplevel
   * ancestor is a #GtkWindow. This signal is emitted when
   * a widget changes from un-anchored to anchored or vice-versa.
   */
  widget_signals[HIERARCHY_CHANGED] =
    g_signal_new (I_("hierarchy-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, hierarchy_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_WIDGET);

  /**
   * GtkWidget::style-set:
   * @widget: the object on which the signal is emitted
   * @previous_style: (allow-none): the previous style, or %NULL if the widget
   *   just got its initial style
   *
   * The ::style-set signal is emitted when a new style has been set
   * on a widget. Note that style-modifying functions like
   * gtk_widget_modify_base() also cause this signal to be emitted.
   *
   * Note that this signal is emitted for changes to the deprecated
   * #GtkStyle. To track changes to the #GtkStyleContext associated
   * with a widget, use the #GtkWidget::style-updated signal.
   *
   * Deprecated:3.0: Use the #GtkWidget::style-updated signal
   */
  widget_signals[STYLE_SET] =
    g_signal_new (I_("style-set"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, style_set),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_STYLE);

  /**
   * GtkWidget::style-updated:
   * @widget: the object on which the signal is emitted
   *
   * The ::style-updated signal is emitted when the #GtkStyleContext
   * of a widget is changed. Note that style-modifying functions like
   * gtk_widget_override_color() also cause this signal to be emitted.
   *
   * Since: 3.0
   */
  widget_signals[STYLE_UPDATED] =
    g_signal_new (I_("style-updated"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, style_updated),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * GtkWidget::direction-changed:
   * @widget: the object on which the signal is emitted
   * @previous_direction: the previous text direction of @widget
   *
   * The ::direction-changed signal is emitted when the text direction
   * of a widget changes.
   */
  widget_signals[DIRECTION_CHANGED] =
    g_signal_new (I_("direction-changed"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (GtkWidgetClass, direction_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM,
		  G_TYPE_NONE, 1,
		  GTK_TYPE_TEXT_DIRECTION);

  /**
   * GtkWidget::grab-notify:
   * @widget: the object which received the signal
   * @was_grabbed: %FALSE if the widget becomes shadowed, %TRUE
   *               if it becomes unshadowed
   *
   * The ::grab-notify signal is emitted when a widget becomes
   * shadowed by a GTK+ grab (not a pointer or keyboard grab) on
   * another widget, or when it becomes unshadowed due to a grab
   * being removed.
   *
   * A widget is shadowed by a gtk_grab_add() when the topmost
   * grab widget in the grab stack of its window group is not
   * its ancestor.
   */
  widget_signals[GRAB_NOTIFY] =
    g_signal_new (I_("grab-notify"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkWidgetClass, grab_notify),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE, 1,
		  G_TYPE_BOOLEAN);

  /**
   * GtkWidget::child-notify:
   * @widget: the object which received the signal
   * @pspec: the #GParamSpec of the changed child property
   *
   * The ::child-notify signal is emitted for each
   * <link linkend="child-properties">child property</link>  that has
   * changed on an object. The signal's detail holds the property name.
   */
  widget_signals[CHILD_NOTIFY] =
    g_signal_new (I_("child-notify"),
		   G_TYPE_FROM_CLASS (gobject_class),
		   G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS,
		   G_STRUCT_OFFSET (GtkWidgetClass, child_notify),
		   NULL, NULL,
		   g_cclosure_marshal_VOID__PARAM,
		   G_TYPE_NONE, 1,
		   G_TYPE_PARAM);

  /**
   * GtkWidget::draw:
   * @widget: the object which received the signal
   * @cr: the cairo context to draw to
   *
   * This signal is emitted when a widget is supposed to render itself.
   * The @widget's top left corner must be painted at the origin of
   * the passed in context and be sized to the values returned by
   * gtk_widget_get_allocated_width() and
   * gtk_widget_get_allocated_height().
   *
   * Signal handlers connected to this signal can modify the cairo
   * context passed as @cr in any way they like and don't need to
   * restore it. The signal emission takes care of calling cairo_save()
   * before and cairo_restore() after invoking the handler.
   *
   * Since: 3.0
   */
  widget_signals[DRAW] =
    g_signal_new (I_("draw"),
		   G_TYPE_FROM_CLASS (gobject_class),
		   G_SIGNAL_RUN_LAST,
		   G_STRUCT_OFFSET (GtkWidgetClass, draw),
                   _gtk_boolean_handled_accumulator, NULL,
                   gtk_widget_draw_marshaller,
		   G_TYPE_BOOLEAN, 1,
		   CAIRO_GOBJECT_TYPE_CONTEXT);

  /**
   * GtkWidget::mnemonic-activate:
   * @widget: the object which received the signal.
   * @arg1:
   */
  widget_signals[MNEMONIC_ACTIVATE] =
    g_signal_new (I_("mnemonic-activate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, mnemonic_activate),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOOLEAN,
		  G_TYPE_BOOLEAN, 1,
		  G_TYPE_BOOLEAN);

  /**
   * GtkWidget::grab-focus:
   * @widget: the object which received the signal.
   */
  widget_signals[GRAB_FOCUS] =
    g_signal_new (I_("grab-focus"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, grab_focus),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::focus:
   * @widget: the object which received the signal.
   * @direction:
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event. %FALSE to propagate the event further.
   */
  widget_signals[FOCUS] =
    g_signal_new (I_("focus"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__ENUM,
		  G_TYPE_BOOLEAN, 1,
		  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkWidget::move-focus:
   * @widget: the object which received the signal.
   * @direction:
   */
  widget_signals[MOVE_FOCUS] =
    g_signal_new (I_("move-focus"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkWidgetClass, move_focus),
                  NULL, NULL,
                  _gtk_marshal_VOID__ENUM,
                  G_TYPE_NONE,
                  1,
                  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkWidget::keynav-failed:
   * @widget: the object which received the signal
   * @direction: the direction of movement
   *
   * Gets emitted if keyboard navigation fails.
   * See gtk_widget_keynav_failed() for details.
   *
   * Returns: %TRUE if stopping keyboard navigation is fine, %FALSE
   *          if the emitting widget should try to handle the keyboard
   *          navigation attempt in its parent container(s).
   *
   * Since: 2.12
   **/
  widget_signals[KEYNAV_FAILED] =
    g_signal_new (I_("keynav-failed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GtkWidgetClass, keynav_failed),
                  _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__ENUM,
                  G_TYPE_BOOLEAN, 1,
                  GTK_TYPE_DIRECTION_TYPE);

  /**
   * GtkWidget::event:
   * @widget: the object which received the signal.
   * @event: the #GdkEvent which triggered this signal
   *
   * The GTK+ main loop will emit three signals for each GDK event delivered
   * to a widget: one generic ::event signal, another, more specific,
   * signal that matches the type of event delivered (e.g.
   * #GtkWidget::key-press-event) and finally a generic
   * #GtkWidget::event-after signal.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event
   * and to cancel the emission of the second specific ::event signal.
   *   %FALSE to propagate the event further and to allow the emission of
   *   the second signal. The ::event-after signal is emitted regardless of
   *   the return value.
   */
  widget_signals[EVENT] =
    g_signal_new (I_("event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::event-after:
   * @widget: the object which received the signal.
   * @event: the #GdkEvent which triggered this signal
   *
   * After the emission of the #GtkWidget::event signal and (optionally)
   * the second more specific signal, ::event-after will be emitted
   * regardless of the previous two signals handlers return values.
   *
   */
  widget_signals[EVENT_AFTER] =
    g_signal_new (I_("event-after"),
		  G_TYPE_FROM_CLASS (klass),
		  0,
		  0,
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::button-press-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventButton): the #GdkEventButton which triggered
   *   this signal.
   *
   * The ::button-press-event signal will be emitted when a button
   * (typically from a mouse) is pressed.
   *
   * To receive this signal, the #GdkWindow associated to the
   * widget needs to enable the #GDK_BUTTON_PRESS_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[BUTTON_PRESS_EVENT] =
    g_signal_new (I_("button-press-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, button_press_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::button-release-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventButton): the #GdkEventButton which triggered
   *   this signal.
   *
   * The ::button-release-event signal will be emitted when a button
   * (typically from a mouse) is released.
   *
   * To receive this signal, the #GdkWindow associated to the
   * widget needs to enable the #GDK_BUTTON_RELEASE_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[BUTTON_RELEASE_EVENT] =
    g_signal_new (I_("button-release-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, button_release_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::scroll-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventScroll): the #GdkEventScroll which triggered
   *   this signal.
   *
   * The ::scroll-event signal is emitted when a button in the 4 to 7
   * range is pressed. Wheel mice are usually configured to generate
   * button press events for buttons 4 and 5 when the wheel is turned.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_BUTTON_PRESS_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[SCROLL_EVENT] =
    g_signal_new (I_("scroll-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, scroll_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::motion-notify-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventMotion): the #GdkEventMotion which triggered
   *   this signal.
   *
   * The ::motion-notify-event signal is emitted when the pointer moves
   * over the widget's #GdkWindow.
   *
   * To receive this signal, the #GdkWindow associated to the widget
   * needs to enable the #GDK_POINTER_MOTION_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[MOTION_NOTIFY_EVENT] =
    g_signal_new (I_("motion-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, motion_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::composited-changed:
   * @widget: the object on which the signal is emitted
   *
   * The ::composited-changed signal is emitted when the composited
   * status of @widget<!-- -->s screen changes.
   * See gdk_screen_is_composited().
   */
  widget_signals[COMPOSITED_CHANGED] =
    g_signal_new (I_("composited-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, composited_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::delete-event:
   * @widget: the object which received the signal
   * @event: the event which triggered this signal
   *
   * The ::delete-event signal is emitted if a user requests that
   * a toplevel window is closed. The default handler for this signal
   * destroys the window. Connecting gtk_widget_hide_on_delete() to
   * this signal will cause the window to be hidden instead, so that
   * it can later be shown again without reconstructing it.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[DELETE_EVENT] =
    g_signal_new (I_("delete-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, delete_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::destroy-event:
   * @widget: the object which received the signal.
   * @event: the event which triggered this signal
   *
   * The ::destroy-event signal is emitted when a #GdkWindow is destroyed.
   * You rarely get this signal, because most widgets disconnect themselves
   * from their window before they destroy it, so no widget owns the
   * window at destroy time.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_STRUCTURE_MASK mask. GDK will enable this mask
   * automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[DESTROY_EVENT] =
    g_signal_new (I_("destroy-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, destroy_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::key-press-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventKey): the #GdkEventKey which triggered this signal.
   *
   * The ::key-press-event signal is emitted when a key is pressed. The signal
   * emission will reoccur at the key-repeat rate when the key is kept pressed.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_KEY_PRESS_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[KEY_PRESS_EVENT] =
    g_signal_new (I_("key-press-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, key_press_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::key-release-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventKey): the #GdkEventKey which triggered this signal.
   *
   * The ::key-release-event signal is emitted when a key is released.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_KEY_RELEASE_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[KEY_RELEASE_EVENT] =
    g_signal_new (I_("key-release-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, key_release_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::enter-notify-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventCrossing): the #GdkEventCrossing which triggered
   *   this signal.
   *
   * The ::enter-notify-event will be emitted when the pointer enters
   * the @widget's window.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_ENTER_NOTIFY_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[ENTER_NOTIFY_EVENT] =
    g_signal_new (I_("enter-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, enter_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::leave-notify-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventCrossing): the #GdkEventCrossing which triggered
   *   this signal.
   *
   * The ::leave-notify-event will be emitted when the pointer leaves
   * the @widget's window.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_LEAVE_NOTIFY_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[LEAVE_NOTIFY_EVENT] =
    g_signal_new (I_("leave-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, leave_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::configure-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventConfigure): the #GdkEventConfigure which triggered
   *   this signal.
   *
   * The ::configure-event signal will be emitted when the size, position or
   * stacking of the @widget's window has changed.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_STRUCTURE_MASK mask. GDK will enable this mask
   * automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[CONFIGURE_EVENT] =
    g_signal_new (I_("configure-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, configure_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::focus-in-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventFocus): the #GdkEventFocus which triggered
   *   this signal.
   *
   * The ::focus-in-event signal will be emitted when the keyboard focus
   * enters the @widget's window.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_FOCUS_CHANGE_MASK mask.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[FOCUS_IN_EVENT] =
    g_signal_new (I_("focus-in-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus_in_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::focus-out-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventFocus): the #GdkEventFocus which triggered this
   *   signal.
   *
   * The ::focus-out-event signal will be emitted when the keyboard focus
   * leaves the @widget's window.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_FOCUS_CHANGE_MASK mask.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[FOCUS_OUT_EVENT] =
    g_signal_new (I_("focus-out-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, focus_out_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::map-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventAny): the #GdkEventAny which triggered this signal.
   *
   * The ::map-event signal will be emitted when the @widget's window is
   * mapped. A window is mapped when it becomes visible on the screen.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_STRUCTURE_MASK mask. GDK will enable this mask
   * automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[MAP_EVENT] =
    g_signal_new (I_("map-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, map_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::unmap-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventAny): the #GdkEventAny which triggered this signal
   *
   * The ::unmap-event signal will be emitted when the @widget's window is
   * unmapped. A window is unmapped when it becomes invisible on the screen.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_STRUCTURE_MASK mask. GDK will enable this mask
   * automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[UNMAP_EVENT] =
    g_signal_new (I_("unmap-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, unmap_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::property-notify-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventProperty): the #GdkEventProperty which triggered
   *   this signal.
   *
   * The ::property-notify-event signal will be emitted when a property on
   * the @widget's window has been changed or deleted.
   *
   * To receive this signal, the #GdkWindow associated to the widget needs
   * to enable the #GDK_PROPERTY_CHANGE_MASK mask.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[PROPERTY_NOTIFY_EVENT] =
    g_signal_new (I_("property-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, property_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::selection-clear-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventSelection): the #GdkEventSelection which triggered
   *   this signal.
   *
   * The ::selection-clear-event signal will be emitted when the
   * the @widget's window has lost ownership of a selection.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[SELECTION_CLEAR_EVENT] =
    g_signal_new (I_("selection-clear-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_clear_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::selection-request-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventSelection): the #GdkEventSelection which triggered
   *   this signal.
   *
   * The ::selection-request-event signal will be emitted when
   * another client requests ownership of the selection owned by
   * the @widget's window.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[SELECTION_REQUEST_EVENT] =
    g_signal_new (I_("selection-request-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_request_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::selection-notify-event:
   * @widget: the object which received the signal.
   * @event: (type Gdk.EventSelection):
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event. %FALSE to propagate the event further.
   */
  widget_signals[SELECTION_NOTIFY_EVENT] =
    g_signal_new (I_("selection-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::selection-received:
   * @widget: the object which received the signal.
   * @data:
   * @time:
   */
  widget_signals[SELECTION_RECEIVED] =
    g_signal_new (I_("selection-received"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_received),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_UINT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT);

  /**
   * GtkWidget::selection-get:
   * @widget: the object which received the signal.
   * @data:
   * @info:
   * @time:
   */
  widget_signals[SELECTION_GET] =
    g_signal_new (I_("selection-get"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, selection_get),
		  NULL, NULL,
		  _gtk_marshal_VOID__BOXED_UINT_UINT,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::proximity-in-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventProximity): the #GdkEventProximity which triggered
   *   this signal.
   *
   * To receive this signal the #GdkWindow associated to the widget needs
   * to enable the #GDK_PROXIMITY_IN_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[PROXIMITY_IN_EVENT] =
    g_signal_new (I_("proximity-in-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, proximity_in_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::proximity-out-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventProximity): the #GdkEventProximity which triggered
   *   this signal.
   *
   * To receive this signal the #GdkWindow associated to the widget needs
   * to enable the #GDK_PROXIMITY_OUT_MASK mask.
   *
   * This signal will be sent to the grab widget if there is one.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[PROXIMITY_OUT_EVENT] =
    g_signal_new (I_("proximity-out-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, proximity_out_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::drag-leave:
   * @widget: the object which received the signal.
   * @drag_context: the drag context
   * @time: the timestamp of the motion event
   *
   * The ::drag-leave signal is emitted on the drop site when the cursor
   * leaves the widget. A typical reason to connect to this signal is to
   * undo things done in #GtkWidget::drag-motion, e.g. undo highlighting
   * with gtk_drag_unhighlight()
   */
  widget_signals[DRAG_LEAVE] =
    g_signal_new (I_("drag-leave"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_leave),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_UINT,
		  G_TYPE_NONE, 2,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-begin:
   * @widget: the object which received the signal
   * @drag_context: the drag context
   *
   * The ::drag-begin signal is emitted on the drag source when a drag is
   * started. A typical reason to connect to this signal is to set up a
   * custom drag icon with gtk_drag_source_set_icon().
   *
   * Note that some widgets set up a drag icon in the default handler of
   * this signal, so you may have to use g_signal_connect_after() to
   * override what the default handler did.
   */
  widget_signals[DRAG_BEGIN] =
    g_signal_new (I_("drag-begin"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_begin),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-end:
   * @widget: the object which received the signal
   * @drag_context: the drag context
   *
   * The ::drag-end signal is emitted on the drag source when a drag is
   * finished.  A typical reason to connect to this signal is to undo
   * things done in #GtkWidget::drag-begin.
   */
  widget_signals[DRAG_END] =
    g_signal_new (I_("drag-end"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_end),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-data-delete:
   * @widget: the object which received the signal
   * @drag_context: the drag context
   *
   * The ::drag-data-delete signal is emitted on the drag source when a drag
   * with the action %GDK_ACTION_MOVE is successfully completed. The signal
   * handler is responsible for deleting the data that has been dropped. What
   * "delete" means depends on the context of the drag operation.
   */
  widget_signals[DRAG_DATA_DELETE] =
    g_signal_new (I_("drag-data-delete"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_delete),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_DRAG_CONTEXT);

  /**
   * GtkWidget::drag-failed:
   * @widget: the object which received the signal
   * @drag_context: the drag context
   * @result: the result of the drag operation
   *
   * The ::drag-failed signal is emitted on the drag source when a drag has
   * failed. The signal handler may hook custom code to handle a failed DND
   * operation based on the type of error, it returns %TRUE is the failure has
   * been already handled (not showing the default "drag operation failed"
   * animation), otherwise it returns %FALSE.
   *
   * Return value: %TRUE if the failed drag operation has been already handled.
   *
   * Since: 2.12
   */
  widget_signals[DRAG_FAILED] =
    g_signal_new (I_("drag-failed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_failed),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_ENUM,
		  G_TYPE_BOOLEAN, 2,
		  GDK_TYPE_DRAG_CONTEXT,
		  GTK_TYPE_DRAG_RESULT);

  /**
   * GtkWidget::drag-motion:
   * @widget: the object which received the signal
   * @drag_context: the drag context
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   * @time: the timestamp of the motion event
   * @returns: whether the cursor position is in a drop zone
   *
   * The drag-motion signal is emitted on the drop site when the user
   * moves the cursor over the widget during a drag. The signal handler
   * must determine whether the cursor position is in a drop zone or not.
   * If it is not in a drop zone, it returns %FALSE and no further processing
   * is necessary. Otherwise, the handler returns %TRUE. In this case, the
   * handler is responsible for providing the necessary information for
   * displaying feedback to the user, by calling gdk_drag_status().
   *
   * If the decision whether the drop will be accepted or rejected can't be
   * made based solely on the cursor position and the type of the data, the
   * handler may inspect the dragged data by calling gtk_drag_get_data() and
   * defer the gdk_drag_status() call to the #GtkWidget::drag-data-received
   * handler. Note that you cannot not pass #GTK_DEST_DEFAULT_DROP,
   * #GTK_DEST_DEFAULT_MOTION or #GTK_DEST_DEFAULT_ALL to gtk_drag_dest_set()
   * when using the drag-motion signal that way.
   *
   * Also note that there is no drag-enter signal. The drag receiver has to
   * keep track of whether he has received any drag-motion signals since the
   * last #GtkWidget::drag-leave and if not, treat the drag-motion signal as
   * an "enter" signal. Upon an "enter", the handler will typically highlight
   * the drop site with gtk_drag_highlight().
   * |[
   * static void
   * drag_motion (GtkWidget *widget,
   *              GdkDragContext *context,
   *              gint x,
   *              gint y,
   *              guint time)
   * {
   *   GdkAtom target;
   *
   *   PrivateData *private_data = GET_PRIVATE_DATA (widget);
   *
   *   if (!private_data->drag_highlight)
   *    {
   *      private_data->drag_highlight = 1;
   *      gtk_drag_highlight (widget);
   *    }
   *
   *   target = gtk_drag_dest_find_target (widget, context, NULL);
   *   if (target == GDK_NONE)
   *     gdk_drag_status (context, 0, time);
   *   else
   *    {
   *      private_data->pending_status = context->suggested_action;
   *      gtk_drag_get_data (widget, context, target, time);
   *    }
   *
   *   return TRUE;
   * }
   *
   * static void
   * drag_data_received (GtkWidget        *widget,
   *                     GdkDragContext   *context,
   *                     gint              x,
   *                     gint              y,
   *                     GtkSelectionData *selection_data,
   *                     guint             info,
   *                     guint             time)
   * {
   *   PrivateData *private_data = GET_PRIVATE_DATA (widget);
   *
   *   if (private_data->suggested_action)
   *    {
   *      private_data->suggested_action = 0;
   *
   *     /&ast; We are getting this data due to a request in drag_motion,
   *      * rather than due to a request in drag_drop, so we are just
   *      * supposed to call gdk_drag_status (), not actually paste in
   *      * the data.
   *      &ast;/
   *      str = gtk_selection_data_get_text (selection_data);
   *      if (!data_is_acceptable (str))
   *        gdk_drag_status (context, 0, time);
   *      else
   *        gdk_drag_status (context, private_data->suggested_action, time);
   *    }
   *   else
   *    {
   *      /&ast; accept the drop &ast;/
   *    }
   * }
   * ]|
   */
  widget_signals[DRAG_MOTION] =
    g_signal_new (I_("drag-motion"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_motion),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_INT_INT_UINT,
		  G_TYPE_BOOLEAN, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-drop:
   * @widget: the object which received the signal
   * @drag_context: the drag context
   * @x: the x coordinate of the current cursor position
   * @y: the y coordinate of the current cursor position
   * @time: the timestamp of the motion event
   * @returns: whether the cursor position is in a drop zone
   *
   * The ::drag-drop signal is emitted on the drop site when the user drops
   * the data onto the widget. The signal handler must determine whether
   * the cursor position is in a drop zone or not. If it is not in a drop
   * zone, it returns %FALSE and no further processing is necessary.
   * Otherwise, the handler returns %TRUE. In this case, the handler must
   * ensure that gtk_drag_finish() is called to let the source know that
   * the drop is done. The call to gtk_drag_finish() can be done either
   * directly or in a #GtkWidget::drag-data-received handler which gets
   * triggered by calling gtk_drag_get_data() to receive the data for one
   * or more of the supported targets.
   */
  widget_signals[DRAG_DROP] =
    g_signal_new (I_("drag-drop"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_drop),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT_INT_INT_UINT,
		  G_TYPE_BOOLEAN, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-data-get:
   * @widget: the object which received the signal
   * @drag_context: the drag context
   * @data: the #GtkSelectionData to be filled with the dragged data
   * @info: the info that has been registered with the target in the
   *        #GtkTargetList
   * @time: the timestamp at which the data was requested
   *
   * The ::drag-data-get signal is emitted on the drag source when the drop
   * site requests the data which is dragged. It is the responsibility of
   * the signal handler to fill @data with the data in the format which
   * is indicated by @info. See gtk_selection_data_set() and
   * gtk_selection_data_set_text().
   */
  widget_signals[DRAG_DATA_GET] =
    g_signal_new (I_("drag-data-get"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_get),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_BOXED_UINT_UINT,
		  G_TYPE_NONE, 4,
		  GDK_TYPE_DRAG_CONTEXT,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::drag-data-received:
   * @widget: the object which received the signal
   * @drag_context: the drag context
   * @x: where the drop happened
   * @y: where the drop happened
   * @data: the received data
   * @info: the info that has been registered with the target in the
   *        #GtkTargetList
   * @time: the timestamp at which the data was received
   *
   * The ::drag-data-received signal is emitted on the drop site when the
   * dragged data has been received. If the data was received in order to
   * determine whether the drop will be accepted, the handler is expected
   * to call gdk_drag_status() and <emphasis>not</emphasis> finish the drag.
   * If the data was received in response to a #GtkWidget::drag-drop signal
   * (and this is the last target to be received), the handler for this
   * signal is expected to process the received data and then call
   * gtk_drag_finish(), setting the @success parameter depending on whether
   * the data was processed successfully.
   *
   * The handler may inspect and modify @drag_context->action before calling
   * gtk_drag_finish(), e.g. to implement %GDK_ACTION_ASK as shown in the
   * following example:
   * |[
   * void
   * drag_data_received (GtkWidget          *widget,
   *                     GdkDragContext     *drag_context,
   *                     gint                x,
   *                     gint                y,
   *                     GtkSelectionData   *data,
   *                     guint               info,
   *                     guint               time)
   * {
   *   if ((data->length >= 0) && (data->format == 8))
   *     {
   *       if (drag_context->action == GDK_ACTION_ASK)
   *         {
   *           GtkWidget *dialog;
   *           gint response;
   *
   *           dialog = gtk_message_dialog_new (NULL,
   *                                            GTK_DIALOG_MODAL |
   *                                            GTK_DIALOG_DESTROY_WITH_PARENT,
   *                                            GTK_MESSAGE_INFO,
   *                                            GTK_BUTTONS_YES_NO,
   *                                            "Move the data ?\n");
   *           response = gtk_dialog_run (GTK_DIALOG (dialog));
   *           gtk_widget_destroy (dialog);
   *
   *           if (response == GTK_RESPONSE_YES)
   *             drag_context->action = GDK_ACTION_MOVE;
   *           else
   *             drag_context->action = GDK_ACTION_COPY;
   *          }
   *
   *       gtk_drag_finish (drag_context, TRUE, FALSE, time);
   *       return;
   *     }
   *
   *    gtk_drag_finish (drag_context, FALSE, FALSE, time);
   *  }
   * ]|
   */
  widget_signals[DRAG_DATA_RECEIVED] =
    g_signal_new (I_("drag-data-received"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, drag_data_received),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_INT_INT_BOXED_UINT_UINT,
		  G_TYPE_NONE, 6,
		  GDK_TYPE_DRAG_CONTEXT,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  GTK_TYPE_SELECTION_DATA | G_SIGNAL_TYPE_STATIC_SCOPE,
		  G_TYPE_UINT,
		  G_TYPE_UINT);

  /**
   * GtkWidget::visibility-notify-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventVisibility): the #GdkEventVisibility which
   *   triggered this signal.
   *
   * The ::visibility-notify-event will be emitted when the @widget's window
   * is obscured or unobscured.
   *
   * To receive this signal the #GdkWindow associated to the widget needs
   * to enable the #GDK_VISIBILITY_NOTIFY_MASK mask.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   */
  widget_signals[VISIBILITY_NOTIFY_EVENT] =
    g_signal_new (I_("visibility-notify-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, visibility_notify_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::window-state-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventWindowState): the #GdkEventWindowState which
   *   triggered this signal.
   *
   * The ::window-state-event will be emitted when the state of the
   * toplevel window associated to the @widget changes.
   *
   * To receive this signal the #GdkWindow associated to the widget
   * needs to enable the #GDK_STRUCTURE_MASK mask. GDK will enable
   * this mask automatically for all new windows.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the
   *   event. %FALSE to propagate the event further.
   */
  widget_signals[WINDOW_STATE_EVENT] =
    g_signal_new (I_("window-state-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, window_state_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::damage-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventExpose): the #GdkEventExpose event
   *
   * Emitted when a redirected window belonging to @widget gets drawn into.
   * The region/area members of the event shows what area of the redirected
   * drawable was drawn into.
   *
   * Returns: %TRUE to stop other handlers from being invoked for the event.
   *   %FALSE to propagate the event further.
   *
   * Since: 2.14
   */
  widget_signals[DAMAGE_EVENT] =
    g_signal_new (I_("damage-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, damage_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

/**
   * GtkWidget::grab-broken-event:
   * @widget: the object which received the signal
   * @event: (type Gdk.EventGrabBroken): the #GdkEventGrabBroken event
   *
   * Emitted when a pointer or keyboard grab on a window belonging
   * to @widget gets broken.
   *
   * On X11, this happens when the grab window becomes unviewable
   * (i.e. it or one of its ancestors is unmapped), or if the same
   * application grabs the pointer or keyboard again.
   *
   * Returns: %TRUE to stop other handlers from being invoked for
   *   the event. %FALSE to propagate the event further.
   *
   * Since: 2.8
   */
  widget_signals[GRAB_BROKEN_EVENT] =
    g_signal_new (I_("grab-broken-event"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, grab_broken_event),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__BOXED,
		  G_TYPE_BOOLEAN, 1,
		  GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GtkWidget::query-tooltip:
   * @widget: the object which received the signal
   * @x: the x coordinate of the cursor position where the request has
   *     been emitted, relative to @widget's left side
   * @y: the y coordinate of the cursor position where the request has
   *     been emitted, relative to @widget's top
   * @keyboard_mode: %TRUE if the tooltip was trigged using the keyboard
   * @tooltip: a #GtkTooltip
   *
   * Emitted when #GtkWidget:has-tooltip is %TRUE and the #GtkSettings:gtk-tooltip-timeout
   * has expired with the cursor hovering "above" @widget; or emitted when @widget got
   * focus in keyboard mode.
   *
   * Using the given coordinates, the signal handler should determine
   * whether a tooltip should be shown for @widget. If this is the case
   * %TRUE should be returned, %FALSE otherwise.  Note that if
   * @keyboard_mode is %TRUE, the values of @x and @y are undefined and
   * should not be used.
   *
   * The signal handler is free to manipulate @tooltip with the therefore
   * destined function calls.
   *
   * Returns: %TRUE if @tooltip should be shown right now, %FALSE otherwise.
   *
   * Since: 2.12
   */
  widget_signals[QUERY_TOOLTIP] =
    g_signal_new (I_("query-tooltip"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, query_tooltip),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__INT_INT_BOOLEAN_OBJECT,
		  G_TYPE_BOOLEAN, 4,
		  G_TYPE_INT,
		  G_TYPE_INT,
		  G_TYPE_BOOLEAN,
		  GTK_TYPE_TOOLTIP);

  /**
   * GtkWidget::popup-menu
   * @widget: the object which received the signal
   *
   * This signal gets emitted whenever a widget should pop up a context
   * menu. This usually happens through the standard key binding mechanism;
   * by pressing a certain key while a widget is focused, the user can cause
   * the widget to pop up a menu.  For example, the #GtkEntry widget creates
   * a menu with clipboard commands. See <xref linkend="checklist-popup-menu"/>
   * for an example of how to use this signal.
   *
   * Returns: %TRUE if a menu was activated
   */
  widget_signals[POPUP_MENU] =
    g_signal_new (I_("popup-menu"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, popup_menu),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__VOID,
		  G_TYPE_BOOLEAN, 0);

  /**
   * GtkWidget::show-help:
   * @widget: the object which received the signal.
   * @help_type:
   */
  widget_signals[SHOW_HELP] =
    g_signal_new (I_("show-help"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GtkWidgetClass, show_help),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__ENUM,
		  G_TYPE_BOOLEAN, 1,
		  GTK_TYPE_WIDGET_HELP_TYPE);

  /**
   * GtkWidget::accel-closures-changed:
   * @widget: the object which received the signal.
   */
  widget_signals[ACCEL_CLOSURES_CHANGED] =
    g_signal_new (I_("accel-closures-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  0,
		  0,
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkWidget::screen-changed:
   * @widget: the object on which the signal is emitted
   * @previous_screen: (allow-none): the previous screen, or %NULL if the
   *   widget was not associated with a screen before
   *
   * The ::screen-changed signal gets emitted when the
   * screen of a widget has changed.
   */
  widget_signals[SCREEN_CHANGED] =
    g_signal_new (I_("screen-changed"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, screen_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  GDK_TYPE_SCREEN);

  /**
   * GtkWidget::can-activate-accel:
   * @widget: the object which received the signal
   * @signal_id: the ID of a signal installed on @widget
   *
   * Determines whether an accelerator that activates the signal
   * identified by @signal_id can currently be activated.
   * This signal is present to allow applications and derived
   * widgets to override the default #GtkWidget handling
   * for determining whether an accelerator can be activated.
   *
   * Returns: %TRUE if the signal can be activated.
   */
  widget_signals[CAN_ACTIVATE_ACCEL] =
     g_signal_new (I_("can-activate-accel"),
		  G_TYPE_FROM_CLASS (klass),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkWidgetClass, can_activate_accel),
                  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__UINT,
                  G_TYPE_BOOLEAN, 1, G_TYPE_UINT);

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F10, GDK_SHIFT_MASK,
                                "popup-menu", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Menu, 0,
                                "popup-menu", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F1, GDK_CONTROL_MASK,
                                "show-help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_TOOLTIP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_F1, GDK_CONTROL_MASK,
                                "show-help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_TOOLTIP);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_F1, GDK_SHIFT_MASK,
                                "show-help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_WHATS_THIS);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_F1, GDK_SHIFT_MASK,
                                "show-help", 1,
                                GTK_TYPE_WIDGET_HELP_TYPE,
                                GTK_WIDGET_HELP_WHATS_THIS);

  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boolean ("interior-focus",
								 P_("Interior Focus"),
								 P_("Whether to draw the focus indicator inside widgets"),
								 TRUE,
								 GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (klass,
					   g_param_spec_int ("focus-line-width",
							     P_("Focus linewidth"),
							     P_("Width, in pixels, of the focus indicator line"),
							     0, G_MAXINT, 1,
							     GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (klass,
					   g_param_spec_string ("focus-line-pattern",
								P_("Focus line dash pattern"),
								P_("Dash pattern used to draw the focus indicator"),
								"\1\1",
								GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_int ("focus-padding",
							     P_("Focus padding"),
							     P_("Width, in pixels, between focus indicator and the widget 'box'"),
							     0, G_MAXINT, 1,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("cursor-color",
							       P_("Cursor color"),
							       P_("Color with which to draw insertion cursor"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("secondary-cursor-color",
							       P_("Secondary cursor color"),
							       P_("Color with which to draw the secondary insertion cursor when editing mixed right-to-left and left-to-right text"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_float ("cursor-aspect-ratio",
							       P_("Cursor line aspect ratio"),
							       P_("Aspect ratio with which to draw insertion cursor"),
							       0.0, 1.0, 0.04,
							       GTK_PARAM_READABLE));

  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_boolean ("window-dragging",
                                                                 P_("Window dragging"),
                                                                 P_("Whether windows can be dragged by clicking on empty areas"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkWidget:link-color:
   *
   * The "link-color" style property defines the color of unvisited links.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("link-color",
							       P_("Unvisited Link Color"),
							       P_("Color of unvisited links"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));

  /**
   * GtkWidget:visited-link-color:
   *
   * The "visited-link-color" style property defines the color of visited links.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
					   g_param_spec_boxed ("visited-link-color",
							       P_("Visited Link Color"),
							       P_("Color of visited links"),
							       GDK_TYPE_COLOR,
							       GTK_PARAM_READABLE));

  /**
   * GtkWidget:wide-separators:
   *
   * The "wide-separators" style property defines whether separators have
   * configurable width and should be drawn using a box instead of a line.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_boolean ("wide-separators",
                                                                 P_("Wide Separators"),
                                                                 P_("Whether separators have configurable width and should be drawn using a box instead of a line"),
                                                                 FALSE,
                                                                 GTK_PARAM_READABLE));

  /**
   * GtkWidget:separator-width:
   *
   * The "separator-width" style property defines the width of separators.
   * This property only takes effect if #GtkWidget:wide-separators is %TRUE.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("separator-width",
                                                             P_("Separator Width"),
                                                             P_("The width of separators if wide-separators is TRUE"),
                                                             0, G_MAXINT, 0,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkWidget:separator-height:
   *
   * The "separator-height" style property defines the height of separators.
   * This property only takes effect if #GtkWidget:wide-separators is %TRUE.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("separator-height",
                                                             P_("Separator Height"),
                                                             P_("The height of separators if \"wide-separators\" is TRUE"),
                                                             0, G_MAXINT, 0,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkWidget:scroll-arrow-hlength:
   *
   * The "scroll-arrow-hlength" style property defines the length of
   * horizontal scroll arrows.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("scroll-arrow-hlength",
                                                             P_("Horizontal Scroll Arrow Length"),
                                                             P_("The length of horizontal scroll arrows"),
                                                             1, G_MAXINT, 16,
                                                             GTK_PARAM_READABLE));

  /**
   * GtkWidget:scroll-arrow-vlength:
   *
   * The "scroll-arrow-vlength" style property defines the length of
   * vertical scroll arrows.
   *
   * Since: 2.10
   */
  gtk_widget_class_install_style_property (klass,
                                           g_param_spec_int ("scroll-arrow-vlength",
                                                             P_("Vertical Scroll Arrow Length"),
                                                             P_("The length of vertical scroll arrows"),
                                                             1, G_MAXINT, 16,
                                                             GTK_PARAM_READABLE));

  g_type_class_add_private (klass, sizeof (GtkWidgetPrivate));

  gtk_widget_class_set_accessible_type (klass, GTK_TYPE_WIDGET_ACCESSIBLE);
}

static void
gtk_widget_base_class_finalize (GtkWidgetClass *klass)
{
  GList *list, *node;

  list = g_param_spec_pool_list_owned (style_property_spec_pool, G_OBJECT_CLASS_TYPE (klass));
  for (node = list; node; node = node->next)
    {
      GParamSpec *pspec = node->data;

      g_param_spec_pool_remove (style_property_spec_pool, pspec);
      g_param_spec_unref (pspec);
    }
  g_list_free (list);
}

static void
gtk_widget_set_property (GObject         *object,
			 guint            prop_id,
			 const GValue    *value,
			 GParamSpec      *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);

  switch (prop_id)
    {
      gboolean tmp;
      gchar *tooltip_markup;
      const gchar *tooltip_text;
      GtkWindow *tooltip_window;

    case PROP_NAME:
      gtk_widget_set_name (widget, g_value_get_string (value));
      break;
    case PROP_PARENT:
      gtk_container_add (GTK_CONTAINER (g_value_get_object (value)), widget);
      break;
    case PROP_WIDTH_REQUEST:
      gtk_widget_set_usize_internal (widget, g_value_get_int (value), -2, 0);
      break;
    case PROP_HEIGHT_REQUEST:
      gtk_widget_set_usize_internal (widget, -2, g_value_get_int (value), 0);
      break;
    case PROP_VISIBLE:
      gtk_widget_set_visible (widget, g_value_get_boolean (value));
      break;
    case PROP_SENSITIVE:
      gtk_widget_set_sensitive (widget, g_value_get_boolean (value));
      break;
    case PROP_APP_PAINTABLE:
      gtk_widget_set_app_paintable (widget, g_value_get_boolean (value));
      break;
    case PROP_CAN_FOCUS:
      gtk_widget_set_can_focus (widget, g_value_get_boolean (value));
      break;
    case PROP_HAS_FOCUS:
      if (g_value_get_boolean (value))
	gtk_widget_grab_focus (widget);
      break;
    case PROP_IS_FOCUS:
      if (g_value_get_boolean (value))
	gtk_widget_grab_focus (widget);
      break;
    case PROP_CAN_DEFAULT:
      gtk_widget_set_can_default (widget, g_value_get_boolean (value));
      break;
    case PROP_HAS_DEFAULT:
      if (g_value_get_boolean (value))
	gtk_widget_grab_default (widget);
      break;
    case PROP_RECEIVES_DEFAULT:
      gtk_widget_set_receives_default (widget, g_value_get_boolean (value));
      break;
    case PROP_STYLE:
      gtk_widget_set_style (widget, g_value_get_object (value));
      break;
    case PROP_EVENTS:
      if (!gtk_widget_get_realized (widget) && gtk_widget_get_has_window (widget))
	gtk_widget_set_events (widget, g_value_get_flags (value));
      break;
    case PROP_NO_SHOW_ALL:
      gtk_widget_set_no_show_all (widget, g_value_get_boolean (value));
      break;
    case PROP_HAS_TOOLTIP:
      gtk_widget_real_set_has_tooltip (widget,
				       g_value_get_boolean (value), FALSE);
      break;
    case PROP_TOOLTIP_MARKUP:
      tooltip_window = g_object_get_qdata (object, quark_tooltip_window);
      tooltip_markup = g_value_dup_string (value);

      /* Treat an empty string as a NULL string,
       * because an empty string would be useless for a tooltip:
       */
      if (tooltip_markup && (strlen (tooltip_markup) == 0))
        {
	  g_free (tooltip_markup);
          tooltip_markup = NULL;
        }

      g_object_set_qdata_full (object, quark_tooltip_markup,
			       tooltip_markup, g_free);

      tmp = (tooltip_window != NULL || tooltip_markup != NULL);
      gtk_widget_real_set_has_tooltip (widget, tmp, FALSE);
      if (gtk_widget_get_visible (widget))
        gtk_widget_queue_tooltip_query (widget);
      break;
    case PROP_TOOLTIP_TEXT:
      tooltip_window = g_object_get_qdata (object, quark_tooltip_window);

      tooltip_text = g_value_get_string (value);

      /* Treat an empty string as a NULL string,
       * because an empty string would be useless for a tooltip:
       */
      if (tooltip_text && (strlen (tooltip_text) == 0))
        tooltip_text = NULL;

      tooltip_markup = tooltip_text ? g_markup_escape_text (tooltip_text, -1) : NULL;

      g_object_set_qdata_full (object, quark_tooltip_markup,
                               tooltip_markup, g_free);

      tmp = (tooltip_window != NULL || tooltip_markup != NULL);
      gtk_widget_real_set_has_tooltip (widget, tmp, FALSE);
      if (gtk_widget_get_visible (widget))
        gtk_widget_queue_tooltip_query (widget);
      break;
    case PROP_DOUBLE_BUFFERED:
      gtk_widget_set_double_buffered (widget, g_value_get_boolean (value));
      break;
    case PROP_HALIGN:
      gtk_widget_set_halign (widget, g_value_get_enum (value));
      break;
    case PROP_VALIGN:
      gtk_widget_set_valign (widget, g_value_get_enum (value));
      break;
    case PROP_MARGIN_LEFT:
      gtk_widget_set_margin_left (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_RIGHT:
      gtk_widget_set_margin_right (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_TOP:
      gtk_widget_set_margin_top (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN_BOTTOM:
      gtk_widget_set_margin_bottom (widget, g_value_get_int (value));
      break;
    case PROP_MARGIN:
      g_object_freeze_notify (G_OBJECT (widget));
      gtk_widget_set_margin_left (widget, g_value_get_int (value));
      gtk_widget_set_margin_right (widget, g_value_get_int (value));
      gtk_widget_set_margin_top (widget, g_value_get_int (value));
      gtk_widget_set_margin_bottom (widget, g_value_get_int (value));
      g_object_thaw_notify (G_OBJECT (widget));
      break;
    case PROP_HEXPAND:
      gtk_widget_set_hexpand (widget, g_value_get_boolean (value));
      break;
    case PROP_HEXPAND_SET:
      gtk_widget_set_hexpand_set (widget, g_value_get_boolean (value));
      break;
    case PROP_VEXPAND:
      gtk_widget_set_vexpand (widget, g_value_get_boolean (value));
      break;
    case PROP_VEXPAND_SET:
      gtk_widget_set_vexpand_set (widget, g_value_get_boolean (value));
      break;
    case PROP_EXPAND:
      g_object_freeze_notify (G_OBJECT (widget));
      gtk_widget_set_hexpand (widget, g_value_get_boolean (value));
      gtk_widget_set_vexpand (widget, g_value_get_boolean (value));
      g_object_thaw_notify (G_OBJECT (widget));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_get_property (GObject         *object,
			 guint            prop_id,
			 GValue          *value,
			 GParamSpec      *pspec)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;

  switch (prop_id)
    {
      gpointer *eventp;

    case PROP_NAME:
      if (priv->name)
	g_value_set_string (value, priv->name);
      else
	g_value_set_static_string (value, "");
      break;
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    case PROP_WIDTH_REQUEST:
      {
        int w;
        gtk_widget_get_size_request (widget, &w, NULL);
        g_value_set_int (value, w);
      }
      break;
    case PROP_HEIGHT_REQUEST:
      {
        int h;
        gtk_widget_get_size_request (widget, NULL, &h);
        g_value_set_int (value, h);
      }
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, (gtk_widget_get_visible (widget) != FALSE));
      break;
    case PROP_SENSITIVE:
      g_value_set_boolean (value, (gtk_widget_get_sensitive (widget) != FALSE));
      break;
    case PROP_APP_PAINTABLE:
      g_value_set_boolean (value, (gtk_widget_get_app_paintable (widget) != FALSE));
      break;
    case PROP_CAN_FOCUS:
      g_value_set_boolean (value, (gtk_widget_get_can_focus (widget) != FALSE));
      break;
    case PROP_HAS_FOCUS:
      g_value_set_boolean (value, (gtk_widget_has_focus (widget) != FALSE));
      break;
    case PROP_IS_FOCUS:
      g_value_set_boolean (value, (gtk_widget_is_focus (widget)));
      break;
    case PROP_CAN_DEFAULT:
      g_value_set_boolean (value, (gtk_widget_get_can_default (widget) != FALSE));
      break;
    case PROP_HAS_DEFAULT:
      g_value_set_boolean (value, (gtk_widget_has_default (widget) != FALSE));
      break;
    case PROP_RECEIVES_DEFAULT:
      g_value_set_boolean (value, (gtk_widget_get_receives_default (widget) != FALSE));
      break;
    case PROP_COMPOSITE_CHILD:
      g_value_set_boolean (value, widget->priv->composite_child);
      break;
    case PROP_STYLE:
      g_value_set_object (value, gtk_widget_get_style (widget));
      break;
    case PROP_EVENTS:
      eventp = g_object_get_qdata (G_OBJECT (widget), quark_event_mask);
      g_value_set_flags (value, GPOINTER_TO_INT (eventp));
      break;
    case PROP_NO_SHOW_ALL:
      g_value_set_boolean (value, gtk_widget_get_no_show_all (widget));
      break;
    case PROP_HAS_TOOLTIP:
      g_value_set_boolean (value, GPOINTER_TO_UINT (g_object_get_qdata (object, quark_has_tooltip)));
      break;
    case PROP_TOOLTIP_TEXT:
      {
        gchar *escaped = g_object_get_qdata (object, quark_tooltip_markup);
        gchar *text = NULL;

        if (escaped && !pango_parse_markup (escaped, -1, 0, NULL, &text, NULL, NULL))
          g_assert (NULL == text); /* text should still be NULL in case of markup errors */

        g_value_take_string (value, text);
      }
      break;
    case PROP_TOOLTIP_MARKUP:
      g_value_set_string (value, g_object_get_qdata (object, quark_tooltip_markup));
      break;
    case PROP_WINDOW:
      g_value_set_object (value, gtk_widget_get_window (widget));
      break;
    case PROP_DOUBLE_BUFFERED:
      g_value_set_boolean (value, gtk_widget_get_double_buffered (widget));
      break;
    case PROP_HALIGN:
      g_value_set_enum (value, gtk_widget_get_halign (widget));
      break;
    case PROP_VALIGN:
      g_value_set_enum (value, gtk_widget_get_valign (widget));
      break;
    case PROP_MARGIN_LEFT:
      g_value_set_int (value, gtk_widget_get_margin_left (widget));
      break;
    case PROP_MARGIN_RIGHT:
      g_value_set_int (value, gtk_widget_get_margin_right (widget));
      break;
    case PROP_MARGIN_TOP:
      g_value_set_int (value, gtk_widget_get_margin_top (widget));
      break;
    case PROP_MARGIN_BOTTOM:
      g_value_set_int (value, gtk_widget_get_margin_bottom (widget));
      break;
    case PROP_MARGIN:
      {
        GtkWidgetAuxInfo *aux_info = gtk_widget_get_aux_info (widget, FALSE);
        if (aux_info == NULL)
          {
            g_value_set_int (value, 0);
          }
        else
          {
            g_value_set_int (value, MAX (MAX (aux_info->margin.left,
                                              aux_info->margin.right),
                                         MAX (aux_info->margin.top,
                                              aux_info->margin.bottom)));
          }
      }
      break;
    case PROP_HEXPAND:
      g_value_set_boolean (value, gtk_widget_get_hexpand (widget));
      break;
    case PROP_HEXPAND_SET:
      g_value_set_boolean (value, gtk_widget_get_hexpand_set (widget));
      break;
    case PROP_VEXPAND:
      g_value_set_boolean (value, gtk_widget_get_vexpand (widget));
      break;
    case PROP_VEXPAND_SET:
      g_value_set_boolean (value, gtk_widget_get_vexpand_set (widget));
      break;
    case PROP_EXPAND:
      g_value_set_boolean (value,
                           gtk_widget_get_hexpand (widget) &&
                           gtk_widget_get_vexpand (widget));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_widget_init (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  widget->priv = G_TYPE_INSTANCE_GET_PRIVATE (widget,
                                              GTK_TYPE_WIDGET,
                                              GtkWidgetPrivate);
  priv = widget->priv;

  priv->child_visible = TRUE;
  priv->name = NULL;
  priv->allocation.x = -1;
  priv->allocation.y = -1;
  priv->allocation.width = 1;
  priv->allocation.height = 1;
  priv->window = NULL;
  priv->parent = NULL;

  priv->sensitive = TRUE;
  priv->composite_child = composite_child_stack != 0;
  priv->double_buffered = TRUE;
  priv->redraw_on_alloc = TRUE;
  priv->width_request_needed = TRUE;
  priv->height_request_needed = TRUE;
  priv->alloc_needed = TRUE;

  /* this will be set to TRUE if the widget gets a child or if the
   * expand flag is set on the widget, but until one of those happen
   * we know the expand is already properly FALSE.
   *
   * We really want to default FALSE here to avoid computing expand
   * all over the place while initially building a widget tree.
   */
  priv->need_compute_expand = FALSE;

  priv->style = gtk_widget_get_default_style ();
  g_object_ref (priv->style);
}


static void
gtk_widget_dispatch_child_properties_changed (GtkWidget   *widget,
					      guint        n_pspecs,
					      GParamSpec **pspecs)
{
  GtkWidgetPrivate *priv = widget->priv;
  GtkWidget *container = priv->parent;
  guint i;

  for (i = 0; widget->priv->parent == container && i < n_pspecs; i++)
    g_signal_emit (widget, widget_signals[CHILD_NOTIFY], g_quark_from_string (pspecs[i]->name), pspecs[i]);
}

/**
 * gtk_widget_freeze_child_notify:
 * @widget: a #GtkWidget
 *
 * Stops emission of #GtkWidget::child-notify signals on @widget. The
 * signals are queued until gtk_widget_thaw_child_notify() is called
 * on @widget.
 *
 * This is the analogue of g_object_freeze_notify() for child properties.
 **/
void
gtk_widget_freeze_child_notify (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!G_OBJECT (widget)->ref_count)
    return;

  g_object_ref (widget);
  g_object_notify_queue_freeze (G_OBJECT (widget), _gtk_widget_child_property_notify_context);
  g_object_unref (widget);
}

/**
 * gtk_widget_child_notify:
 * @widget: a #GtkWidget
 * @child_property: the name of a child property installed on the
 *                  class of @widget<!-- -->'s parent
 *
 * Emits a #GtkWidget::child-notify signal for the
 * <link linkend="child-properties">child property</link> @child_property
 * on @widget.
 *
 * This is the analogue of g_object_notify() for child properties.
 *
 * Also see gtk_container_child_notify().
 */
void
gtk_widget_child_notify (GtkWidget    *widget,
                         const gchar  *child_property)
{
  if (widget->priv->parent == NULL)
    return;

  gtk_container_child_notify (GTK_CONTAINER (widget->priv->parent), widget, child_property);
}

/**
 * gtk_widget_thaw_child_notify:
 * @widget: a #GtkWidget
 *
 * Reverts the effect of a previous call to gtk_widget_freeze_child_notify().
 * This causes all queued #GtkWidget::child-notify signals on @widget to be
 * emitted.
 */
void
gtk_widget_thaw_child_notify (GtkWidget *widget)
{
  GObjectNotifyQueue *nqueue;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!G_OBJECT (widget)->ref_count)
    return;

  g_object_ref (widget);
  nqueue = g_object_notify_queue_from_object (G_OBJECT (widget), _gtk_widget_child_property_notify_context);
  if (!nqueue || !nqueue->freeze_count)
    g_warning (G_STRLOC ": child-property-changed notification for %s(%p) is not frozen",
	       G_OBJECT_TYPE_NAME (widget), widget);
  else
    g_object_notify_queue_thaw (G_OBJECT (widget), nqueue);
  g_object_unref (widget);
}


/**
 * gtk_widget_new:
 * @type: type ID of the widget to create
 * @first_property_name: name of first property to set
 * @...: value of first property, followed by more properties,
 *     %NULL-terminated
 *
 * This is a convenience function for creating a widget and setting
 * its properties in one go. For example you might write:
 * <literal>gtk_widget_new (GTK_TYPE_LABEL, "label", "Hello World", "xalign",
 * 0.0, NULL)</literal> to create a left-aligned label. Equivalent to
 * g_object_new(), but returns a widget so you don't have to
 * cast the object yourself.
 *
 * Return value: a new #GtkWidget of type @widget_type
 **/
GtkWidget*
gtk_widget_new (GType        type,
		const gchar *first_property_name,
		...)
{
  GtkWidget *widget;
  va_list var_args;

  g_return_val_if_fail (g_type_is_a (type, GTK_TYPE_WIDGET), NULL);

  va_start (var_args, first_property_name);
  widget = (GtkWidget *)g_object_new_valist (type, first_property_name, var_args);
  va_end (var_args);

  return widget;
}

static inline void
gtk_widget_queue_draw_child (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;
  GtkWidget *parent;

  parent = priv->parent;
  if (parent && gtk_widget_is_drawable (parent))
    gtk_widget_queue_draw_area (parent,
				priv->allocation.x,
				priv->allocation.y,
				priv->allocation.width,
				priv->allocation.height);
}

/**
 * gtk_widget_unparent:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations.
 * Should be called by implementations of the remove method
 * on #GtkContainer, to dissociate a child from the container.
 **/
void
gtk_widget_unparent (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  GObjectNotifyQueue *nqueue;
  GtkWidget *toplevel;
  GtkWidget *old_parent;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (priv->parent == NULL)
    return;

  /* keep this function in sync with gtk_menu_detach() */

  gtk_widget_push_verify_invariants (widget);

  g_object_freeze_notify (G_OBJECT (widget));
  nqueue = g_object_notify_queue_freeze (G_OBJECT (widget), _gtk_widget_child_property_notify_context);

  toplevel = gtk_widget_get_toplevel (widget);
  if (gtk_widget_is_toplevel (toplevel))
    _gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);

  if (gtk_container_get_focus_child (GTK_CONTAINER (priv->parent)) == widget)
    gtk_container_set_focus_child (GTK_CONTAINER (priv->parent), NULL);

  gtk_widget_queue_draw_child (widget);

  /* Reset the width and height here, to force reallocation if we
   * get added back to a new parent. This won't work if our new
   * allocation is smaller than 1x1 and we actually want a size of 1x1...
   * (would 0x0 be OK here?)
   */
  priv->allocation.width = 1;
  priv->allocation.height = 1;

  if (gtk_widget_get_realized (widget))
    {
      if (priv->in_reparent)
	gtk_widget_unmap (widget);
      else
	gtk_widget_unrealize (widget);
    }

  /* If we are unanchoring the child, we save around the toplevel
   * to emit hierarchy changed
   */
  if (priv->parent->priv->anchored)
    g_object_ref (toplevel);
  else
    toplevel = NULL;

  /* Removing a widget from a container restores the child visible
   * flag to the default state, so it doesn't affect the child
   * in the next parent.
   */
  priv->child_visible = TRUE;

  old_parent = priv->parent;
  priv->parent = NULL;

  /* parent may no longer expand if the removed
   * child was expand=TRUE and could therefore
   * be forcing it to.
   */
  if (gtk_widget_get_visible (widget) &&
      (priv->need_compute_expand ||
       priv->computed_hexpand ||
       priv->computed_vexpand))
    {
      gtk_widget_queue_compute_expand (old_parent);
    }

  g_signal_emit (widget, widget_signals[PARENT_SET], 0, old_parent);
  if (toplevel)
    {
      _gtk_widget_propagate_hierarchy_changed (widget, toplevel);
      g_object_unref (toplevel);
    }

  /* Now that the parent pointer is nullified and the hierarchy-changed
   * already passed, go ahead and unset the parent window, if we are unparenting
   * an embeded GtkWindow the window will become toplevel again and hierarchy-changed
   * will fire again for the new subhierarchy.
   */
  gtk_widget_set_parent_window (widget, NULL);

  g_object_notify (G_OBJECT (widget), "parent");
  g_object_thaw_notify (G_OBJECT (widget));
  if (!priv->parent)
    g_object_notify_queue_clear (G_OBJECT (widget), nqueue);
  g_object_notify_queue_thaw (G_OBJECT (widget), nqueue);

  gtk_widget_pop_verify_invariants (widget);
  g_object_unref (widget);
}

/**
 * gtk_widget_destroy:
 * @widget: a #GtkWidget
 *
 * Destroys a widget.
 *
 * When a widget is
 * destroyed, it will break any references it holds to other objects.
 * If the widget is inside a container, the widget will be removed
 * from the container. If the widget is a toplevel (derived from
 * #GtkWindow), it will be removed from the list of toplevels, and the
 * reference GTK+ holds to it will be removed. Removing a
 * widget from its container or the list of toplevels results in the
 * widget being finalized, unless you've added additional references
 * to the widget with g_object_ref().
 *
 * In most cases, only toplevel widgets (windows) require explicit
 * destruction, because when you destroy a toplevel its children will
 * be destroyed as well.
 **/
void
gtk_widget_destroy (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!widget->priv->in_destruction)
    g_object_run_dispose (G_OBJECT (widget));
}

/**
 * gtk_widget_destroyed:
 * @widget: a #GtkWidget
 * @widget_pointer: (inout) (transfer none): address of a variable that contains @widget
 *
 * This function sets *@widget_pointer to %NULL if @widget_pointer !=
 * %NULL.  It's intended to be used as a callback connected to the
 * "destroy" signal of a widget. You connect gtk_widget_destroyed()
 * as a signal handler, and pass the address of your widget variable
 * as user data. Then when the widget is destroyed, the variable will
 * be set to %NULL. Useful for example to avoid multiple copies
 * of the same dialog.
 **/
void
gtk_widget_destroyed (GtkWidget      *widget,
		      GtkWidget      **widget_pointer)
{
  /* Don't make any assumptions about the
   *  value of widget!
   *  Even check widget_pointer.
   */
  if (widget_pointer)
    *widget_pointer = NULL;
}

/**
 * gtk_widget_show:
 * @widget: a #GtkWidget
 *
 * Flags a widget to be displayed. Any widget that isn't shown will
 * not appear on the screen. If you want to show all the widgets in a
 * container, it's easier to call gtk_widget_show_all() on the
 * container, instead of individually showing the widgets.
 *
 * Remember that you have to show the containers containing a widget,
 * in addition to the widget itself, before it will appear onscreen.
 *
 * When a toplevel container is shown, it is immediately realized and
 * mapped; other shown widgets are realized and mapped when their
 * toplevel container is realized and mapped.
 **/
void
gtk_widget_show (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!gtk_widget_get_visible (widget))
    {
      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      if (!gtk_widget_is_toplevel (widget))
        gtk_widget_queue_resize (widget);

      /* see comment in set_parent() for why this should and can be
       * conditional
       */
      if (widget->priv->need_compute_expand ||
          widget->priv->computed_hexpand ||
          widget->priv->computed_vexpand)
        {
          if (widget->priv->parent != NULL)
            gtk_widget_queue_compute_expand (widget->priv->parent);
        }

      g_signal_emit (widget, widget_signals[SHOW], 0);
      g_object_notify (G_OBJECT (widget), "visible");

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_show (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (!gtk_widget_get_visible (widget))
    {
      priv->visible = TRUE;

      if (priv->parent &&
	  gtk_widget_get_mapped (priv->parent) &&
          gtk_widget_get_child_visible (widget) &&
	  !gtk_widget_get_mapped (widget))
	gtk_widget_map (widget);
    }
}

static void
gtk_widget_show_map_callback (GtkWidget *widget, GdkEvent *event, gint *flag)
{
  *flag = TRUE;
  g_signal_handlers_disconnect_by_func (widget,
					gtk_widget_show_map_callback,
					flag);
}

/**
 * gtk_widget_show_now:
 * @widget: a #GtkWidget
 *
 * Shows a widget. If the widget is an unmapped toplevel widget
 * (i.e. a #GtkWindow that has not yet been shown), enter the main
 * loop and wait for the window to actually be mapped. Be careful;
 * because the main loop is running, anything can happen during
 * this function.
 **/
void
gtk_widget_show_now (GtkWidget *widget)
{
  gint flag = FALSE;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  /* make sure we will get event */
  if (!gtk_widget_get_mapped (widget) &&
      gtk_widget_is_toplevel (widget))
    {
      gtk_widget_show (widget);

      g_signal_connect (widget, "map-event",
			G_CALLBACK (gtk_widget_show_map_callback),
			&flag);

      while (!flag)
	gtk_main_iteration ();
    }
  else
    gtk_widget_show (widget);
}

/**
 * gtk_widget_hide:
 * @widget: a #GtkWidget
 *
 * Reverses the effects of gtk_widget_show(), causing the widget to be
 * hidden (invisible to the user).
 **/
void
gtk_widget_hide (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_visible (widget))
    {
      GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

      g_object_ref (widget);
      gtk_widget_push_verify_invariants (widget);

      if (toplevel != widget && gtk_widget_is_toplevel (toplevel))
        _gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);

      /* a parent may now be expand=FALSE since we're hidden. */
      if (widget->priv->need_compute_expand ||
          widget->priv->computed_hexpand ||
          widget->priv->computed_vexpand)
        {
          gtk_widget_queue_compute_expand (widget);
        }

      g_signal_emit (widget, widget_signals[HIDE], 0);
      if (!gtk_widget_is_toplevel (widget))
	gtk_widget_queue_resize (widget);
      g_object_notify (G_OBJECT (widget), "visible");

      gtk_widget_pop_verify_invariants (widget);
      g_object_unref (widget);
    }
}

static void
gtk_widget_real_hide (GtkWidget *widget)
{
  if (gtk_widget_get_visible (widget))
    {
      widget->priv->visible = FALSE;

      if (gtk_widget_get_mapped (widget))
	gtk_widget_unmap (widget);
    }
}

/**
 * gtk_widget_hide_on_delete:
 * @widget: a #GtkWidget
 *
 * Utility function; intended to be connected to the #GtkWidget::delete-event
 * signal on a #GtkWindow. The function calls gtk_widget_hide() on its
 * argument, then returns %TRUE. If connected to ::delete-event, the
 * result is that clicking the close button for a window (on the
 * window frame, top right corner usually) will hide but not destroy
 * the window. By default, GTK+ destroys windows when ::delete-event
 * is received.
 *
 * Return value: %TRUE
 **/
gboolean
gtk_widget_hide_on_delete (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  gtk_widget_hide (widget);

  return TRUE;
}

/**
 * gtk_widget_show_all:
 * @widget: a #GtkWidget
 *
 * Recursively shows a widget, and any child widgets (if the widget is
 * a container).
 **/
void
gtk_widget_show_all (GtkWidget *widget)
{
  GtkWidgetClass *class;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_no_show_all (widget))
    return;

  class = GTK_WIDGET_GET_CLASS (widget);

  if (class->show_all)
    class->show_all (widget);
}

static void
_gtk_widget_notify_state_change (GtkWidget     *widget,
                                 GtkStateFlags  flag,
                                 gboolean       target)
{
  GtkStateType state;

  switch (flag)
    {
    case GTK_STATE_FLAG_ACTIVE:
      state = GTK_STATE_ACTIVE;
      break;
    case GTK_STATE_FLAG_PRELIGHT:
      state = GTK_STATE_PRELIGHT;
      break;
    case GTK_STATE_FLAG_SELECTED:
      state = GTK_STATE_SELECTED;
      break;
    case GTK_STATE_FLAG_INSENSITIVE:
      state = GTK_STATE_INSENSITIVE;
      break;
    case GTK_STATE_FLAG_INCONSISTENT:
      state = GTK_STATE_INCONSISTENT;
      break;
    case GTK_STATE_FLAG_FOCUSED:
      state = GTK_STATE_FOCUSED;
      break;
    default:
      return;
    }

  gtk_style_context_notify_state_change (widget->priv->context,
                                         gtk_widget_get_window (widget),
                                         NULL, state, target);
}

/* Initializes state transitions for those states that
 * were enabled before mapping and have a looping animation.
 */
static void
_gtk_widget_start_state_transitions (GtkWidget *widget)
{
  GtkStateFlags state, flag;

  if (!widget->priv->context)
    return;

  state = gtk_widget_get_state_flags (widget);
  flag = GTK_STATE_FLAG_FOCUSED;

  while (flag)
    {
      GtkAnimationDescription *animation_desc;

      if ((state & flag) == 0)
        {
          flag >>= 1;
          continue;
        }

      gtk_style_context_get (widget->priv->context, state,
                             "transition", &animation_desc,
                             NULL);

      if (animation_desc)
        {
          if (_gtk_animation_description_get_loop (animation_desc))
            _gtk_widget_notify_state_change (widget, flag, TRUE);

          _gtk_animation_description_unref (animation_desc);
        }

      flag >>= 1;
    }
}

/**
 * gtk_widget_map:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations. Causes
 * a widget to be mapped if it isn't already.
 **/
void
gtk_widget_map (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_visible (widget));
  g_return_if_fail (gtk_widget_get_child_visible (widget));

  priv = widget->priv;

  if (!gtk_widget_get_mapped (widget))
    {
      gtk_widget_push_verify_invariants (widget);

      if (!gtk_widget_get_realized (widget))
        gtk_widget_realize (widget);

      g_signal_emit (widget, widget_signals[MAP], 0);

      if (!gtk_widget_get_has_window (widget))
        gdk_window_invalidate_rect (priv->window, &priv->allocation, FALSE);

      gtk_widget_pop_verify_invariants (widget);

      _gtk_widget_start_state_transitions (widget);
    }
}

/**
 * gtk_widget_unmap:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations. Causes
 * a widget to be unmapped if it's currently mapped.
 **/
void
gtk_widget_unmap (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (gtk_widget_get_mapped (widget))
    {
      gtk_widget_push_verify_invariants (widget);

      if (!gtk_widget_get_has_window (widget))
	gdk_window_invalidate_rect (priv->window, &priv->allocation, FALSE);
      _gtk_tooltip_hide (widget);
      g_signal_emit (widget, widget_signals[UNMAP], 0);

      gtk_widget_pop_verify_invariants (widget);

      if (priv->context)
        gtk_style_context_cancel_animations (priv->context, NULL);

      /* Unset pointer/window info */
      g_object_set_qdata (G_OBJECT (widget), quark_pointer_window, NULL);
    }
}

static void
_gtk_widget_enable_device_events (GtkWidget *widget)
{
  GHashTable *device_events;
  GHashTableIter iter;
  gpointer key, value;

  device_events = g_object_get_qdata (G_OBJECT (widget), quark_device_event_mask);

  if (!device_events)
    return;

  g_hash_table_iter_init (&iter, device_events);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkDevice *device;
      GdkEventMask event_mask;

      device = key;
      event_mask = GPOINTER_TO_UINT (value);
      gtk_widget_add_events_internal (widget, device, event_mask);
    }
}

static GList *
get_widget_windows (GtkWidget *widget)
{
  GList *window_list, *last, *l, *children, *ret;

  if (gtk_widget_get_has_window (widget))
    window_list = g_list_prepend (NULL, gtk_widget_get_window (widget));
  else
    window_list = gdk_window_peek_children (gtk_widget_get_window (widget));

  last = g_list_last (window_list);
  ret = NULL;

  for (l = window_list; l; l = l->next)
    {
      GtkWidget *window_widget = NULL;

      gdk_window_get_user_data (l->data, (gpointer *) &window_widget);

      if (widget != window_widget)
        continue;

      ret = g_list_prepend (ret, l->data);
      children = gdk_window_peek_children (GDK_WINDOW (l->data));

      if (children)
        {
          last = g_list_concat (last, children);
          last = g_list_last (last);
        }
    }

  g_list_free (window_list);

  return ret;
}

static void
device_enable_foreach (GtkWidget *widget,
                       gpointer   user_data)
{
  GdkDevice *device = user_data;
  gtk_widget_set_device_enabled_internal (widget, device, TRUE, TRUE);
}

static void
device_disable_foreach (GtkWidget *widget,
                        gpointer   user_data)
{
  GdkDevice *device = user_data;
  gtk_widget_set_device_enabled_internal (widget, device, TRUE, FALSE);
}

static void
gtk_widget_set_device_enabled_internal (GtkWidget *widget,
                                        GdkDevice *device,
                                        gboolean   recurse,
                                        gboolean   enabled)
{
  GList *window_list, *l;

  window_list = get_widget_windows (widget);

  for (l = window_list; l; l = l->next)
    {
      GdkEventMask events = 0;
      GdkWindow *window;

      window = l->data;

      if (enabled)
        events = gdk_window_get_events (window);

      gdk_window_set_device_events (window, device, events);
    }

  if (recurse && GTK_IS_CONTAINER (widget))
    {
      if (enabled)
        gtk_container_forall (GTK_CONTAINER (widget), device_enable_foreach, device);
      else
        gtk_container_forall (GTK_CONTAINER (widget), device_disable_foreach, device);
    }

  g_list_free (window_list);
}

static void
gtk_widget_update_devices_mask (GtkWidget *widget,
                                gboolean   recurse)
{
  GList *enabled_devices, *l;

  enabled_devices = g_object_get_qdata (G_OBJECT (widget), quark_enabled_devices);

  for (l = enabled_devices; l; l = l->next)
    gtk_widget_set_device_enabled_internal (widget, GDK_DEVICE (l->data), recurse, TRUE);
}

/**
 * gtk_widget_realize:
 * @widget: a #GtkWidget
 *
 * Creates the GDK (windowing system) resources associated with a
 * widget.  For example, @widget->window will be created when a widget
 * is realized.  Normally realization happens implicitly; if you show
 * a widget and all its parent containers, then the widget will be
 * realized and mapped automatically.
 *
 * Realizing a widget requires all
 * the widget's parent widgets to be realized; calling
 * gtk_widget_realize() realizes the widget's parents in addition to
 * @widget itself. If a widget is not yet inside a toplevel window
 * when you realize it, bad things will happen.
 *
 * This function is primarily used in widget implementations, and
 * isn't very useful otherwise. Many times when you think you might
 * need it, a better approach is to connect to a signal that will be
 * called after the widget is realized automatically, such as
 * #GtkWidget::draw. Or simply g_signal_connect () to the
 * #GtkWidget::realize signal.
 **/
void
gtk_widget_realize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  cairo_region_t *region;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->priv->anchored ||
		    GTK_IS_INVISIBLE (widget));

  priv = widget->priv;

  if (!gtk_widget_get_realized (widget))
    {
      gtk_widget_push_verify_invariants (widget);

      /*
	if (GTK_IS_CONTAINER (widget) && gtk_widget_get_has_window (widget))
	  g_message ("gtk_widget_realize(%s)", G_OBJECT_TYPE_NAME (widget));
      */

      if (priv->parent == NULL &&
          !gtk_widget_is_toplevel (widget))
        g_warning ("Calling gtk_widget_realize() on a widget that isn't "
                   "inside a toplevel window is not going to work very well. "
                   "Widgets must be inside a toplevel container before realizing them.");

      if (priv->parent && !gtk_widget_get_realized (priv->parent))
	gtk_widget_realize (priv->parent);

      gtk_widget_ensure_style (widget);

      if (priv->style_update_pending)
        g_signal_emit (widget, widget_signals[STYLE_UPDATED], 0);

      g_signal_emit (widget, widget_signals[REALIZE], 0);

      gtk_widget_real_set_has_tooltip (widget,
				       GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (widget), quark_has_tooltip)),
				       TRUE);

      if (priv->has_shape_mask)
	{
	  region = g_object_get_qdata (G_OBJECT (widget), quark_shape_info);
	  gdk_window_shape_combine_region (priv->window, region, 0, 0);
	}

      region = g_object_get_qdata (G_OBJECT (widget), quark_input_shape_info);
      if (region)
	gdk_window_input_shape_combine_region (priv->window, region, 0, 0);

      if (priv->multidevice)
        gdk_window_set_support_multidevice (priv->window, TRUE);

      _gtk_widget_enable_device_events (widget);
      gtk_widget_update_devices_mask (widget, TRUE);

      gtk_widget_pop_verify_invariants (widget);
    }
}

/**
 * gtk_widget_unrealize:
 * @widget: a #GtkWidget
 *
 * This function is only useful in widget implementations.
 * Causes a widget to be unrealized (frees all GDK resources
 * associated with the widget, such as @widget->window).
 **/
void
gtk_widget_unrealize (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_push_verify_invariants (widget);

  if (widget->priv->has_shape_mask)
    gtk_widget_shape_combine_region (widget, NULL);

  if (g_object_get_qdata (G_OBJECT (widget), quark_input_shape_info))
    gtk_widget_input_shape_combine_region (widget, NULL);

  if (gtk_widget_get_realized (widget))
    {
      g_object_ref (widget);

      if (widget->priv->mapped)
        gtk_widget_unmap (widget);

      g_signal_emit (widget, widget_signals[UNREALIZE], 0);
      g_assert (!widget->priv->mapped);
      gtk_widget_set_realized (widget, FALSE);

      g_object_unref (widget);
    }

  gtk_widget_pop_verify_invariants (widget);
}

/*****************************************
 * Draw queueing.
 *****************************************/

/**
 * gtk_widget_queue_draw_region:
 * @widget: a #GtkWidget
 * @region: region to draw
 *
 * Invalidates the rectangular area of @widget defined by @region by
 * calling gdk_window_invalidate_region() on the widget's window and
 * all its child windows. Once the main loop becomes idle (after the
 * current batch of events has been processed, roughly), the window
 * will receive expose events for the union of all regions that have
 * been invalidated.
 *
 * Normally you would only use this function in widget
 * implementations. You might also use it to schedule a redraw of a
 * #GtkDrawingArea or some portion thereof.
 *
 * Since: 3.0
 **/
void
gtk_widget_queue_draw_region (GtkWidget            *widget,
                              const cairo_region_t *region)
{
  GtkWidgetPrivate *priv;
  GtkWidget *w;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (!gtk_widget_get_realized (widget))
    return;

  /* Just return if the widget or one of its ancestors isn't mapped */
  for (w = widget; w != NULL; w = w->priv->parent)
    if (!gtk_widget_get_mapped (w))
      return;

  gdk_window_invalidate_region (priv->window, region, TRUE);
}

/**
 * gtk_widget_queue_draw_area:
 * @widget: a #GtkWidget
 * @x: x coordinate of upper-left corner of rectangle to redraw
 * @y: y coordinate of upper-left corner of rectangle to redraw
 * @width: width of region to draw
 * @height: height of region to draw
 *
 * Convenience function that calls gtk_widget_queue_draw_region() on
 * the region created from the given coordinates.
 *
 * The region here is specified in widget coordinates.
 * Widget coordinates are a bit odd; for historical reasons, they are
 * defined as @widget->window coordinates for widgets that are not
 * #GTK_NO_WINDOW widgets, and are relative to @widget->allocation.x,
 * @widget->allocation.y for widgets that are #GTK_NO_WINDOW widgets.
 */
void
gtk_widget_queue_draw_area (GtkWidget *widget,
			    gint       x,
			    gint       y,
			    gint       width,
			    gint       height)
{
  GdkRectangle rect;
  cairo_region_t *region;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  rect.x = x;
  rect.y = y;
  rect.width = width;
  rect.height = height;

  region = cairo_region_create_rectangle (&rect);
  gtk_widget_queue_draw_region (widget, region);
  cairo_region_destroy (region);
}

/**
 * gtk_widget_queue_draw:
 * @widget: a #GtkWidget
 *
 * Equivalent to calling gtk_widget_queue_draw_area() for the
 * entire area of a widget.
 **/
void
gtk_widget_queue_draw (GtkWidget *widget)
{
  GdkRectangle rect;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_get_allocation (widget, &rect);

  if (!gtk_widget_get_has_window (widget))
    gtk_widget_queue_draw_area (widget,
                                rect.x, rect.y, rect.width, rect.height);
  else
    gtk_widget_queue_draw_area (widget,
                                0, 0, rect.width, rect.height);
}

/**
 * gtk_widget_queue_resize:
 * @widget: a #GtkWidget
 *
 * This function is only for use in widget implementations.
 * Flags a widget to have its size renegotiated; should
 * be called when a widget for some reason has a new size request.
 * For example, when you change the text in a #GtkLabel, #GtkLabel
 * queues a resize to ensure there's enough space for the new text.
 *
 * <note><para>You cannot call gtk_widget_queue_resize() on a widget
 * from inside its implementation of the GtkWidgetClass::size_allocate 
 * virtual method. Calls to gtk_widget_queue_resize() from inside
 * GtkWidgetClass::size_allocate will be silently ignored.</para></note>
 **/
void
gtk_widget_queue_resize (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_realized (widget))
    gtk_widget_queue_shallow_draw (widget);

  _gtk_size_group_queue_resize (widget, 0);
}

/**
 * gtk_widget_queue_resize_no_redraw:
 * @widget: a #GtkWidget
 *
 * This function works like gtk_widget_queue_resize(),
 * except that the widget is not invalidated.
 *
 * Since: 2.4
 **/
void
gtk_widget_queue_resize_no_redraw (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  _gtk_size_group_queue_resize (widget, 0);
}

/**
 * gtk_widget_size_request:
 * @widget: a #GtkWidget
 * @requisition: (out): a #GtkRequisition to be filled in
 *
 * This function is typically used when implementing a #GtkContainer
 * subclass.  Obtains the preferred size of a widget. The container
 * uses this information to arrange its child widgets and decide what
 * size allocations to give them with gtk_widget_size_allocate().
 *
 * You can also call this function from an application, with some
 * caveats. Most notably, getting a size request requires the widget
 * to be associated with a screen, because font information may be
 * needed. Multihead-aware applications should keep this in mind.
 *
 * Also remember that the size request is not necessarily the size
 * a widget will actually be allocated.
 *
 * Deprecated: 3.0: Use gtk_widget_get_preferred_size() instead.
 **/
void
gtk_widget_size_request (GtkWidget	*widget,
			 GtkRequisition *requisition)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_get_preferred_size (widget, requisition, NULL);
}

/**
 * gtk_widget_get_child_requisition:
 * @widget: a #GtkWidget
 * @requisition: (out): a #GtkRequisition to be filled in
 *
 * This function is only for use in widget implementations. Obtains
 * @widget->requisition, unless someone has forced a particular
 * geometry on the widget (e.g. with gtk_widget_set_size_request()),
 * in which case it returns that geometry instead of the widget's
 * requisition.
 *
 * This function differs from gtk_widget_size_request() in that
 * it retrieves the last size request value from @widget->requisition,
 * while gtk_widget_size_request() actually calls the "size_request" method
 * on @widget to compute the size request and fill in @widget->requisition,
 * and only then returns @widget->requisition.
 *
 * Because this function does not call the "size_request" method, it
 * can only be used when you know that @widget->requisition is
 * up-to-date, that is, gtk_widget_size_request() has been called
 * since the last time a resize was queued. In general, only container
 * implementations have this information; applications should use
 * gtk_widget_size_request().
 *
 *
 * Deprecated: 3.0: Use gtk_widget_get_preferred_size() instead.
 **/
void
gtk_widget_get_child_requisition (GtkWidget	 *widget,
				  GtkRequisition *requisition)
{
  gtk_widget_get_preferred_size (widget, requisition, NULL);
}

static gboolean
invalidate_predicate (GdkWindow *window,
		      gpointer   data)
{
  gpointer user_data;

  gdk_window_get_user_data (window, &user_data);

  return (user_data == data);
}

/* Invalidate @region in widget->window and all children
 * of widget->window owned by widget. @region is in the
 * same coordinates as widget->allocation and will be
 * modified by this call.
 */
static void
gtk_widget_invalidate_widget_windows (GtkWidget *widget,
				      cairo_region_t *region)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (!gtk_widget_get_realized (widget))
    return;

  if (gtk_widget_get_has_window (widget) && priv->parent)
    {
      int x, y;

      gdk_window_get_position (priv->window, &x, &y);
      cairo_region_translate (region, -x, -y);
    }

  gdk_window_invalidate_maybe_recurse (priv->window, region,
				       invalidate_predicate, widget);
}

/**
 * gtk_widget_queue_shallow_draw:
 * @widget: a #GtkWidget
 *
 * Like gtk_widget_queue_draw(), but only windows owned
 * by @widget are invalidated.
 **/
static void
gtk_widget_queue_shallow_draw (GtkWidget *widget)
{
  GdkRectangle rect;
  cairo_region_t *region;

  if (!gtk_widget_get_realized (widget))
    return;

  gtk_widget_get_allocation (widget, &rect);

  region = cairo_region_create_rectangle (&rect);
  gtk_widget_invalidate_widget_windows (widget, region);
  cairo_region_destroy (region);
}

/**
 * gtk_widget_size_allocate:
 * @widget: a #GtkWidget
 * @allocation: position and size to be allocated to @widget
 *
 * This function is only used by #GtkContainer subclasses, to assign a size
 * and position to their child widgets.
 *
 * In this function, the allocation may be adjusted. It will be forced
 * to a 1x1 minimum size, and the adjust_size_allocation virtual
 * method on the child will be used to adjust the allocation. Standard
 * adjustments include removing the widget's margins, and applying the
 * widget's #GtkWidget:halign and #GtkWidget:valign properties.
 **/
void
gtk_widget_size_allocate (GtkWidget	*widget,
			  GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv;
  GdkRectangle real_allocation;
  GdkRectangle old_allocation;
  GdkRectangle adjusted_allocation;
  gboolean alloc_needed;
  gboolean size_changed;
  gboolean position_changed;
  gint natural_width, natural_height, dummy;
  gint min_width, min_height;

  priv = widget->priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_push_verify_invariants (widget);

#ifdef G_ENABLE_DEBUG
  if (gtk_get_debug_flags () & GTK_DEBUG_GEOMETRY)
    {
      gint depth;
      GtkWidget *parent;
      const gchar *name;

      depth = 0;
      parent = widget;
      while (parent)
	{
	  depth++;
	  parent = gtk_widget_get_parent (parent);
	}

      name = g_type_name (G_OBJECT_TYPE (G_OBJECT (widget)));
      g_print ("gtk_widget_size_allocate: %*s%s %d %d\n",
	       2 * depth, " ", name,
	       allocation->width, allocation->height);
    }
#endif /* G_ENABLE_DEBUG */

  alloc_needed = priv->alloc_needed;
  if (!priv->width_request_needed && !priv->height_request_needed)
    /* Preserve request/allocate ordering */
    priv->alloc_needed = FALSE;

  old_allocation = priv->allocation;
  real_allocation = *allocation;

  adjusted_allocation = real_allocation;
  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      /* Go ahead and request the height for allocated width, note that the internals
       * of get_height_for_width will internally limit the for_size to natural size
       * when aligning implicitly.
       */
      gtk_widget_get_preferred_width (widget, &min_width, &natural_width);
      gtk_widget_get_preferred_height_for_width (widget, real_allocation.width, &min_height, &natural_height);
    }
  else
    {
      /* Go ahead and request the width for allocated height, note that the internals
       * of get_width_for_height will internally limit the for_size to natural size
       * when aligning implicitly.
       */
      gtk_widget_get_preferred_height (widget, &min_height, &natural_height);
      gtk_widget_get_preferred_width_for_height (widget, real_allocation.height, &min_width, &natural_width);
    }

#ifdef G_ENABLE_DEBUG
  if (gtk_get_debug_flags () & GTK_DEBUG_GEOMETRY)
    {
      if ((min_width > real_allocation.width || min_height > real_allocation.height) &&
          !GTK_IS_SCROLLABLE (widget))
        g_warning ("gtk_widget_size_allocate(): attempt to underallocate %s%s %s %p. "
                   "Allocation is %dx%d, but minimum required size is %dx%d.",
                   priv->parent ? G_OBJECT_TYPE_NAME (priv->parent) : "", priv->parent ? "'s child" : "toplevel",
                   G_OBJECT_TYPE_NAME (widget), widget,
                   real_allocation.width, real_allocation.height,
                   min_width, min_height);
    }
#endif
  /* Now that we have the right natural height and width, go ahead and remove any margins from the
   * allocated sizes and possibly limit them to the natural sizes */
  GTK_WIDGET_GET_CLASS (widget)->adjust_size_allocation (widget,
							 GTK_ORIENTATION_HORIZONTAL,
							 &dummy,
							 &natural_width,
							 &adjusted_allocation.x,
							 &adjusted_allocation.width);
  GTK_WIDGET_GET_CLASS (widget)->adjust_size_allocation (widget,
							 GTK_ORIENTATION_VERTICAL,
							 &dummy,
							 &natural_height,
							 &adjusted_allocation.y,
							 &adjusted_allocation.height);

  if (adjusted_allocation.x < real_allocation.x ||
      adjusted_allocation.y < real_allocation.y ||
      (adjusted_allocation.x + adjusted_allocation.width) >
      (real_allocation.x + real_allocation.width) ||
      (adjusted_allocation.y + adjusted_allocation.height >
       real_allocation.y + real_allocation.height))
    {
      g_warning ("%s %p attempted to adjust its size allocation from %d,%d %dx%d to %d,%d %dx%d. adjust_size_allocation must keep allocation inside original bounds",
                 G_OBJECT_TYPE_NAME (widget), widget,
                 real_allocation.x, real_allocation.y, real_allocation.width, real_allocation.height,
                 adjusted_allocation.x, adjusted_allocation.y, adjusted_allocation.width, adjusted_allocation.height);
      adjusted_allocation = real_allocation; /* veto it */
    }
  else
    {
      real_allocation = adjusted_allocation;
    }

  if (real_allocation.width < 0 || real_allocation.height < 0)
    {
      g_warning ("gtk_widget_size_allocate(): attempt to allocate widget with width %d and height %d",
		 real_allocation.width,
		 real_allocation.height);
    }

  real_allocation.width = MAX (real_allocation.width, 1);
  real_allocation.height = MAX (real_allocation.height, 1);

  size_changed = (old_allocation.width != real_allocation.width ||
		  old_allocation.height != real_allocation.height);
  position_changed = (old_allocation.x != real_allocation.x ||
		      old_allocation.y != real_allocation.y);

  if (!alloc_needed && !size_changed && !position_changed)
    goto out;

  g_signal_emit (widget, widget_signals[SIZE_ALLOCATE], 0, &real_allocation);

  /* Size allocation is god... after consulting god, no further requests or allocations are needed */
  priv->width_request_needed  = FALSE;
  priv->height_request_needed = FALSE;
  priv->alloc_needed          = FALSE;

  if (gtk_widget_get_mapped (widget))
    {
      if (!gtk_widget_get_has_window (widget) && priv->redraw_on_alloc && position_changed)
	{
	  /* Invalidate union(old_allaction,priv->allocation) in priv->window
	   */
	  cairo_region_t *invalidate = cairo_region_create_rectangle (&priv->allocation);
	  cairo_region_union_rectangle (invalidate, &old_allocation);

	  gdk_window_invalidate_region (priv->window, invalidate, FALSE);
	  cairo_region_destroy (invalidate);
	}

      if (size_changed)
	{
	  if (priv->redraw_on_alloc)
	    {
	      /* Invalidate union(old_allaction,priv->allocation) in priv->window and descendents owned by widget
	       */
	      cairo_region_t *invalidate = cairo_region_create_rectangle (&priv->allocation);
	      cairo_region_union_rectangle (invalidate, &old_allocation);

	      gtk_widget_invalidate_widget_windows (widget, invalidate);
	      cairo_region_destroy (invalidate);
	    }
	}

      if (size_changed || position_changed)
        {
          GtkStyleContext *context;

          context = gtk_widget_get_style_context (widget);
          _gtk_style_context_invalidate_animation_areas (context);
        }
    }

  if ((size_changed || position_changed) && priv->parent &&
      gtk_widget_get_realized (priv->parent) && _gtk_container_get_reallocate_redraws (GTK_CONTAINER (priv->parent)))
    {
      cairo_region_t *invalidate = cairo_region_create_rectangle (&priv->parent->priv->allocation);
      gtk_widget_invalidate_widget_windows (priv->parent, invalidate);
      cairo_region_destroy (invalidate);
    }

out:
  gtk_widget_pop_verify_invariants (widget);
}

/**
 * gtk_widget_common_ancestor:
 * @widget_a: a #GtkWidget
 * @widget_b: a #GtkWidget
 *
 * Find the common ancestor of @widget_a and @widget_b that
 * is closest to the two widgets.
 *
 * Return value: the closest common ancestor of @widget_a and
 *   @widget_b or %NULL if @widget_a and @widget_b do not
 *   share a common ancestor.
 **/
static GtkWidget *
gtk_widget_common_ancestor (GtkWidget *widget_a,
			    GtkWidget *widget_b)
{
  GtkWidget *parent_a;
  GtkWidget *parent_b;
  gint depth_a = 0;
  gint depth_b = 0;

  parent_a = widget_a;
  while (parent_a->priv->parent)
    {
      parent_a = parent_a->priv->parent;
      depth_a++;
    }

  parent_b = widget_b;
  while (parent_b->priv->parent)
    {
      parent_b = parent_b->priv->parent;
      depth_b++;
    }

  if (parent_a != parent_b)
    return NULL;

  while (depth_a > depth_b)
    {
      widget_a = widget_a->priv->parent;
      depth_a--;
    }

  while (depth_b > depth_a)
    {
      widget_b = widget_b->priv->parent;
      depth_b--;
    }

  while (widget_a != widget_b)
    {
      widget_a = widget_a->priv->parent;
      widget_b = widget_b->priv->parent;
    }

  return widget_a;
}

/**
 * gtk_widget_translate_coordinates:
 * @src_widget:  a #GtkWidget
 * @dest_widget: a #GtkWidget
 * @src_x: X position relative to @src_widget
 * @src_y: Y position relative to @src_widget
 * @dest_x: (out): location to store X position relative to @dest_widget
 * @dest_y: (out): location to store Y position relative to @dest_widget
 *
 * Translate coordinates relative to @src_widget's allocation to coordinates
 * relative to @dest_widget's allocations. In order to perform this
 * operation, both widgets must be realized, and must share a common
 * toplevel.
 *
 * Return value: %FALSE if either widget was not realized, or there
 *   was no common ancestor. In this case, nothing is stored in
 *   *@dest_x and *@dest_y. Otherwise %TRUE.
 **/
gboolean
gtk_widget_translate_coordinates (GtkWidget  *src_widget,
				  GtkWidget  *dest_widget,
				  gint        src_x,
				  gint        src_y,
				  gint       *dest_x,
				  gint       *dest_y)
{
  GtkWidgetPrivate *src_priv = src_widget->priv;
  GtkWidgetPrivate *dest_priv = dest_widget->priv;
  GtkWidget *ancestor;
  GdkWindow *window;
  GList *dest_list = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (src_widget), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (dest_widget), FALSE);

  ancestor = gtk_widget_common_ancestor (src_widget, dest_widget);
  if (!ancestor || !gtk_widget_get_realized (src_widget) || !gtk_widget_get_realized (dest_widget))
    return FALSE;

  /* Translate from allocation relative to window relative */
  if (gtk_widget_get_has_window (src_widget) && src_priv->parent)
    {
      gint wx, wy;
      gdk_window_get_position (src_priv->window, &wx, &wy);

      src_x -= wx - src_priv->allocation.x;
      src_y -= wy - src_priv->allocation.y;
    }
  else
    {
      src_x += src_priv->allocation.x;
      src_y += src_priv->allocation.y;
    }

  /* Translate to the common ancestor */
  window = src_priv->window;
  while (window != ancestor->priv->window)
    {
      gdouble dx, dy;

      gdk_window_coords_to_parent (window, src_x, src_y, &dx, &dy);

      src_x = dx;
      src_y = dy;

      window = gdk_window_get_effective_parent (window);

      if (!window)		/* Handle GtkHandleBox */
	return FALSE;
    }

  /* And back */
  window = dest_priv->window;
  while (window != ancestor->priv->window)
    {
      dest_list = g_list_prepend (dest_list, window);

      window = gdk_window_get_effective_parent (window);

      if (!window)		/* Handle GtkHandleBox */
        {
          g_list_free (dest_list);
          return FALSE;
        }
    }

  while (dest_list)
    {
      gdouble dx, dy;

      gdk_window_coords_from_parent (dest_list->data, src_x, src_y, &dx, &dy);

      src_x = dx;
      src_y = dy;

      dest_list = g_list_remove (dest_list, dest_list->data);
    }

  /* Translate from window relative to allocation relative */
  if (gtk_widget_get_has_window (dest_widget) && dest_priv->parent)
    {
      gint wx, wy;
      gdk_window_get_position (dest_priv->window, &wx, &wy);

      src_x += wx - dest_priv->allocation.x;
      src_y += wy - dest_priv->allocation.y;
    }
  else
    {
      src_x -= dest_priv->allocation.x;
      src_y -= dest_priv->allocation.y;
    }

  if (dest_x)
    *dest_x = src_x;
  if (dest_y)
    *dest_y = src_y;

  return TRUE;
}

static void
gtk_widget_real_size_allocate (GtkWidget     *widget,
			       GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv = widget->priv;

  priv->allocation = *allocation;

  if (gtk_widget_get_realized (widget) &&
      gtk_widget_get_has_window (widget))
     {
	gdk_window_move_resize (priv->window,
				allocation->x, allocation->y,
				allocation->width, allocation->height);
     }
}

/* translate initial/final into start/end */
static GtkAlign
effective_align (GtkAlign         align,
                 GtkTextDirection direction)
{
  switch (align)
    {
    case GTK_ALIGN_START:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_END : GTK_ALIGN_START;
    case GTK_ALIGN_END:
      return direction == GTK_TEXT_DIR_RTL ? GTK_ALIGN_START : GTK_ALIGN_END;
    default:
      return align;
    }
}

static void
adjust_for_align (GtkAlign  align,
                  gint     *natural_size,
                  gint     *allocated_pos,
                  gint     *allocated_size)
{
  switch (align)
    {
    case GTK_ALIGN_FILL:
      /* change nothing */
      break;
    case GTK_ALIGN_START:
      /* keep *allocated_pos where it is */
      *allocated_size = MIN (*allocated_size, *natural_size);
      break;
    case GTK_ALIGN_END:
      if (*allocated_size > *natural_size)
	{
	  *allocated_pos += (*allocated_size - *natural_size);
	  *allocated_size = *natural_size;
	}
      break;
    case GTK_ALIGN_CENTER:
      if (*allocated_size > *natural_size)
	{
	  *allocated_pos += (*allocated_size - *natural_size) / 2;
	  *allocated_size = MIN (*allocated_size, *natural_size);
	}
      break;
    }
}

static void
adjust_for_margin(gint               start_margin,
                  gint               end_margin,
                  gint              *minimum_size,
                  gint              *natural_size,
                  gint              *allocated_pos,
                  gint              *allocated_size)
{
  *minimum_size -= (start_margin + end_margin);
  *natural_size -= (start_margin + end_margin);
  *allocated_pos += start_margin;
  *allocated_size -= (start_margin + end_margin);
}

static void
gtk_widget_real_adjust_size_allocation (GtkWidget         *widget,
                                        GtkOrientation     orientation,
                                        gint              *minimum_size,
                                        gint              *natural_size,
                                        gint              *allocated_pos,
                                        gint              *allocated_size)
{
  const GtkWidgetAuxInfo *aux_info;

  aux_info = _gtk_widget_get_aux_info_or_defaults (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      adjust_for_margin (aux_info->margin.left,
                         aux_info->margin.right,
                         minimum_size, natural_size,
                         allocated_pos, allocated_size);
      adjust_for_align (effective_align (aux_info->halign, gtk_widget_get_direction (widget)),
                        natural_size, allocated_pos, allocated_size);
    }
  else
    {
      adjust_for_margin (aux_info->margin.top,
                         aux_info->margin.bottom,
                         minimum_size, natural_size,
                         allocated_pos, allocated_size);
      adjust_for_align (effective_align (aux_info->valign, GTK_TEXT_DIR_NONE),
                        natural_size, allocated_pos, allocated_size);
    }
}

static gboolean
gtk_widget_real_can_activate_accel (GtkWidget *widget,
                                    guint      signal_id)
{
  GtkWidgetPrivate *priv = widget->priv;

  /* widgets must be onscreen for accels to take effect */
  return gtk_widget_is_sensitive (widget) &&
         gtk_widget_is_drawable (widget) &&
         gdk_window_is_viewable (priv->window);
}

/**
 * gtk_widget_can_activate_accel:
 * @widget: a #GtkWidget
 * @signal_id: the ID of a signal installed on @widget
 *
 * Determines whether an accelerator that activates the signal
 * identified by @signal_id can currently be activated.
 * This is done by emitting the #GtkWidget::can-activate-accel
 * signal on @widget; if the signal isn't overridden by a
 * handler or in a derived widget, then the default check is
 * that the widget must be sensitive, and the widget and all
 * its ancestors mapped.
 *
 * Return value: %TRUE if the accelerator can be activated.
 *
 * Since: 2.4
 **/
gboolean
gtk_widget_can_activate_accel (GtkWidget *widget,
                               guint      signal_id)
{
  gboolean can_activate = FALSE;
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_signal_emit (widget, widget_signals[CAN_ACTIVATE_ACCEL], 0, signal_id, &can_activate);
  return can_activate;
}

typedef struct {
  GClosure   closure;
  guint      signal_id;
} AccelClosure;

static void
closure_accel_activate (GClosure     *closure,
			GValue       *return_value,
			guint         n_param_values,
			const GValue *param_values,
			gpointer      invocation_hint,
			gpointer      marshal_data)
{
  AccelClosure *aclosure = (AccelClosure*) closure;
  gboolean can_activate = gtk_widget_can_activate_accel (closure->data, aclosure->signal_id);

  if (can_activate)
    g_signal_emit (closure->data, aclosure->signal_id, 0);

  /* whether accelerator was handled */
  g_value_set_boolean (return_value, can_activate);
}

static void
closures_destroy (gpointer data)
{
  GSList *slist, *closures = data;

  for (slist = closures; slist; slist = slist->next)
    {
      g_closure_invalidate (slist->data);
      g_closure_unref (slist->data);
    }
  g_slist_free (closures);
}

static GClosure*
widget_new_accel_closure (GtkWidget *widget,
			  guint      signal_id)
{
  AccelClosure *aclosure;
  GClosure *closure = NULL;
  GSList *slist, *closures;

  closures = g_object_steal_qdata (G_OBJECT (widget), quark_accel_closures);
  for (slist = closures; slist; slist = slist->next)
    if (!gtk_accel_group_from_accel_closure (slist->data))
      {
	/* reuse this closure */
	closure = slist->data;
	break;
      }
  if (!closure)
    {
      closure = g_closure_new_object (sizeof (AccelClosure), G_OBJECT (widget));
      closures = g_slist_prepend (closures, g_closure_ref (closure));
      g_closure_sink (closure);
      g_closure_set_marshal (closure, closure_accel_activate);
    }
  g_object_set_qdata_full (G_OBJECT (widget), quark_accel_closures, closures, closures_destroy);

  aclosure = (AccelClosure*) closure;
  g_assert (closure->data == widget);
  g_assert (closure->marshal == closure_accel_activate);
  aclosure->signal_id = signal_id;

  return closure;
}

/**
 * gtk_widget_add_accelerator
 * @widget:       widget to install an accelerator on
 * @accel_signal: widget signal to emit on accelerator activation
 * @accel_group:  accel group for this widget, added to its toplevel
 * @accel_key:    GDK keyval of the accelerator
 * @accel_mods:   modifier key combination of the accelerator
 * @accel_flags:  flag accelerators, e.g. %GTK_ACCEL_VISIBLE
 *
 * Installs an accelerator for this @widget in @accel_group that causes
 * @accel_signal to be emitted if the accelerator is activated.
 * The @accel_group needs to be added to the widget's toplevel via
 * gtk_window_add_accel_group(), and the signal must be of type %G_RUN_ACTION.
 * Accelerators added through this function are not user changeable during
 * runtime. If you want to support accelerators that can be changed by the
 * user, use gtk_accel_map_add_entry() and gtk_widget_set_accel_path() or
 * gtk_menu_item_set_accel_path() instead.
 */
void
gtk_widget_add_accelerator (GtkWidget      *widget,
			    const gchar    *accel_signal,
			    GtkAccelGroup  *accel_group,
			    guint           accel_key,
			    GdkModifierType accel_mods,
			    GtkAccelFlags   accel_flags)
{
  GClosure *closure;
  GSignalQuery query;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (accel_signal != NULL);
  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  g_signal_query (g_signal_lookup (accel_signal, G_OBJECT_TYPE (widget)), &query);
  if (!query.signal_id ||
      !(query.signal_flags & G_SIGNAL_ACTION) ||
      query.return_type != G_TYPE_NONE ||
      query.n_params)
    {
      /* hmm, should be elaborate enough */
      g_warning (G_STRLOC ": widget `%s' has no activatable signal \"%s\" without arguments",
		 G_OBJECT_TYPE_NAME (widget), accel_signal);
      return;
    }

  closure = widget_new_accel_closure (widget, query.signal_id);

  g_object_ref (widget);

  /* install the accelerator. since we don't map this onto an accel_path,
   * the accelerator will automatically be locked.
   */
  gtk_accel_group_connect (accel_group,
			   accel_key,
			   accel_mods,
			   accel_flags | GTK_ACCEL_LOCKED,
			   closure);

  g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);

  g_object_unref (widget);
}

/**
 * gtk_widget_remove_accelerator:
 * @widget:       widget to install an accelerator on
 * @accel_group:  accel group for this widget
 * @accel_key:    GDK keyval of the accelerator
 * @accel_mods:   modifier key combination of the accelerator
 *
 * Removes an accelerator from @widget, previously installed with
 * gtk_widget_add_accelerator().
 *
 * Returns: whether an accelerator was installed and could be removed
 */
gboolean
gtk_widget_remove_accelerator (GtkWidget      *widget,
			       GtkAccelGroup  *accel_group,
			       guint           accel_key,
			       GdkModifierType accel_mods)
{
  GtkAccelGroupEntry *ag_entry;
  GList *slist, *clist;
  guint n;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GTK_IS_ACCEL_GROUP (accel_group), FALSE);

  ag_entry = gtk_accel_group_query (accel_group, accel_key, accel_mods, &n);
  clist = gtk_widget_list_accel_closures (widget);
  for (slist = clist; slist; slist = slist->next)
    {
      guint i;

      for (i = 0; i < n; i++)
	if (slist->data == (gpointer) ag_entry[i].closure)
	  {
	    gboolean is_removed = gtk_accel_group_disconnect (accel_group, slist->data);

	    g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);

	    g_list_free (clist);

	    return is_removed;
	  }
    }
  g_list_free (clist);

  g_warning (G_STRLOC ": no accelerator (%u,%u) installed in accel group (%p) for %s (%p)",
	     accel_key, accel_mods, accel_group,
	     G_OBJECT_TYPE_NAME (widget), widget);

  return FALSE;
}

/**
 * gtk_widget_list_accel_closures:
 * @widget:  widget to list accelerator closures for
 *
 * Lists the closures used by @widget for accelerator group connections
 * with gtk_accel_group_connect_by_path() or gtk_accel_group_connect().
 * The closures can be used to monitor accelerator changes on @widget,
 * by connecting to the @GtkAccelGroup::accel-changed signal of the
 * #GtkAccelGroup of a closure which can be found out with
 * gtk_accel_group_from_accel_closure().
 *
 * Return value: (transfer container) (element-type GClosure):
 *     a newly allocated #GList of closures
 */
GList*
gtk_widget_list_accel_closures (GtkWidget *widget)
{
  GSList *slist;
  GList *clist = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  for (slist = g_object_get_qdata (G_OBJECT (widget), quark_accel_closures); slist; slist = slist->next)
    if (gtk_accel_group_from_accel_closure (slist->data))
      clist = g_list_prepend (clist, slist->data);
  return clist;
}

typedef struct {
  GQuark         path_quark;
  GtkAccelGroup *accel_group;
  GClosure      *closure;
} AccelPath;

static void
destroy_accel_path (gpointer data)
{
  AccelPath *apath = data;

  gtk_accel_group_disconnect (apath->accel_group, apath->closure);

  /* closures_destroy takes care of unrefing the closure */
  g_object_unref (apath->accel_group);

  g_slice_free (AccelPath, apath);
}


/**
 * gtk_widget_set_accel_path:
 * @widget: a #GtkWidget
 * @accel_path: (allow-none): path used to look up the accelerator
 * @accel_group: (allow-none): a #GtkAccelGroup.
 *
 * Given an accelerator group, @accel_group, and an accelerator path,
 * @accel_path, sets up an accelerator in @accel_group so whenever the
 * key binding that is defined for @accel_path is pressed, @widget
 * will be activated.  This removes any accelerators (for any
 * accelerator group) installed by previous calls to
 * gtk_widget_set_accel_path(). Associating accelerators with
 * paths allows them to be modified by the user and the modifications
 * to be saved for future use. (See gtk_accel_map_save().)
 *
 * This function is a low level function that would most likely
 * be used by a menu creation system like #GtkUIManager. If you
 * use #GtkUIManager, setting up accelerator paths will be done
 * automatically.
 *
 * Even when you you aren't using #GtkUIManager, if you only want to
 * set up accelerators on menu items gtk_menu_item_set_accel_path()
 * provides a somewhat more convenient interface.
 *
 * Note that @accel_path string will be stored in a #GQuark. Therefore, if you
 * pass a static string, you can save some memory by interning it first with
 * g_intern_static_string().
 **/
void
gtk_widget_set_accel_path (GtkWidget     *widget,
			   const gchar   *accel_path,
			   GtkAccelGroup *accel_group)
{
  AccelPath *apath;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_WIDGET_GET_CLASS (widget)->activate_signal != 0);

  if (accel_path)
    {
      g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));
      g_return_if_fail (_gtk_accel_path_is_valid (accel_path));

      gtk_accel_map_add_entry (accel_path, 0, 0);
      apath = g_slice_new (AccelPath);
      apath->accel_group = g_object_ref (accel_group);
      apath->path_quark = g_quark_from_string (accel_path);
      apath->closure = widget_new_accel_closure (widget, GTK_WIDGET_GET_CLASS (widget)->activate_signal);
    }
  else
    apath = NULL;

  /* also removes possible old settings */
  g_object_set_qdata_full (G_OBJECT (widget), quark_accel_path, apath, destroy_accel_path);

  if (apath)
    gtk_accel_group_connect_by_path (apath->accel_group, g_quark_to_string (apath->path_quark), apath->closure);

  g_signal_emit (widget, widget_signals[ACCEL_CLOSURES_CHANGED], 0);
}

const gchar*
_gtk_widget_get_accel_path (GtkWidget *widget,
			    gboolean  *locked)
{
  AccelPath *apath;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  apath = g_object_get_qdata (G_OBJECT (widget), quark_accel_path);
  if (locked)
    *locked = apath ? gtk_accel_group_get_is_locked (apath->accel_group) : TRUE;
  return apath ? g_quark_to_string (apath->path_quark) : NULL;
}

/**
 * gtk_widget_mnemonic_activate:
 * @widget: a #GtkWidget
 * @group_cycling:  %TRUE if there are other widgets with the same mnemonic
 *
 * Emits the #GtkWidget::mnemonic-activate signal.
 *
 * The default handler for this signal activates the @widget if
 * @group_cycling is %FALSE, and just grabs the focus if @group_cycling
 * is %TRUE.
 *
 * Returns: %TRUE if the signal has been handled
 */
gboolean
gtk_widget_mnemonic_activate (GtkWidget *widget,
                              gboolean   group_cycling)
{
  gboolean handled;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  group_cycling = group_cycling != FALSE;
  if (!gtk_widget_is_sensitive (widget))
    handled = TRUE;
  else
    g_signal_emit (widget,
		   widget_signals[MNEMONIC_ACTIVATE],
		   0,
		   group_cycling,
		   &handled);
  return handled;
}

static gboolean
gtk_widget_real_mnemonic_activate (GtkWidget *widget,
                                   gboolean   group_cycling)
{
  if (!group_cycling && GTK_WIDGET_GET_CLASS (widget)->activate_signal)
    gtk_widget_activate (widget);
  else if (gtk_widget_get_can_focus (widget))
    gtk_widget_grab_focus (widget);
  else
    {
      g_warning ("widget `%s' isn't suitable for mnemonic activation",
		 G_OBJECT_TYPE_NAME (widget));
      gtk_widget_error_bell (widget);
    }
  return TRUE;
}

static const cairo_user_data_key_t event_key;

GdkEventExpose *
_gtk_cairo_get_event (cairo_t *cr)
{
  g_return_val_if_fail (cr != NULL, NULL);

  return cairo_get_user_data (cr, &event_key);
}

static void
gtk_cairo_set_event (cairo_t        *cr,
                     GdkEventExpose *event)
{
  cairo_set_user_data (cr, &event_key, event, NULL);
}

/**
 * gtk_cairo_should_draw_window:
 * @cr: a cairo context
 * @window: the window to check. @window may not be an input-only
 *          window.
 *
 * This function is supposed to be called in #GtkWidget::draw
 * implementations for widgets that support multiple windows.
 * @cr must be untransformed from invoking of the draw function.
 * This function will return %TRUE if the contents of the given
 * @window are supposed to be drawn and %FALSE otherwise. Note
 * that when the drawing was not initiated by the windowing
 * system this function will return %TRUE for all windows, so
 * you need to draw the bottommost window first. Also, do not
 * use "else if" statements to check which window should be drawn.
 *
 * Returns: %TRUE if @window should be drawn
 *
 * Since: 3.0
 **/
gboolean
gtk_cairo_should_draw_window (cairo_t *cr,
                              GdkWindow *window)
{
  GdkEventExpose *event;

  g_return_val_if_fail (cr != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  event = _gtk_cairo_get_event (cr);

  return event == NULL ||
         event->window == window;
}

static gboolean
gtk_widget_get_clip_draw (GtkWidget *widget)
{
  /* labels are not clipped, because clipping them would cause
   * mnemonics to not appear on characters that go beyond the
   * baseline.
   * https://bugzilla.gnome.org/show_bug.cgi?id=648570
   */
  if (GTK_IS_LABEL (widget))
    return FALSE;

  return TRUE;
}

/* code shared by gtk_container_propagate_draw() and
 * gtk_widget_draw()
 */
void
_gtk_widget_draw_internal (GtkWidget *widget,
                           cairo_t   *cr,
                           gboolean   clip_to_size)
{
  GtkStyleContext *context;

  if (!gtk_widget_is_drawable (widget))
    return;

  clip_to_size &= gtk_widget_get_clip_draw (widget);

  if (clip_to_size)
    {
      cairo_rectangle (cr,
                       0, 0,
                       widget->priv->allocation.width,
                       widget->priv->allocation.height);
      cairo_clip (cr);
    }

  if (gdk_cairo_get_clip_rectangle (cr, NULL))
    {
      gboolean result;

      g_signal_emit (widget, widget_signals[DRAW],
                     0, cr,
                     &result);
    }

  context = gtk_widget_get_style_context (widget);
  _gtk_style_context_coalesce_animation_areas (context, widget);
}

/**
 * gtk_widget_draw:
 * @widget: the widget to draw. It must be drawable (see
 *   gtk_widget_is_drawable()) and a size must have been allocated.
 * @cr: a cairo context to draw to
 *
 * Draws @widget to @cr. The top left corner of the widget will be
 * drawn to the currently set origin point of @cr.
 *
 * You should pass a cairo context as @cr argument that is in an
 * original state. Otherwise the resulting drawing is undefined. For
 * example changing the operator using cairo_set_operator() or the
 * line width using cairo_set_line_width() might have unwanted side
 * effects.
 * You may however change the context's transform matrix - like with
 * cairo_scale(), cairo_translate() or cairo_set_matrix() and clip
 * region with cairo_clip() prior to calling this function. Also, it
 * is fine to modify the context with cairo_save() and
 * cairo_push_group() prior to calling this function.
 *
 * <note><para>Special purpose widgets may contain special code for
 * rendering to the screen and might appear differently on screen
 * and when rendered using gtk_widget_draw().</para></note>
 *
 * Since: 3.0
 **/
void
gtk_widget_draw (GtkWidget *widget,
                 cairo_t   *cr)
{
  GdkEventExpose *tmp_event;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!widget->priv->alloc_needed);
  g_return_if_fail (cr != NULL);

  cairo_save (cr);
  /* We have to reset the event here so that draw functions can call
   * gtk_widget_draw() on random other widgets and get the desired
   * effect: Drawing all contents, not just the current window.
   */
  tmp_event = _gtk_cairo_get_event (cr);
  gtk_cairo_set_event (cr, NULL);

  _gtk_widget_draw_internal (widget, cr, TRUE);

  gtk_cairo_set_event (cr, tmp_event);
  cairo_restore (cr);
}

static gboolean
gtk_widget_real_key_press_event (GtkWidget         *widget,
				 GdkEventKey       *event)
{
  return gtk_bindings_activate_event (G_OBJECT (widget), event);
}

static gboolean
gtk_widget_real_key_release_event (GtkWidget         *widget,
				   GdkEventKey       *event)
{
  return gtk_bindings_activate_event (G_OBJECT (widget), event);
}

static gboolean
gtk_widget_real_focus_in_event (GtkWidget     *widget,
                                GdkEventFocus *event)
{
  gtk_widget_queue_shallow_draw (widget);

  return FALSE;
}

static gboolean
gtk_widget_real_focus_out_event (GtkWidget     *widget,
                                 GdkEventFocus *event)
{
  gtk_widget_queue_shallow_draw (widget);

  return FALSE;
}

#define WIDGET_REALIZED_FOR_EVENT(widget, event) \
     (event->type == GDK_FOCUS_CHANGE || gtk_widget_get_realized(widget))

/**
 * gtk_widget_event:
 * @widget: a #GtkWidget
 * @event: a #GdkEvent
 *
 * Rarely-used function. This function is used to emit
 * the event signals on a widget (those signals should never
 * be emitted without using this function to do so).
 * If you want to synthesize an event though, don't use this function;
 * instead, use gtk_main_do_event() so the event will behave as if
 * it were in the event queue. Don't synthesize expose events; instead,
 * use gdk_window_invalidate_rect() to invalidate a region of the
 * window.
 *
 * Return value: return from the event signal emission (%TRUE if
 *               the event was handled)
 **/
gboolean
gtk_widget_event (GtkWidget *widget,
		  GdkEvent  *event)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (WIDGET_REALIZED_FOR_EVENT (widget, event), TRUE);

  if (event->type == GDK_EXPOSE)
    {
      g_warning ("Events of type GDK_EXPOSE cannot be synthesized. To get "
		 "the same effect, call gdk_window_invalidate_rect/region(), "
		 "followed by gdk_window_process_updates().");
      return TRUE;
    }

  return gtk_widget_event_internal (widget, event);
}

/* Returns TRUE if a translation should be done */
gboolean
_gtk_widget_get_translation_to_window (GtkWidget      *widget,
				       GdkWindow      *window,
				       int            *x,
				       int            *y)
{
  GdkWindow *w, *widget_window;

  if (!gtk_widget_get_has_window (widget))
    {
      *x = -widget->priv->allocation.x;
      *y = -widget->priv->allocation.y;
    }
  else
    {
      *x = 0;
      *y = 0;
    }

  widget_window = gtk_widget_get_window (widget);

  for (w = window; w && w != widget_window; w = gdk_window_get_parent (w))
    {
      int wx, wy;
      gdk_window_get_position (w, &wx, &wy);
      *x += wx;
      *y += wy;
    }

  if (w == NULL)
    {
      *x = 0;
      *y = 0;
      return FALSE;
    }

  return TRUE;
}


/**
 * gtk_cairo_transform_to_window:
 * @cr: the cairo context to transform
 * @widget: the widget the context is currently centered for
 * @window: the window to transform the context to
 *
 * Transforms the given cairo context @cr that from @widget-relative
 * coordinates to @window-relative coordinates.
 * If the @widget's window is not an ancestor of @window, no
 * modification will be applied.
 *
 * This is the inverse to the transformation GTK applies when
 * preparing an expose event to be emitted with the #GtkWidget::draw
 * signal. It is intended to help porting multiwindow widgets from
 * GTK+ 2 to the rendering architecture of GTK+ 3.
 *
 * Since: 3.0
 **/
void
gtk_cairo_transform_to_window (cairo_t   *cr,
                               GtkWidget *widget,
                               GdkWindow *window)
{
  int x, y;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (_gtk_widget_get_translation_to_window (widget, window, &x, &y))
    cairo_translate (cr, x, y);
}

/**
 * gtk_widget_send_expose:
 * @widget: a #GtkWidget
 * @event: a expose #GdkEvent
 *
 * Very rarely-used function. This function is used to emit
 * an expose event on a widget. This function is not normally used
 * directly. The only time it is used is when propagating an expose
 * event to a child %NO_WINDOW widget, and that is normally done
 * using gtk_container_propagate_draw().
 *
 * If you want to force an area of a window to be redrawn,
 * use gdk_window_invalidate_rect() or gdk_window_invalidate_region().
 * To cause the redraw to be done immediately, follow that call
 * with a call to gdk_window_process_updates().
 *
 * Return value: return from the event signal emission (%TRUE if
 *               the event was handled)
 **/
gint
gtk_widget_send_expose (GtkWidget *widget,
			GdkEvent  *event)
{
  gboolean result = FALSE;
  cairo_t *cr;
  int x, y;
  gboolean do_clip;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), TRUE);
  g_return_val_if_fail (gtk_widget_get_realized (widget), TRUE);
  g_return_val_if_fail (event != NULL, TRUE);
  g_return_val_if_fail (event->type == GDK_EXPOSE, TRUE);

  cr = gdk_cairo_create (event->expose.window);
  gtk_cairo_set_event (cr, &event->expose);

  gdk_cairo_region (cr, event->expose.region);
  cairo_clip (cr);

  do_clip = _gtk_widget_get_translation_to_window (widget,
						   event->expose.window,
						   &x, &y);
  cairo_translate (cr, -x, -y);

  _gtk_widget_draw_internal (widget, cr, do_clip);

  /* unset here, so if someone keeps a reference to cr we
   * don't leak the window. */
  gtk_cairo_set_event (cr, NULL);
  cairo_destroy (cr);

  return result;
}

static gboolean
event_window_is_still_viewable (GdkEvent *event)
{
  /* Check that we think the event's window is viewable before
   * delivering the event, to prevent suprises. We do this here
   * at the last moment, since the event may have been queued
   * up behind other events, held over a recursive main loop, etc.
   */
  switch (event->type)
    {
    case GDK_EXPOSE:
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_KEY_PRESS:
    case GDK_ENTER_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_SCROLL:
      return event->any.window && gdk_window_is_viewable (event->any.window);

#if 0
    /* The following events are the second half of paired events;
     * we always deliver them to deal with widgets that clean up
     * on the second half.
     */
    case GDK_BUTTON_RELEASE:
    case GDK_KEY_RELEASE:
    case GDK_LEAVE_NOTIFY:
    case GDK_PROXIMITY_OUT:
#endif

    default:
      /* Remaining events would make sense on an not-viewable window,
       * or don't have an associated window.
       */
      return TRUE;
    }
}

static gint
gtk_widget_event_internal (GtkWidget *widget,
			   GdkEvent  *event)
{
  gboolean return_val = FALSE;

  /* We check only once for is-still-visible; if someone
   * hides the window in on of the signals on the widget,
   * they are responsible for returning TRUE to terminate
   * handling.
   */
  if (!event_window_is_still_viewable (event))
    return TRUE;

  g_object_ref (widget);

  g_signal_emit (widget, widget_signals[EVENT], 0, event, &return_val);
  return_val |= !WIDGET_REALIZED_FOR_EVENT (widget, event);
  if (!return_val)
    {
      gint signal_num;

      switch (event->type)
	{
	case GDK_EXPOSE:
	case GDK_NOTHING:
	  signal_num = -1;
	  break;
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	  signal_num = BUTTON_PRESS_EVENT;
	  break;
	case GDK_SCROLL:
	  signal_num = SCROLL_EVENT;
	  break;
	case GDK_BUTTON_RELEASE:
	  signal_num = BUTTON_RELEASE_EVENT;
	  break;
	case GDK_MOTION_NOTIFY:
	  signal_num = MOTION_NOTIFY_EVENT;
	  break;
	case GDK_DELETE:
	  signal_num = DELETE_EVENT;
	  break;
	case GDK_DESTROY:
	  signal_num = DESTROY_EVENT;
	  _gtk_tooltip_hide (widget);
	  break;
	case GDK_KEY_PRESS:
	  signal_num = KEY_PRESS_EVENT;
	  break;
	case GDK_KEY_RELEASE:
	  signal_num = KEY_RELEASE_EVENT;
	  break;
	case GDK_ENTER_NOTIFY:
	  signal_num = ENTER_NOTIFY_EVENT;
	  break;
	case GDK_LEAVE_NOTIFY:
	  signal_num = LEAVE_NOTIFY_EVENT;
	  break;
	case GDK_FOCUS_CHANGE:
	  signal_num = event->focus_change.in ? FOCUS_IN_EVENT : FOCUS_OUT_EVENT;
	  if (event->focus_change.in)
	    _gtk_tooltip_focus_in (widget);
	  else
	    _gtk_tooltip_focus_out (widget);
	  break;
	case GDK_CONFIGURE:
	  signal_num = CONFIGURE_EVENT;
	  break;
	case GDK_MAP:
	  signal_num = MAP_EVENT;
	  break;
	case GDK_UNMAP:
	  signal_num = UNMAP_EVENT;
	  break;
	case GDK_WINDOW_STATE:
	  signal_num = WINDOW_STATE_EVENT;
	  break;
	case GDK_PROPERTY_NOTIFY:
	  signal_num = PROPERTY_NOTIFY_EVENT;
	  break;
	case GDK_SELECTION_CLEAR:
	  signal_num = SELECTION_CLEAR_EVENT;
	  break;
	case GDK_SELECTION_REQUEST:
	  signal_num = SELECTION_REQUEST_EVENT;
	  break;
	case GDK_SELECTION_NOTIFY:
	  signal_num = SELECTION_NOTIFY_EVENT;
	  break;
	case GDK_PROXIMITY_IN:
	  signal_num = PROXIMITY_IN_EVENT;
	  break;
	case GDK_PROXIMITY_OUT:
	  signal_num = PROXIMITY_OUT_EVENT;
	  break;
	case GDK_VISIBILITY_NOTIFY:
	  signal_num = VISIBILITY_NOTIFY_EVENT;
	  break;
	case GDK_GRAB_BROKEN:
	  signal_num = GRAB_BROKEN_EVENT;
	  break;
	case GDK_DAMAGE:
	  signal_num = DAMAGE_EVENT;
	  break;
	default:
	  g_warning ("gtk_widget_event(): unhandled event type: %d", event->type);
	  signal_num = -1;
	  break;
	}
      if (signal_num != -1)
	g_signal_emit (widget, widget_signals[signal_num], 0, event, &return_val);
    }
  if (WIDGET_REALIZED_FOR_EVENT (widget, event))
    g_signal_emit (widget, widget_signals[EVENT_AFTER], 0, event);
  else
    return_val = TRUE;

  g_object_unref (widget);

  return return_val;
}

/**
 * gtk_widget_activate:
 * @widget: a #GtkWidget that's activatable
 *
 * For widgets that can be "activated" (buttons, menu items, etc.)
 * this function activates them. Activation is what happens when you
 * press Enter on a widget during key navigation. If @widget isn't
 * activatable, the function returns %FALSE.
 *
 * Return value: %TRUE if the widget was activatable
 **/
gboolean
gtk_widget_activate (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (WIDGET_CLASS (widget)->activate_signal)
    {
      /* FIXME: we should eventually check the signals signature here */
      g_signal_emit (widget, WIDGET_CLASS (widget)->activate_signal, 0);

      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_widget_reparent_subwindows (GtkWidget *widget,
				GdkWindow *new_window)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (!gtk_widget_get_has_window (widget))
    {
      GList *children = gdk_window_get_children (priv->window);
      GList *tmp_list;

      for (tmp_list = children; tmp_list; tmp_list = tmp_list->next)
	{
	  GdkWindow *window = tmp_list->data;
	  gpointer child;

	  gdk_window_get_user_data (window, &child);
	  while (child && child != widget)
	    child = ((GtkWidget*) child)->priv->parent;

	  if (child)
	    gdk_window_reparent (window, new_window, 0, 0);
	}

      g_list_free (children);
    }
  else
   {
     GdkWindow *parent;
     GList *tmp_list, *children;

     parent = gdk_window_get_parent (priv->window);

     if (parent == NULL)
       gdk_window_reparent (priv->window, new_window, 0, 0);
     else
       {
	 children = gdk_window_get_children (parent);

	 for (tmp_list = children; tmp_list; tmp_list = tmp_list->next)
	   {
	     GdkWindow *window = tmp_list->data;
	     gpointer child;

	     gdk_window_get_user_data (window, &child);

	     if (child == widget)
	       gdk_window_reparent (window, new_window, 0, 0);
	   }

	 g_list_free (children);
       }
   }
}

static void
gtk_widget_reparent_fixup_child (GtkWidget *widget,
				 gpointer   client_data)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_assert (client_data != NULL);

  if (!gtk_widget_get_has_window (widget))
    {
      if (priv->window)
	g_object_unref (priv->window);
      priv->window = (GdkWindow*) client_data;
      if (priv->window)
	g_object_ref (priv->window);

      if (GTK_IS_CONTAINER (widget))
        gtk_container_forall (GTK_CONTAINER (widget),
                              gtk_widget_reparent_fixup_child,
                              client_data);
    }
}

/**
 * gtk_widget_reparent:
 * @widget: a #GtkWidget
 * @new_parent: a #GtkContainer to move the widget into
 *
 * Moves a widget from one #GtkContainer to another, handling reference
 * count issues to avoid destroying the widget.
 **/
void
gtk_widget_reparent (GtkWidget *widget,
		     GtkWidget *new_parent)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_CONTAINER (new_parent));
  priv = widget->priv;
  g_return_if_fail (priv->parent != NULL);

  if (priv->parent != new_parent)
    {
      /* First try to see if we can get away without unrealizing
       * the widget as we reparent it. if so we set a flag so
       * that gtk_widget_unparent doesn't unrealize widget
       */
      if (gtk_widget_get_realized (widget) && gtk_widget_get_realized (new_parent))
	priv->in_reparent = TRUE;

      g_object_ref (widget);
      gtk_container_remove (GTK_CONTAINER (priv->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      g_object_unref (widget);

      if (priv->in_reparent)
	{
          priv->in_reparent = FALSE;

	  gtk_widget_reparent_subwindows (widget, gtk_widget_get_parent_window (widget));
	  gtk_widget_reparent_fixup_child (widget,
					   gtk_widget_get_parent_window (widget));
	}

      g_object_notify (G_OBJECT (widget), "parent");
    }
}

/**
 * gtk_widget_intersect:
 * @widget: a #GtkWidget
 * @area: a rectangle
 * @intersection: rectangle to store intersection of @widget and @area
 *
 * Computes the intersection of a @widget's area and @area, storing
 * the intersection in @intersection, and returns %TRUE if there was
 * an intersection.  @intersection may be %NULL if you're only
 * interested in whether there was an intersection.
 *
 * Return value: %TRUE if there was an intersection
 **/
gboolean
gtk_widget_intersect (GtkWidget	         *widget,
		      const GdkRectangle *area,
		      GdkRectangle       *intersection)
{
  GtkWidgetPrivate *priv;
  GdkRectangle *dest;
  GdkRectangle tmp;
  gint return_val;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (area != NULL, FALSE);

  priv = widget->priv;

  if (intersection)
    dest = intersection;
  else
    dest = &tmp;

  return_val = gdk_rectangle_intersect (&priv->allocation, area, dest);

  if (return_val && intersection && gtk_widget_get_has_window (widget))
    {
      intersection->x -= priv->allocation.x;
      intersection->y -= priv->allocation.y;
    }

  return return_val;
}

/**
 * gtk_widget_region_intersect:
 * @widget: a #GtkWidget
 * @region: a #cairo_region_t, in the same coordinate system as
 *          @widget->allocation. That is, relative to @widget->window
 *          for %NO_WINDOW widgets; relative to the parent window
 *          of @widget->window for widgets with their own window.
 *
 * Computes the intersection of a @widget's area and @region, returning
 * the intersection. The result may be empty, use cairo_region_is_empty() to
 * check.
 *
 * Returns: A newly allocated region holding the intersection of @widget
 *     and @region. The coordinates of the return value are relative to
 *     @widget->window for %NO_WINDOW widgets, and relative to the parent
 *     window of @widget->window for widgets with their own window.
 */
cairo_region_t *
gtk_widget_region_intersect (GtkWidget       *widget,
			     const cairo_region_t *region)
{
  GdkRectangle rect;
  cairo_region_t *dest;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (region != NULL, NULL);

  gtk_widget_get_allocation (widget, &rect);

  dest = cairo_region_create_rectangle (&rect);

  cairo_region_intersect (dest, region);

  return dest;
}

/**
 * _gtk_widget_grab_notify:
 * @widget: a #GtkWidget
 * @was_grabbed: whether a grab is now in effect
 *
 * Emits the #GtkWidget::grab-notify signal on @widget.
 *
 * Since: 2.6
 **/
void
_gtk_widget_grab_notify (GtkWidget *widget,
			 gboolean   was_grabbed)
{
  g_signal_emit (widget, widget_signals[GRAB_NOTIFY], 0, was_grabbed);
}

/**
 * gtk_widget_grab_focus:
 * @widget: a #GtkWidget
 *
 * Causes @widget to have the keyboard focus for the #GtkWindow it's
 * inside. @widget must be a focusable widget, such as a #GtkEntry;
 * something like #GtkFrame won't work.
 *
 * More precisely, it must have the %GTK_CAN_FOCUS flag set. Use
 * gtk_widget_set_can_focus() to modify that flag.
 *
 * The widget also needs to be realized and mapped. This is indicated by the
 * related signals. Grabbing the focus immediately after creating the widget
 * will likely fail and cause critical warnings.
 **/
void
gtk_widget_grab_focus (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!gtk_widget_is_sensitive (widget))
    return;

  g_object_ref (widget);
  g_signal_emit (widget, widget_signals[GRAB_FOCUS], 0);
  g_object_notify (G_OBJECT (widget), "has-focus");
  g_object_unref (widget);
}

static void
reset_focus_recurse (GtkWidget *widget,
		     gpointer   data)
{
  if (GTK_IS_CONTAINER (widget))
    {
      GtkContainer *container;

      container = GTK_CONTAINER (widget);
      gtk_container_set_focus_child (container, NULL);

      gtk_container_foreach (container,
			     reset_focus_recurse,
			     NULL);
    }
}

static void
gtk_widget_real_grab_focus (GtkWidget *focus_widget)
{
  if (gtk_widget_get_can_focus (focus_widget))
    {
      GtkWidget *toplevel;
      GtkWidget *widget;

      /* clear the current focus setting, break if the current widget
       * is the focus widget's parent, since containers above that will
       * be set by the next loop.
       */
      toplevel = gtk_widget_get_toplevel (focus_widget);
      if (gtk_widget_is_toplevel (toplevel) && GTK_IS_WINDOW (toplevel))
	{
          widget = gtk_window_get_focus (GTK_WINDOW (toplevel));

	  if (widget == focus_widget)
	    {
	      /* We call _gtk_window_internal_set_focus() here so that the
	       * toplevel window can request the focus if necessary.
	       * This is needed when the toplevel is a GtkPlug
	       */
	      if (!gtk_widget_has_focus (widget))
		_gtk_window_internal_set_focus (GTK_WINDOW (toplevel), focus_widget);

	      return;
	    }

	  if (widget)
	    {
	      while (widget->priv->parent && widget->priv->parent != focus_widget->priv->parent)
		{
		  widget = widget->priv->parent;
		  gtk_container_set_focus_child (GTK_CONTAINER (widget), NULL);
		}
	    }
	}
      else if (toplevel != focus_widget)
	{
	  /* gtk_widget_grab_focus() operates on a tree without window...
	   * actually, this is very questionable behaviour.
	   */

	  gtk_container_foreach (GTK_CONTAINER (toplevel),
				 reset_focus_recurse,
				 NULL);
	}

      /* now propagate the new focus up the widget tree and finally
       * set it on the window
       */
      widget = focus_widget;
      while (widget->priv->parent)
	{
	  gtk_container_set_focus_child (GTK_CONTAINER (widget->priv->parent), widget);
	  widget = widget->priv->parent;
	}
      if (GTK_IS_WINDOW (widget))
	_gtk_window_internal_set_focus (GTK_WINDOW (widget), focus_widget);
    }
}

static gboolean
gtk_widget_real_query_tooltip (GtkWidget  *widget,
			       gint        x,
			       gint        y,
			       gboolean    keyboard_tip,
			       GtkTooltip *tooltip)
{
  gchar *tooltip_markup;
  gboolean has_tooltip;

  tooltip_markup = g_object_get_qdata (G_OBJECT (widget), quark_tooltip_markup);
  has_tooltip = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (widget), quark_has_tooltip));

  if (has_tooltip && tooltip_markup)
    {
      gtk_tooltip_set_markup (tooltip, tooltip_markup);
      return TRUE;
    }

  return FALSE;
}

static void
gtk_widget_real_style_updated (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  gtk_widget_update_pango_context (widget);

  if (priv->style != NULL &&
      priv->style != gtk_widget_get_default_style ())
    {
      /* Trigger ::style-set for old
       * widgets not listening to this
       */
      g_signal_emit (widget,
                     widget_signals[STYLE_SET],
                     0,
                     widget->priv->style);
    }

  if (widget->priv->context)
    {
      if (gtk_widget_get_realized (widget) &&
          gtk_widget_get_has_window (widget))
        gtk_style_context_set_background (widget->priv->context,
                                          widget->priv->window);
    }

  if (widget->priv->anchored)
    gtk_widget_queue_resize (widget);
}

static gboolean
gtk_widget_real_show_help (GtkWidget        *widget,
                           GtkWidgetHelpType help_type)
{
  if (help_type == GTK_WIDGET_HELP_TOOLTIP)
    {
      _gtk_tooltip_toggle_keyboard_mode (widget);

      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_widget_real_focus (GtkWidget         *widget,
                       GtkDirectionType   direction)
{
  if (!gtk_widget_get_can_focus (widget))
    return FALSE;

  if (!gtk_widget_is_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      return TRUE;
    }
  else
    return FALSE;
}

static void
gtk_widget_real_move_focus (GtkWidget         *widget,
                            GtkDirectionType   direction)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (widget != toplevel && GTK_IS_WINDOW (toplevel))
    {
      g_signal_emit (toplevel, widget_signals[MOVE_FOCUS], 0,
                     direction);
    }
}

static gboolean
gtk_widget_real_keynav_failed (GtkWidget        *widget,
                               GtkDirectionType  direction)
{
  gboolean cursor_only;

  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
      return FALSE;

    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      g_object_get (gtk_widget_get_settings (widget),
                    "gtk-keynav-cursor-only", &cursor_only,
                    NULL);
      if (cursor_only)
        return FALSE;
      break;
    }

  gtk_widget_error_bell (widget);

  return TRUE;
}

/**
 * gtk_widget_set_can_focus:
 * @widget: a #GtkWidget
 * @can_focus: whether or not @widget can own the input focus.
 *
 * Specifies whether @widget can own the input focus. See
 * gtk_widget_grab_focus() for actually setting the input focus on a
 * widget.
 *
 * Since: 2.18
 **/
void
gtk_widget_set_can_focus (GtkWidget *widget,
                          gboolean   can_focus)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->priv->can_focus != can_focus)
    {
      widget->priv->can_focus = can_focus;

      gtk_widget_queue_resize (widget);
      g_object_notify (G_OBJECT (widget), "can-focus");
    }
}

/**
 * gtk_widget_get_can_focus:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget can own the input focus. See
 * gtk_widget_set_can_focus().
 *
 * Return value: %TRUE if @widget can own the input focus, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_can_focus (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->can_focus;
}

/**
 * gtk_widget_has_focus:
 * @widget: a #GtkWidget
 *
 * Determines if the widget has the global input focus. See
 * gtk_widget_is_focus() for the difference between having the global
 * input focus, and only having the focus within a toplevel.
 *
 * Return value: %TRUE if the widget has the global input focus.
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_has_focus (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->has_focus;
}

/**
 * gtk_widget_has_visible_focus:
 * @widget: a #GtkWidget
 *
 * Determines if the widget should show a visible indication that
 * it has the global input focus. This is a convenience function for
 * use in ::draw handlers that takes into account whether focus
 * indication should currently be shown in the toplevel window of
 * @widget. See gtk_window_get_focus_visible() for more information
 * about focus indication.
 *
 * To find out if the widget has the global input focus, use
 * gtk_widget_has_focus().
 *
 * Return value: %TRUE if the widget should display a 'focus rectangle'
 *
 * Since: 3.2
 */
gboolean
gtk_widget_has_visible_focus (GtkWidget *widget)
{
  gboolean draw_focus;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (widget->priv->has_focus)
    {
      GtkWidget *toplevel;

      toplevel = gtk_widget_get_toplevel (widget);

      if (GTK_IS_WINDOW (toplevel))
        draw_focus = gtk_window_get_focus_visible (GTK_WINDOW (toplevel));
      else
        draw_focus = TRUE;
    }
  else
    draw_focus = FALSE;

  return draw_focus;
}

/**
 * gtk_widget_is_focus:
 * @widget: a #GtkWidget
 *
 * Determines if the widget is the focus widget within its
 * toplevel. (This does not mean that the %HAS_FOCUS flag is
 * necessarily set; %HAS_FOCUS will only be set if the
 * toplevel widget additionally has the global input focus.)
 *
 * Return value: %TRUE if the widget is the focus widget.
 **/
gboolean
gtk_widget_is_focus (GtkWidget *widget)
{
  GtkWidget *toplevel;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    return widget == gtk_window_get_focus (GTK_WINDOW (toplevel));
  else
    return FALSE;
}

/**
 * gtk_widget_set_can_default:
 * @widget: a #GtkWidget
 * @can_default: whether or not @widget can be a default widget.
 *
 * Specifies whether @widget can be a default widget. See
 * gtk_widget_grab_default() for details about the meaning of
 * "default".
 *
 * Since: 2.18
 **/
void
gtk_widget_set_can_default (GtkWidget *widget,
                            gboolean   can_default)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->priv->can_default != can_default)
    {
      widget->priv->can_default = can_default;

      gtk_widget_queue_resize (widget);
      g_object_notify (G_OBJECT (widget), "can-default");
    }
}

/**
 * gtk_widget_get_can_default:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget can be a default widget. See
 * gtk_widget_set_can_default().
 *
 * Return value: %TRUE if @widget can be a default widget, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_can_default (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->can_default;
}

/**
 * gtk_widget_has_default:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is the current default widget within its
 * toplevel. See gtk_widget_set_can_default().
 *
 * Return value: %TRUE if @widget is the current default widget within
 *     its toplevel, %FALSE otherwise
 *
 * Since: 2.18
 */
gboolean
gtk_widget_has_default (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->has_default;
}

void
_gtk_widget_set_has_default (GtkWidget *widget,
                             gboolean   has_default)
{
  widget->priv->has_default = has_default;
}

/**
 * gtk_widget_grab_default:
 * @widget: a #GtkWidget
 *
 * Causes @widget to become the default widget. @widget must have the
 * %GTK_CAN_DEFAULT flag set; typically you have to set this flag
 * yourself by calling <literal>gtk_widget_set_can_default (@widget,
 * %TRUE)</literal>. The default widget is activated when
 * the user presses Enter in a window. Default widgets must be
 * activatable, that is, gtk_widget_activate() should affect them. Note
 * that #GtkEntry widgets require the "activates-default" property
 * set to %TRUE before they activate the default widget when Enter
 * is pressed and the #GtkEntry is focused.
 **/
void
gtk_widget_grab_default (GtkWidget *widget)
{
  GtkWidget *window;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_can_default (widget));

  window = gtk_widget_get_toplevel (widget);

  if (window && gtk_widget_is_toplevel (window))
    gtk_window_set_default (GTK_WINDOW (window), widget);
  else
    g_warning (G_STRLOC ": widget not within a GtkWindow");
}

/**
 * gtk_widget_set_receives_default:
 * @widget: a #GtkWidget
 * @receives_default: whether or not @widget can be a default widget.
 *
 * Specifies whether @widget will be treated as the default widget
 * within its toplevel when it has the focus, even if another widget
 * is the default.
 *
 * See gtk_widget_grab_default() for details about the meaning of
 * "default".
 *
 * Since: 2.18
 **/
void
gtk_widget_set_receives_default (GtkWidget *widget,
                                 gboolean   receives_default)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (widget->priv->receives_default != receives_default)
    {
      widget->priv->receives_default = receives_default;

      g_object_notify (G_OBJECT (widget), "receives-default");
    }
}

/**
 * gtk_widget_get_receives_default:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is alyways treated as default widget
 * withing its toplevel when it has the focus, even if another widget
 * is the default.
 *
 * See gtk_widget_set_receives_default().
 *
 * Return value: %TRUE if @widget acts as default widget when focussed,
 *               %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_receives_default (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->receives_default;
}

/**
 * gtk_widget_has_grab:
 * @widget: a #GtkWidget
 *
 * Determines whether the widget is currently grabbing events, so it
 * is the only widget receiving input events (keyboard and mouse).
 *
 * See also gtk_grab_add().
 *
 * Return value: %TRUE if the widget is in the grab_widgets stack
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_has_grab (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->has_grab;
}

void
_gtk_widget_set_has_grab (GtkWidget *widget,
                          gboolean   has_grab)
{
  widget->priv->has_grab = has_grab;
}

/**
 * gtk_widget_device_is_shadowed:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Returns %TRUE if @device has been shadowed by a GTK+
 * device grab on another widget, so it would stop sending
 * events to @widget. This may be used in the
 * #GtkWidget::grab-notify signal to check for specific
 * devices. See gtk_device_grab_add().
 *
 * Returns: %TRUE if there is an ongoing grab on @device
 *          by another #GtkWidget than @widget.
 *
 * Since: 3.0
 **/
gboolean
gtk_widget_device_is_shadowed (GtkWidget *widget,
                               GdkDevice *device)
{
  GtkWindowGroup *group;
  GtkWidget *grab_widget, *toplevel;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  if (!gtk_widget_get_realized (widget))
    return TRUE;

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    group = gtk_window_get_group (GTK_WINDOW (toplevel));
  else
    group = gtk_window_get_group (NULL);

  grab_widget = gtk_window_group_get_current_device_grab (group, device);

  /* Widget not inside the hierarchy of grab_widget */
  if (grab_widget &&
      widget != grab_widget &&
      !gtk_widget_is_ancestor (widget, grab_widget))
    return TRUE;

  grab_widget = gtk_window_group_get_current_grab (group);
  if (grab_widget && widget != grab_widget &&
      !gtk_widget_is_ancestor (widget, grab_widget))
    return TRUE;

  return FALSE;
}

/**
 * gtk_widget_set_name:
 * @widget: a #GtkWidget
 * @name: name for the widget
 *
 * Widgets can be named, which allows you to refer to them from a
 * CSS file. You can apply a style to widgets with a particular name
 * in the CSS file. See the documentation for the CSS syntax (on the
 * same page as the docs for #GtkStyleContext).
 *
 * Note that the CSS syntax has certain special characters to delimit
 * and represent elements in a selector (period, &num;, &gt;, &ast;...),
 * so using these will make your widget impossible to match by name.
 * Any combination of alphanumeric symbols, dashes and underscores will
 * suffice.
 **/
void
gtk_widget_set_name (GtkWidget	 *widget,
		     const gchar *name)
{
  GtkWidgetPrivate *priv;
  gchar *new_name;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  new_name = g_strdup (name);
  g_free (priv->name);
  priv->name = new_name;

  gtk_widget_reset_style (widget);

  g_object_notify (G_OBJECT (widget), "name");
}

/**
 * gtk_widget_get_name:
 * @widget: a #GtkWidget
 *
 * Retrieves the name of a widget. See gtk_widget_set_name() for the
 * significance of widget names.
 *
 * Return value: name of the widget. This string is owned by GTK+ and
 * should not be modified or freed
 **/
const gchar*
gtk_widget_get_name (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;

  if (priv->name)
    return priv->name;
  return G_OBJECT_TYPE_NAME (widget);
}

static void
_gtk_widget_update_state_flags (GtkWidget     *widget,
                                GtkStateFlags  flags,
                                guint          operation)
{
  GtkWidgetPrivate *priv;

  priv = widget->priv;

  /* Handle insensitive first, since it is propagated
   * differently throughout the widget hierarchy.
   */
  if ((priv->state_flags & GTK_STATE_FLAG_INSENSITIVE) && (flags & GTK_STATE_FLAG_INSENSITIVE) && (operation == STATE_CHANGE_UNSET))
    gtk_widget_set_sensitive (widget, TRUE);
  else if (!(priv->state_flags & GTK_STATE_FLAG_INSENSITIVE) && (flags & GTK_STATE_FLAG_INSENSITIVE) && (operation != STATE_CHANGE_UNSET))
    gtk_widget_set_sensitive (widget, FALSE);
  else if ((priv->state_flags & GTK_STATE_FLAG_INSENSITIVE) && !(flags & GTK_STATE_FLAG_INSENSITIVE) && (operation == STATE_CHANGE_REPLACE))
    gtk_widget_set_sensitive (widget, TRUE);

  if (operation != STATE_CHANGE_REPLACE)
    flags &= ~(GTK_STATE_FLAG_INSENSITIVE);

  if (flags != 0 ||
      operation == STATE_CHANGE_REPLACE)
    {
      GtkStateData data;

      data.flags = flags;
      data.operation = operation;
      data.use_forall = FALSE;

      gtk_widget_propagate_state (widget, &data);

      gtk_widget_queue_resize (widget);
    }
}

/**
 * gtk_widget_set_state_flags:
 * @widget: a #GtkWidget
 * @flags: State flags to turn on
 * @clear: Whether to clear state before turning on @flags
 *
 * This function is for use in widget implementations. Turns on flag
 * values in the current widget state (insensitive, prelighted, etc.).
 *
 * It is worth mentioning that any other state than %GTK_STATE_FLAG_INSENSITIVE,
 * will be propagated down to all non-internal children if @widget is a
 * #GtkContainer, while %GTK_STATE_FLAG_INSENSITIVE itself will be propagated
 * down to all #GtkContainer children by different means than turning on the
 * state flag down the hierarchy, both gtk_widget_get_state_flags() and
 * gtk_widget_is_sensitive() will make use of these.
 *
 * Since: 3.0
 **/
void
gtk_widget_set_state_flags (GtkWidget     *widget,
                            GtkStateFlags  flags,
                            gboolean       clear)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if ((!clear && (widget->priv->state_flags & flags) == flags) ||
      (clear && widget->priv->state_flags == flags))
    return;

  if (clear)
    _gtk_widget_update_state_flags (widget, flags, STATE_CHANGE_REPLACE);
  else
    _gtk_widget_update_state_flags (widget, flags, STATE_CHANGE_SET);
}

/**
 * gtk_widget_unset_state_flags:
 * @widget: a #GtkWidget
 * @flags: State flags to turn off
 *
 * This function is for use in widget implementations. Turns off flag
 * values for the current widget state (insensitive, prelighted, etc.).
 * See gtk_widget_set_state_flags().
 *
 * Since: 3.0
 **/
void
gtk_widget_unset_state_flags (GtkWidget     *widget,
                              GtkStateFlags  flags)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if ((widget->priv->state_flags & flags) == 0)
    return;

  _gtk_widget_update_state_flags (widget, flags, STATE_CHANGE_UNSET);
}

/**
 * gtk_widget_get_state_flags:
 * @widget: a #GtkWidget
 *
 * Returns the widget state as a flag set. It is worth mentioning
 * that the effective %GTK_STATE_FLAG_INSENSITIVE state will be
 * returned, that is, also based on parent insensitivity, even if
 * @widget itself is sensitive.
 *
 * Returns: The state flags for widget
 *
 * Since: 3.0
 **/
GtkStateFlags
gtk_widget_get_state_flags (GtkWidget *widget)
{
  GtkStateFlags flags;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  flags = widget->priv->state_flags;

  if (gtk_widget_has_focus (widget))
    flags |= GTK_STATE_FLAG_FOCUSED;

  return flags;
}

/**
 * gtk_widget_set_state:
 * @widget: a #GtkWidget
 * @state: new state for @widget
 *
 * This function is for use in widget implementations. Sets the state
 * of a widget (insensitive, prelighted, etc.) Usually you should set
 * the state using wrapper functions such as gtk_widget_set_sensitive().
 *
 * Deprecated: 3.0. Use gtk_widget_set_state_flags() instead.
 **/
void
gtk_widget_set_state (GtkWidget           *widget,
		      GtkStateType         state)
{
  GtkStateFlags flags;

  if (state == gtk_widget_get_state (widget))
    return;

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_INCONSISTENT:
      flags = GTK_STATE_FLAG_INCONSISTENT;
      break;
    case GTK_STATE_FOCUSED:
      flags = GTK_STATE_FLAG_FOCUSED;
      break;
    case GTK_STATE_NORMAL:
    default:
      flags = 0;
      break;
    }

  _gtk_widget_update_state_flags (widget, flags, STATE_CHANGE_REPLACE);
}

/**
 * gtk_widget_get_state:
 * @widget: a #GtkWidget
 *
 * Returns the widget's state. See gtk_widget_set_state().
 *
 * Returns: the state of @widget.
 *
 * Since: 2.18
 *
 * Deprecated: 3.0. Use gtk_widget_get_state_flags() instead.
 */
GtkStateType
gtk_widget_get_state (GtkWidget *widget)
{
  GtkStateFlags flags;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_STATE_NORMAL);

  flags = gtk_widget_get_state_flags (widget);

  if (flags & GTK_STATE_FLAG_INSENSITIVE)
    return GTK_STATE_INSENSITIVE;
  else if (flags & GTK_STATE_FLAG_ACTIVE)
    return GTK_STATE_ACTIVE;
  else if (flags & GTK_STATE_FLAG_SELECTED)
    return GTK_STATE_SELECTED;
  else if (flags & GTK_STATE_FLAG_PRELIGHT)
    return GTK_STATE_PRELIGHT;
  else
    return GTK_STATE_NORMAL;
}

/**
 * gtk_widget_set_visible:
 * @widget: a #GtkWidget
 * @visible: whether the widget should be shown or not
 *
 * Sets the visibility state of @widget. Note that setting this to
 * %TRUE doesn't mean the widget is actually viewable, see
 * gtk_widget_get_visible().
 *
 * This function simply calls gtk_widget_show() or gtk_widget_hide()
 * but is nicer to use when the visibility of the widget depends on
 * some condition.
 *
 * Since: 2.18
 **/
void
gtk_widget_set_visible (GtkWidget *widget,
                        gboolean   visible)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (visible != gtk_widget_get_visible (widget))
    {
      if (visible)
        gtk_widget_show (widget);
      else
        gtk_widget_hide (widget);
    }
}

void
_gtk_widget_set_visible_flag (GtkWidget *widget,
                              gboolean   visible)
{
  widget->priv->visible = visible;
}

/**
 * gtk_widget_get_visible:
 * @widget: a #GtkWidget
 *
 * Determines whether the widget is visible. Note that this doesn't
 * take into account whether the widget's parent is also visible
 * or the widget is obscured in any way.
 *
 * See gtk_widget_set_visible().
 *
 * Return value: %TRUE if the widget is visible
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_visible (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->visible;
}

/**
 * gtk_widget_set_has_window:
 * @widget: a #GtkWidget
 * @has_window: whether or not @widget has a window.
 *
 * Specifies whether @widget has a #GdkWindow of its own. Note that
 * all realized widgets have a non-%NULL "window" pointer
 * (gtk_widget_get_window() never returns a %NULL window when a widget
 * is realized), but for many of them it's actually the #GdkWindow of
 * one of its parent widgets. Widgets that do not create a %window for
 * themselves in #GtkWidget::realize must announce this by
 * calling this function with @has_window = %FALSE.
 *
 * This function should only be called by widget implementations,
 * and they should call it in their init() function.
 *
 * Since: 2.18
 **/
void
gtk_widget_set_has_window (GtkWidget *widget,
                           gboolean   has_window)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->priv->no_window = !has_window;
}

/**
 * gtk_widget_get_has_window:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget has a #GdkWindow of its own. See
 * gtk_widget_set_has_window().
 *
 * Return value: %TRUE if @widget has a window, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_has_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return ! widget->priv->no_window;
}

/**
 * gtk_widget_is_toplevel:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is a toplevel widget.
 *
 * Currently only #GtkWindow and #GtkInvisible (and out-of-process
 * #GtkPlugs) are toplevel widgets. Toplevel widgets have no parent
 * widget.
 *
 * Return value: %TRUE if @widget is a toplevel, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_is_toplevel (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->toplevel;
}

void
_gtk_widget_set_is_toplevel (GtkWidget *widget,
                             gboolean   is_toplevel)
{
  widget->priv->toplevel = is_toplevel;
}

/**
 * gtk_widget_is_drawable:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget can be drawn to. A widget can be drawn
 * to if it is mapped and visible.
 *
 * Return value: %TRUE if @widget is drawable, %FALSE otherwise
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_is_drawable (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return (gtk_widget_get_visible (widget) &&
          gtk_widget_get_mapped (widget));
}

/**
 * gtk_widget_get_realized:
 * @widget: a #GtkWidget
 *
 * Determines whether @widget is realized.
 *
 * Return value: %TRUE if @widget is realized, %FALSE otherwise
 *
 * Since: 2.20
 **/
gboolean
gtk_widget_get_realized (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->realized;
}

/**
 * gtk_widget_set_realized:
 * @widget: a #GtkWidget
 * @realized: %TRUE to mark the widget as realized
 *
 * Marks the widget as being realized.
 *
 * This function should only ever be called in a derived widget's
 * "realize" or "unrealize" implementation.
 *
 * Since: 2.20
 */
void
gtk_widget_set_realized (GtkWidget *widget,
                         gboolean   realized)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->priv->realized = realized;
}

/**
 * gtk_widget_get_mapped:
 * @widget: a #GtkWidget
 *
 * Whether the widget is mapped.
 *
 * Return value: %TRUE if the widget is mapped, %FALSE otherwise.
 *
 * Since: 2.20
 */
gboolean
gtk_widget_get_mapped (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->mapped;
}

/**
 * gtk_widget_set_mapped:
 * @widget: a #GtkWidget
 * @mapped: %TRUE to mark the widget as mapped
 *
 * Marks the widget as being realized.
 *
 * This function should only ever be called in a derived widget's
 * "map" or "unmap" implementation.
 *
 * Since: 2.20
 */
void
gtk_widget_set_mapped (GtkWidget *widget,
                       gboolean   mapped)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->priv->mapped = mapped;
}

/**
 * gtk_widget_set_app_paintable:
 * @widget: a #GtkWidget
 * @app_paintable: %TRUE if the application will paint on the widget
 *
 * Sets whether the application intends to draw on the widget in
 * an #GtkWidget::draw handler.
 *
 * This is a hint to the widget and does not affect the behavior of
 * the GTK+ core; many widgets ignore this flag entirely. For widgets
 * that do pay attention to the flag, such as #GtkEventBox and #GtkWindow,
 * the effect is to suppress default themed drawing of the widget's
 * background. (Children of the widget will still be drawn.) The application
 * is then entirely responsible for drawing the widget background.
 *
 * Note that the background is still drawn when the widget is mapped.
 **/
void
gtk_widget_set_app_paintable (GtkWidget *widget,
			      gboolean   app_paintable)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  app_paintable = (app_paintable != FALSE);

  if (widget->priv->app_paintable != app_paintable)
    {
      widget->priv->app_paintable = app_paintable;

      if (gtk_widget_is_drawable (widget))
	gtk_widget_queue_draw (widget);

      g_object_notify (G_OBJECT (widget), "app-paintable");
    }
}

/**
 * gtk_widget_get_app_paintable:
 * @widget: a #GtkWidget
 *
 * Determines whether the application intends to draw on the widget in
 * an #GtkWidget::draw handler.
 *
 * See gtk_widget_set_app_paintable()
 *
 * Return value: %TRUE if the widget is app paintable
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_app_paintable (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->app_paintable;
}

/**
 * gtk_widget_set_double_buffered:
 * @widget: a #GtkWidget
 * @double_buffered: %TRUE to double-buffer a widget
 *
 * Widgets are double buffered by default; you can use this function
 * to turn off the buffering. "Double buffered" simply means that
 * gdk_window_begin_paint_region() and gdk_window_end_paint() are called
 * automatically around expose events sent to the
 * widget. gdk_window_begin_paint() diverts all drawing to a widget's
 * window to an offscreen buffer, and gdk_window_end_paint() draws the
 * buffer to the screen. The result is that users see the window
 * update in one smooth step, and don't see individual graphics
 * primitives being rendered.
 *
 * In very simple terms, double buffered widgets don't flicker,
 * so you would only use this function to turn off double buffering
 * if you had special needs and really knew what you were doing.
 *
 * Note: if you turn off double-buffering, you have to handle
 * expose events, since even the clearing to the background color or
 * pixmap will not happen automatically (as it is done in
 * gdk_window_begin_paint()).
 **/
void
gtk_widget_set_double_buffered (GtkWidget *widget,
				gboolean   double_buffered)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  double_buffered = (double_buffered != FALSE);

  if (widget->priv->double_buffered != double_buffered)
    {
      widget->priv->double_buffered = double_buffered;

      g_object_notify (G_OBJECT (widget), "double-buffered");
    }
}

/**
 * gtk_widget_get_double_buffered:
 * @widget: a #GtkWidget
 *
 * Determines whether the widget is double buffered.
 *
 * See gtk_widget_set_double_buffered()
 *
 * Return value: %TRUE if the widget is double buffered
 *
 * Since: 2.18
 **/
gboolean
gtk_widget_get_double_buffered (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->double_buffered;
}

/**
 * gtk_widget_set_redraw_on_allocate:
 * @widget: a #GtkWidget
 * @redraw_on_allocate: if %TRUE, the entire widget will be redrawn
 *   when it is allocated to a new size. Otherwise, only the
 *   new portion of the widget will be redrawn.
 *
 * Sets whether the entire widget is queued for drawing when its size
 * allocation changes. By default, this setting is %TRUE and
 * the entire widget is redrawn on every size change. If your widget
 * leaves the upper left unchanged when made bigger, turning this
 * setting off will improve performance.

 * Note that for %NO_WINDOW widgets setting this flag to %FALSE turns
 * off all allocation on resizing: the widget will not even redraw if
 * its position changes; this is to allow containers that don't draw
 * anything to avoid excess invalidations. If you set this flag on a
 * %NO_WINDOW widget that <emphasis>does</emphasis> draw on @widget->window,
 * you are responsible for invalidating both the old and new allocation
 * of the widget when the widget is moved and responsible for invalidating
 * regions newly when the widget increases size.
 **/
void
gtk_widget_set_redraw_on_allocate (GtkWidget *widget,
				   gboolean   redraw_on_allocate)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  widget->priv->redraw_on_alloc = redraw_on_allocate;
}

/**
 * gtk_widget_set_sensitive:
 * @widget: a #GtkWidget
 * @sensitive: %TRUE to make the widget sensitive
 *
 * Sets the sensitivity of a widget. A widget is sensitive if the user
 * can interact with it. Insensitive widgets are "grayed out" and the
 * user can't interact with them. Insensitive widgets are known as
 * "inactive", "disabled", or "ghosted" in some other toolkits.
 **/
void
gtk_widget_set_sensitive (GtkWidget *widget,
			  gboolean   sensitive)
{
  GtkWidgetPrivate *priv;
  GtkStateData data;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  sensitive = (sensitive != FALSE);

  if (priv->sensitive == sensitive)
    return;

  data.flags = GTK_STATE_FLAG_INSENSITIVE;

  if (sensitive)
    {
      priv->sensitive = TRUE;
      data.operation = STATE_CHANGE_UNSET;
    }
  else
    {
      priv->sensitive = FALSE;
      data.operation = STATE_CHANGE_SET;
    }

  data.use_forall = TRUE;

  gtk_widget_propagate_state (widget, &data);

  gtk_widget_queue_resize (widget);

  g_object_notify (G_OBJECT (widget), "sensitive");
}

/**
 * gtk_widget_get_sensitive:
 * @widget: a #GtkWidget
 *
 * Returns the widget's sensitivity (in the sense of returning
 * the value that has been set using gtk_widget_set_sensitive()).
 *
 * The effective sensitivity of a widget is however determined by both its
 * own and its parent widget's sensitivity. See gtk_widget_is_sensitive().
 *
 * Returns: %TRUE if the widget is sensitive
 *
 * Since: 2.18
 */
gboolean
gtk_widget_get_sensitive (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->sensitive;
}

/**
 * gtk_widget_is_sensitive:
 * @widget: a #GtkWidget
 *
 * Returns the widget's effective sensitivity, which means
 * it is sensitive itself and also its parent widget is sensitive
 *
 * Returns: %TRUE if the widget is effectively sensitive
 *
 * Since: 2.18
 */
gboolean
gtk_widget_is_sensitive (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return !(widget->priv->state_flags & GTK_STATE_FLAG_INSENSITIVE);
}

static void
_gtk_widget_update_path (GtkWidget *widget)
{
  if (widget->priv->path)
    {
      gtk_widget_path_free (widget->priv->path);
      widget->priv->path = NULL;
    }

  gtk_widget_get_path (widget);
}

/**
 * gtk_widget_set_parent:
 * @widget: a #GtkWidget
 * @parent: parent container
 *
 * This function is useful only when implementing subclasses of
 * #GtkContainer.
 * Sets the container as the parent of @widget, and takes care of
 * some details such as updating the state and style of the child
 * to reflect its new location. The opposite function is
 * gtk_widget_unparent().
 **/
void
gtk_widget_set_parent (GtkWidget *widget,
		       GtkWidget *parent)
{
  GtkStateFlags parent_flags;
  GtkWidgetPrivate *priv;
  GtkStateData data;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (parent));
  g_return_if_fail (widget != parent);

  priv = widget->priv;

  if (priv->parent != NULL)
    {
      g_warning ("Can't set a parent on widget which has a parent\n");
      return;
    }
  if (gtk_widget_is_toplevel (widget))
    {
      g_warning ("Can't set a parent on a toplevel widget\n");
      return;
    }

  /* keep this function in sync with gtk_menu_attach_to_widget()
   */

  g_object_ref_sink (widget);

  gtk_widget_push_verify_invariants (widget);

  priv->parent = parent;

  parent_flags = gtk_widget_get_state_flags (parent);

  /* Merge both old state and current parent state,
   * making sure to only propagate the right states */
  data.flags = parent_flags & GTK_STATE_FLAGS_DO_PROPAGATE;
  data.flags |= priv->state_flags;

  data.operation = STATE_CHANGE_REPLACE;
  data.use_forall = gtk_widget_is_sensitive (parent) != gtk_widget_is_sensitive (widget);
  gtk_widget_propagate_state (widget, &data);

  gtk_widget_reset_style (widget);

  g_signal_emit (widget, widget_signals[PARENT_SET], 0, NULL);
  if (priv->parent->priv->anchored)
    _gtk_widget_propagate_hierarchy_changed (widget, NULL);
  g_object_notify (G_OBJECT (widget), "parent");

  /* Enforce realized/mapped invariants
   */
  if (gtk_widget_get_realized (priv->parent))
    gtk_widget_realize (widget);

  if (gtk_widget_get_visible (priv->parent) &&
      gtk_widget_get_visible (widget))
    {
      if (gtk_widget_get_child_visible (widget) &&
	  gtk_widget_get_mapped (priv->parent))
	gtk_widget_map (widget);

      gtk_widget_queue_resize (widget);
    }

  /* child may cause parent's expand to change, if the child is
   * expanded. If child is not expanded, then it can't modify the
   * parent's expand. If the child becomes expanded later then it will
   * queue compute_expand then. This optimization plus defaulting
   * newly-constructed widgets to need_compute_expand=FALSE should
   * mean that initially building a widget tree doesn't have to keep
   * walking up setting need_compute_expand on parents over and over.
   *
   * We can't change a parent to need to expand unless we're visible.
   */
  if (gtk_widget_get_visible (widget) &&
      (priv->need_compute_expand ||
       priv->computed_hexpand ||
       priv->computed_vexpand))
    {
      gtk_widget_queue_compute_expand (parent);
    }

  gtk_widget_pop_verify_invariants (widget);
}

/**
 * gtk_widget_get_parent:
 * @widget: a #GtkWidget
 *
 * Returns the parent container of @widget.
 *
 * Return value: (transfer none): the parent container of @widget, or %NULL
 **/
GtkWidget *
gtk_widget_get_parent (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return widget->priv->parent;
}

/*****************************************
 * Widget styles
 * see docs/styles.txt
 *****************************************/

/**
 * gtk_widget_style_attach:
 * @widget: a #GtkWidget
 *
 * This function attaches the widget's #GtkStyle to the widget's
 * #GdkWindow. It is a replacement for
 *
 * <programlisting>
 * widget->style = gtk_style_attach (widget->style, widget->window);
 * </programlisting>
 *
 * and should only ever be called in a derived widget's "realize"
 * implementation which does not chain up to its parent class'
 * "realize" implementation, because one of the parent classes
 * (finally #GtkWidget) would attach the style itself.
 *
 * Since: 2.20
 *
 * Deprecated: 3.0. This step is unnecessary with #GtkStyleContext.
 **/
void
gtk_widget_style_attach (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (gtk_widget_get_realized (widget));
}

/**
 * gtk_widget_has_rc_style:
 * @widget: a #GtkWidget
 *
 * Determines if the widget style has been looked up through the rc mechanism.
 *
 * Returns: %TRUE if the widget has been looked up through the rc
 *   mechanism, %FALSE otherwise.
 *
 * Since: 2.20
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 **/
gboolean
gtk_widget_has_rc_style (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->rc_style;
}

/**
 * gtk_widget_set_style:
 * @widget: a #GtkWidget
 * @style: (allow-none): a #GtkStyle, or %NULL to remove the effect
 *     of a previous call to gtk_widget_set_style() and go back to
 *     the default style
 *
 * Used to set the #GtkStyle for a widget (@widget->style). Since
 * GTK 3, this function does nothing, the passed in style is ignored.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 */
void
gtk_widget_set_style (GtkWidget *widget,
                      GtkStyle  *style)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
}

/**
 * gtk_widget_ensure_style:
 * @widget: a #GtkWidget
 *
 * Ensures that @widget has a style (@widget->style).
 *
 * Not a very useful function; most of the time, if you
 * want the style, the widget is realized, and realized
 * widgets are guaranteed to have a style already.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 */
void
gtk_widget_ensure_style (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (priv->style == gtk_widget_get_default_style ())
    {
      g_object_unref (priv->style);

      priv->style = NULL;

      g_signal_emit (widget,
                     widget_signals[STYLE_SET],
                     0, NULL);
    }
}

/**
 * gtk_widget_get_style:
 * @widget: a #GtkWidget
 *
 * Simply an accessor function that returns @widget->style.
 *
 * Return value: (transfer none): the widget's #GtkStyle
 *
 * Deprecated:3.0: Use #GtkStyleContext instead
 */
GtkStyle*
gtk_widget_get_style (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;

  if (priv->style == NULL)
    {
      priv->style = g_object_new (GTK_TYPE_STYLE,
                                  "context", gtk_widget_get_style_context (widget),
                                  NULL);

    }

  return priv->style;
}

/**
 * gtk_widget_modify_style:
 * @widget: a #GtkWidget
 * @style: the #GtkRcStyle holding the style modifications
 *
 * Modifies style values on the widget.
 *
 * Modifications made using this technique take precedence over
 * style values set via an RC file, however, they will be overridden
 * if a style is explicitely set on the widget using gtk_widget_set_style().
 * The #GtkRcStyle structure is designed so each field can either be
 * set or unset, so it is possible, using this function, to modify some
 * style values and leave the others unchanged.
 *
 * Note that modifications made with this function are not cumulative
 * with previous calls to gtk_widget_modify_style() or with such
 * functions as gtk_widget_modify_fg(). If you wish to retain
 * previous values, you must first call gtk_widget_get_modifier_style(),
 * make your modifications to the returned style, then call
 * gtk_widget_modify_style() with that style. On the other hand,
 * if you first call gtk_widget_modify_style(), subsequent calls
 * to such functions gtk_widget_modify_fg() will have a cumulative
 * effect with the initial modifications.
 *
 * Deprecated:3.0: Use #GtkStyleContext with a custom #GtkStyleProvider instead
 */
void
gtk_widget_modify_style (GtkWidget      *widget,
                         GtkRcStyle     *style)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_RC_STYLE (style));

  g_object_set_qdata_full (G_OBJECT (widget),
                           quark_rc_style,
                           gtk_rc_style_copy (style),
                           (GDestroyNotify) g_object_unref);
}

/**
 * gtk_widget_get_modifier_style:
 * @widget: a #GtkWidget
 *
 * Returns the current modifier style for the widget. (As set by
 * gtk_widget_modify_style().) If no style has previously set, a new
 * #GtkRcStyle will be created with all values unset, and set as the
 * modifier style for the widget. If you make changes to this rc
 * style, you must call gtk_widget_modify_style(), passing in the
 * returned rc style, to make sure that your changes take effect.
 *
 * Caution: passing the style back to gtk_widget_modify_style() will
 * normally end up destroying it, because gtk_widget_modify_style() copies
 * the passed-in style and sets the copy as the new modifier style,
 * thus dropping any reference to the old modifier style. Add a reference
 * to the modifier style if you want to keep it alive.
 *
 * Return value: (transfer none): the modifier style for the widget.
 *     This rc style is owned by the widget. If you want to keep a
 *     pointer to value this around, you must add a refcount using
 *     g_object_ref().
 *
 * Deprecated:3.0: Use #GtkStyleContext with a custom #GtkStyleProvider instead
 */
GtkRcStyle *
gtk_widget_get_modifier_style (GtkWidget *widget)
{
  GtkRcStyle *rc_style;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  rc_style = g_object_get_qdata (G_OBJECT (widget), quark_rc_style);

  if (!rc_style)
    {
      rc_style = gtk_rc_style_new ();
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_rc_style,
                               rc_style,
                               (GDestroyNotify) g_object_unref);
    }

  return rc_style;
}

static void
gtk_widget_modify_color_component (GtkWidget      *widget,
                                   GtkRcFlags      component,
                                   GtkStateType    state,
                                   const GdkColor *color)
{
  GtkRcStyle *rc_style = gtk_widget_get_modifier_style (widget);

  if (color)
    {
      switch (component)
        {
        case GTK_RC_FG:
          rc_style->fg[state] = *color;
          break;
        case GTK_RC_BG:
          rc_style->bg[state] = *color;
          break;
        case GTK_RC_TEXT:
          rc_style->text[state] = *color;
          break;
        case GTK_RC_BASE:
          rc_style->base[state] = *color;
          break;
        default:
          g_assert_not_reached();
        }

      rc_style->color_flags[state] |= component;
    }
  else
    rc_style->color_flags[state] &= ~component;

  gtk_widget_modify_style (widget, rc_style);
}

static void
modifier_style_changed (GtkModifierStyle *style,
                        GtkWidget        *widget)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_invalidate (context);
}

static GtkModifierStyle *
_gtk_widget_get_modifier_properties (GtkWidget *widget)
{
  GtkModifierStyle *style;

  style = g_object_get_qdata (G_OBJECT (widget), quark_modifier_style);

  if (G_UNLIKELY (!style))
    {
      GtkStyleContext *context;

      style = _gtk_modifier_style_new ();
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_modifier_style,
                               style,
                               (GDestroyNotify) g_object_unref);

      g_signal_connect (style, "changed",
                        G_CALLBACK (modifier_style_changed), widget);

      context = gtk_widget_get_style_context (widget);

      gtk_style_context_add_provider (context,
                                      GTK_STYLE_PROVIDER (style),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  return style;
}

/**
 * gtk_widget_override_color:
 * @widget: a #GtkWidget
 * @state: the state for which to set the color
 * @color: (allow-none): the color to assign, or %NULL to undo the effect
 *     of previous calls to gtk_widget_override_color()
 *
 * Sets the color to use for a widget.
 *
 * All other style values are left untouched.
 *
 * <note><para>
 * This API is mostly meant as a quick way for applications to
 * change a widget appearance. If you are developing a widgets
 * library and intend this change to be themeable, it is better
 * done by setting meaningful CSS classes and regions in your
 * widget/container implementation through gtk_style_context_add_class()
 * and gtk_style_context_add_region().
 * </para><para>
 * This way, your widget library can install a #GtkCssProvider
 * with the %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK priority in order
 * to provide a default styling for those widgets that need so, and
 * this theming may fully overridden by the user's theme.
 * </para></note>
 * <note><para>
 * Note that for complex widgets this may bring in undesired
 * results (such as uniform background color everywhere), in
 * these cases it is better to fully style such widgets through a
 * #GtkCssProvider with the %GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
 * priority.
 * </para></note>
 *
 * Since: 3.0
 */
void
gtk_widget_override_color (GtkWidget     *widget,
                           GtkStateFlags  state,
                           const GdkRGBA *color)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_set_color (style, state, color);
}

/**
 * gtk_widget_override_background_color:
 * @widget: a #GtkWidget
 * @state: the state for which to set the background color
 * @color: (allow-none): the color to assign, or %NULL to undo the effect
 *     of previous calls to gtk_widget_override_background_color()
 *
 * Sets the background color to use for a widget.
 *
 * All other style values are left untouched.
 * See gtk_widget_override_color().
 *
 * Since: 3.0
 */
void
gtk_widget_override_background_color (GtkWidget     *widget,
                                      GtkStateFlags  state,
                                      const GdkRGBA *color)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_set_background_color (style, state, color);
}

/**
 * gtk_widget_override_font:
 * @widget: a #GtkWidget
 * @font_desc: (allow-none): the font descriptiong to use, or %NULL to undo
 *     the effect of previous calls to gtk_widget_override_font()
 *
 * Sets the font to use for a widget. All other style values are
 * left untouched. See gtk_widget_override_color().
 *
 * Since: 3.0
 */
void
gtk_widget_override_font (GtkWidget                  *widget,
                          const PangoFontDescription *font_desc)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_set_font (style, font_desc);
}

/**
 * gtk_widget_override_symbolic_color:
 * @widget: a #GtkWidget
 * @name: the name of the symbolic color to modify
 * @color: (allow-none): the color to assign (does not need
 *     to be allocated), or %NULL to undo the effect of previous
 *     calls to gtk_widget_override_symbolic_color()
 *
 * Sets a symbolic color for a widget.
 *
 * All other style values are left untouched.
 * See gtk_widget_override_color() for overriding the foreground
 * or background color.
 *
 * Since: 3.0
 */
void
gtk_widget_override_symbolic_color (GtkWidget     *widget,
                                    const gchar   *name,
                                    const GdkRGBA *color)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_map_color (style, name, color);
}

/**
 * gtk_widget_override_cursor:
 * @widget: a #GtkWidget
 * @cursor: (allow-none): the color to use for primary cursor (does not need to be
 *     allocated), or %NULL to undo the effect of previous calls to
 *     of gtk_widget_override_cursor().
 * @secondary_cursor: (allow-none): the color to use for secondary cursor (does not
 *     need to be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_override_cursor().
 *
 * Sets the cursor color to use in a widget, overriding the
 * #GtkWidget:cursor-color and #GtkWidget:secondary-cursor-color
 * style properties. All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * Note that the underlying properties have the #GdkColor type,
 * so the alpha value in @primary and @secondary will be ignored.
 *
 * Since: 3.0
 */
void
gtk_widget_override_cursor (GtkWidget     *widget,
                            const GdkRGBA *cursor,
                            const GdkRGBA *secondary_cursor)
{
  GtkModifierStyle *style;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  style = _gtk_widget_get_modifier_properties (widget);
  _gtk_modifier_style_set_color_property (style,
                                          GTK_TYPE_WIDGET,
                                          "cursor-color", cursor);
  _gtk_modifier_style_set_color_property (style,
                                          GTK_TYPE_WIDGET,
                                          "secondary-cursor-color",
                                          secondary_cursor);
}

/**
 * gtk_widget_modify_fg:
 * @widget: a #GtkWidget
 * @state: the state for which to set the foreground color
 * @color: (allow-none): the color to assign (does not need to be allocated),
 *     or %NULL to undo the effect of previous calls to
 *     of gtk_widget_modify_fg().
 *
 * Sets the foreground color for a widget in a particular state.
 *
 * All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * Deprecated:3.0: Use gtk_widget_override_color() instead
 */
void
gtk_widget_modify_fg (GtkWidget      *widget,
                      GtkStateType    state,
                      const GdkColor *color)
{
  GtkStateFlags flags;
  GdkRGBA rgba;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_NORMAL:
    default:
      flags = 0;
    }

  if (color)
    {
      rgba.red = color->red / 65535.;
      rgba.green = color->green / 65535.;
      rgba.blue = color->blue / 65535.;
      rgba.alpha = 1;

      gtk_widget_override_color (widget, flags, &rgba);
    }
  else
    gtk_widget_override_color (widget, flags, NULL);
}

/**
 * gtk_widget_modify_bg:
 * @widget: a #GtkWidget
 * @state: the state for which to set the background color
 * @color: (allow-none): the color to assign (does not need
 *     to be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_modify_bg().
 *
 * Sets the background color for a widget in a particular state.
 *
 * All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * <note><para>
 * Note that "no window" widgets (which have the %GTK_NO_WINDOW
 * flag set) draw on their parent container's window and thus may
 * not draw any background themselves. This is the case for e.g.
 * #GtkLabel.
 * </para><para>
 * To modify the background of such widgets, you have to set the
 * background color on their parent; if you want to set the background
 * of a rectangular area around a label, try placing the label in
 * a #GtkEventBox widget and setting the background color on that.
 * </para></note>
 *
 * Deprecated:3.0: Use gtk_widget_override_background_color() instead
 */
void
gtk_widget_modify_bg (GtkWidget      *widget,
                      GtkStateType    state,
                      const GdkColor *color)
{
  GtkStateFlags flags;
  GdkRGBA rgba;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  switch (state)
    {
    case GTK_STATE_ACTIVE:
      flags = GTK_STATE_FLAG_ACTIVE;
      break;
    case GTK_STATE_PRELIGHT:
      flags = GTK_STATE_FLAG_PRELIGHT;
      break;
    case GTK_STATE_SELECTED:
      flags = GTK_STATE_FLAG_SELECTED;
      break;
    case GTK_STATE_INSENSITIVE:
      flags = GTK_STATE_FLAG_INSENSITIVE;
      break;
    case GTK_STATE_NORMAL:
    default:
      flags = 0;
    }

  if (color)
    {
      rgba.red = color->red / 65535.;
      rgba.green = color->green / 65535.;
      rgba.blue = color->blue / 65535.;
      rgba.alpha = 1;

      gtk_widget_override_background_color (widget, flags, &rgba);
    }
  else
    gtk_widget_override_background_color (widget, flags, NULL);
}

/**
 * gtk_widget_modify_text:
 * @widget: a #GtkWidget
 * @state: the state for which to set the text color
 * @color: (allow-none): the color to assign (does not need to
 *     be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_modify_text().
 *
 * Sets the text color for a widget in a particular state.
 *
 * All other style values are left untouched.
 * The text color is the foreground color used along with the
 * base color (see gtk_widget_modify_base()) for widgets such
 * as #GtkEntry and #GtkTextView.
 * See also gtk_widget_modify_style().
 *
 * Deprecated:3.0: Use gtk_widget_override_color() instead
 */
void
gtk_widget_modify_text (GtkWidget      *widget,
                        GtkStateType    state,
                        const GdkColor *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  gtk_widget_modify_color_component (widget, GTK_RC_TEXT, state, color);
}

/**
 * gtk_widget_modify_base:
 * @widget: a #GtkWidget
 * @state: the state for which to set the base color
 * @color: (allow-none): the color to assign (does not need to
 *     be allocated), or %NULL to undo the effect of previous
 *     calls to of gtk_widget_modify_base().
 *
 * Sets the base color for a widget in a particular state.
 * All other style values are left untouched. The base color
 * is the background color used along with the text color
 * (see gtk_widget_modify_text()) for widgets such as #GtkEntry
 * and #GtkTextView. See also gtk_widget_modify_style().
 *
 * <note><para>
 * Note that "no window" widgets (which have the %GTK_NO_WINDOW
 * flag set) draw on their parent container's window and thus may
 * not draw any background themselves. This is the case for e.g.
 * #GtkLabel.
 * </para><para>
 * To modify the background of such widgets, you have to set the
 * base color on their parent; if you want to set the background
 * of a rectangular area around a label, try placing the label in
 * a #GtkEventBox widget and setting the base color on that.
 * </para></note>
 *
 * Deprecated:3.0: Use gtk_widget_override_background_color() instead
 */
void
gtk_widget_modify_base (GtkWidget      *widget,
                        GtkStateType    state,
                        const GdkColor *color)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (state >= GTK_STATE_NORMAL && state <= GTK_STATE_INSENSITIVE);

  gtk_widget_modify_color_component (widget, GTK_RC_BASE, state, color);
}

/**
 * gtk_widget_modify_cursor:
 * @widget: a #GtkWidget
 * @primary: the color to use for primary cursor (does not need to be
 *     allocated), or %NULL to undo the effect of previous calls to
 *     of gtk_widget_modify_cursor().
 * @secondary: the color to use for secondary cursor (does not need to be
 *     allocated), or %NULL to undo the effect of previous calls to
 *     of gtk_widget_modify_cursor().
 *
 * Sets the cursor color to use in a widget, overriding the
 * #GtkWidget:cursor-color and #GtkWidget:secondary-cursor-color
 * style properties.
 *
 * All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * Since: 2.12
 *
 * Deprecated: 3.0. Use gtk_widget_override_cursor() instead.
 */
void
gtk_widget_modify_cursor (GtkWidget      *widget,
                          const GdkColor *primary,
                          const GdkColor *secondary)
{
  GdkRGBA primary_rgba, secondary_rgba;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  primary_rgba.red = primary->red / 65535.;
  primary_rgba.green = primary->green / 65535.;
  primary_rgba.blue = primary->blue / 65535.;
  primary_rgba.alpha = 1;

  secondary_rgba.red = secondary->red / 65535.;
  secondary_rgba.green = secondary->green / 65535.;
  secondary_rgba.blue = secondary->blue / 65535.;
  secondary_rgba.alpha = 1;

  gtk_widget_override_cursor (widget, &primary_rgba, &secondary_rgba);
}

/**
 * gtk_widget_modify_font:
 * @widget: a #GtkWidget
 * @font_desc: (allow-none): the font description to use, or %NULL
 *     to undo the effect of previous calls to gtk_widget_modify_font()
 *
 * Sets the font to use for a widget.
 *
 * All other style values are left untouched.
 * See also gtk_widget_modify_style().
 *
 * Deprecated:3.0: Use gtk_widget_override_font() instead
 */
void
gtk_widget_modify_font (GtkWidget            *widget,
                        PangoFontDescription *font_desc)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_override_font (widget, font_desc);
}

static void
gtk_widget_real_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  previous_direction)
{
  gtk_widget_queue_resize (widget);
}

static void
gtk_widget_real_style_set (GtkWidget *widget,
                           GtkStyle  *previous_style)
{
}

typedef struct {
  GtkWidget *previous_toplevel;
  GdkScreen *previous_screen;
  GdkScreen *new_screen;
} HierarchyChangedInfo;

static void
do_screen_change (GtkWidget *widget,
		  GdkScreen *old_screen,
		  GdkScreen *new_screen)
{
  if (old_screen != new_screen)
    {
      GtkWidgetPrivate *priv = widget->priv;

      if (old_screen)
	{
	  PangoContext *context = g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
	  if (context)
	    g_object_set_qdata (G_OBJECT (widget), quark_pango_context, NULL);
	}

      _gtk_tooltip_hide (widget);

      if (new_screen && priv->context)
        gtk_style_context_set_screen (priv->context, new_screen);

      g_signal_emit (widget, widget_signals[SCREEN_CHANGED], 0, old_screen);
    }
}

static void
gtk_widget_propagate_hierarchy_changed_recurse (GtkWidget *widget,
						gpointer   client_data)
{
  GtkWidgetPrivate *priv = widget->priv;
  HierarchyChangedInfo *info = client_data;
  gboolean new_anchored = gtk_widget_is_toplevel (widget) ||
                 (priv->parent && priv->parent->priv->anchored);

  if (priv->anchored != new_anchored)
    {
      g_object_ref (widget);

      priv->anchored = new_anchored;

      g_signal_emit (widget, widget_signals[HIERARCHY_CHANGED], 0, info->previous_toplevel);
      do_screen_change (widget, info->previous_screen, info->new_screen);

      if (GTK_IS_CONTAINER (widget))
	gtk_container_forall (GTK_CONTAINER (widget),
			      gtk_widget_propagate_hierarchy_changed_recurse,
			      client_data);

      g_object_unref (widget);
    }
}

/**
 * _gtk_widget_propagate_hierarchy_changed:
 * @widget: a #GtkWidget
 * @previous_toplevel: Previous toplevel
 *
 * Propagates changes in the anchored state to a widget and all
 * children, unsetting or setting the %ANCHORED flag, and
 * emitting #GtkWidget::hierarchy-changed.
 **/
void
_gtk_widget_propagate_hierarchy_changed (GtkWidget *widget,
					 GtkWidget *previous_toplevel)
{
  GtkWidgetPrivate *priv = widget->priv;
  HierarchyChangedInfo info;

  info.previous_toplevel = previous_toplevel;
  info.previous_screen = previous_toplevel ? gtk_widget_get_screen (previous_toplevel) : NULL;

  if (gtk_widget_is_toplevel (widget) ||
      (priv->parent && priv->parent->priv->anchored))
    info.new_screen = gtk_widget_get_screen (widget);
  else
    info.new_screen = NULL;

  if (info.previous_screen)
    g_object_ref (info.previous_screen);
  if (previous_toplevel)
    g_object_ref (previous_toplevel);

  gtk_widget_propagate_hierarchy_changed_recurse (widget, &info);

  if (previous_toplevel)
    g_object_unref (previous_toplevel);
  if (info.previous_screen)
    g_object_unref (info.previous_screen);
}

static void
gtk_widget_propagate_screen_changed_recurse (GtkWidget *widget,
					     gpointer   client_data)
{
  HierarchyChangedInfo *info = client_data;

  g_object_ref (widget);

  do_screen_change (widget, info->previous_screen, info->new_screen);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  gtk_widget_propagate_screen_changed_recurse,
			  client_data);

  g_object_unref (widget);
}

/**
 * gtk_widget_is_composited:
 * @widget: a #GtkWidget
 *
 * Whether @widget can rely on having its alpha channel
 * drawn correctly. On X11 this function returns whether a
 * compositing manager is running for @widget's screen.
 *
 * Please note that the semantics of this call will change
 * in the future if used on a widget that has a composited
 * window in its hierarchy (as set by gdk_window_set_composited()).
 *
 * Return value: %TRUE if the widget can rely on its alpha
 * channel being drawn correctly.
 *
 * Since: 2.10
 */
gboolean
gtk_widget_is_composited (GtkWidget *widget)
{
  GdkScreen *screen;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  screen = gtk_widget_get_screen (widget);

  return gdk_screen_is_composited (screen);
}

static void
propagate_composited_changed (GtkWidget *widget,
			      gpointer dummy)
{
  if (GTK_IS_CONTAINER (widget))
    {
      gtk_container_forall (GTK_CONTAINER (widget),
			    propagate_composited_changed,
			    NULL);
    }

  g_signal_emit (widget, widget_signals[COMPOSITED_CHANGED], 0);
}

void
_gtk_widget_propagate_composited_changed (GtkWidget *widget)
{
  propagate_composited_changed (widget, NULL);
}

/**
 * _gtk_widget_propagate_screen_changed:
 * @widget: a #GtkWidget
 * @previous_screen: Previous screen
 *
 * Propagates changes in the screen for a widget to all
 * children, emitting #GtkWidget::screen-changed.
 **/
void
_gtk_widget_propagate_screen_changed (GtkWidget    *widget,
				      GdkScreen    *previous_screen)
{
  HierarchyChangedInfo info;

  info.previous_screen = previous_screen;
  info.new_screen = gtk_widget_get_screen (widget);

  if (previous_screen)
    g_object_ref (previous_screen);

  gtk_widget_propagate_screen_changed_recurse (widget, &info);

  if (previous_screen)
    g_object_unref (previous_screen);
}

static void
reset_style_recurse (GtkWidget *widget, gpointer data)
{
  _gtk_widget_update_path (widget);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  reset_style_recurse,
			  NULL);
}

/**
 * gtk_widget_reset_style:
 * @widget: a #GtkWidget
 *
 * Updates the style context of @widget and all descendents
 * by updating its widget path. #GtkContainer<!-- -->s may want
 * to use this on a child when reordering it in a way that a different
 * style might apply to it. See also gtk_container_get_path_for_child().
 *
 * Since: 3.0
 */
void
gtk_widget_reset_style (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  reset_style_recurse (widget, NULL);
}

/**
 * gtk_widget_reset_rc_styles:
 * @widget: a #GtkWidget.
 *
 * Reset the styles of @widget and all descendents, so when
 * they are looked up again, they get the correct values
 * for the currently loaded RC file settings.
 *
 * This function is not useful for applications.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead, and gtk_widget_reset_style()
 */
void
gtk_widget_reset_rc_styles (GtkWidget *widget)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  reset_style_recurse (widget, NULL);
}

/**
 * gtk_widget_get_default_style:
 *
 * Returns the default style used by all widgets initially.
 *
 * Returns: (transfer none): the default style. This #GtkStyle
 *     object is owned by GTK+ and should not be modified or freed.
 *
 * Deprecated:3.0: Use #GtkStyleContext instead, and
 *     gtk_css_provider_get_default() to obtain a #GtkStyleProvider
 *     with the default widget style information.
 */
GtkStyle*
gtk_widget_get_default_style (void)
{
  if (!gtk_default_style)
    {
      gtk_default_style = gtk_style_new ();
      g_object_ref (gtk_default_style);
    }

  return gtk_default_style;
}

#ifdef G_ENABLE_DEBUG

/* Verify invariants, see docs/widget_system.txt for notes on much of
 * this.  Invariants may be temporarily broken while we're in the
 * process of updating state, of course, so you can only
 * verify_invariants() after a given operation is complete.
 * Use push/pop_verify_invariants to help with that.
 */
static void
gtk_widget_verify_invariants (GtkWidget *widget)
{
  GtkWidget *parent;

  if (widget->priv->verifying_invariants_count > 0)
    return;

  parent = widget->priv->parent;

  if (widget->priv->mapped)
    {
      /* Mapped implies ... */

      if (!widget->priv->realized)
        g_warning ("%s %p is mapped but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (!widget->priv->visible)
        g_warning ("%s %p is mapped but not visible",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (!widget->priv->toplevel)
        {
          if (!widget->priv->child_visible)
            g_warning ("%s %p is mapped but not child_visible",
                       G_OBJECT_TYPE_NAME (widget), widget);
        }
    }
  else
    {
      /* Not mapped implies... */

#if 0
  /* This check makes sense for normal toplevels, but for
   * something like a toplevel that is embedded within a clutter
   * state, mapping may depend on external factors.
   */
      if (widget->priv->toplevel)
        {
          if (widget->priv->visible)
            g_warning ("%s %p toplevel is visible but not mapped",
                       G_OBJECT_TYPE_NAME (widget), widget);
        }
#endif
    }

  /* Parent related checks aren't possible if parent has
   * verifying_invariants_count > 0 because parent needs to recurse
   * children first before the invariants will hold.
   */
  if (parent == NULL || parent->priv->verifying_invariants_count == 0)
    {
      if (parent &&
          parent->priv->realized)
        {
          /* Parent realized implies... */

#if 0
          /* This is in widget_system.txt but appears to fail
           * because there's no gtk_container_realize() that
           * realizes all children... instead we just lazily
           * wait for map to fix things up.
           */
          if (!widget->priv->realized)
            g_warning ("%s %p is realized but child %s %p is not realized",
                       G_OBJECT_TYPE_NAME (parent), parent,
                       G_OBJECT_TYPE_NAME (widget), widget);
#endif
        }
      else if (!widget->priv->toplevel)
        {
          /* No parent or parent not realized on non-toplevel implies... */

          if (widget->priv->realized && !widget->priv->in_reparent)
            g_warning ("%s %p is not realized but child %s %p is realized",
                       parent ? G_OBJECT_TYPE_NAME (parent) : "no parent", parent,
                       G_OBJECT_TYPE_NAME (widget), widget);
        }

      if (parent &&
          parent->priv->mapped &&
          widget->priv->visible &&
          widget->priv->child_visible)
        {
          /* Parent mapped and we are visible implies... */

          if (!widget->priv->mapped)
            g_warning ("%s %p is mapped but visible child %s %p is not mapped",
                       G_OBJECT_TYPE_NAME (parent), parent,
                       G_OBJECT_TYPE_NAME (widget), widget);
        }
      else if (!widget->priv->toplevel)
        {
          /* No parent or parent not mapped on non-toplevel implies... */

          if (widget->priv->mapped && !widget->priv->in_reparent)
            g_warning ("%s %p is mapped but visible=%d child_visible=%d parent %s %p mapped=%d",
                       G_OBJECT_TYPE_NAME (widget), widget,
                       widget->priv->visible,
                       widget->priv->child_visible,
                       parent ? G_OBJECT_TYPE_NAME (parent) : "no parent", parent,
                       parent ? parent->priv->mapped : FALSE);
        }
    }

  if (!widget->priv->realized)
    {
      /* Not realized implies... */

#if 0
      /* widget_system.txt says these hold, but they don't. */
      if (widget->priv->resize_pending)
        g_warning ("%s %p resize pending but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (widget->priv->alloc_needed)
        g_warning ("%s %p alloc needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (widget->priv->width_request_needed)
        g_warning ("%s %p width request needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);

      if (widget->priv->height_request_needed)
        g_warning ("%s %p height request needed but not realized",
                   G_OBJECT_TYPE_NAME (widget), widget);
#endif
    }
}

/* The point of this push/pop is that invariants may not hold while
 * we're busy making changes. So we only check at the outermost call
 * on the call stack, after we finish updating everything.
 */
static void
gtk_widget_push_verify_invariants (GtkWidget *widget)
{
  widget->priv->verifying_invariants_count += 1;
}

static void
gtk_widget_verify_child_invariants (GtkWidget *widget,
                                    gpointer   client_data)
{
  /* We don't recurse further; this is a one-level check. */
  gtk_widget_verify_invariants (widget);
}

static void
gtk_widget_pop_verify_invariants (GtkWidget *widget)
{
  g_assert (widget->priv->verifying_invariants_count > 0);

  widget->priv->verifying_invariants_count -= 1;

  if (widget->priv->verifying_invariants_count == 0)
    {
      gtk_widget_verify_invariants (widget);

      if (GTK_IS_CONTAINER (widget))
        {
          /* Check one level of children, because our
           * push_verify_invariants() will have prevented some of the
           * checks. This does not recurse because if recursion is
           * needed, it will happen naturally as each child has a
           * push/pop on that child. For example if we're recursively
           * mapping children, we'll push/pop on each child as we map
           * it.
           */
          gtk_container_forall (GTK_CONTAINER (widget),
                                gtk_widget_verify_child_invariants,
                                NULL);
        }
    }
}
#endif /* G_ENABLE_DEBUG */

static PangoContext *
gtk_widget_peek_pango_context (GtkWidget *widget)
{
  return g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
}

/**
 * gtk_widget_get_pango_context:
 * @widget: a #GtkWidget
 *
 * Gets a #PangoContext with the appropriate font map, font description,
 * and base direction for this widget. Unlike the context returned
 * by gtk_widget_create_pango_context(), this context is owned by
 * the widget (it can be used until the screen for the widget changes
 * or the widget is removed from its toplevel), and will be updated to
 * match any changes to the widget's attributes.
 *
 * If you create and keep a #PangoLayout using this context, you must
 * deal with changes to the context by calling pango_layout_context_changed()
 * on the layout in response to the #GtkWidget::style-updated and
 * #GtkWidget::direction-changed signals for the widget.
 *
 * Return value: (transfer none): the #PangoContext for the widget.
 **/
PangoContext *
gtk_widget_get_pango_context (GtkWidget *widget)
{
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = g_object_get_qdata (G_OBJECT (widget), quark_pango_context);
  if (!context)
    {
      context = gtk_widget_create_pango_context (GTK_WIDGET (widget));
      g_object_set_qdata_full (G_OBJECT (widget),
			       quark_pango_context,
			       context,
			       g_object_unref);
    }

  return context;
}

static void
update_pango_context (GtkWidget    *widget,
		      PangoContext *context)
{
  const PangoFontDescription *font_desc;
  GtkStyleContext *style_context;

  style_context = gtk_widget_get_style_context (widget);

  font_desc = gtk_style_context_get_font (style_context,
                                          gtk_widget_get_state_flags (widget));

  pango_context_set_font_description (context, font_desc);
  pango_context_set_base_dir (context,
			      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR ?
			      PANGO_DIRECTION_LTR : PANGO_DIRECTION_RTL);
}

static void
gtk_widget_update_pango_context (GtkWidget *widget)
{
  PangoContext *context = gtk_widget_peek_pango_context (widget);

  if (context)
    {
      GdkScreen *screen;

      update_pango_context (widget, context);

      screen = gtk_widget_get_screen_unchecked (widget);
      if (screen)
	{
	  pango_cairo_context_set_resolution (context,
					      gdk_screen_get_resolution (screen));
	  pango_cairo_context_set_font_options (context,
						gdk_screen_get_font_options (screen));
	}
    }
}

/**
 * gtk_widget_create_pango_context:
 * @widget: a #GtkWidget
 *
 * Creates a new #PangoContext with the appropriate font map,
 * font description, and base direction for drawing text for
 * this widget. See also gtk_widget_get_pango_context().
 *
 * Return value: (transfer full): the new #PangoContext
 **/
PangoContext *
gtk_widget_create_pango_context (GtkWidget *widget)
{
  GdkScreen *screen;
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  screen = gtk_widget_get_screen_unchecked (widget);
  if (!screen)
    {
      GTK_NOTE (MULTIHEAD,
		g_warning ("gtk_widget_create_pango_context ()) called without screen"));

      screen = gdk_screen_get_default ();
    }

  context = gdk_pango_context_get_for_screen (screen);

  update_pango_context (widget, context);
  pango_context_set_language (context, gtk_get_default_language ());

  return context;
}

/**
 * gtk_widget_create_pango_layout:
 * @widget: a #GtkWidget
 * @text: text to set on the layout (can be %NULL)
 *
 * Creates a new #PangoLayout with the appropriate font map,
 * font description, and base direction for drawing text for
 * this widget.
 *
 * If you keep a #PangoLayout created in this way around, in order to
 * notify the layout of changes to the base direction or font of this
 * widget, you must call pango_layout_context_changed() in response to
 * the #GtkWidget::style-updated and #GtkWidget::direction-changed signals
 * for the widget.
 *
 * Return value: (transfer full): the new #PangoLayout
 **/
PangoLayout *
gtk_widget_create_pango_layout (GtkWidget   *widget,
				const gchar *text)
{
  PangoLayout *layout;
  PangoContext *context;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  context = gtk_widget_get_pango_context (widget);
  layout = pango_layout_new (context);

  if (text)
    pango_layout_set_text (layout, text, -1);

  return layout;
}

/**
 * gtk_widget_render_icon_pixbuf:
 * @widget: a #GtkWidget
 * @stock_id: a stock ID
 * @size: (type int): a stock size. A size of (GtkIconSize)-1 means
 *     render at the size of the source and don't scale (if there are
 *     multiple source sizes, GTK+ picks one of the available sizes).
 *
 * A convenience function that uses the theme engine and style
 * settings for @widget to look up @stock_id and render it to
 * a pixbuf. @stock_id should be a stock icon ID such as
 * #GTK_STOCK_OPEN or #GTK_STOCK_OK. @size should be a size
 * such as #GTK_ICON_SIZE_MENU.
 *
 * The pixels in the returned #GdkPixbuf are shared with the rest of
 * the application and should not be modified. The pixbuf should be freed
 * after use with g_object_unref().
 *
 * Return value: (transfer full): a new pixbuf, or %NULL if the
 *     stock ID wasn't known
 *
 * Since: 3.0
 **/
GdkPixbuf*
gtk_widget_render_icon_pixbuf (GtkWidget   *widget,
                               const gchar *stock_id,
                               GtkIconSize  size)
{
  GtkStyleContext *context;
  GtkIconSet *icon_set;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);
  g_return_val_if_fail (size > GTK_ICON_SIZE_INVALID || size == -1, NULL);

  context = gtk_widget_get_style_context (widget);
  icon_set = gtk_style_context_lookup_icon_set (context, stock_id);

  if (icon_set == NULL)
    return NULL;

  return gtk_icon_set_render_icon_pixbuf (icon_set, context, size);
}

/**
 * gtk_widget_render_icon:
 * @widget: a #GtkWidget
 * @stock_id: a stock ID
 * @size: (type int): a stock size. A size of (GtkIconSize)-1 means
 *     render at the size of the source and don't scale (if there are
 *     multiple source sizes, GTK+ picks one of the available sizes).
 * @detail: (allow-none): render detail to pass to theme engine
 *
 * A convenience function that uses the theme settings for @widget
 * to look up @stock_id and render it to a pixbuf. @stock_id should
 * be a stock icon ID such as #GTK_STOCK_OPEN or #GTK_STOCK_OK. @size
 * should be a size such as #GTK_ICON_SIZE_MENU. @detail should be a
 * string that identifies the widget or code doing the rendering, so
 * that theme engines can special-case rendering for that widget or
 * code.
 *
 * The pixels in the returned #GdkPixbuf are shared with the rest of
 * the application and should not be modified. The pixbuf should be
 * freed after use with g_object_unref().
 *
 * Return value: (transfer full): a new pixbuf, or %NULL if the
 *     stock ID wasn't known
 *
 * Deprecated: 3.0: Use gtk_widget_render_icon_pixbuf() instead.
 **/
GdkPixbuf*
gtk_widget_render_icon (GtkWidget      *widget,
                        const gchar    *stock_id,
                        GtkIconSize     size,
                        const gchar    *detail)
{
  gtk_widget_ensure_style (widget);

  return gtk_widget_render_icon_pixbuf (widget, stock_id, size);
}

/**
 * gtk_widget_set_parent_window:
 * @widget: a #GtkWidget.
 * @parent_window: the new parent window.
 *
 * Sets a non default parent window for @widget.
 *
 * For GtkWindow classes, setting a @parent_window effects whether
 * the window is a toplevel window or can be embedded into other
 * widgets.
 *
 * <note><para>
 * For GtkWindow classes, this needs to be called before the
 * window is realized.
 * </para></note>
 * 
 **/
void
gtk_widget_set_parent_window   (GtkWidget           *widget,
				GdkWindow           *parent_window)
{
  GdkWindow *old_parent_window;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_parent_window = g_object_get_qdata (G_OBJECT (widget),
					  quark_parent_window);

  if (parent_window != old_parent_window)
    {
      gboolean is_plug;

      g_object_set_qdata (G_OBJECT (widget), quark_parent_window,
			  parent_window);
      if (old_parent_window)
	g_object_unref (old_parent_window);
      if (parent_window)
	g_object_ref (parent_window);

      /* Unset toplevel flag when adding a parent window to a widget,
       * this is the primary entry point to allow toplevels to be
       * embeddable.
       */
#ifdef GDK_WINDOWING_X11
      is_plug = GTK_IS_PLUG (widget);
#else
      is_plug = FALSE;
#endif
      if (GTK_IS_WINDOW (widget) && !is_plug)
	_gtk_window_set_is_toplevel (GTK_WINDOW (widget), parent_window == NULL);
    }
}

/**
 * gtk_widget_get_parent_window:
 * @widget: a #GtkWidget.
 *
 * Gets @widget's parent window.
 *
 * Returns: (transfer none): the parent window of @widget.
 **/
GdkWindow *
gtk_widget_get_parent_window (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  GdkWindow *parent_window;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;

  parent_window = g_object_get_qdata (G_OBJECT (widget), quark_parent_window);

  return (parent_window != NULL) ? parent_window :
	 (priv->parent != NULL) ? priv->parent->priv->window : NULL;
}


/**
 * gtk_widget_set_child_visible:
 * @widget: a #GtkWidget
 * @is_visible: if %TRUE, @widget should be mapped along with its parent.
 *
 * Sets whether @widget should be mapped along with its when its parent
 * is mapped and @widget has been shown with gtk_widget_show().
 *
 * The child visibility can be set for widget before it is added to
 * a container with gtk_widget_set_parent(), to avoid mapping
 * children unnecessary before immediately unmapping them. However
 * it will be reset to its default state of %TRUE when the widget
 * is removed from a container.
 *
 * Note that changing the child visibility of a widget does not
 * queue a resize on the widget. Most of the time, the size of
 * a widget is computed from all visible children, whether or
 * not they are mapped. If this is not the case, the container
 * can queue a resize itself.
 *
 * This function is only useful for container implementations and
 * never should be called by an application.
 **/
void
gtk_widget_set_child_visible (GtkWidget *widget,
			      gboolean   is_visible)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!gtk_widget_is_toplevel (widget));

  priv = widget->priv;

  g_object_ref (widget);
  gtk_widget_verify_invariants (widget);

  if (is_visible)
    priv->child_visible = TRUE;
  else
    {
      GtkWidget *toplevel;

      priv->child_visible = FALSE;

      toplevel = gtk_widget_get_toplevel (widget);
      if (toplevel != widget && gtk_widget_is_toplevel (toplevel))
	_gtk_window_unset_focus_and_default (GTK_WINDOW (toplevel), widget);
    }

  if (priv->parent && gtk_widget_get_realized (priv->parent))
    {
      if (gtk_widget_get_mapped (priv->parent) &&
	  priv->child_visible &&
	  gtk_widget_get_visible (widget))
	gtk_widget_map (widget);
      else
	gtk_widget_unmap (widget);
    }

  gtk_widget_verify_invariants (widget);
  g_object_unref (widget);
}

/**
 * gtk_widget_get_child_visible:
 * @widget: a #GtkWidget
 *
 * Gets the value set with gtk_widget_set_child_visible().
 * If you feel a need to use this function, your code probably
 * needs reorganization.
 *
 * This function is only useful for container implementations and
 * never should be called by an application.
 *
 * Return value: %TRUE if the widget is mapped with the parent.
 **/
gboolean
gtk_widget_get_child_visible (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->child_visible;
}

static GdkScreen *
gtk_widget_get_screen_unchecked (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (widget);

  if (gtk_widget_is_toplevel (toplevel))
    {
      if (GTK_IS_WINDOW (toplevel))
	return gtk_window_get_screen (GTK_WINDOW (toplevel));
      else if (GTK_IS_INVISIBLE (toplevel))
	return gtk_invisible_get_screen (GTK_INVISIBLE (widget));
    }

  return NULL;
}

/**
 * gtk_widget_get_screen:
 * @widget: a #GtkWidget
 *
 * Get the #GdkScreen from the toplevel window associated with
 * this widget. This function can only be called after the widget
 * has been added to a widget hierarchy with a #GtkWindow
 * at the top.
 *
 * In general, you should only create screen specific
 * resources when a widget has been realized, and you should
 * free those resources when the widget is unrealized.
 *
 * Return value: (transfer none): the #GdkScreen for the toplevel for this widget.
 *
 * Since: 2.2
 **/
GdkScreen*
gtk_widget_get_screen (GtkWidget *widget)
{
  GdkScreen *screen;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  screen = gtk_widget_get_screen_unchecked (widget);

  if (screen)
    return screen;
  else
    {
#if 0
      g_warning (G_STRLOC ": Can't get associated screen"
		 " for a widget unless it is inside a toplevel GtkWindow\n"
		 " widget type is %s associated top level type is %s",
		 g_type_name (G_OBJECT_TYPE(G_OBJECT (widget))),
		 g_type_name (G_OBJECT_TYPE(G_OBJECT (toplevel))));
#endif
      return gdk_screen_get_default ();
    }
}

/**
 * gtk_widget_has_screen:
 * @widget: a #GtkWidget
 *
 * Checks whether there is a #GdkScreen is associated with
 * this widget. All toplevel widgets have an associated
 * screen, and all widgets added into a hierarchy with a toplevel
 * window at the top.
 *
 * Return value: %TRUE if there is a #GdkScreen associcated
 *   with the widget.
 *
 * Since: 2.2
 **/
gboolean
gtk_widget_has_screen (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return (gtk_widget_get_screen_unchecked (widget) != NULL);
}

/**
 * gtk_widget_get_display:
 * @widget: a #GtkWidget
 *
 * Get the #GdkDisplay for the toplevel window associated with
 * this widget. This function can only be called after the widget
 * has been added to a widget hierarchy with a #GtkWindow at the top.
 *
 * In general, you should only create display specific
 * resources when a widget has been realized, and you should
 * free those resources when the widget is unrealized.
 *
 * Return value: (transfer none): the #GdkDisplay for the toplevel for this widget.
 *
 * Since: 2.2
 **/
GdkDisplay*
gtk_widget_get_display (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_screen_get_display (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_get_root_window:
 * @widget: a #GtkWidget
 *
 * Get the root window where this widget is located. This function can
 * only be called after the widget has been added to a widget
 * hierarchy with #GtkWindow at the top.
 *
 * The root window is useful for such purposes as creating a popup
 * #GdkWindow associated with the window. In general, you should only
 * create display specific resources when a widget has been realized,
 * and you should free those resources when the widget is unrealized.
 *
 * Return value: (transfer none): the #GdkWindow root window for the toplevel for this widget.
 *
 * Since: 2.2
 **/
GdkWindow*
gtk_widget_get_root_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gdk_screen_get_root_window (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_child_focus:
 * @widget: a #GtkWidget
 * @direction: direction of focus movement
 *
 * This function is used by custom widget implementations; if you're
 * writing an app, you'd use gtk_widget_grab_focus() to move the focus
 * to a particular widget, and gtk_container_set_focus_chain() to
 * change the focus tab order. So you may want to investigate those
 * functions instead.
 *
 * gtk_widget_child_focus() is called by containers as the user moves
 * around the window using keyboard shortcuts. @direction indicates
 * what kind of motion is taking place (up, down, left, right, tab
 * forward, tab backward). gtk_widget_child_focus() emits the
 * #GtkWidget::focus signal; widgets override the default handler
 * for this signal in order to implement appropriate focus behavior.
 *
 * The default ::focus handler for a widget should return %TRUE if
 * moving in @direction left the focus on a focusable location inside
 * that widget, and %FALSE if moving in @direction moved the focus
 * outside the widget. If returning %TRUE, widgets normally
 * call gtk_widget_grab_focus() to place the focus accordingly;
 * if returning %FALSE, they don't modify the current focus location.
 *
 * Return value: %TRUE if focus ended up inside @widget
 **/
gboolean
gtk_widget_child_focus (GtkWidget       *widget,
                        GtkDirectionType direction)
{
  gboolean return_val;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!gtk_widget_get_visible (widget) ||
      !gtk_widget_is_sensitive (widget))
    return FALSE;

  /* child widgets must set CAN_FOCUS, containers
   * don't have to though.
   */
  if (!GTK_IS_CONTAINER (widget) &&
      !gtk_widget_get_can_focus (widget))
    return FALSE;

  g_signal_emit (widget,
		 widget_signals[FOCUS],
		 0,
		 direction, &return_val);

  return return_val;
}

/**
 * gtk_widget_keynav_failed:
 * @widget: a #GtkWidget
 * @direction: direction of focus movement
 *
 * This function should be called whenever keyboard navigation within
 * a single widget hits a boundary. The function emits the
 * #GtkWidget::keynav-failed signal on the widget and its return
 * value should be interpreted in a way similar to the return value of
 * gtk_widget_child_focus():
 *
 * When %TRUE is returned, stay in the widget, the failed keyboard
 * navigation is Ok and/or there is nowhere we can/should move the
 * focus to.
 *
 * When %FALSE is returned, the caller should continue with keyboard
 * navigation outside the widget, e.g. by calling
 * gtk_widget_child_focus() on the widget's toplevel.
 *
 * The default ::keynav-failed handler returns %TRUE for
 * %GTK_DIR_TAB_FORWARD and %GTK_DIR_TAB_BACKWARD. For the other
 * values of #GtkDirectionType, it looks at the
 * #GtkSettings:gtk-keynav-cursor-only setting and returns %FALSE
 * if the setting is %TRUE. This way the entire user interface
 * becomes cursor-navigatable on input devices such as mobile phones
 * which only have cursor keys but no tab key.
 *
 * Whenever the default handler returns %TRUE, it also calls
 * gtk_widget_error_bell() to notify the user of the failed keyboard
 * navigation.
 *
 * A use case for providing an own implementation of ::keynav-failed
 * (either by connecting to it or by overriding it) would be a row of
 * #GtkEntry widgets where the user should be able to navigate the
 * entire row with the cursor keys, as e.g. known from user interfaces
 * that require entering license keys.
 *
 * Return value: %TRUE if stopping keyboard navigation is fine, %FALSE
 *               if the emitting widget should try to handle the keyboard
 *               navigation attempt in its parent container(s).
 *
 * Since: 2.12
 **/
gboolean
gtk_widget_keynav_failed (GtkWidget        *widget,
                          GtkDirectionType  direction)
{
  gboolean return_val;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  g_signal_emit (widget, widget_signals[KEYNAV_FAILED], 0,
		 direction, &return_val);

  return return_val;
}

/**
 * gtk_widget_error_bell:
 * @widget: a #GtkWidget
 *
 * Notifies the user about an input-related error on this widget.
 * If the #GtkSettings:gtk-error-bell setting is %TRUE, it calls
 * gdk_window_beep(), otherwise it does nothing.
 *
 * Note that the effect of gdk_window_beep() can be configured in many
 * ways, depending on the windowing backend and the desktop environment
 * or window manager that is used.
 *
 * Since: 2.12
 **/
void
gtk_widget_error_bell (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  GtkSettings* settings;
  gboolean beep;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  settings = gtk_widget_get_settings (widget);
  if (!settings)
    return;

  g_object_get (settings,
                "gtk-error-bell", &beep,
                NULL);

  if (beep && priv->window)
    gdk_window_beep (priv->window);
}

static void
gtk_widget_set_usize_internal (GtkWidget          *widget,
			       gint                width,
			       gint                height,
			       GtkQueueResizeFlags flags)
{
  GtkWidgetAuxInfo *aux_info;
  gboolean changed = FALSE;

  g_object_freeze_notify (G_OBJECT (widget));

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (width > -2 && aux_info->width != width)
    {
      if ((flags & GTK_QUEUE_RESIZE_INVALIDATE_ONLY) == 0)
	g_object_notify (G_OBJECT (widget), "width-request");
      aux_info->width = width;
      changed = TRUE;
    }
  if (height > -2 && aux_info->height != height)
    {
      if ((flags & GTK_QUEUE_RESIZE_INVALIDATE_ONLY) == 0)
	g_object_notify (G_OBJECT (widget), "height-request");
      aux_info->height = height;
      changed = TRUE;
    }

  if (gtk_widget_get_visible (widget) && changed)
    {
      if ((flags & GTK_QUEUE_RESIZE_INVALIDATE_ONLY) == 0)
	gtk_widget_queue_resize (widget);
      else
	_gtk_size_group_queue_resize (widget, GTK_QUEUE_RESIZE_INVALIDATE_ONLY);
    }

  g_object_thaw_notify (G_OBJECT (widget));
}

/**
 * gtk_widget_set_size_request:
 * @widget: a #GtkWidget
 * @width: width @widget should request, or -1 to unset
 * @height: height @widget should request, or -1 to unset
 *
 * Sets the minimum size of a widget; that is, the widget's size
 * request will be @width by @height. You can use this function to
 * force a widget to be either larger or smaller than it normally
 * would be.
 *
 * In most cases, gtk_window_set_default_size() is a better choice for
 * toplevel windows than this function; setting the default size will
 * still allow users to shrink the window. Setting the size request
 * will force them to leave the window at least as large as the size
 * request. When dealing with window sizes,
 * gtk_window_set_geometry_hints() can be a useful function as well.
 *
 * Note the inherent danger of setting any fixed size - themes,
 * translations into other languages, different fonts, and user action
 * can all change the appropriate size for a given widget. So, it's
 * basically impossible to hardcode a size that will always be
 * correct.
 *
 * The size request of a widget is the smallest size a widget can
 * accept while still functioning well and drawing itself correctly.
 * However in some strange cases a widget may be allocated less than
 * its requested size, and in many cases a widget may be allocated more
 * space than it requested.
 *
 * If the size request in a given direction is -1 (unset), then
 * the "natural" size request of the widget will be used instead.
 *
 * Widgets can't actually be allocated a size less than 1 by 1, but
 * you can pass 0,0 to this function to mean "as small as possible."
 *
 * The size request set here does not include any margin from the
 * #GtkWidget properties margin-left, margin-right, margin-top, and
 * margin-bottom, but it does include pretty much all other padding
 * or border properties set by any subclass of #GtkWidget.
 **/
void
gtk_widget_set_size_request (GtkWidget *widget,
                             gint       width,
                             gint       height)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (width >= -1);
  g_return_if_fail (height >= -1);

  if (width == 0)
    width = 1;
  if (height == 0)
    height = 1;

  gtk_widget_set_usize_internal (widget, width, height, 0);
}


/**
 * gtk_widget_get_size_request:
 * @widget: a #GtkWidget
 * @width: (out) (allow-none): return location for width, or %NULL
 * @height: (out) (allow-none): return location for height, or %NULL
 *
 * Gets the size request that was explicitly set for the widget using
 * gtk_widget_set_size_request(). A value of -1 stored in @width or
 * @height indicates that that dimension has not been set explicitly
 * and the natural requisition of the widget will be used intead. See
 * gtk_widget_set_size_request(). To get the size a widget will
 * actually request, call gtk_widget_get_preferred_size() instead of
 * this function.
 **/
void
gtk_widget_get_size_request (GtkWidget *widget,
                             gint      *width,
                             gint      *height)
{
  const GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  aux_info = _gtk_widget_get_aux_info_or_defaults (widget);

  if (width)
    *width = aux_info->width;

  if (height)
    *height = aux_info->height;
}

/**
 * _gtk_widget_override_size_request:
 * @widget: a #GtkWidget
 * @width: new forced minimum width
 * @height: new forced minimum height
 * @old_width: location to store previous forced minimum width
 * @old_width: location to store previous forced minumum height
 *
 * Temporarily establishes a forced minimum size for a widget; this
 * is used by GtkWindow when calculating the size to add to the
 * window's geometry widget. Cached sizes for the widget and its
 * parents are invalidated, so that subsequent calls to the size
 * negotiation machinery produce the overriden result, but the
 * widget is not queued for relayout or redraw. The old size must
 * be restored with _gtk_widget_restore_size_request() or things
 * will go screwy.
 */
void
_gtk_widget_override_size_request (GtkWidget *widget,
				   int        width,
				   int        height,
				   int       *old_width,
				   int       *old_height)
{
  gtk_widget_get_size_request (widget, old_width, old_height);
  gtk_widget_set_usize_internal (widget, width, height,
				 GTK_QUEUE_RESIZE_INVALIDATE_ONLY);
}

/**
 * _gtk_widget_restore_size_request:
 * @widget: a #GtkWidget
 * @old_width: saved forced minimum size
 * @old_height: saved forced minimum size
 *
 * Undoes the operation of_gtk_widget_override_size_request().
 */
void
_gtk_widget_restore_size_request (GtkWidget *widget,
				  int        old_width,
				  int        old_height)
{
  gtk_widget_set_usize_internal (widget, old_width, old_height,
				 GTK_QUEUE_RESIZE_INVALIDATE_ONLY);
}

/**
 * gtk_widget_set_events:
 * @widget: a #GtkWidget
 * @events: event mask
 *
 * Sets the event mask (see #GdkEventMask) for a widget. The event
 * mask determines which events a widget will receive. Keep in mind
 * that different widgets have different default event masks, and by
 * changing the event mask you may disrupt a widget's functionality,
 * so be careful. This function must be called while a widget is
 * unrealized. Consider gtk_widget_add_events() for widgets that are
 * already realized, or if you want to preserve the existing event
 * mask. This function can't be used with #GTK_NO_WINDOW widgets;
 * to get events on those widgets, place them inside a #GtkEventBox
 * and receive events on the event box.
 **/
void
gtk_widget_set_events (GtkWidget *widget,
		       gint	  events)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!gtk_widget_get_realized (widget));

  g_object_set_qdata (G_OBJECT (widget), quark_event_mask,
                      GINT_TO_POINTER (events));
  g_object_notify (G_OBJECT (widget), "events");
}

/**
 * gtk_widget_set_device_events:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 * @events: event mask
 *
 * Sets the device event mask (see #GdkEventMask) for a widget. The event
 * mask determines which events a widget will receive from @device. Keep
 * in mind that different widgets have different default event masks, and by
 * changing the event mask you may disrupt a widget's functionality,
 * so be careful. This function must be called while a widget is
 * unrealized. Consider gtk_widget_add_device_events() for widgets that are
 * already realized, or if you want to preserve the existing event
 * mask. This function can't be used with #GTK_NO_WINDOW widgets;
 * to get events on those widgets, place them inside a #GtkEventBox
 * and receive events on the event box.
 *
 * Since: 3.0
 **/
void
gtk_widget_set_device_events (GtkWidget    *widget,
                              GdkDevice    *device,
                              GdkEventMask  events)
{
  GHashTable *device_events;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (!gtk_widget_get_realized (widget));

  device_events = g_object_get_qdata (G_OBJECT (widget), quark_device_event_mask);

  if (G_UNLIKELY (!device_events))
    {
      device_events = g_hash_table_new (NULL, NULL);
      g_object_set_qdata_full (G_OBJECT (widget), quark_device_event_mask, device_events,
                               (GDestroyNotify) g_hash_table_unref);
    }

  g_hash_table_insert (device_events, device, GUINT_TO_POINTER (events));
}

/**
 * gtk_widget_set_device_enabled:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 * @enabled: whether to enable the device
 *
 * Enables or disables a #GdkDevice to interact with @widget
 * and all its children.
 *
 * It does so by descending through the #GdkWindow hierarchy
 * and enabling the same mask that is has for core events
 * (i.e. the one that gdk_window_get_events() returns).
 *
 * Since: 3.0
 */
void
gtk_widget_set_device_enabled (GtkWidget *widget,
                               GdkDevice *device,
                               gboolean   enabled)
{
  GList *enabled_devices;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));

  enabled_devices = g_object_get_qdata (G_OBJECT (widget), quark_enabled_devices);
  enabled_devices = g_list_append (enabled_devices, device);

  g_object_set_qdata_full (G_OBJECT (widget), quark_enabled_devices,
                           enabled_devices, (GDestroyNotify) g_list_free);;

  if (gtk_widget_get_realized (widget))
    gtk_widget_set_device_enabled_internal (widget, device, TRUE, enabled);
}

/**
 * gtk_widget_get_device_enabled:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Returns whether @device can interact with @widget and its
 * children. See gtk_widget_set_device_enabled().
 *
 * Return value: %TRUE is @device is enabled for @widget
 *
 * Since: 3.0
 */
gboolean
gtk_widget_get_device_enabled (GtkWidget *widget,
                               GdkDevice *device)
{
  GList *enabled_devices;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  enabled_devices = g_object_get_qdata (G_OBJECT (widget), quark_enabled_devices);

  return g_list_find (enabled_devices, device) != NULL;
}

static void
gtk_widget_add_events_internal_list (GtkWidget *widget,
                                     GdkDevice *device,
                                     gint       events,
                                     GList     *window_list)
{
  GList *l;

  for (l = window_list; l != NULL; l = l->next)
    {
      GdkWindow *window = l->data;
      gpointer user_data;

      gdk_window_get_user_data (window, &user_data);
      if (user_data == widget)
        {
          GList *children;

          if (device)
            gdk_window_set_device_events (window, device, gdk_window_get_events (window) | events);
          else
            gdk_window_set_events (window, gdk_window_get_events (window) | events);

          children = gdk_window_get_children (window);
          gtk_widget_add_events_internal_list (widget, device, events, children);
          g_list_free (children);
        }
    }
}

static void
gtk_widget_add_events_internal (GtkWidget *widget,
                                GdkDevice *device,
                                gint       events)
{
  GtkWidgetPrivate *priv = widget->priv;
  GList *window_list;

  if (!gtk_widget_get_has_window (widget))
    window_list = gdk_window_get_children (priv->window);
  else
    window_list = g_list_prepend (NULL, priv->window);

  gtk_widget_add_events_internal_list (widget, device, events, window_list);

  g_list_free (window_list);
}

/**
 * gtk_widget_add_events:
 * @widget: a #GtkWidget
 * @events: an event mask, see #GdkEventMask
 *
 * Adds the events in the bitfield @events to the event mask for
 * @widget. See gtk_widget_set_events() for details.
 **/
void
gtk_widget_add_events (GtkWidget *widget,
		       gint	  events)
{
  gint old_events;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_events = GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget), quark_event_mask));
  g_object_set_qdata (G_OBJECT (widget), quark_event_mask,
                      GINT_TO_POINTER (old_events | events));

  if (gtk_widget_get_realized (widget))
    {
      gtk_widget_add_events_internal (widget, NULL, events);
      gtk_widget_update_devices_mask (widget, FALSE);
    }

  g_object_notify (G_OBJECT (widget), "events");
}

/**
 * gtk_widget_add_device_events:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 * @events: an event mask, see #GdkEventMask
 *
 * Adds the device events in the bitfield @events to the event mask for
 * @widget. See gtk_widget_set_device_events() for details.
 *
 * Since: 3.0
 **/
void
gtk_widget_add_device_events (GtkWidget    *widget,
                              GdkDevice    *device,
                              GdkEventMask  events)
{
  GdkEventMask old_events;
  GHashTable *device_events;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));

  old_events = gtk_widget_get_device_events (widget, device);

  device_events = g_object_get_qdata (G_OBJECT (widget), quark_device_event_mask);

  if (G_UNLIKELY (!device_events))
    {
      device_events = g_hash_table_new (NULL, NULL);
      g_object_set_qdata_full (G_OBJECT (widget), quark_device_event_mask, device_events,
                               (GDestroyNotify) g_hash_table_unref);
    }

  g_hash_table_insert (device_events, device,
                       GUINT_TO_POINTER (old_events | events));

  if (gtk_widget_get_realized (widget))
    gtk_widget_add_events_internal (widget, device, events);

  g_object_notify (G_OBJECT (widget), "events");
}

/**
 * gtk_widget_get_toplevel:
 * @widget: a #GtkWidget
 *
 * This function returns the topmost widget in the container hierarchy
 * @widget is a part of. If @widget has no parent widgets, it will be
 * returned as the topmost widget. No reference will be added to the
 * returned widget; it should not be unreferenced.
 *
 * Note the difference in behavior vs. gtk_widget_get_ancestor();
 * <literal>gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW)</literal>
 * would return
 * %NULL if @widget wasn't inside a toplevel window, and if the
 * window was inside a #GtkWindow-derived widget which was in turn
 * inside the toplevel #GtkWindow. While the second case may
 * seem unlikely, it actually happens when a #GtkPlug is embedded
 * inside a #GtkSocket within the same application.
 *
 * To reliably find the toplevel #GtkWindow, use
 * gtk_widget_get_toplevel() and check if the %TOPLEVEL flags
 * is set on the result.
 * |[
 *  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
 *  if (gtk_widget_is_toplevel (toplevel))
 *    {
 *      /&ast; Perform action on toplevel. &ast;/
 *    }
 * ]|
 *
 * Return value: (transfer none): the topmost ancestor of @widget, or @widget itself
 *    if there's no ancestor.
 **/
GtkWidget*
gtk_widget_get_toplevel (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  while (widget->priv->parent)
    widget = widget->priv->parent;

  return widget;
}

/**
 * gtk_widget_get_ancestor:
 * @widget: a #GtkWidget
 * @widget_type: ancestor type
 *
 * Gets the first ancestor of @widget with type @widget_type. For example,
 * <literal>gtk_widget_get_ancestor (widget, GTK_TYPE_BOX)</literal> gets
 * the first #GtkBox that's an ancestor of @widget. No reference will be
 * added to the returned widget; it should not be unreferenced. See note
 * about checking for a toplevel #GtkWindow in the docs for
 * gtk_widget_get_toplevel().
 *
 * Note that unlike gtk_widget_is_ancestor(), gtk_widget_get_ancestor()
 * considers @widget to be an ancestor of itself.
 *
 * Return value: (transfer none): the ancestor widget, or %NULL if not found
 **/
GtkWidget*
gtk_widget_get_ancestor (GtkWidget *widget,
			 GType      widget_type)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  while (widget && !g_type_is_a (G_OBJECT_TYPE (widget), widget_type))
    widget = widget->priv->parent;

  if (!(widget && g_type_is_a (G_OBJECT_TYPE (widget), widget_type)))
    return NULL;

  return widget;
}

/**
 * gtk_widget_set_visual:
 * @widget: a #GtkWidget
 * @visual: visual to be used or %NULL to unset a previous one
 *
 * Sets the visual that should be used for by widget and its children for
 * creating #GdkWindows. The visual must be on the same #GdkScreen as
 * returned by gdk_widget_get_screen(), so handling the
 * #GtkWidget::screen-changed signal is necessary.
 *
 * Setting a new @visual will not cause @widget to recreate its windows,
 * so you should call this function before @widget is realized.
 **/
void
gtk_widget_set_visual (GtkWidget *widget,
                       GdkVisual *visual)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (visual == NULL || GDK_IS_VISUAL (visual));
  if (visual)
    {
      g_return_if_fail (gtk_widget_get_screen (widget) == gdk_visual_get_screen (visual));
    }

  g_object_set_qdata_full (G_OBJECT (widget),
                           quark_visual,
                           g_object_ref (visual),
                           g_object_unref);
}

/**
 * gtk_widget_get_visual:
 * @widget: a #GtkWidget
 *
 * Gets the visual that will be used to render @widget.
 *
 * Return value: (transfer none): the visual for @widget
 **/
GdkVisual*
gtk_widget_get_visual (GtkWidget *widget)
{
  GtkWidget *w;
  GdkVisual *visual;
  GdkScreen *screen;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (gtk_widget_get_has_window (widget) &&
      widget->priv->window)
    return gdk_window_get_visual (widget->priv->window);

  screen = gtk_widget_get_screen (widget);

  for (w = widget; w != NULL; w = w->priv->parent)
    {
      visual = g_object_get_qdata (G_OBJECT (w), quark_visual);
      if (visual)
        {
          if (gdk_visual_get_screen (visual) == screen)
            return visual;

          g_warning ("Ignoring visual set on widget `%s' that is not on the correct screen.",
                     gtk_widget_get_name (widget));
        }
    }

  return gdk_screen_get_system_visual (screen);
}

/**
 * gtk_widget_get_settings:
 * @widget: a #GtkWidget
 *
 * Gets the settings object holding the settings used for this widget.
 *
 * Note that this function can only be called when the #GtkWidget
 * is attached to a toplevel, since the settings object is specific
 * to a particular #GdkScreen.
 *
 * Return value: (transfer none): the relevant #GtkSettings object
 */
GtkSettings*
gtk_widget_get_settings (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return gtk_settings_get_for_screen (gtk_widget_get_screen (widget));
}

/**
 * gtk_widget_get_events:
 * @widget: a #GtkWidget
 *
 * Returns the event mask for the widget (a bitfield containing flags
 * from the #GdkEventMask enumeration). These are the events that the widget
 * will receive.
 *
 * Return value: event mask for @widget
 **/
gint
gtk_widget_get_events (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (widget), quark_event_mask));
}

/**
 * gtk_widget_get_device_events:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Returns the events mask for the widget corresponding to an specific device. These
 * are the events that the widget will receive when @device operates on it.
 *
 * Returns: device event mask for @widget
 *
 * Since: 3.0
 **/
GdkEventMask
gtk_widget_get_device_events (GtkWidget *widget,
                              GdkDevice *device)
{
  GHashTable *device_events;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  device_events = g_object_get_qdata (G_OBJECT (widget), quark_device_event_mask);

  if (!device_events)
    return 0;

  return GPOINTER_TO_UINT (g_hash_table_lookup (device_events, device));
}

/**
 * gtk_widget_get_pointer:
 * @widget: a #GtkWidget
 * @x: (out) (allow-none): return location for the X coordinate, or %NULL
 * @y: (out) (allow-none): return location for the Y coordinate, or %NULL
 *
 * Obtains the location of the mouse pointer in widget coordinates.
 * Widget coordinates are a bit odd; for historical reasons, they are
 * defined as @widget->window coordinates for widgets that are not
 * #GTK_NO_WINDOW widgets, and are relative to @widget->allocation.x,
 * @widget->allocation.y for widgets that are #GTK_NO_WINDOW widgets.
 **/
void
gtk_widget_get_pointer (GtkWidget *widget,
			gint	  *x,
			gint	  *y)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  if (x)
    *x = -1;
  if (y)
    *y = -1;

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_get_pointer (priv->window, x, y, NULL);

      if (!gtk_widget_get_has_window (widget))
	{
	  if (x)
	    *x -= priv->allocation.x;
	  if (y)
	    *y -= priv->allocation.y;
	}
    }
}

/**
 * gtk_widget_is_ancestor:
 * @widget: a #GtkWidget
 * @ancestor: another #GtkWidget
 *
 * Determines whether @widget is somewhere inside @ancestor, possibly with
 * intermediate containers.
 *
 * Return value: %TRUE if @ancestor contains @widget as a child,
 *    grandchild, great grandchild, etc.
 **/
gboolean
gtk_widget_is_ancestor (GtkWidget *widget,
			GtkWidget *ancestor)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);

  while (widget)
    {
      if (widget->priv->parent == ancestor)
	return TRUE;
      widget = widget->priv->parent;
    }

  return FALSE;
}

static GQuark quark_composite_name = 0;

/**
 * gtk_widget_set_composite_name:
 * @widget: a #GtkWidget.
 * @name: the name to set
 *
 * Sets a widgets composite name. The widget must be
 * a composite child of its parent; see gtk_widget_push_composite_child().
 **/
void
gtk_widget_set_composite_name (GtkWidget   *widget,
			       const gchar *name)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (widget->priv->composite_child);
  g_return_if_fail (name != NULL);

  if (!quark_composite_name)
    quark_composite_name = g_quark_from_static_string ("gtk-composite-name");

  g_object_set_qdata_full (G_OBJECT (widget),
			   quark_composite_name,
			   g_strdup (name),
			   g_free);
}

/**
 * gtk_widget_get_composite_name:
 * @widget: a #GtkWidget
 *
 * Obtains the composite name of a widget.
 *
 * Returns: the composite name of @widget, or %NULL if @widget is not
 *   a composite child. The string should be freed when it is no
 *   longer needed.
 **/
gchar*
gtk_widget_get_composite_name (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;

  if (widget->priv->composite_child && priv->parent)
    return _gtk_container_child_composite_name (GTK_CONTAINER (priv->parent),
					       widget);
  else
    return NULL;
}

/**
 * gtk_widget_push_composite_child:
 *
 * Makes all newly-created widgets as composite children until
 * the corresponding gtk_widget_pop_composite_child() call.
 *
 * A composite child is a child that's an implementation detail of the
 * container it's inside and should not be visible to people using the
 * container. Composite children aren't treated differently by GTK (but
 * see gtk_container_foreach() vs. gtk_container_forall()), but e.g. GUI
 * builders might want to treat them in a different way.
 *
 * Here is a simple example:
 * |[
 *   gtk_widget_push_composite_child ();
 *   scrolled_window->hscrollbar = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, hadjustment);
 *   gtk_widget_set_composite_name (scrolled_window->hscrollbar, "hscrollbar");
 *   gtk_widget_pop_composite_child ();
 *   gtk_widget_set_parent (scrolled_window->hscrollbar,
 *                          GTK_WIDGET (scrolled_window));
 *   g_object_ref (scrolled_window->hscrollbar);
 * ]|
 **/
void
gtk_widget_push_composite_child (void)
{
  composite_child_stack++;
}

/**
 * gtk_widget_pop_composite_child:
 *
 * Cancels the effect of a previous call to gtk_widget_push_composite_child().
 **/
void
gtk_widget_pop_composite_child (void)
{
  if (composite_child_stack)
    composite_child_stack--;
}

static void
gtk_widget_emit_direction_changed (GtkWidget        *widget,
                                   GtkTextDirection  old_dir)
{
  gtk_widget_update_pango_context (widget);

  if (widget->priv->context)
    gtk_style_context_set_direction (widget->priv->context,
                                     gtk_widget_get_direction (widget));

  g_signal_emit (widget, widget_signals[DIRECTION_CHANGED], 0, old_dir);
}

/**
 * gtk_widget_set_direction:
 * @widget: a #GtkWidget
 * @dir:    the new direction
 *
 * Sets the reading direction on a particular widget. This direction
 * controls the primary direction for widgets containing text,
 * and also the direction in which the children of a container are
 * packed. The ability to set the direction is present in order
 * so that correct localization into languages with right-to-left
 * reading directions can be done. Generally, applications will
 * let the default reading direction present, except for containers
 * where the containers are arranged in an order that is explicitely
 * visual rather than logical (such as buttons for text justification).
 *
 * If the direction is set to %GTK_TEXT_DIR_NONE, then the value
 * set by gtk_widget_set_default_direction() will be used.
 **/
void
gtk_widget_set_direction (GtkWidget        *widget,
                          GtkTextDirection  dir)
{
  GtkTextDirection old_dir;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (dir >= GTK_TEXT_DIR_NONE && dir <= GTK_TEXT_DIR_RTL);

  old_dir = gtk_widget_get_direction (widget);

  widget->priv->direction = dir;

  if (old_dir != gtk_widget_get_direction (widget))
    gtk_widget_emit_direction_changed (widget, old_dir);
}

/**
 * gtk_widget_get_direction:
 * @widget: a #GtkWidget
 *
 * Gets the reading direction for a particular widget. See
 * gtk_widget_set_direction().
 *
 * Return value: the reading direction for the widget.
 **/
GtkTextDirection
gtk_widget_get_direction (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_TEXT_DIR_LTR);

  if (widget->priv->direction == GTK_TEXT_DIR_NONE)
    return gtk_default_direction;
  else
    return widget->priv->direction;
}

static void
gtk_widget_set_default_direction_recurse (GtkWidget *widget, gpointer data)
{
  GtkTextDirection old_dir = GPOINTER_TO_UINT (data);

  g_object_ref (widget);

  if (widget->priv->direction == GTK_TEXT_DIR_NONE)
    gtk_widget_emit_direction_changed (widget, old_dir);

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  gtk_widget_set_default_direction_recurse,
			  data);

  g_object_unref (widget);
}

/**
 * gtk_widget_set_default_direction:
 * @dir: the new default direction. This cannot be
 *        %GTK_TEXT_DIR_NONE.
 *
 * Sets the default reading direction for widgets where the
 * direction has not been explicitly set by gtk_widget_set_direction().
 **/
void
gtk_widget_set_default_direction (GtkTextDirection dir)
{
  g_return_if_fail (dir == GTK_TEXT_DIR_RTL || dir == GTK_TEXT_DIR_LTR);

  if (dir != gtk_default_direction)
    {
      GList *toplevels, *tmp_list;
      GtkTextDirection old_dir = gtk_default_direction;

      gtk_default_direction = dir;

      tmp_list = toplevels = gtk_window_list_toplevels ();
      g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);

      while (tmp_list)
	{
	  gtk_widget_set_default_direction_recurse (tmp_list->data,
						    GUINT_TO_POINTER (old_dir));
	  g_object_unref (tmp_list->data);
	  tmp_list = tmp_list->next;
	}

      g_list_free (toplevels);
    }
}

/**
 * gtk_widget_get_default_direction:
 *
 * Obtains the current default reading direction. See
 * gtk_widget_set_default_direction().
 *
 * Return value: the current default direction.
 **/
GtkTextDirection
gtk_widget_get_default_direction (void)
{
  return gtk_default_direction;
}

static void
gtk_widget_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;

  if (priv->parent)
    gtk_container_remove (GTK_CONTAINER (priv->parent), widget);
  else if (gtk_widget_get_visible (widget))
    gtk_widget_hide (widget);

  priv->visible = FALSE;
  if (gtk_widget_get_realized (widget))
    gtk_widget_unrealize (widget);

  if (!priv->in_destruction)
    {
      priv->in_destruction = TRUE;
      g_signal_emit (object, widget_signals[DESTROY], 0);
      priv->in_destruction = FALSE;
    }

  G_OBJECT_CLASS (gtk_widget_parent_class)->dispose (object);
}

static void
gtk_widget_real_destroy (GtkWidget *object)
{
  /* gtk_object_destroy() will already hold a refcount on object */
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;

  /* wipe accelerator closures (keep order) */
  g_object_set_qdata (G_OBJECT (widget), quark_accel_path, NULL);
  g_object_set_qdata (G_OBJECT (widget), quark_accel_closures, NULL);

  /* Callers of add_mnemonic_label() should disconnect on ::destroy */
  g_object_set_qdata (G_OBJECT (widget), quark_mnemonic_labels, NULL);

  gtk_grab_remove (widget);

  if (priv->style)
    g_object_unref (priv->style);
  priv->style = gtk_widget_get_default_style ();
  g_object_ref (priv->style);
}

static void
gtk_widget_finalize (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetPrivate *priv = widget->priv;
  GtkWidgetAuxInfo *aux_info;
  GtkAccessible *accessible;

  gtk_grab_remove (widget);

  g_object_unref (priv->style);
  priv->style = NULL;

  g_free (priv->name);

  aux_info = gtk_widget_get_aux_info (widget, FALSE);
  if (aux_info)
    gtk_widget_aux_info_destroy (aux_info);

  accessible = g_object_get_qdata (G_OBJECT (widget), quark_accessible_object);
  if (accessible)
    g_object_unref (accessible);

  if (priv->path)
    gtk_widget_path_free (priv->path);

  if (priv->context)
    g_object_unref (priv->context);

  _gtk_widget_free_cached_sizes (widget);

  if (g_object_is_floating (object))
    g_warning ("A floating object was finalized. This means that someone\n"
               "called g_object_unref() on an object that had only a floating\n"
               "reference; the initial floating reference is not owned by anyone\n"
               "and must be removed with g_object_ref_sink().");

  G_OBJECT_CLASS (gtk_widget_parent_class)->finalize (object);
}

/*****************************************
 * gtk_widget_real_map:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_map (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_assert (gtk_widget_get_realized (widget));

  if (!gtk_widget_get_mapped (widget))
    {
      gtk_widget_set_mapped (widget, TRUE);

      if (gtk_widget_get_has_window (widget))
	gdk_window_show (priv->window);
    }
}

/*****************************************
 * gtk_widget_real_unmap:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_unmap (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  if (gtk_widget_get_mapped (widget))
    {
      gtk_widget_set_mapped (widget, FALSE);

      if (gtk_widget_get_has_window (widget))
	gdk_window_hide (priv->window);
    }
}

/*****************************************
 * gtk_widget_real_realize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_realize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_assert (!gtk_widget_get_has_window (widget));

  gtk_widget_set_realized (widget, TRUE);
  if (priv->parent)
    {
      priv->window = gtk_widget_get_parent_window (widget);
      g_object_ref (priv->window);
    }
}

/*****************************************
 * gtk_widget_real_unrealize:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_real_unrealize (GtkWidget *widget)
{
  GtkWidgetPrivate *priv = widget->priv;

  g_assert (!widget->priv->mapped);

  /* printf ("unrealizing %s\n", g_type_name (G_TYPE_FROM_INSTANCE (widget)));
   */

   /* We must do unrealize child widget BEFORE container widget.
    * gdk_window_destroy() destroys specified xwindow and its sub-xwindows.
    * So, unrealizing container widget bofore its children causes the problem
    * (for example, gdk_ic_destroy () with destroyed window causes crash. )
    */

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
			  (GtkCallback) gtk_widget_unrealize,
			  NULL);

  if (gtk_widget_get_has_window (widget))
    {
      gdk_window_set_user_data (priv->window, NULL);
      gdk_window_destroy (priv->window);
      priv->window = NULL;
    }
  else
    {
      g_object_unref (priv->window);
      priv->window = NULL;
    }

  gtk_selection_remove_all (widget);

  gtk_widget_set_realized (widget, FALSE);
}

static void
gtk_widget_real_adjust_size_request (GtkWidget         *widget,
                                     GtkOrientation     orientation,
                                     gint              *minimum_size,
                                     gint              *natural_size)
{
  const GtkWidgetAuxInfo *aux_info;

  aux_info =_gtk_widget_get_aux_info_or_defaults (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL &&
      aux_info->width > 0)
    {
      *minimum_size = MAX (*minimum_size, aux_info->width);
    }
  else if (orientation == GTK_ORIENTATION_VERTICAL &&
           aux_info->height > 0)
    {
      *minimum_size = MAX (*minimum_size, aux_info->height);
    }

  /* Fix it if set_size_request made natural size smaller than min size.
   * This would also silently fix broken widgets, but we warn about them
   * in gtksizerequest.c when calling their size request vfuncs.
   */
  *natural_size = MAX (*natural_size, *minimum_size);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum_size += (aux_info->margin.left + aux_info->margin.right);
      *natural_size += (aux_info->margin.left + aux_info->margin.right);
    }
  else
    {
      *minimum_size += (aux_info->margin.top + aux_info->margin.bottom);
      *natural_size += (aux_info->margin.top + aux_info->margin.bottom);
    }
}

/**
 * _gtk_widget_peek_request_cache:
 *
 * Returns the address of the widget's request cache (strictly for
 * internal use in gtksizerequest.c)
 *
 * Return value: the address of @widget's size request cache.
 **/
gpointer
_gtk_widget_peek_request_cache (GtkWidget *widget)
{
  /* Don't bother slowing things down with the return_if_fail guards here */
  return &widget->priv->requests;
}

/*
 * _gtk_widget_set_device_window:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 * @window: the new device window
 *
 * Sets pointer window for @widget and @device.
 * Does not ref @window.
 */
void
_gtk_widget_set_device_window (GtkWidget *widget,
                               GdkDevice *device,
                               GdkWindow *window)
{
  GHashTable *device_window;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  if (!gtk_widget_get_mapped (widget))
    return;

  device_window = g_object_get_qdata (G_OBJECT (widget), quark_pointer_window);

  if (!device_window && window)
    {
      device_window = g_hash_table_new (NULL, NULL);
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_pointer_window,
                               device_window,
                               (GDestroyNotify) g_hash_table_destroy);
    }

  if (window)
    g_hash_table_insert (device_window, device, window);
  else if (device_window)
    {
      g_hash_table_remove (device_window, device);

      if (g_hash_table_size (device_window) == 0)
        g_object_set_qdata (G_OBJECT (widget), quark_pointer_window, NULL);
    }
}

/*
 * _gtk_widget_get_device_window:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Return value: the device window set on @widget, or %NULL
 */
GdkWindow *
_gtk_widget_get_device_window (GtkWidget *widget,
                               GdkDevice *device)
{
  GHashTable *device_window;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  if (!gtk_widget_get_mapped (widget))
    return NULL;

  device_window = g_object_get_qdata (G_OBJECT (widget), quark_pointer_window);

  if (!device_window)
    return NULL;

  return g_hash_table_lookup (device_window, device);
}

/*
 * _gtk_widget_list_devices:
 * @widget: a #GtkWidget
 *
 * Returns the list of #GdkDevices that is currently on top
 * of any window belonging to @widget.
 * Free the list with g_list_free(), the elements are owned
 * by GTK+ and must not be freed.
 */
GList *
_gtk_widget_list_devices (GtkWidget *widget)
{
  GHashTableIter iter;
  GHashTable *device_window;
  GList *devices = NULL;
  gpointer key, value;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  if (!gtk_widget_get_mapped (widget))
    return NULL;

  device_window = g_object_get_qdata (G_OBJECT (widget), quark_pointer_window);

  if (G_UNLIKELY (!device_window))
    return NULL;

  g_hash_table_iter_init (&iter, device_window);

  while (g_hash_table_iter_next (&iter, &key, &value))
    devices = g_list_prepend (devices, key);

  return devices;
}

static void
synth_crossing (GtkWidget       *widget,
                GdkEventType     type,
                GdkWindow       *window,
                GdkDevice       *device,
                GdkCrossingMode  mode,
                GdkNotifyType    detail)
{
  GdkEvent *event;

  event = gdk_event_new (type);

  event->crossing.window = g_object_ref (window);
  event->crossing.send_event = TRUE;
  event->crossing.subwindow = g_object_ref (window);
  event->crossing.time = GDK_CURRENT_TIME;
  event->crossing.x = event->crossing.y = 0;
  event->crossing.x_root = event->crossing.y_root = 0;
  event->crossing.mode = mode;
  event->crossing.detail = detail;
  event->crossing.focus = FALSE;
  event->crossing.state = 0;
  gdk_event_set_device (event, device);

  if (!widget)
    widget = gtk_get_event_widget (event);

  if (widget)
    gtk_widget_event_internal (widget, event);

  gdk_event_free (event);
}

/*
 * _gtk_widget_synthesize_crossing:
 * @from: the #GtkWidget the virtual pointer is leaving.
 * @to: the #GtkWidget the virtual pointer is moving to.
 * @mode: the #GdkCrossingMode to place on the synthesized events.
 *
 * Generate crossing event(s) on widget state (sensitivity) or GTK+ grab change.
 *
 * The real pointer window is the window that most recently received an enter notify
 * event.  Windows that don't select for crossing events can't become the real
 * poiner window.  The real pointer widget that owns the real pointer window.  The
 * effective pointer window is the same as the real pointer window unless the real
 * pointer widget is either insensitive or there is a grab on a widget that is not
 * an ancestor of the real pointer widget (in which case the effective pointer
 * window should be the root window).
 *
 * When the effective pointer window is the same as the real poiner window, we
 * receive crossing events from the windowing system.  When the effective pointer
 * window changes to become different from the real pointer window we synthesize
 * crossing events, attempting to follow X protocol rules:
 *
 * When the root window becomes the effective pointer window:
 *   - leave notify on real pointer window, detail Ancestor
 *   - leave notify on all of its ancestors, detail Virtual
 *   - enter notify on root window, detail Inferior
 *
 * When the root window ceases to be the effective pointer window:
 *   - leave notify on root window, detail Inferior
 *   - enter notify on all ancestors of real pointer window, detail Virtual
 *   - enter notify on real pointer window, detail Ancestor
 */
void
_gtk_widget_synthesize_crossing (GtkWidget       *from,
				 GtkWidget       *to,
                                 GdkDevice       *device,
				 GdkCrossingMode  mode)
{
  GdkWindow *from_window = NULL, *to_window = NULL;

  g_return_if_fail (from != NULL || to != NULL);

  if (from != NULL)
    {
      from_window = _gtk_widget_get_device_window (from, device);

      if (!from_window)
        from_window = from->priv->window;
    }

  if (to != NULL)
    {
      to_window = _gtk_widget_get_device_window (to, device);

      if (!to_window)
        to_window = to->priv->window;
    }

  if (from_window == NULL && to_window == NULL)
    ;
  else if (from_window != NULL && to_window == NULL)
    {
      GList *from_ancestors = NULL, *list;
      GdkWindow *from_ancestor = from_window;

      while (from_ancestor != NULL)
	{
	  from_ancestor = gdk_window_get_effective_parent (from_ancestor);
          if (from_ancestor == NULL)
            break;
          from_ancestors = g_list_prepend (from_ancestors, from_ancestor);
	}

      synth_crossing (from, GDK_LEAVE_NOTIFY, from_window,
		      device, mode, GDK_NOTIFY_ANCESTOR);
      for (list = g_list_last (from_ancestors); list; list = list->prev)
	{
	  synth_crossing (NULL, GDK_LEAVE_NOTIFY, (GdkWindow *) list->data,
			  device, mode, GDK_NOTIFY_VIRTUAL);
	}

      /* XXX: enter/inferior on root window? */

      g_list_free (from_ancestors);
    }
  else if (from_window == NULL && to_window != NULL)
    {
      GList *to_ancestors = NULL, *list;
      GdkWindow *to_ancestor = to_window;

      while (to_ancestor != NULL)
	{
	  to_ancestor = gdk_window_get_effective_parent (to_ancestor);
	  if (to_ancestor == NULL)
            break;
          to_ancestors = g_list_prepend (to_ancestors, to_ancestor);
        }

      /* XXX: leave/inferior on root window? */

      for (list = to_ancestors; list; list = list->next)
	{
	  synth_crossing (NULL, GDK_ENTER_NOTIFY, (GdkWindow *) list->data,
			  device, mode, GDK_NOTIFY_VIRTUAL);
	}
      synth_crossing (to, GDK_ENTER_NOTIFY, to_window,
		      device, mode, GDK_NOTIFY_ANCESTOR);

      g_list_free (to_ancestors);
    }
  else if (from_window == to_window)
    ;
  else
    {
      GList *from_ancestors = NULL, *to_ancestors = NULL, *list;
      GdkWindow *from_ancestor = from_window, *to_ancestor = to_window;

      while (from_ancestor != NULL || to_ancestor != NULL)
	{
	  if (from_ancestor != NULL)
	    {
	      from_ancestor = gdk_window_get_effective_parent (from_ancestor);
	      if (from_ancestor == to_window)
		break;
              if (from_ancestor)
	        from_ancestors = g_list_prepend (from_ancestors, from_ancestor);
	    }
	  if (to_ancestor != NULL)
	    {
	      to_ancestor = gdk_window_get_effective_parent (to_ancestor);
	      if (to_ancestor == from_window)
		break;
              if (to_ancestor)
	        to_ancestors = g_list_prepend (to_ancestors, to_ancestor);
	    }
	}
      if (to_ancestor == from_window)
	{
	  if (mode != GDK_CROSSING_GTK_UNGRAB)
	    synth_crossing (from, GDK_LEAVE_NOTIFY, from_window,
			    device, mode, GDK_NOTIFY_INFERIOR);
	  for (list = to_ancestors; list; list = list->next)
	    synth_crossing (NULL, GDK_ENTER_NOTIFY, (GdkWindow *) list->data,
			    device, mode, GDK_NOTIFY_VIRTUAL);
	  synth_crossing (to, GDK_ENTER_NOTIFY, to_window,
			  device, mode, GDK_NOTIFY_ANCESTOR);
	}
      else if (from_ancestor == to_window)
	{
	  synth_crossing (from, GDK_LEAVE_NOTIFY, from_window,
			  device, mode, GDK_NOTIFY_ANCESTOR);
	  for (list = g_list_last (from_ancestors); list; list = list->prev)
	    {
	      synth_crossing (NULL, GDK_LEAVE_NOTIFY, (GdkWindow *) list->data,
			      device, mode, GDK_NOTIFY_VIRTUAL);
	    }
	  if (mode != GDK_CROSSING_GTK_GRAB)
	    synth_crossing (to, GDK_ENTER_NOTIFY, to_window,
			    device, mode, GDK_NOTIFY_INFERIOR);
	}
      else
	{
	  while (from_ancestors != NULL && to_ancestors != NULL
		 && from_ancestors->data == to_ancestors->data)
	    {
	      from_ancestors = g_list_delete_link (from_ancestors,
						   from_ancestors);
	      to_ancestors = g_list_delete_link (to_ancestors, to_ancestors);
	    }

	  synth_crossing (from, GDK_LEAVE_NOTIFY, from_window,
			  device, mode, GDK_NOTIFY_NONLINEAR);

	  for (list = g_list_last (from_ancestors); list; list = list->prev)
	    {
	      synth_crossing (NULL, GDK_LEAVE_NOTIFY, (GdkWindow *) list->data,
			      device, mode, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  for (list = to_ancestors; list; list = list->next)
	    {
	      synth_crossing (NULL, GDK_ENTER_NOTIFY, (GdkWindow *) list->data,
			      device, mode, GDK_NOTIFY_NONLINEAR_VIRTUAL);
	    }
	  synth_crossing (to, GDK_ENTER_NOTIFY, to_window,
			  device, mode, GDK_NOTIFY_NONLINEAR);
	}
      g_list_free (from_ancestors);
      g_list_free (to_ancestors);
    }
}

static void
gtk_widget_propagate_state (GtkWidget    *widget,
                            GtkStateData *data)
{
  GtkWidgetPrivate *priv = widget->priv;
  GtkStateFlags new_flags, old_flags = priv->state_flags;
  GtkStateType old_state;

  old_state = gtk_widget_get_state (widget);

  switch (data->operation)
    {
    case STATE_CHANGE_REPLACE:
      priv->state_flags = data->flags;
      break;
    case STATE_CHANGE_SET:
      priv->state_flags |= data->flags;
      break;
    case STATE_CHANGE_UNSET:
      priv->state_flags &= ~(data->flags);
      break;
    }

  /* make insensitivity unoverridable */
  if (!priv->sensitive)
    priv->state_flags |= GTK_STATE_FLAG_INSENSITIVE;

  if (gtk_widget_is_focus (widget) && !gtk_widget_is_sensitive (widget))
    {
      GtkWidget *window;

      window = gtk_widget_get_toplevel (widget);

      if (window && gtk_widget_is_toplevel (window))
        gtk_window_set_focus (GTK_WINDOW (window), NULL);
    }

  new_flags = priv->state_flags;

  if (old_flags != new_flags)
    {
      g_object_ref (widget);

      if (!gtk_widget_is_sensitive (widget) && gtk_widget_has_grab (widget))
        gtk_grab_remove (widget);

      g_signal_emit (widget, widget_signals[STATE_CHANGED], 0, old_state);
      g_signal_emit (widget, widget_signals[STATE_FLAGS_CHANGED], 0, old_flags);

      if (!priv->shadowed)
        {
          GList *event_windows = NULL;
          GList *devices, *d;

          devices = _gtk_widget_list_devices (widget);

          for (d = devices; d; d = d->next)
            {
              GdkWindow *window;
              GdkDevice *device;

              device = d->data;
              window = _gtk_widget_get_device_window (widget, device);

              /* Do not propagate more than once to the
               * same window if non-multidevice aware.
               */
              if (!gdk_window_get_support_multidevice (window) &&
                  g_list_find (event_windows, window))
                continue;

              if (!gtk_widget_is_sensitive (widget))
                _gtk_widget_synthesize_crossing (widget, NULL, d->data,
                                                 GDK_CROSSING_STATE_CHANGED);
              else if (old_flags & GTK_STATE_FLAG_INSENSITIVE)
                _gtk_widget_synthesize_crossing (NULL, widget, d->data,
                                                 GDK_CROSSING_STATE_CHANGED);

              event_windows = g_list_prepend (event_windows, window);
            }

          g_list_free (event_windows);
          g_list_free (devices);
        }

      if (GTK_IS_CONTAINER (widget))
        {
          GtkStateData child_data = *data;

          /* Make sure to only propate the right states further */
          child_data.flags &= GTK_STATE_FLAGS_DO_PROPAGATE;

          if (child_data.use_forall)
            gtk_container_forall (GTK_CONTAINER (widget),
                                  (GtkCallback) gtk_widget_propagate_state,
                                  &child_data);
          else
            gtk_container_foreach (GTK_CONTAINER (widget),
                                   (GtkCallback) gtk_widget_propagate_state,
                                   &child_data);
        }

      /* Trigger state change transitions for the widget */
      if (priv->context &&
          gtk_widget_get_mapped (widget))
        {
          gint diff, flag = 1;

          diff = old_flags ^ new_flags;

          while (diff != 0)
            {
              if ((diff & flag) != 0)
                {
                  gboolean target;

                  target = ((new_flags & flag) != 0);
                  _gtk_widget_notify_state_change (widget, flag, target);

                  diff &= ~flag;
                }

              flag <<= 1;
            }
        }

      g_object_unref (widget);
    }
}

static const GtkWidgetAuxInfo default_aux_info = {
  -1, -1,
  GTK_ALIGN_FILL,
  GTK_ALIGN_FILL,
  { 0, 0, 0, 0 }
};

/*
 * gtk_widget_get_aux_info:
 * @widget: a #GtkWidget
 * @create: if %TRUE, create the structure if it doesn't exist
 *
 * Get the #GtkWidgetAuxInfo structure for the widget.
 *
 * Return value: the #GtkAuxInfo structure for the widget, or
 *    %NULL if @create is %FALSE and one doesn't already exist.
 */
static GtkWidgetAuxInfo *
gtk_widget_get_aux_info (GtkWidget *widget,
			 gboolean   create)
{
  GtkWidgetAuxInfo *aux_info;

  aux_info = g_object_get_qdata (G_OBJECT (widget), quark_aux_info);
  if (!aux_info && create)
    {
      aux_info = g_slice_new0 (GtkWidgetAuxInfo);

      *aux_info = default_aux_info;

      g_object_set_qdata (G_OBJECT (widget), quark_aux_info, aux_info);
    }

  return aux_info;
}

static const GtkWidgetAuxInfo*
_gtk_widget_get_aux_info_or_defaults (GtkWidget *widget)
{
  GtkWidgetAuxInfo *aux_info;

  aux_info = gtk_widget_get_aux_info (widget, FALSE);
  if (aux_info == NULL)
    {
      return &default_aux_info;
    }
  else
    {
      return aux_info;
    }
}

/*****************************************
 * gtk_widget_aux_info_destroy:
 *
 *   arguments:
 *
 *   results:
 *****************************************/

static void
gtk_widget_aux_info_destroy (GtkWidgetAuxInfo *aux_info)
{
  g_slice_free (GtkWidgetAuxInfo, aux_info);
}

/**
 * gtk_widget_shape_combine_region:
 * @widget: a #GtkWidget
 * @region: (allow-none): shape to be added, or %NULL to remove an existing shape
 *
 * Sets a shape for this widget's GDK window. This allows for
 * transparent windows etc., see gdk_window_shape_combine_region()
 * for more information.
 *
 * Since: 3.0
 **/
void
gtk_widget_shape_combine_region (GtkWidget *widget,
                                 cairo_region_t *region)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  /*  set_shape doesn't work on widgets without gdk window */
  g_return_if_fail (gtk_widget_get_has_window (widget));

  priv = widget->priv;

  if (region == NULL)
    {
      priv->has_shape_mask = FALSE;

      if (priv->window)
	gdk_window_shape_combine_region (priv->window, NULL, 0, 0);

      g_object_set_qdata (G_OBJECT (widget), quark_shape_info, NULL);
    }
  else
    {
      priv->has_shape_mask = TRUE;

      g_object_set_qdata_full (G_OBJECT (widget), quark_shape_info,
                               cairo_region_copy (region),
			       (GDestroyNotify) cairo_region_destroy);

      /* set shape if widget has a gdk window already.
       * otherwise the shape is scheduled to be set by gtk_widget_realize().
       */
      if (priv->window)
	gdk_window_shape_combine_region (priv->window, region, 0, 0);
    }
}

/**
 * gtk_widget_input_shape_combine_region:
 * @widget: a #GtkWidget
 * @region: (allow-none): shape to be added, or %NULL to remove an existing shape
 *
 * Sets an input shape for this widget's GDK window. This allows for
 * windows which react to mouse click in a nonrectangular region, see
 * gdk_window_input_shape_combine_region() for more information.
 *
 * Since: 3.0
 **/
void
gtk_widget_input_shape_combine_region (GtkWidget *widget,
                                       cairo_region_t *region)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  /*  set_shape doesn't work on widgets without gdk window */
  g_return_if_fail (gtk_widget_get_has_window (widget));

  priv = widget->priv;

  if (region == NULL)
    {
      if (priv->window)
	gdk_window_input_shape_combine_region (priv->window, NULL, 0, 0);

      g_object_set_qdata (G_OBJECT (widget), quark_input_shape_info, NULL);
    }
  else
    {
      g_object_set_qdata_full (G_OBJECT (widget), quark_input_shape_info,
			       cairo_region_copy (region),
			       (GDestroyNotify) cairo_region_destroy);

      /* set shape if widget has a gdk window already.
       * otherwise the shape is scheduled to be set by gtk_widget_realize().
       */
      if (priv->window)
	gdk_window_input_shape_combine_region (priv->window, region, 0, 0);
    }
}


/* style properties
 */

/**
 * gtk_widget_class_install_style_property_parser: (skip)
 * @klass: a #GtkWidgetClass
 * @pspec: the #GParamSpec for the style property
 * @parser: the parser for the style property
 *
 * Installs a style property on a widget class.
 **/
void
gtk_widget_class_install_style_property_parser (GtkWidgetClass     *klass,
						GParamSpec         *pspec,
						GtkRcPropertyParser parser)
{
  g_return_if_fail (GTK_IS_WIDGET_CLASS (klass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  g_return_if_fail (pspec->flags & G_PARAM_READABLE);
  g_return_if_fail (!(pspec->flags & (G_PARAM_CONSTRUCT_ONLY | G_PARAM_CONSTRUCT)));

  if (g_param_spec_pool_lookup (style_property_spec_pool, pspec->name, G_OBJECT_CLASS_TYPE (klass), FALSE))
    {
      g_warning (G_STRLOC ": class `%s' already contains a style property named `%s'",
		 G_OBJECT_CLASS_NAME (klass),
		 pspec->name);
      return;
    }

  g_param_spec_ref_sink (pspec);
  g_param_spec_set_qdata (pspec, quark_property_parser, (gpointer) parser);
  g_param_spec_pool_insert (style_property_spec_pool, pspec, G_OBJECT_CLASS_TYPE (klass));
}

/**
 * gtk_widget_class_install_style_property:
 * @klass: a #GtkWidgetClass
 * @pspec: the #GParamSpec for the property
 *
 * Installs a style property on a widget class. The parser for the
 * style property is determined by the value type of @pspec.
 **/
void
gtk_widget_class_install_style_property (GtkWidgetClass *klass,
					 GParamSpec     *pspec)
{
  GtkRcPropertyParser parser;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (klass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  parser = _gtk_rc_property_parser_from_type (G_PARAM_SPEC_VALUE_TYPE (pspec));

  gtk_widget_class_install_style_property_parser (klass, pspec, parser);
}

/**
 * gtk_widget_class_find_style_property:
 * @klass: a #GtkWidgetClass
 * @property_name: the name of the style property to find
 *
 * Finds a style property of a widget class by name.
 *
 * Returns: (transfer none): the #GParamSpec of the style property or
 *   %NULL if @class has no style property with that name.
 *
 * Since: 2.2
 */
GParamSpec*
gtk_widget_class_find_style_property (GtkWidgetClass *klass,
				      const gchar    *property_name)
{
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (style_property_spec_pool,
				   property_name,
				   G_OBJECT_CLASS_TYPE (klass),
				   TRUE);
}

/**
 * gtk_widget_class_list_style_properties:
 * @klass: a #GtkWidgetClass
 * @n_properties: location to return the number of style properties found
 *
 * Returns all style properties of a widget class.
 *
 * Returns: (array length=n_properties) (transfer container): a
 *     newly allocated array of #GParamSpec*. The array must be
 *     freed with g_free().
 *
 * Since: 2.2
 */
GParamSpec**
gtk_widget_class_list_style_properties (GtkWidgetClass *klass,
					guint          *n_properties)
{
  GParamSpec **pspecs;
  guint n;

  pspecs = g_param_spec_pool_list (style_property_spec_pool,
				   G_OBJECT_CLASS_TYPE (klass),
				   &n);
  if (n_properties)
    *n_properties = n;

  return pspecs;
}

/**
 * gtk_widget_style_get_property:
 * @widget: a #GtkWidget
 * @property_name: the name of a style property
 * @value: location to return the property value
 *
 * Gets the value of a style property of @widget.
 */
void
gtk_widget_style_get_property (GtkWidget   *widget,
			       const gchar *property_name,
			       GValue      *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));

  g_object_ref (widget);
  pspec = g_param_spec_pool_lookup (style_property_spec_pool,
				    property_name,
				    G_OBJECT_TYPE (widget),
				    TRUE);
  if (!pspec)
    g_warning ("%s: widget class `%s' has no property named `%s'",
	       G_STRLOC,
	       G_OBJECT_TYPE_NAME (widget),
	       property_name);
  else
    {
      GtkStyleContext *context;
      const GValue *peek_value;
      GtkStateFlags state;

      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);

      peek_value = _gtk_style_context_peek_style_property (context,
                                                           G_OBJECT_TYPE (widget),
                                                           state, pspec);

      /* auto-conversion of the caller's value type
       */
      if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
	g_value_copy (peek_value, value);
      else if (g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
	g_value_transform (peek_value, value);
      else
	g_warning ("can't retrieve style property `%s' of type `%s' as value of type `%s'",
		   pspec->name,
		   g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		   G_VALUE_TYPE_NAME (value));
    }
  g_object_unref (widget);
}

/**
 * gtk_widget_style_get_valist:
 * @widget: a #GtkWidget
 * @first_property_name: the name of the first property to get
 * @var_args: a <type>va_list</type> of pairs of property names and
 *     locations to return the property values, starting with the location
 *     for @first_property_name.
 *
 * Non-vararg variant of gtk_widget_style_get(). Used primarily by language
 * bindings.
 */
void
gtk_widget_style_get_valist (GtkWidget   *widget,
			     const gchar *first_property_name,
			     va_list      var_args)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  const gchar *name;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_ref (widget);
  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  name = first_property_name;
  while (name)
    {
      const GValue *peek_value;
      GParamSpec *pspec;
      gchar *error;

      pspec = g_param_spec_pool_lookup (style_property_spec_pool,
					name,
					G_OBJECT_TYPE (widget),
					TRUE);
      if (!pspec)
	{
	  g_warning ("%s: widget class `%s' has no property named `%s'",
		     G_STRLOC,
		     G_OBJECT_TYPE_NAME (widget),
		     name);
	  break;
	}
      /* style pspecs are always readable so we can spare that check here */

      peek_value = _gtk_style_context_peek_style_property (context,
                                                           G_OBJECT_TYPE (widget),
                                                           state, pspec);

      G_VALUE_LCOPY (peek_value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  break;
	}

      name = va_arg (var_args, gchar*);
    }

  g_object_unref (widget);
}

/**
 * gtk_widget_style_get:
 * @widget: a #GtkWidget
 * @first_property_name: the name of the first property to get
 * @...: pairs of property names and locations to return the
 *     property values, starting with the location for
 *     @first_property_name, terminated by %NULL.
 *
 * Gets the values of a multiple style properties of @widget.
 */
void
gtk_widget_style_get (GtkWidget   *widget,
		      const gchar *first_property_name,
		      ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  va_start (var_args, first_property_name);
  gtk_widget_style_get_valist (widget, first_property_name, var_args);
  va_end (var_args);
}

/**
 * gtk_widget_path:
 * @widget: a #GtkWidget
 * @path_length: (out) (allow-none): location to store length of the path,
 *     or %NULL
 * @path: (out) (allow-none): location to store allocated path string,
 *     or %NULL
 * @path_reversed: (out) (allow-none): location to store allocated reverse
 *     path string, or %NULL
 *
 * Obtains the full path to @widget. The path is simply the name of a
 * widget and all its parents in the container hierarchy, separated by
 * periods. The name of a widget comes from
 * gtk_widget_get_name(). Paths are used to apply styles to a widget
 * in gtkrc configuration files. Widget names are the type of the
 * widget by default (e.g. "GtkButton") or can be set to an
 * application-specific value with gtk_widget_set_name(). By setting
 * the name of a widget, you allow users or theme authors to apply
 * styles to that specific widget in their gtkrc
 * file. @path_reversed_p fills in the path in reverse order,
 * i.e. starting with @widget's name instead of starting with the name
 * of @widget's outermost ancestor.
 *
 * Deprecated:3.0: Use gtk_widget_get_path() instead
 **/
void
gtk_widget_path (GtkWidget *widget,
		 guint     *path_length,
		 gchar    **path,
		 gchar    **path_reversed)
{
  static gchar *rev_path = NULL;
  static guint tmp_path_len = 0;
  guint len;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  len = 0;
  do
    {
      const gchar *string;
      const gchar *s;
      gchar *d;
      guint l;

      string = gtk_widget_get_name (widget);
      l = strlen (string);
      while (tmp_path_len <= len + l + 1)
	{
	  tmp_path_len += INIT_PATH_SIZE;
	  rev_path = g_realloc (rev_path, tmp_path_len);
	}
      s = string + l - 1;
      d = rev_path + len;
      while (s >= string)
	*(d++) = *(s--);
      len += l;

      widget = widget->priv->parent;

      if (widget)
	rev_path[len++] = '.';
      else
	rev_path[len++] = 0;
    }
  while (widget);

  if (path_length)
    *path_length = len - 1;
  if (path_reversed)
    *path_reversed = g_strdup (rev_path);
  if (path)
    {
      *path = g_strdup (rev_path);
      g_strreverse (*path);
    }
}

/**
 * gtk_widget_class_path:
 * @widget: a #GtkWidget
 * @path_length: (out) (allow-none): location to store the length of the
 *     class path, or %NULL
 * @path: (out) (allow-none): location to store the class path as an
 *     allocated string, or %NULL
 * @path_reversed: (out) (allow-none): location to store the reverse
 *     class path as an allocated string, or %NULL
 *
 * Same as gtk_widget_path(), but always uses the name of a widget's type,
 * never uses a custom name set with gtk_widget_set_name().
 *
 * Deprecated:3.0: Use gtk_widget_get_path() instead
 **/
void
gtk_widget_class_path (GtkWidget *widget,
		       guint     *path_length,
		       gchar    **path,
		       gchar    **path_reversed)
{
  static gchar *rev_path = NULL;
  static guint tmp_path_len = 0;
  guint len;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  len = 0;
  do
    {
      const gchar *string;
      const gchar *s;
      gchar *d;
      guint l;

      string = g_type_name (G_OBJECT_TYPE (widget));
      l = strlen (string);
      while (tmp_path_len <= len + l + 1)
	{
	  tmp_path_len += INIT_PATH_SIZE;
	  rev_path = g_realloc (rev_path, tmp_path_len);
	}
      s = string + l - 1;
      d = rev_path + len;
      while (s >= string)
	*(d++) = *(s--);
      len += l;

      widget = widget->priv->parent;

      if (widget)
	rev_path[len++] = '.';
      else
	rev_path[len++] = 0;
    }
  while (widget);

  if (path_length)
    *path_length = len - 1;
  if (path_reversed)
    *path_reversed = g_strdup (rev_path);
  if (path)
    {
      *path = g_strdup (rev_path);
      g_strreverse (*path);
    }
}

/**
 * gtk_requisition_new:
 *
 * Allocates a new #GtkRequisition structure and initializes its elements to zero.
 *
 * Returns: a new empty #GtkRequisition. The newly allocated #GtkRequisition should
 *   be freed with gtk_requisition_free().
 *
 * Since: 3.0
 */
GtkRequisition *
gtk_requisition_new (void)
{
  return g_slice_new0 (GtkRequisition);
}

/**
 * gtk_requisition_copy:
 * @requisition: a #GtkRequisition
 *
 * Copies a #GtkRequisition.
 *
 * Returns: a copy of @requisition
 **/
GtkRequisition *
gtk_requisition_copy (const GtkRequisition *requisition)
{
  return g_slice_dup (GtkRequisition, requisition);
}

/**
 * gtk_requisition_free:
 * @requisition: a #GtkRequisition
 *
 * Frees a #GtkRequisition.
 **/
void
gtk_requisition_free (GtkRequisition *requisition)
{
  g_slice_free (GtkRequisition, requisition);
}

G_DEFINE_BOXED_TYPE (GtkRequisition, gtk_requisition,
                     gtk_requisition_copy,
                     gtk_requisition_free)

/**
 * gtk_widget_class_set_accessible_type:
 * @widget_class: class to set the accessible type for
 * @type: The object type that implements the accessible for @widget_class
 *
 * Sets the type to be used for creating accessibles for widgets of
 * @widget_class. The given @type must be a subtype of the type used for
 * accessibles of the parent class.
 *
 * This function should only be called from class init functions of widgets.
 *
 * Since: 3.2
 **/
void
gtk_widget_class_set_accessible_type (GtkWidgetClass *widget_class,
                                      GType           type)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));
  g_return_if_fail (g_type_is_a (type, widget_class->priv->accessible_type));

  priv = widget_class->priv;

  priv->accessible_type = type;
  /* reset this - honoring the type's role is better. */
  priv->accessible_role = ATK_ROLE_INVALID;
}

/**
 * gtk_widget_class_set_accessible_role:
 * @widget_class: class to set the accessible role for
 * @role: The role to use for accessibles created for @widget_class
 *
 * Sets the default #AtkRole to be set on accessibles created for
 * widgets of @widget_class. Accessibles may decide to not honor this
 * setting if their role reporting is more refined. Calls to 
 * gtk_widget_class_set_accessible_type() will reset this value.
 *
 * In cases where you want more fine-grained control over the role of
 * accessibles created for @widget_class, you should provide your own
 * accessible type and use gtk_widget_class_set_accessible_type()
 * instead.
 *
 * If @role is #ATK_ROLE_INVALID, the default role will not be changed
 * and the accessible's default role will be used instead.
 *
 * This function should only be called from class init functions of widgets.
 *
 * Since: 3.2
 **/
void
gtk_widget_class_set_accessible_role (GtkWidgetClass *widget_class,
                                      AtkRole         role)
{
  GtkWidgetClassPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET_CLASS (widget_class));

  priv = widget_class->priv;

  priv->accessible_role = role;
}

/**
 * gtk_widget_get_accessible:
 * @widget: a #GtkWidget
 *
 * Returns the accessible object that describes the widget to an
 * assistive technology.
 *
 * If accessibility support is not available, this #AtkObject
 * instance may be a no-op. Likewise, if no class-specific #AtkObject
 * implementation is available for the widget instance in question,
 * it will inherit an #AtkObject implementation from the first ancestor
 * class for which such an implementation is defined.
 *
 * The documentation of the
 * <ulink url="http://library.gnome.org/devel/atk/stable/">ATK</ulink>
 * library contains more information about accessible objects and their uses.
 *
 * Returns: (transfer none): the #AtkObject associated with @widget
 */
AtkObject*
gtk_widget_get_accessible (GtkWidget *widget)
{
  GtkWidgetClass *klass;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  klass = GTK_WIDGET_GET_CLASS (widget);

  g_return_val_if_fail (klass->get_accessible != NULL, NULL);

  return klass->get_accessible (widget);
}

static AtkObject*
gtk_widget_real_get_accessible (GtkWidget *widget)
{
  AtkObject* accessible;

  accessible = g_object_get_qdata (G_OBJECT (widget),
                                   quark_accessible_object);
  if (!accessible)
  {
    GtkWidgetClass *widget_class;
    GtkWidgetClassPrivate *priv;
    AtkObjectFactory *factory;
    AtkRegistry *default_registry;

    widget_class = GTK_WIDGET_GET_CLASS (widget);
    priv = widget_class->priv;

    if (priv->accessible_type == GTK_TYPE_ACCESSIBLE)
      {
        default_registry = atk_get_default_registry ();
        factory = atk_registry_get_factory (default_registry,
                                            G_TYPE_FROM_INSTANCE (widget));
        accessible =
          atk_object_factory_create_accessible (factory,
                                                G_OBJECT (widget));

        if (priv->accessible_role != ATK_ROLE_INVALID)
          atk_object_set_role (accessible, priv->accessible_role);

        g_object_set_qdata (G_OBJECT (widget),
                            quark_accessible_object,
                            accessible);
      }
    else
      {
        accessible = g_object_new (priv->accessible_type, NULL);
        if (priv->accessible_role != ATK_ROLE_INVALID)
          atk_object_set_role (accessible, priv->accessible_role);

        g_object_set_qdata (G_OBJECT (widget),
                            quark_accessible_object,
                            accessible);

        atk_object_initialize (accessible, widget);

        /* Set the role again, since we don't want a role set
         * in some parent initialize() function to override
         * our own.
         */
        if (priv->accessible_role != ATK_ROLE_INVALID)
          atk_object_set_role (accessible, priv->accessible_role);
      }
  }
  return accessible;
}

/*
 * Initialize a AtkImplementorIface instance's virtual pointers as
 * appropriate to this implementor's class (GtkWidget).
 */
static void
gtk_widget_accessible_interface_init (AtkImplementorIface *iface)
{
  iface->ref_accessible = gtk_widget_ref_accessible;
}

static AtkObject*
gtk_widget_ref_accessible (AtkImplementor *implementor)
{
  AtkObject *accessible;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (implementor));
  if (accessible)
    g_object_ref (accessible);
  return accessible;
}

/*
 * Expand flag management
 */

static void
gtk_widget_update_computed_expand (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;

  priv = widget->priv;

  if (priv->need_compute_expand)
    {
      gboolean h, v;

      if (priv->hexpand_set)
        h = priv->hexpand;
      else
        h = FALSE;

      if (priv->vexpand_set)
        v = priv->vexpand;
      else
        v = FALSE;

      /* we don't need to use compute_expand if both expands are
       * forced by the app
       */
      if (!(priv->hexpand_set && priv->vexpand_set))
        {
          if (GTK_WIDGET_GET_CLASS (widget)->compute_expand != NULL)
            {
              gboolean ignored;

              GTK_WIDGET_GET_CLASS (widget)->compute_expand (widget,
                                                             priv->hexpand_set ? &ignored : &h,
                                                             priv->vexpand_set ? &ignored : &v);
            }
        }

      priv->need_compute_expand = FALSE;
      priv->computed_hexpand = h != FALSE;
      priv->computed_vexpand = v != FALSE;
    }
}

/**
 * gtk_widget_queue_compute_expand:
 * @widget: a #GtkWidget
 *
 * Mark @widget as needing to recompute its expand flags. Call
 * this function when setting legacy expand child properties
 * on the child of a container.
 *
 * See gtk_widget_compute_expand().
 */
void
gtk_widget_queue_compute_expand (GtkWidget *widget)
{
  GtkWidget *parent;
  gboolean changed_anything;

  if (widget->priv->need_compute_expand)
    return;

  changed_anything = FALSE;
  parent = widget;
  while (parent != NULL)
    {
      if (!parent->priv->need_compute_expand)
        {
          parent->priv->need_compute_expand = TRUE;
          changed_anything = TRUE;
        }

      /* Note: if we had an invariant that "if a child needs to
       * compute expand, its parents also do" then we could stop going
       * up when we got to a parent that already needed to
       * compute. However, in general we compute expand lazily (as
       * soon as we see something in a subtree that is expand, we know
       * we're expanding) and so this invariant does not hold and we
       * have to always walk all the way up in case some ancestor
       * is not currently need_compute_expand.
       */

      parent = parent->priv->parent;
    }

  /* recomputing expand always requires
   * a relayout as well
   */
  if (changed_anything)
    gtk_widget_queue_resize (widget);
}

/**
 * gtk_widget_compute_expand:
 * @widget: the widget
 * @orientation: expand direction
 *
 * Computes whether a container should give this widget extra space
 * when possible. Containers should check this, rather than
 * looking at gtk_widget_get_hexpand() or gtk_widget_get_vexpand().
 *
 * This function already checks whether the widget is visible, so
 * visibility does not need to be checked separately. Non-visible
 * widgets are not expanded.
 *
 * The computed expand value uses either the expand setting explicitly
 * set on the widget itself, or, if none has been explicitly set,
 * the widget may expand if some of its children do.
 *
 * Return value: whether widget tree rooted here should be expanded
 */
gboolean
gtk_widget_compute_expand (GtkWidget     *widget,
                           GtkOrientation orientation)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  /* We never make a widget expand if not even showing. */
  if (!gtk_widget_get_visible (widget))
    return FALSE;

  gtk_widget_update_computed_expand (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      return widget->priv->computed_hexpand;
    }
  else
    {
      return widget->priv->computed_vexpand;
    }
}

static void
gtk_widget_set_expand (GtkWidget     *widget,
                       GtkOrientation orientation,
                       gboolean       expand)
{
  const char *expand_prop;
  const char *expand_set_prop;
  gboolean was_both;
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;

  expand = expand != FALSE;

  was_both = priv->hexpand && priv->vexpand;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (priv->hexpand_set &&
          priv->hexpand == expand)
        return;

      priv->hexpand_set = TRUE;
      priv->hexpand = expand;

      expand_prop = "hexpand";
      expand_set_prop = "hexpand-set";
    }
  else
    {
      if (priv->vexpand_set &&
          priv->vexpand == expand)
        return;

      priv->vexpand_set = TRUE;
      priv->vexpand = expand;

      expand_prop = "vexpand";
      expand_set_prop = "vexpand-set";
    }

  gtk_widget_queue_compute_expand (widget);

  g_object_freeze_notify (G_OBJECT (widget));
  g_object_notify (G_OBJECT (widget), expand_prop);
  g_object_notify (G_OBJECT (widget), expand_set_prop);
  if (was_both != (priv->hexpand && priv->vexpand))
    g_object_notify (G_OBJECT (widget), "expand");
  g_object_thaw_notify (G_OBJECT (widget));
}

static void
gtk_widget_set_expand_set (GtkWidget      *widget,
                           GtkOrientation  orientation,
                           gboolean        set)
{
  GtkWidgetPrivate *priv;
  const char *prop;

  priv = widget->priv;

  set = set != FALSE;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (set == priv->hexpand_set)
        return;

      priv->hexpand_set = set;
      prop = "hexpand-set";
    }
  else
    {
      if (set == priv->vexpand_set)
        return;

      priv->vexpand_set = set;
      prop = "vexpand-set";
    }

  gtk_widget_queue_compute_expand (widget);

  g_object_notify (G_OBJECT (widget), prop);
}

/**
 * gtk_widget_get_hexpand:
 * @widget: the widget
 *
 * Gets whether the widget would like any available extra horizontal
 * space. When a user resizes a #GtkWindow, widgets with expand=TRUE
 * generally receive the extra space. For example, a list or
 * scrollable area or document in your window would often be set to
 * expand.
 *
 * Containers should use gtk_widget_compute_expand() rather than
 * this function, to see whether a widget, or any of its children,
 * has the expand flag set. If any child of a widget wants to
 * expand, the parent may ask to expand also.
 *
 * This function only looks at the widget's own hexpand flag, rather
 * than computing whether the entire widget tree rooted at this widget
 * wants to expand.
 *
 * Return value: whether hexpand flag is set
 */
gboolean
gtk_widget_get_hexpand (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->hexpand;
}

/**
 * gtk_widget_set_hexpand:
 * @widget: the widget
 * @expand: whether to expand
 *
 * Sets whether the widget would like any available extra horizontal
 * space. When a user resizes a #GtkWindow, widgets with expand=TRUE
 * generally receive the extra space. For example, a list or
 * scrollable area or document in your window would often be set to
 * expand.
 *
 * Call this function to set the expand flag if you would like your
 * widget to become larger horizontally when the window has extra
 * room.
 *
 * By default, widgets automatically expand if any of their children
 * want to expand. (To see if a widget will automatically expand given
 * its current children and state, call gtk_widget_compute_expand(). A
 * container can decide how the expandability of children affects the
 * expansion of the container by overriding the compute_expand virtual
 * method on #GtkWidget.).
 *
 * Setting hexpand explicitly with this function will override the
 * automatic expand behavior.
 *
 * This function forces the widget to expand or not to expand,
 * regardless of children.  The override occurs because
 * gtk_widget_set_hexpand() sets the hexpand-set property (see
 * gtk_widget_set_hexpand_set()) which causes the widget's hexpand
 * value to be used, rather than looking at children and widget state.
 */
void
gtk_widget_set_hexpand (GtkWidget      *widget,
                        gboolean        expand)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand (widget, GTK_ORIENTATION_HORIZONTAL, expand);
}

/**
 * gtk_widget_get_hexpand_set:
 * @widget: the widget
 *
 * Gets whether gtk_widget_set_hexpand() has been used to
 * explicitly set the expand flag on this widget.
 *
 * If hexpand is set, then it overrides any computed
 * expand value based on child widgets. If hexpand is not
 * set, then the expand value depends on whether any
 * children of the widget would like to expand.
 *
 * There are few reasons to use this function, but it's here
 * for completeness and consistency.
 *
 * Return value: whether hexpand has been explicitly set
 */
gboolean
gtk_widget_get_hexpand_set (GtkWidget      *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->hexpand_set;
}

/**
 * gtk_widget_set_hexpand_set:
 * @widget: the widget
 * @set: value for hexpand-set property
 *
 * Sets whether the hexpand flag (see gtk_widget_get_hexpand()) will
 * be used.
 *
 * The hexpand-set property will be set automatically when you call
 * gtk_widget_set_hexpand() to set hexpand, so the most likely
 * reason to use this function would be to unset an explicit expand
 * flag.
 *
 * If hexpand is set, then it overrides any computed
 * expand value based on child widgets. If hexpand is not
 * set, then the expand value depends on whether any
 * children of the widget would like to expand.
 *
 * There are few reasons to use this function, but it's here
 * for completeness and consistency.
 */
void
gtk_widget_set_hexpand_set (GtkWidget      *widget,
                            gboolean        set)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand_set (widget, GTK_ORIENTATION_HORIZONTAL, set);
}


/**
 * gtk_widget_get_vexpand:
 * @widget: the widget
 *
 * Gets whether the widget would like any available extra vertical
 * space.
 *
 * See gtk_widget_get_hexpand() for more detail.
 *
 * Return value: whether vexpand flag is set
 */
gboolean
gtk_widget_get_vexpand (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->vexpand;
}

/**
 * gtk_widget_set_vexpand:
 * @widget: the widget
 * @expand: whether to expand
 *
 * Sets whether the widget would like any available extra vertical
 * space.
 *
 * See gtk_widget_set_hexpand() for more detail.
 */
void
gtk_widget_set_vexpand (GtkWidget      *widget,
                        gboolean        expand)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand (widget, GTK_ORIENTATION_VERTICAL, expand);
}

/**
 * gtk_widget_get_vexpand_set:
 * @widget: the widget
 *
 * Gets whether gtk_widget_set_vexpand() has been used to
 * explicitly set the expand flag on this widget.
 *
 * See gtk_widget_get_hexpand_set() for more detail.
 *
 * Return value: whether vexpand has been explicitly set
 */
gboolean
gtk_widget_get_vexpand_set (GtkWidget      *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->vexpand_set;
}

/**
 * gtk_widget_set_vexpand_set:
 * @widget: the widget
 * @set: value for vexpand-set property
 *
 * Sets whether the vexpand flag (see gtk_widget_get_vexpand()) will
 * be used.
 *
 * See gtk_widget_set_hexpand_set() for more detail.
 */
void
gtk_widget_set_vexpand_set (GtkWidget      *widget,
                            gboolean        set)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  gtk_widget_set_expand_set (widget, GTK_ORIENTATION_VERTICAL, set);
}

/*
 * GtkBuildable implementation
 */
static GQuark		 quark_builder_has_default = 0;
static GQuark		 quark_builder_has_focus = 0;
static GQuark		 quark_builder_atk_relations = 0;
static GQuark            quark_builder_set_name = 0;

static void
gtk_widget_buildable_interface_init (GtkBuildableIface *iface)
{
  quark_builder_has_default = g_quark_from_static_string ("gtk-builder-has-default");
  quark_builder_has_focus = g_quark_from_static_string ("gtk-builder-has-focus");
  quark_builder_atk_relations = g_quark_from_static_string ("gtk-builder-atk-relations");
  quark_builder_set_name = g_quark_from_static_string ("gtk-builder-set-name");

  iface->set_name = gtk_widget_buildable_set_name;
  iface->get_name = gtk_widget_buildable_get_name;
  iface->get_internal_child = gtk_widget_buildable_get_internal_child;
  iface->set_buildable_property = gtk_widget_buildable_set_buildable_property;
  iface->parser_finished = gtk_widget_buildable_parser_finished;
  iface->custom_tag_start = gtk_widget_buildable_custom_tag_start;
  iface->custom_finished = gtk_widget_buildable_custom_finished;
}

static void
gtk_widget_buildable_set_name (GtkBuildable *buildable,
			       const gchar  *name)
{
  g_object_set_qdata_full (G_OBJECT (buildable), quark_builder_set_name,
                           g_strdup (name), g_free);
}

static const gchar *
gtk_widget_buildable_get_name (GtkBuildable *buildable)
{
  return g_object_get_qdata (G_OBJECT (buildable), quark_builder_set_name);
}

static GObject *
gtk_widget_buildable_get_internal_child (GtkBuildable *buildable,
					 GtkBuilder   *builder,
					 const gchar  *childname)
{
  if (strcmp (childname, "accessible") == 0)
    return G_OBJECT (gtk_widget_get_accessible (GTK_WIDGET (buildable)));

  return NULL;
}

static void
gtk_widget_buildable_set_buildable_property (GtkBuildable *buildable,
					     GtkBuilder   *builder,
					     const gchar  *name,
					     const GValue *value)
{
  if (strcmp (name, "has-default") == 0 && g_value_get_boolean (value))
      g_object_set_qdata (G_OBJECT (buildable), quark_builder_has_default,
			  GINT_TO_POINTER (TRUE));
  else if (strcmp (name, "has-focus") == 0 && g_value_get_boolean (value))
      g_object_set_qdata (G_OBJECT (buildable), quark_builder_has_focus,
			  GINT_TO_POINTER (TRUE));
  else
    g_object_set_property (G_OBJECT (buildable), name, value);
}

typedef struct
{
  gchar *action_name;
  GString *description;
  gchar *context;
  gboolean translatable;
} AtkActionData;

typedef struct
{
  gchar *target;
  gchar *type;
} AtkRelationData;

static void
free_action (AtkActionData *data, gpointer user_data)
{
  g_free (data->action_name);
  g_string_free (data->description, TRUE);
  g_free (data->context);
  g_slice_free (AtkActionData, data);
}

static void
free_relation (AtkRelationData *data, gpointer user_data)
{
  g_free (data->target);
  g_free (data->type);
  g_slice_free (AtkRelationData, data);
}

static void
gtk_widget_buildable_parser_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder)
{
  GSList *atk_relations;

  if (g_object_get_qdata (G_OBJECT (buildable), quark_builder_has_default))
    gtk_widget_grab_default (GTK_WIDGET (buildable));
  if (g_object_get_qdata (G_OBJECT (buildable), quark_builder_has_focus))
    gtk_widget_grab_focus (GTK_WIDGET (buildable));

  atk_relations = g_object_get_qdata (G_OBJECT (buildable),
				      quark_builder_atk_relations);
  if (atk_relations)
    {
      AtkObject *accessible;
      AtkRelationSet *relation_set;
      GSList *l;
      GObject *target;
      AtkRelationType relation_type;
      AtkObject *target_accessible;

      accessible = gtk_widget_get_accessible (GTK_WIDGET (buildable));
      relation_set = atk_object_ref_relation_set (accessible);

      for (l = atk_relations; l; l = l->next)
	{
	  AtkRelationData *relation = (AtkRelationData*)l->data;

	  target = gtk_builder_get_object (builder, relation->target);
	  if (!target)
	    {
	      g_warning ("Target object %s in <relation> does not exist",
			 relation->target);
	      continue;
	    }
	  target_accessible = gtk_widget_get_accessible (GTK_WIDGET (target));
	  g_assert (target_accessible != NULL);

	  relation_type = atk_relation_type_for_name (relation->type);
	  if (relation_type == ATK_RELATION_NULL)
	    {
	      g_warning ("<relation> type %s not found",
			 relation->type);
	      continue;
	    }
	  atk_relation_set_add_relation_by_type (relation_set, relation_type,
						 target_accessible);
	}
      g_object_unref (relation_set);

      g_slist_foreach (atk_relations, (GFunc)free_relation, NULL);
      g_slist_free (atk_relations);
      g_object_set_qdata (G_OBJECT (buildable), quark_builder_atk_relations,
			  NULL);
    }
}

typedef struct
{
  GSList *actions;
  GSList *relations;
} AccessibilitySubParserData;

static void
accessibility_start_element (GMarkupParseContext  *context,
			     const gchar          *element_name,
			     const gchar         **names,
			     const gchar         **values,
			     gpointer              user_data,
			     GError              **error)
{
  AccessibilitySubParserData *data = (AccessibilitySubParserData*)user_data;
  guint i;
  gint line_number, char_number;

  if (strcmp (element_name, "relation") == 0)
    {
      gchar *target = NULL;
      gchar *type = NULL;
      AtkRelationData *relation;

      for (i = 0; names[i]; i++)
	{
	  if (strcmp (names[i], "target") == 0)
	    target = g_strdup (values[i]);
	  else if (strcmp (names[i], "type") == 0)
	    type = g_strdup (values[i]);
	  else
	    {
	      g_markup_parse_context_get_position (context,
						   &line_number,
						   &char_number);
	      g_set_error (error,
			   GTK_BUILDER_ERROR,
			   GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
			   "%s:%d:%d '%s' is not a valid attribute of <%s>",
			   "<input>",
			   line_number, char_number, names[i], "relation");
	      g_free (target);
	      g_free (type);
	      return;
	    }
	}

      if (!target || !type)
	{
	  g_markup_parse_context_get_position (context,
					       &line_number,
					       &char_number);
	  g_set_error (error,
		       GTK_BUILDER_ERROR,
		       GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
		       "%s:%d:%d <%s> requires attribute \"%s\"",
		       "<input>",
		       line_number, char_number, "relation",
		       type ? "target" : "type");
	  g_free (target);
	  g_free (type);
	  return;
	}

      relation = g_slice_new (AtkRelationData);
      relation->target = target;
      relation->type = type;

      data->relations = g_slist_prepend (data->relations, relation);
    }
  else if (strcmp (element_name, "action") == 0)
    {
      const gchar *action_name = NULL;
      const gchar *description = NULL;
      const gchar *msg_context = NULL;
      gboolean translatable = FALSE;
      AtkActionData *action;

      for (i = 0; names[i]; i++)
	{
	  if (strcmp (names[i], "action_name") == 0)
	    action_name = values[i];
	  else if (strcmp (names[i], "description") == 0)
	    description = values[i];
          else if (strcmp (names[i], "translatable") == 0)
            {
              if (!_gtk_builder_boolean_from_string (values[i], &translatable, error))
                return;
            }
          else if (strcmp (names[i], "comments") == 0)
            {
              /* do nothing, comments are for translators */
            }
          else if (strcmp (names[i], "context") == 0)
            msg_context = values[i];
	  else
	    {
	      g_markup_parse_context_get_position (context,
						   &line_number,
						   &char_number);
	      g_set_error (error,
			   GTK_BUILDER_ERROR,
			   GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
			   "%s:%d:%d '%s' is not a valid attribute of <%s>",
			   "<input>",
			   line_number, char_number, names[i], "action");
	      return;
	    }
	}

      if (!action_name)
	{
	  g_markup_parse_context_get_position (context,
					       &line_number,
					       &char_number);
	  g_set_error (error,
		       GTK_BUILDER_ERROR,
		       GTK_BUILDER_ERROR_MISSING_ATTRIBUTE,
		       "%s:%d:%d <%s> requires attribute \"%s\"",
		       "<input>",
		       line_number, char_number, "action",
		       "action_name");
	  return;
	}

      action = g_slice_new (AtkActionData);
      action->action_name = g_strdup (action_name);
      action->description = g_string_new (description);
      action->context = g_strdup (msg_context);
      action->translatable = translatable;

      data->actions = g_slist_prepend (data->actions, action);
    }
  else if (strcmp (element_name, "accessibility") == 0)
    ;
  else
    g_warning ("Unsupported tag for GtkWidget: %s\n", element_name);
}

static void
accessibility_text (GMarkupParseContext  *context,
                    const gchar          *text,
                    gsize                 text_len,
                    gpointer              user_data,
                    GError              **error)
{
  AccessibilitySubParserData *data = (AccessibilitySubParserData*)user_data;

  if (strcmp (g_markup_parse_context_get_element (context), "action") == 0)
    {
      AtkActionData *action = data->actions->data;

      g_string_append_len (action->description, text, text_len);
    }
}

static const GMarkupParser accessibility_parser =
  {
    accessibility_start_element,
    NULL,
    accessibility_text,
  };

typedef struct
{
  GObject *object;
  guint    key;
  guint    modifiers;
  gchar   *signal;
} AccelGroupParserData;

static void
accel_group_start_element (GMarkupParseContext  *context,
			   const gchar          *element_name,
			   const gchar         **names,
			   const gchar         **values,
			   gpointer              user_data,
			   GError              **error)
{
  gint i;
  guint key = 0;
  guint modifiers = 0;
  gchar *signal = NULL;
  AccelGroupParserData *parser_data = (AccelGroupParserData*)user_data;

  for (i = 0; names[i]; i++)
    {
      if (strcmp (names[i], "key") == 0)
	key = gdk_keyval_from_name (values[i]);
      else if (strcmp (names[i], "modifiers") == 0)
	{
	  if (!_gtk_builder_flags_from_string (GDK_TYPE_MODIFIER_TYPE,
					       values[i],
					       &modifiers,
					       error))
	      return;
	}
      else if (strcmp (names[i], "signal") == 0)
	signal = g_strdup (values[i]);
    }

  if (key == 0 || signal == NULL)
    {
      g_warning ("<accelerator> requires key and signal attributes");
      return;
    }
  parser_data->key = key;
  parser_data->modifiers = modifiers;
  parser_data->signal = signal;
}

static const GMarkupParser accel_group_parser =
  {
    accel_group_start_element,
  };

typedef struct
{
  GSList *classes;
} StyleParserData;

static void
style_start_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **names,
                     const gchar         **values,
                     gpointer              user_data,
                     GError              **error)
{
  StyleParserData *style_data = (StyleParserData *)user_data;
  gchar *class_name;

  if (strcmp (element_name, "class") == 0)
    {
      if (g_markup_collect_attributes (element_name,
                                       names,
                                       values,
                                       error,
                                       G_MARKUP_COLLECT_STRDUP, "name", &class_name,
                                       G_MARKUP_COLLECT_INVALID))
        {
          style_data->classes = g_slist_append (style_data->classes, class_name);
        }
    }
  else if (strcmp (element_name, "style") == 0)
    ;
  else
    g_warning ("Unsupported tag for GtkWidget: %s\n", element_name);
}

static const GMarkupParser style_parser =
  {
    style_start_element,
  };

static gboolean
gtk_widget_buildable_custom_tag_start (GtkBuildable     *buildable,
				       GtkBuilder       *builder,
				       GObject          *child,
				       const gchar      *tagname,
				       GMarkupParser    *parser,
				       gpointer         *data)
{
  g_assert (buildable);

  if (strcmp (tagname, "accelerator") == 0)
    {
      AccelGroupParserData *parser_data;

      parser_data = g_slice_new0 (AccelGroupParserData);
      parser_data->object = g_object_ref (buildable);
      *parser = accel_group_parser;
      *data = parser_data;
      return TRUE;
    }
  if (strcmp (tagname, "accessibility") == 0)
    {
      AccessibilitySubParserData *parser_data;

      parser_data = g_slice_new0 (AccessibilitySubParserData);
      *parser = accessibility_parser;
      *data = parser_data;
      return TRUE;
    }
  if (strcmp (tagname, "style") == 0)
    {
      StyleParserData *parser_data;

      parser_data = g_slice_new0 (StyleParserData);
      *parser = style_parser;
      *data = parser_data;
      return TRUE;
    }

  return FALSE;
}

void
_gtk_widget_buildable_finish_accelerator (GtkWidget *widget,
					  GtkWidget *toplevel,
					  gpointer   user_data)
{
  AccelGroupParserData *accel_data;
  GSList *accel_groups;
  GtkAccelGroup *accel_group;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (toplevel));
  g_return_if_fail (user_data != NULL);

  accel_data = (AccelGroupParserData*)user_data;
  accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
  if (g_slist_length (accel_groups) == 0)
    {
      accel_group = gtk_accel_group_new ();
      gtk_window_add_accel_group (GTK_WINDOW (toplevel), accel_group);
    }
  else
    {
      g_assert (g_slist_length (accel_groups) == 1);
      accel_group = g_slist_nth_data (accel_groups, 0);
    }

  gtk_widget_add_accelerator (GTK_WIDGET (accel_data->object),
			      accel_data->signal,
			      accel_group,
			      accel_data->key,
			      accel_data->modifiers,
			      GTK_ACCEL_VISIBLE);

  g_object_unref (accel_data->object);
  g_free (accel_data->signal);
  g_slice_free (AccelGroupParserData, accel_data);
}

static void
gtk_widget_buildable_custom_finished (GtkBuildable *buildable,
				      GtkBuilder   *builder,
				      GObject      *child,
				      const gchar  *tagname,
				      gpointer      user_data)
{
  if (strcmp (tagname, "accelerator") == 0)
    {
      AccelGroupParserData *accel_data;
      GtkWidget *toplevel;

      accel_data = (AccelGroupParserData*)user_data;
      g_assert (accel_data->object);

      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (accel_data->object));

      _gtk_widget_buildable_finish_accelerator (GTK_WIDGET (buildable), toplevel, user_data);
    }
  else if (strcmp (tagname, "accessibility") == 0)
    {
      AccessibilitySubParserData *a11y_data;

      a11y_data = (AccessibilitySubParserData*)user_data;

      if (a11y_data->actions)
	{
	  AtkObject *accessible;
	  AtkAction *action;
	  gint i, n_actions;
	  GSList *l;

	  accessible = gtk_widget_get_accessible (GTK_WIDGET (buildable));

          if (ATK_IS_ACTION (accessible))
            {
	      action = ATK_ACTION (accessible);
	      n_actions = atk_action_get_n_actions (action);

	      for (l = a11y_data->actions; l; l = l->next)
	        {
	          AtkActionData *action_data = (AtkActionData*)l->data;

	          for (i = 0; i < n_actions; i++)
		    if (strcmp (atk_action_get_name (action, i),
		  	        action_data->action_name) == 0)
		      break;

	          if (i < n_actions)
                    {
                      gchar *description;

                      if (action_data->translatable && action_data->description->len)
                        description = _gtk_builder_parser_translate (gtk_builder_get_translation_domain (builder),
                                                                     action_data->context,
                                                                     action_data->description->str);
                      else
                        description = action_data->description->str;

		      atk_action_set_description (action, i, description);
                    }
                }
	    }
          else
            g_warning ("accessibility action on a widget that does not implement AtkAction");

	  g_slist_foreach (a11y_data->actions, (GFunc)free_action, NULL);
	  g_slist_free (a11y_data->actions);
	}

      if (a11y_data->relations)
	g_object_set_qdata (G_OBJECT (buildable), quark_builder_atk_relations,
			    a11y_data->relations);

      g_slice_free (AccessibilitySubParserData, a11y_data);
    }
  else if (strcmp (tagname, "style") == 0)
    {
      StyleParserData *style_data = (StyleParserData *)user_data;
      GtkStyleContext *context;
      GSList *l;

      context = gtk_widget_get_style_context (GTK_WIDGET (buildable));

      for (l = style_data->classes; l; l = l->next)
        gtk_style_context_add_class (context, (const gchar *)l->data);

      gtk_widget_reset_style (GTK_WIDGET (buildable));

      g_slist_free_full (style_data->classes, g_free);
      g_slice_free (StyleParserData, style_data);
    }
}

static GtkSizeRequestMode 
gtk_widget_real_get_request_mode (GtkWidget *widget)
{ 
  /* By default widgets dont trade size at all. */
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
gtk_widget_real_get_width (GtkWidget *widget,
			   gint      *minimum_size,
			   gint      *natural_size)
{
  if (minimum_size)
    *minimum_size = 0;

  if (natural_size)
    *natural_size = 0;
}

static void
gtk_widget_real_get_height (GtkWidget *widget,
			    gint      *minimum_size,
			    gint      *natural_size)
{
  if (minimum_size)
    *minimum_size = 0;

  if (natural_size)
    *natural_size = 0;
}

static void
gtk_widget_real_get_height_for_width (GtkWidget *widget,
                                      gint       width,
                                      gint      *minimum_height,
                                      gint      *natural_height)
{
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, minimum_height, natural_height);
}

static void
gtk_widget_real_get_width_for_height (GtkWidget *widget,
                                      gint       height,
                                      gint      *minimum_width,
                                      gint      *natural_width)
{
  GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, minimum_width, natural_width);
}

/**
 * gtk_widget_get_halign:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:halign property.
 *
 * Returns: the horizontal alignment of @widget
 */
GtkAlign
gtk_widget_get_halign (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_ALIGN_FILL);
  return _gtk_widget_get_aux_info_or_defaults (widget)->halign;
}

/**
 * gtk_widget_set_halign:
 * @widget: a #GtkWidget
 * @align: the horizontal alignment
 *
 * Sets the horizontal alignment of @widget.
 * See the #GtkWidget:halign property.
 */
void
gtk_widget_set_halign (GtkWidget *widget,
                       GtkAlign   align)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->halign == align)
    return;

  aux_info->halign = align;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "halign");
}

/**
 * gtk_widget_get_valign:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:valign property.
 *
 * Returns: the vertical alignment of @widget
 */
GtkAlign
gtk_widget_get_valign (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_ALIGN_FILL);
  return _gtk_widget_get_aux_info_or_defaults (widget)->valign;
}

/**
 * gtk_widget_set_valign:
 * @widget: a #GtkWidget
 * @align: the vertical alignment
 *
 * Sets the vertical alignment of @widget.
 * See the #GtkWidget:valign property.
 */
void
gtk_widget_set_valign (GtkWidget *widget,
                       GtkAlign   align)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->valign == align)
    return;

  aux_info->valign = align;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "valign");
}

/**
 * gtk_widget_get_margin_left:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-left property.
 *
 * Returns: The left margin of @widget
 *
 * Since: 3.0
 */
gint
gtk_widget_get_margin_left (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return _gtk_widget_get_aux_info_or_defaults (widget)->margin.left;
}

/**
 * gtk_widget_set_margin_left:
 * @widget: a #GtkWidget
 * @margin: the left margin
 *
 * Sets the left margin of @widget.
 * See the #GtkWidget:margin-left property.
 *
 * Since: 3.0
 */
void
gtk_widget_set_margin_left (GtkWidget *widget,
                            gint       margin)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->margin.left == margin)
    return;

  aux_info->margin.left = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-left");
}

/**
 * gtk_widget_get_margin_right:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-right property.
 *
 * Returns: The right margin of @widget
 *
 * Since: 3.0
 */
gint
gtk_widget_get_margin_right (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return _gtk_widget_get_aux_info_or_defaults (widget)->margin.right;
}

/**
 * gtk_widget_set_margin_right:
 * @widget: a #GtkWidget
 * @margin: the right margin
 *
 * Sets the right margin of @widget.
 * See the #GtkWidget:margin-right property.
 *
 * Since: 3.0
 */
void
gtk_widget_set_margin_right (GtkWidget *widget,
                             gint       margin)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->margin.right == margin)
    return;

  aux_info->margin.right = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-right");
}

/**
 * gtk_widget_get_margin_top:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-top property.
 *
 * Returns: The top margin of @widget
 *
 * Since: 3.0
 */
gint
gtk_widget_get_margin_top (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return _gtk_widget_get_aux_info_or_defaults (widget)->margin.top;
}

/**
 * gtk_widget_set_margin_top:
 * @widget: a #GtkWidget
 * @margin: the top margin
 *
 * Sets the top margin of @widget.
 * See the #GtkWidget:margin-top property.
 *
 * Since: 3.0
 */
void
gtk_widget_set_margin_top (GtkWidget *widget,
                           gint       margin)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->margin.top == margin)
    return;

  aux_info->margin.top = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-top");
}

/**
 * gtk_widget_get_margin_bottom:
 * @widget: a #GtkWidget
 *
 * Gets the value of the #GtkWidget:margin-bottom property.
 *
 * Returns: The bottom margin of @widget
 *
 * Since: 3.0
 */
gint
gtk_widget_get_margin_bottom (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return _gtk_widget_get_aux_info_or_defaults (widget)->margin.bottom;
}

/**
 * gtk_widget_set_margin_bottom:
 * @widget: a #GtkWidget
 * @margin: the bottom margin
 *
 * Sets the bottom margin of @widget.
 * See the #GtkWidget:margin-bottom property.
 *
 * Since: 3.0
 */
void
gtk_widget_set_margin_bottom (GtkWidget *widget,
                              gint       margin)
{
  GtkWidgetAuxInfo *aux_info;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (margin <= G_MAXINT16);

  aux_info = gtk_widget_get_aux_info (widget, TRUE);

  if (aux_info->margin.bottom == margin)
    return;

  aux_info->margin.bottom = margin;
  gtk_widget_queue_resize (widget);
  g_object_notify (G_OBJECT (widget), "margin-bottom");
}

/**
 * gtk_widget_get_clipboard:
 * @widget: a #GtkWidget
 * @selection: a #GdkAtom which identifies the clipboard
 *             to use. %GDK_SELECTION_CLIPBOARD gives the
 *             default clipboard. Another common value
 *             is %GDK_SELECTION_PRIMARY, which gives
 *             the primary X selection.
 *
 * Returns the clipboard object for the given selection to
 * be used with @widget. @widget must have a #GdkDisplay
 * associated with it, so must be attached to a toplevel
 * window.
 *
 * Return value: (transfer none): the appropriate clipboard object. If no
 *             clipboard already exists, a new one will
 *             be created. Once a clipboard object has
 *             been created, it is persistent for all time.
 *
 * Since: 2.2
 **/
GtkClipboard *
gtk_widget_get_clipboard (GtkWidget *widget, GdkAtom selection)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (gtk_widget_has_screen (widget), NULL);

  return gtk_clipboard_get_for_display (gtk_widget_get_display (widget),
					selection);
}

/**
 * gtk_widget_list_mnemonic_labels:
 * @widget: a #GtkWidget
 *
 * Returns a newly allocated list of the widgets, normally labels, for
 * which this widget is the target of a mnemonic (see for example,
 * gtk_label_set_mnemonic_widget()).

 * The widgets in the list are not individually referenced. If you
 * want to iterate through the list and perform actions involving
 * callbacks that might destroy the widgets, you
 * <emphasis>must</emphasis> call <literal>g_list_foreach (result,
 * (GFunc)g_object_ref, NULL)</literal> first, and then unref all the
 * widgets afterwards.

 * Return value: (element-type GtkWidget) (transfer container): the list of
 *  mnemonic labels; free this list
 *  with g_list_free() when you are done with it.
 *
 * Since: 2.4
 **/
GList *
gtk_widget_list_mnemonic_labels (GtkWidget *widget)
{
  GList *list = NULL;
  GSList *l;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  for (l = g_object_get_qdata (G_OBJECT (widget), quark_mnemonic_labels); l; l = l->next)
    list = g_list_prepend (list, l->data);

  return list;
}

/**
 * gtk_widget_add_mnemonic_label:
 * @widget: a #GtkWidget
 * @label: a #GtkWidget that acts as a mnemonic label for @widget
 *
 * Adds a widget to the list of mnemonic labels for
 * this widget. (See gtk_widget_list_mnemonic_labels()). Note the
 * list of mnemonic labels for the widget is cleared when the
 * widget is destroyed, so the caller must make sure to update
 * its internal state at this point as well, by using a connection
 * to the #GtkWidget::destroy signal or a weak notifier.
 *
 * Since: 2.4
 **/
void
gtk_widget_add_mnemonic_label (GtkWidget *widget,
                               GtkWidget *label)
{
  GSList *old_list, *new_list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (label));

  old_list = g_object_steal_qdata (G_OBJECT (widget), quark_mnemonic_labels);
  new_list = g_slist_prepend (old_list, label);

  g_object_set_qdata_full (G_OBJECT (widget), quark_mnemonic_labels,
			   new_list, (GDestroyNotify) g_slist_free);
}

/**
 * gtk_widget_remove_mnemonic_label:
 * @widget: a #GtkWidget
 * @label: a #GtkWidget that was previously set as a mnemnic label for
 *         @widget with gtk_widget_add_mnemonic_label().
 *
 * Removes a widget from the list of mnemonic labels for
 * this widget. (See gtk_widget_list_mnemonic_labels()). The widget
 * must have previously been added to the list with
 * gtk_widget_add_mnemonic_label().
 *
 * Since: 2.4
 **/
void
gtk_widget_remove_mnemonic_label (GtkWidget *widget,
                                  GtkWidget *label)
{
  GSList *old_list, *new_list;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_WIDGET (label));

  old_list = g_object_steal_qdata (G_OBJECT (widget), quark_mnemonic_labels);
  new_list = g_slist_remove (old_list, label);

  if (new_list)
    g_object_set_qdata_full (G_OBJECT (widget), quark_mnemonic_labels,
			     new_list, (GDestroyNotify) g_slist_free);
}

/**
 * gtk_widget_get_no_show_all:
 * @widget: a #GtkWidget
 *
 * Returns the current value of the #GtkWidget:no-show-all property,
 * which determines whether calls to gtk_widget_show_all()
 * will affect this widget.
 *
 * Return value: the current value of the "no-show-all" property.
 *
 * Since: 2.4
 **/
gboolean
gtk_widget_get_no_show_all (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->no_show_all;
}

/**
 * gtk_widget_set_no_show_all:
 * @widget: a #GtkWidget
 * @no_show_all: the new value for the "no-show-all" property
 *
 * Sets the #GtkWidget:no-show-all property, which determines whether
 * calls to gtk_widget_show_all() will affect this widget.
 *
 * This is mostly for use in constructing widget hierarchies with externally
 * controlled visibility, see #GtkUIManager.
 *
 * Since: 2.4
 **/
void
gtk_widget_set_no_show_all (GtkWidget *widget,
			    gboolean   no_show_all)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  no_show_all = (no_show_all != FALSE);

  if (widget->priv->no_show_all != no_show_all)
    {
      widget->priv->no_show_all = no_show_all;

      g_object_notify (G_OBJECT (widget), "no-show-all");
    }
}


static void
gtk_widget_real_set_has_tooltip (GtkWidget *widget,
			         gboolean   has_tooltip,
			         gboolean   force)
{
  GtkWidgetPrivate *priv = widget->priv;
  gboolean priv_has_tooltip;

  priv_has_tooltip = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (widget),
				       quark_has_tooltip));

  if (priv_has_tooltip != has_tooltip || force)
    {
      priv_has_tooltip = has_tooltip;

      if (priv_has_tooltip)
        {
	  if (gtk_widget_get_realized (widget) && !gtk_widget_get_has_window (widget))
	    gdk_window_set_events (priv->window,
				   gdk_window_get_events (priv->window) |
				   GDK_LEAVE_NOTIFY_MASK |
				   GDK_POINTER_MOTION_MASK |
				   GDK_POINTER_MOTION_HINT_MASK);

	  if (gtk_widget_get_has_window (widget))
	      gtk_widget_add_events (widget,
				     GDK_LEAVE_NOTIFY_MASK |
				     GDK_POINTER_MOTION_MASK |
				     GDK_POINTER_MOTION_HINT_MASK);
	}

      g_object_set_qdata (G_OBJECT (widget), quark_has_tooltip,
			  GUINT_TO_POINTER (priv_has_tooltip));
    }
}

/**
 * gtk_widget_set_tooltip_window:
 * @widget: a #GtkWidget
 * @custom_window: (allow-none): a #GtkWindow, or %NULL
 *
 * Replaces the default, usually yellow, window used for displaying
 * tooltips with @custom_window. GTK+ will take care of showing and
 * hiding @custom_window at the right moment, to behave likewise as
 * the default tooltip window. If @custom_window is %NULL, the default
 * tooltip window will be used.
 *
 * If the custom window should have the default theming it needs to
 * have the name "gtk-tooltip", see gtk_widget_set_name().
 *
 * Since: 2.12
 */
void
gtk_widget_set_tooltip_window (GtkWidget *widget,
			       GtkWindow *custom_window)
{
  gboolean has_tooltip;
  gchar *tooltip_markup;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (custom_window == NULL || GTK_IS_WINDOW (custom_window));

  tooltip_markup = g_object_get_qdata (G_OBJECT (widget), quark_tooltip_markup);

  if (custom_window)
    g_object_ref (custom_window);

  g_object_set_qdata_full (G_OBJECT (widget), quark_tooltip_window,
			   custom_window, g_object_unref);

  has_tooltip = (custom_window != NULL || tooltip_markup != NULL);
  gtk_widget_real_set_has_tooltip (widget, has_tooltip, FALSE);

  if (has_tooltip && gtk_widget_get_visible (widget))
    gtk_widget_queue_tooltip_query (widget);
}

/**
 * gtk_widget_get_tooltip_window:
 * @widget: a #GtkWidget
 *
 * Returns the #GtkWindow of the current tooltip. This can be the
 * GtkWindow created by default, or the custom tooltip window set
 * using gtk_widget_set_tooltip_window().
 *
 * Return value: (transfer none): The #GtkWindow of the current tooltip.
 *
 * Since: 2.12
 */
GtkWindow *
gtk_widget_get_tooltip_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return g_object_get_qdata (G_OBJECT (widget), quark_tooltip_window);
}

/**
 * gtk_widget_trigger_tooltip_query:
 * @widget: a #GtkWidget
 *
 * Triggers a tooltip query on the display where the toplevel of @widget
 * is located. See gtk_tooltip_trigger_tooltip_query() for more
 * information.
 *
 * Since: 2.12
 */
void
gtk_widget_trigger_tooltip_query (GtkWidget *widget)
{
  gtk_tooltip_trigger_tooltip_query (gtk_widget_get_display (widget));
}

static guint tooltip_query_id;
static GSList *tooltip_query_displays;

static gboolean
tooltip_query_idle (gpointer data)
{
  g_slist_foreach (tooltip_query_displays, (GFunc)gtk_tooltip_trigger_tooltip_query, NULL);
  g_slist_foreach (tooltip_query_displays, (GFunc)g_object_unref, NULL);
  g_slist_free (tooltip_query_displays);

  tooltip_query_displays = NULL;
  tooltip_query_id = 0;

  return FALSE;
}

static void
gtk_widget_queue_tooltip_query (GtkWidget *widget)
{
  GdkDisplay *display;

  display = gtk_widget_get_display (widget);

  if (!g_slist_find (tooltip_query_displays, display))
    tooltip_query_displays = g_slist_prepend (tooltip_query_displays, g_object_ref (display));

  if (tooltip_query_id == 0)
    tooltip_query_id = gdk_threads_add_idle (tooltip_query_idle, NULL);
}

/**
 * gtk_widget_set_tooltip_text:
 * @widget: a #GtkWidget
 * @text: the contents of the tooltip for @widget
 *
 * Sets @text as the contents of the tooltip. This function will take
 * care of setting #GtkWidget:has-tooltip to %TRUE and of the default
 * handler for the #GtkWidget::query-tooltip signal.
 *
 * See also the #GtkWidget:tooltip-text property and gtk_tooltip_set_text().
 *
 * Since: 2.12
 */
void
gtk_widget_set_tooltip_text (GtkWidget   *widget,
                             const gchar *text)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_set (G_OBJECT (widget), "tooltip-text", text, NULL);
}

/**
 * gtk_widget_get_tooltip_text:
 * @widget: a #GtkWidget
 *
 * Gets the contents of the tooltip for @widget.
 *
 * Return value: the tooltip text, or %NULL. You should free the
 *   returned string with g_free() when done.
 *
 * Since: 2.12
 */
gchar *
gtk_widget_get_tooltip_text (GtkWidget *widget)
{
  gchar *text = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  g_object_get (G_OBJECT (widget), "tooltip-text", &text, NULL);

  return text;
}

/**
 * gtk_widget_set_tooltip_markup:
 * @widget: a #GtkWidget
 * @markup: (allow-none): the contents of the tooltip for @widget, or %NULL
 *
 * Sets @markup as the contents of the tooltip, which is marked up with
 *  the <link linkend="PangoMarkupFormat">Pango text markup language</link>.
 *
 * This function will take care of setting #GtkWidget:has-tooltip to %TRUE
 * and of the default handler for the #GtkWidget::query-tooltip signal.
 *
 * See also the #GtkWidget:tooltip-markup property and
 * gtk_tooltip_set_markup().
 *
 * Since: 2.12
 */
void
gtk_widget_set_tooltip_markup (GtkWidget   *widget,
                               const gchar *markup)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_set (G_OBJECT (widget), "tooltip-markup", markup, NULL);
}

/**
 * gtk_widget_get_tooltip_markup:
 * @widget: a #GtkWidget
 *
 * Gets the contents of the tooltip for @widget.
 *
 * Return value: the tooltip text, or %NULL. You should free the
 *   returned string with g_free() when done.
 *
 * Since: 2.12
 */
gchar *
gtk_widget_get_tooltip_markup (GtkWidget *widget)
{
  gchar *text = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  g_object_get (G_OBJECT (widget), "tooltip-markup", &text, NULL);

  return text;
}

/**
 * gtk_widget_set_has_tooltip:
 * @widget: a #GtkWidget
 * @has_tooltip: whether or not @widget has a tooltip.
 *
 * Sets the has-tooltip property on @widget to @has_tooltip.  See
 * #GtkWidget:has-tooltip for more information.
 *
 * Since: 2.12
 */
void
gtk_widget_set_has_tooltip (GtkWidget *widget,
			    gboolean   has_tooltip)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));

  g_object_set (G_OBJECT (widget), "has-tooltip", has_tooltip, NULL);
}

/**
 * gtk_widget_get_has_tooltip:
 * @widget: a #GtkWidget
 *
 * Returns the current value of the has-tooltip property.  See
 * #GtkWidget:has-tooltip for more information.
 *
 * Return value: current value of has-tooltip on @widget.
 *
 * Since: 2.12
 */
gboolean
gtk_widget_get_has_tooltip (GtkWidget *widget)
{
  gboolean has_tooltip = FALSE;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  g_object_get (G_OBJECT (widget), "has-tooltip", &has_tooltip, NULL);

  return has_tooltip;
}

/**
 * gtk_widget_get_allocation:
 * @widget: a #GtkWidget
 * @allocation: (out): a pointer to a #GtkAllocation to copy to
 *
 * Retrieves the widget's allocation.
 *
 * Note, when implementing a #GtkContainer: a widget's allocation will
 * be its "adjusted" allocation, that is, the widget's parent
 * container typically calls gtk_widget_size_allocate() with an
 * allocation, and that allocation is then adjusted (to handle margin
 * and alignment for example) before assignment to the widget.
 * gtk_widget_get_allocation() returns the adjusted allocation that
 * was actually assigned to the widget. The adjusted allocation is
 * guaranteed to be completely contained within the
 * gtk_widget_size_allocate() allocation, however. So a #GtkContainer
 * is guaranteed that its children stay inside the assigned bounds,
 * but not that they have exactly the bounds the container assigned.
 * There is no way to get the original allocation assigned by
 * gtk_widget_size_allocate(), since it isn't stored; if a container
 * implementation needs that information it will have to track it itself.
 *
 * Since: 2.18
 */
void
gtk_widget_get_allocation (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (allocation != NULL);

  priv = widget->priv;

  *allocation = priv->allocation;
}

/**
 * gtk_widget_set_allocation:
 * @widget: a #GtkWidget
 * @allocation: a pointer to a #GtkAllocation to copy from
 *
 * Sets the widget's allocation.  This should not be used
 * directly, but from within a widget's size_allocate method.
 *
 * The allocation set should be the "adjusted" or actual
 * allocation. If you're implementing a #GtkContainer, you want to use
 * gtk_widget_size_allocate() instead of gtk_widget_set_allocation().
 * The GtkWidgetClass::adjust_size_allocation virtual method adjusts the
 * allocation inside gtk_widget_size_allocate() to create an adjusted
 * allocation.
 *
 * Since: 2.18
 */
void
gtk_widget_set_allocation (GtkWidget           *widget,
                           const GtkAllocation *allocation)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (allocation != NULL);

  priv = widget->priv;

  priv->allocation = *allocation;
}

/**
 * gtk_widget_get_allocated_width:
 * @widget: the widget to query
 *
 * Returns the width that has currently been allocated to @widget.
 * This function is intended to be used when implementing handlers
 * for the #GtkWidget::draw function.
 *
 * Returns: the width of the @widget
 **/
int
gtk_widget_get_allocated_width (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return widget->priv->allocation.width;
}

/**
 * gtk_widget_get_allocated_height:
 * @widget: the widget to query
 *
 * Returns the height that has currently been allocated to @widget.
 * This function is intended to be used when implementing handlers
 * for the #GtkWidget::draw function.
 *
 * Returns: the height of the @widget
 **/
int
gtk_widget_get_allocated_height (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  return widget->priv->allocation.height;
}

/**
 * gtk_widget_get_requisition:
 * @widget: a #GtkWidget
 * @requisition: (out): a pointer to a #GtkRequisition to copy to
 *
 * Retrieves the widget's requisition.
 *
 * This function should only be used by widget implementations in
 * order to figure whether the widget's requisition has actually
 * changed after some internal state change (so that they can call
 * gtk_widget_queue_resize() instead of gtk_widget_queue_draw()).
 *
 * Normally, gtk_widget_size_request() should be used.
 *
 * Since: 2.20
 *
 * Deprecated: 3.0: The #GtkRequisition cache on the widget was
 * removed, If you need to cache sizes across requests and allocations,
 * add an explicit cache to the widget in question instead.
 */
void
gtk_widget_get_requisition (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (requisition != NULL);

  gtk_widget_get_preferred_size (widget, requisition, NULL);
}

/**
 * gtk_widget_set_window:
 * @widget: a #GtkWidget
 * @window: (transfer full): a #GdkWindow
 *
 * Sets a widget's window. This function should only be used in a
 * widget's #GtkWidget::realize implementation. The %window passed is
 * usually either new window created with gdk_window_new(), or the
 * window of its parent widget as returned by
 * gtk_widget_get_parent_window().
 *
 * Widgets must indicate whether they will create their own #GdkWindow
 * by calling gtk_widget_set_has_window(). This is usually done in the
 * widget's init() function.
 *
 * <note><para>This function does not add any reference to @window.</para></note>
 *
 * Since: 2.18
 */
void
gtk_widget_set_window (GtkWidget *widget,
                       GdkWindow *window)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));

  priv = widget->priv;

  if (priv->window != window)
    {
      priv->window = window;
      g_object_notify (G_OBJECT (widget), "window");
    }
}

/**
 * gtk_widget_get_window:
 * @widget: a #GtkWidget
 *
 * Returns the widget's window if it is realized, %NULL otherwise
 *
 * Return value: (transfer none): @widget's window.
 *
 * Since: 2.14
 */
GdkWindow*
gtk_widget_get_window (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  return widget->priv->window;
}

/**
 * gtk_widget_get_support_multidevice:
 * @widget: a #GtkWidget
 *
 * Returns %TRUE if @widget is multiple pointer aware. See
 * gtk_widget_set_support_multidevice() for more information.
 *
 * Returns: %TRUE if @widget is multidevice aware.
 **/
gboolean
gtk_widget_get_support_multidevice (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return widget->priv->multidevice;
}

/**
 * gtk_widget_set_support_multidevice:
 * @widget: a #GtkWidget
 * @support_multidevice: %TRUE to support input from multiple devices.
 *
 * Enables or disables multiple pointer awareness. If this setting is %TRUE,
 * @widget will start receiving multiple, per device enter/leave events. Note
 * that if custom #GdkWindow<!-- -->s are created in #GtkWidget::realize,
 * gdk_window_set_support_multidevice() will have to be called manually on them.
 *
 * Since: 3.0
 **/
void
gtk_widget_set_support_multidevice (GtkWidget *widget,
                                    gboolean   support_multidevice)
{
  GtkWidgetPrivate *priv;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = widget->priv;
  priv->multidevice = (support_multidevice == TRUE);

  if (gtk_widget_get_realized (widget))
    gdk_window_set_support_multidevice (priv->window, support_multidevice);
}

static void
_gtk_widget_set_has_focus (GtkWidget *widget,
                           gboolean   has_focus)
{
  widget->priv->has_focus = has_focus;

  if (has_focus)
    gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_FOCUSED, FALSE);
  else
    gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_FOCUSED);
}

/**
 * gtk_widget_send_focus_change:
 * @widget: a #GtkWidget
 * @event: a #GdkEvent of type GDK_FOCUS_CHANGE
 *
 * Sends the focus change @event to @widget
 *
 * This function is not meant to be used by applications. The only time it
 * should be used is when it is necessary for a #GtkWidget to assign focus
 * to a widget that is semantically owned by the first widget even though
 * it's not a direct child - for instance, a search entry in a floating
 * window similar to the quick search in #GtkTreeView.
 *
 * An example of its usage is:
 *
 * |[
 *   GdkEvent *fevent = gdk_event_new (GDK_FOCUS_CHANGE);
 *
 *   fevent->focus_change.type = GDK_FOCUS_CHANGE;
 *   fevent->focus_change.in = TRUE;
 *   fevent->focus_change.window = gtk_widget_get_window (widget);
 *   if (fevent->focus_change.window != NULL)
 *     g_object_ref (fevent->focus_change.window);
 *
 *   gtk_widget_send_focus_change (widget, fevent);
 *
 *   gdk_event_free (event);
 * ]|
 *
 * Return value: the return value from the event signal emission: %TRUE
 *   if the event was handled, and %FALSE otherwise
 *
 * Since: 2.20
 */
gboolean
gtk_widget_send_focus_change (GtkWidget *widget,
                              GdkEvent  *event)
{
  gboolean res;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (event != NULL && event->type == GDK_FOCUS_CHANGE, FALSE);

  g_object_ref (widget);

  _gtk_widget_set_has_focus (widget, event->focus_change.in);

  res = gtk_widget_event (widget, event);

  g_object_notify (G_OBJECT (widget), "has-focus");

  g_object_unref (widget);

  return res;
}

/**
 * gtk_widget_in_destruction:
 * @widget: a #GtkWidget
 *
 * Returns whether the widget is currently being destroyed.
 * This information can sometimes be used to avoid doing
 * unnecessary work.
 *
 * Returns: %TRUE if @widget is being destroyed
 */
gboolean
gtk_widget_in_destruction (GtkWidget *widget)
{
  return widget->priv->in_destruction;
}

gboolean
_gtk_widget_get_resize_pending (GtkWidget *widget)
{
  return widget->priv->resize_pending;
}

void
_gtk_widget_set_resize_pending (GtkWidget *widget,
                                gboolean   resize_pending)
{
  widget->priv->resize_pending = resize_pending;
}

gboolean
_gtk_widget_get_in_reparent (GtkWidget *widget)
{
  return widget->priv->in_reparent;
}

void
_gtk_widget_set_in_reparent (GtkWidget *widget,
                             gboolean   in_reparent)
{
  widget->priv->in_reparent = in_reparent;
}

gboolean
_gtk_widget_get_anchored (GtkWidget *widget)
{
  return widget->priv->anchored;
}

void
_gtk_widget_set_anchored (GtkWidget *widget,
                          gboolean   anchored)
{
  widget->priv->anchored = anchored;
}

gboolean
_gtk_widget_get_shadowed (GtkWidget *widget)
{
  return widget->priv->shadowed;
}

void
_gtk_widget_set_shadowed (GtkWidget *widget,
                          gboolean   shadowed)
{
  widget->priv->shadowed = shadowed;
}

gboolean
_gtk_widget_get_alloc_needed (GtkWidget *widget)
{
  return widget->priv->alloc_needed;
}

void
_gtk_widget_set_alloc_needed (GtkWidget *widget,
                              gboolean   alloc_needed)
{
  widget->priv->alloc_needed = alloc_needed;
}

gboolean
_gtk_widget_get_width_request_needed (GtkWidget *widget)
{
  return widget->priv->width_request_needed;
}

void
_gtk_widget_set_width_request_needed (GtkWidget *widget,
                                      gboolean   width_request_needed)
{
  widget->priv->width_request_needed = width_request_needed;
}

gboolean
_gtk_widget_get_height_request_needed (GtkWidget *widget)
{
  return widget->priv->height_request_needed;
}

void
_gtk_widget_set_height_request_needed (GtkWidget *widget,
                                       gboolean   height_request_needed)
{
  widget->priv->height_request_needed = height_request_needed;
}

gboolean
_gtk_widget_get_sizegroup_visited (GtkWidget    *widget)
{
  return widget->priv->sizegroup_visited;
}

void
_gtk_widget_set_sizegroup_visited (GtkWidget    *widget,
				   gboolean      visited)
{
  widget->priv->sizegroup_visited = visited;
}

gboolean
_gtk_widget_get_sizegroup_bumping (GtkWidget    *widget)
{
  return widget->priv->sizegroup_bumping;
}

void
_gtk_widget_set_sizegroup_bumping (GtkWidget    *widget,
				   gboolean      bumping)
{
  widget->priv->sizegroup_bumping = bumping;
}

void
_gtk_widget_add_sizegroup (GtkWidget    *widget,
			   gpointer      group)
{
  GSList *groups;

  groups = g_object_get_qdata (G_OBJECT (widget), quark_size_groups);
  groups = g_slist_prepend (groups, group);
  g_object_set_qdata (G_OBJECT (widget), quark_size_groups, groups);

  widget->priv->have_size_groups = TRUE;
}

void
_gtk_widget_remove_sizegroup (GtkWidget    *widget,
			      gpointer      group)
{
  GSList *groups;

  groups = g_object_get_qdata (G_OBJECT (widget), quark_size_groups);
  groups = g_slist_remove (groups, group);
  g_object_set_qdata (G_OBJECT (widget), quark_size_groups, groups);

  widget->priv->have_size_groups = groups != NULL;
}

GSList *
_gtk_widget_get_sizegroups (GtkWidget    *widget)
{
  if (widget->priv->have_size_groups)
    return g_object_get_qdata (G_OBJECT (widget), quark_size_groups);

  return NULL;
}

/**
 * gtk_widget_path_append_for_widget:
 * @path: a widget path
 * @widget: the widget to append to the widget path
 *
 * Appends the data from @widget to the widget hierarchy represented
 * by @path. This function is a shortcut for adding information from
 * @widget to the given @path. This includes setting the name or
 * adding the style classes from @widget.
 *
 * Returns: the position where the data was inserted
 *
 * Since: 3.2
 */
gint
gtk_widget_path_append_for_widget (GtkWidgetPath *path,
                                   GtkWidget     *widget)
{
  gint pos;

  g_return_val_if_fail (path != NULL, 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  pos = gtk_widget_path_append_type (path, G_OBJECT_TYPE (widget));

  if (widget->priv->name)
    gtk_widget_path_iter_set_name (path, pos, widget->priv->name);

  if (widget->priv->context)
    {
      GList *classes, *l;

      /* Also add any persistent classes in
       * the style context the widget path
       */
      classes = gtk_style_context_list_classes (widget->priv->context);

      for (l = classes; l; l = l->next)
        gtk_widget_path_iter_add_class (path, pos, l->data);

      g_list_free (classes);
    }

  return pos;
}

/**
 * gtk_widget_get_path:
 * @widget: a #GtkWidget
 *
 * Returns the #GtkWidgetPath representing @widget, if the widget
 * is not connected to a toplevel widget, a partial path will be
 * created.
 *
 * Returns: (transfer none): The #GtkWidgetPath representing @widget
 **/
GtkWidgetPath *
gtk_widget_get_path (GtkWidget *widget)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  /* As strange as it may seem, this may happen on object construction.
   * init() implementations of parent types may eventually call this function,
   * each with its corresponding GType, which could leave a child
   * implementation with a wrong widget type in the widget path
   */
  if (widget->priv->path &&
      G_OBJECT_TYPE (widget) != gtk_widget_path_get_object_type (widget->priv->path))
    {
      gtk_widget_path_free (widget->priv->path);
      widget->priv->path = NULL;
    }

  if (!widget->priv->path)
    {
      GtkWidget *parent;

      parent = widget->priv->parent;

      if (parent)
        widget->priv->path = gtk_container_get_path_for_child (GTK_CONTAINER (parent), widget);
      else
        {
          /* Widget is either toplevel or unparented, treat both
           * as toplevels style wise, since there are situations
           * where style properties might be retrieved on that
           * situation.
           */
          widget->priv->path = gtk_widget_path_new ();
    
          gtk_widget_path_append_for_widget (widget->priv->path, widget);
        }

      if (widget->priv->context)
        gtk_style_context_set_path (widget->priv->context,
                                    widget->priv->path);
    }

  return widget->priv->path;
}

static void
style_context_changed (GtkStyleContext *context,
                       gpointer         user_data)
{
  GtkWidget *widget = user_data;

  if (gtk_widget_get_realized (widget))
    g_signal_emit (widget, widget_signals[STYLE_UPDATED], 0);
  else
    {
      /* Compress all style updates so it
       * is only emitted once pre-realize.
       */
      widget->priv->style_update_pending = TRUE;
    }

  if (widget->priv->anchored)
    gtk_widget_queue_resize (widget);
}

/**
 * gtk_widget_get_style_context:
 * @widget: a #GtkWidget
 *
 * Returns the style context associated to @widget.
 *
 * Returns: (transfer none): a #GtkStyleContext. This memory is owned by @widget and
 *          must not be freed.
 **/
GtkStyleContext *
gtk_widget_get_style_context (GtkWidget *widget)
{
  GtkWidgetPrivate *priv;
  GtkWidgetPath *path;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  priv = widget->priv;
  
  /* updates style context if it exists already */
  path = gtk_widget_get_path (widget);

  if (G_UNLIKELY (priv->context == NULL))
    {
      GdkScreen *screen;

      priv->context = g_object_new (GTK_TYPE_STYLE_CONTEXT,
                                    "direction", gtk_widget_get_direction (widget),
                                    NULL);

      g_signal_connect (widget->priv->context, "changed",
                        G_CALLBACK (style_context_changed), widget);

      screen = gtk_widget_get_screen (widget);

      if (screen)
        gtk_style_context_set_screen (priv->context, screen);

      gtk_style_context_set_path (priv->context, path);
    }

  return widget->priv->context;
}
