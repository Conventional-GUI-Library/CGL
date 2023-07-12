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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdk.h>
#include <math.h>
#include <stdlib.h>
#include <gobject/gvaluecollector.h>

#include "gtkstylecontextprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkcssanimatedvaluesprivate.h"
#include "gtkcssenginevalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkstylepropertiesprivate.h"
#include "gtktypebuiltins.h"
#include "gtkthemingengineprivate.h"
#include "gtkintl.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtksymboliccolorprivate.h"
#include "gtkiconfactory.h"
#include "gtkwidgetpath.h"
#include "gtkwidgetprivate.h"
#include "gtkstylecascadeprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtksettings.h"

/**
 * SECTION:gtkstylecontext
 * @Short_description: Rendering UI elements
 * @Title: GtkStyleContext
 *
 * #GtkStyleContext is an object that stores styling information affecting
 * a widget defined by #GtkWidgetPath.
 *
 * In order to construct the final style information, #GtkStyleContext
 * queries information from all attached #GtkStyleProviders. Style providers
 * can be either attached explicitly to the context through
 * gtk_style_context_add_provider(), or to the screen through
 * gtk_style_context_add_provider_for_screen(). The resulting style is a
 * combination of all providers' information in priority order.
 *
 * For GTK+ widgets, any #GtkStyleContext returned by
 * gtk_widget_get_style_context() will already have a #GtkWidgetPath, a
 * #GdkScreen and RTL/LTR information set. The style context will be also
 * updated automatically if any of these settings change on the widget.
 *
 * If you are using the theming layer standalone, you will need to set a
 * widget path and a screen yourself to the created style context through
 * gtk_style_context_set_path() and gtk_style_context_set_screen(), as well
 * as updating the context yourself using gtk_style_context_invalidate()
 * whenever any of the conditions change, such as a change in the
 * #GtkSettings:gtk-theme-name setting or a hierarchy change in the rendered
 * widget.
 *
 * <refsect2 id="gtkstylecontext-animations">
 * <title>Transition animations</title>
 * <para>
 * #GtkStyleContext has built-in support for state change transitions.
 * Note that these animations respect the #GtkSettings:gtk-enable-animations
 * setting.
 * </para>
 * <para>
 * For simple widgets where state changes affect the whole widget area,
 * calling gtk_style_context_notify_state_change() with a %NULL region
 * is sufficient to trigger the transition animation. And GTK+ already
 * does that when gtk_widget_set_state() or gtk_widget_set_state_flags()
 * are called.
 * </para>
 * <para>
 * If a widget needs to declare several animatable regions (i.e. not
 * affecting the whole widget area), its #GtkWidget::draw signal handler
 * needs to wrap the render operations for the different regions with
 * calls to gtk_style_context_push_animatable_region() and
 * gtk_style_context_pop_animatable_region(). These functions take an
 * identifier for the region which must be unique within the style context.
 * For simple widgets with a fixed set of animatable regions, using an
 * enumeration works well:
 * </para>
 * <example>
 * <title>Using an enumeration to identify  animatable regions</title>
 * <programlisting>
 * enum {
 *   REGION_ENTRY,
 *   REGION_BUTTON_UP,
 *   REGION_BUTTON_DOWN
 * };
 *
 * ...
 *
 * gboolean
 * spin_button_draw (GtkWidget *widget,
 *                   cairo_t   *cr)
 * {
 *   GtkStyleContext *context;
 *
 *   context = gtk_widget_get_style_context (widget);
 *
 *   gtk_style_context_push_animatable_region (context,
 *                                             GUINT_TO_POINTER (REGION_ENTRY));
 *
 *   gtk_render_background (cr, 0, 0, 100, 30);
 *   gtk_render_frame (cr, 0, 0, 100, 30);
 *
 *   gtk_style_context_pop_animatable_region (context);
 *
 *   ...
 * }
 * </programlisting>
 * </example>
 * <para>
 * For complex widgets with an arbitrary number of animatable regions, it
 * is up to the implementation to come up with a way to uniquely identify
 * each animatable region. Using pointers to internal structs is one way
 * to achieve this:
 * </para>
 * <example>
 * <title>Using struct pointers to identify animatable regions</title>
 * <programlisting>
 * void
 * notebook_draw_tab (GtkWidget    *widget,
 *                    NotebookPage *page,
 *                    cairo_t      *cr)
 * {
 *   gtk_style_context_push_animatable_region (context, page);
 *   gtk_render_extension (cr, page->x, page->y, page->width, page->height);
 *   gtk_style_context_pop_animatable_region (context);
 * }
 * </programlisting>
 * </example>
 * <para>
 * The widget also needs to notify the style context about a state change
 * for a given animatable region so the animation is triggered.
 * </para>
 * <example>
 * <title>Triggering a state change animation on a region</title>
 * <programlisting>
 * gboolean
 * notebook_motion_notify (GtkWidget      *widget,
 *                         GdkEventMotion *event)
 * {
 *   GtkStyleContext *context;
 *   NotebookPage *page;
 *
 *   context = gtk_widget_get_style_context (widget);
 *   page = find_page_under_pointer (widget, event);
 *   gtk_style_context_notify_state_change (context,
 *                                          gtk_widget_get_window (widget),
 *                                          page,
 *                                          GTK_STATE_PRELIGHT,
 *                                          TRUE);
 *   ...
 * }
 * </programlisting>
 * </example>
 * <para>
 * gtk_style_context_notify_state_change() accepts %NULL region IDs as a
 * special value, in this case, the whole widget area will be updated
 * by the animation.
 * </para>
 * </refsect2>
 * <refsect2 id="gtkstylecontext-classes">
 * <title>Style classes and regions</title>
 * <para>
 * Widgets can add style classes to their context, which can be used
 * to associate different styles by class (see <xref linkend="gtkcssprovider-selectors"/>). Theme engines can also use style classes to vary their
 * rendering. GTK+ has a number of predefined style classes:
 * #GTK_STYLE_CLASS_CELL,
 * #GTK_STYLE_CLASS_ENTRY,
 * #GTK_STYLE_CLASS_BUTTON,
 * #GTK_STYLE_CLASS_COMBOBOX_ENTRY,
 * #GTK_STYLE_CLASS_CALENDAR,
 * #GTK_STYLE_CLASS_SLIDER,
 * #GTK_STYLE_CLASS_BACKGROUND,
 * #GTK_STYLE_CLASS_RUBBERBAND,
 * #GTK_STYLE_CLASS_TOOLTIP,
 * #GTK_STYLE_CLASS_MENU,
 * #GTK_STYLE_CLASS_MENUBAR,
 * #GTK_STYLE_CLASS_MENUITEM,
 * #GTK_STYLE_CLASS_TOOLBAR,
 * #GTK_STYLE_CLASS_PRIMARY_TOOLBAR,
 * #GTK_STYLE_CLASS_INLINE_TOOLBAR,
 * #GTK_STYLE_CLASS_RADIO,
 * #GTK_STYLE_CLASS_CHECK,
 * #GTK_STYLE_CLASS_TROUGH,
 * #GTK_STYLE_CLASS_SCROLLBAR,
 * #GTK_STYLE_CLASS_SCALE,
 * #GTK_STYLE_CLASS_SCALE_HAS_MARKS_ABOVE,
 * #GTK_STYLE_CLASS_SCALE_HAS_MARKS_BELOW,
 * #GTK_STYLE_CLASS_HEADER,
 * #GTK_STYLE_CLASS_ACCELERATOR,
 * #GTK_STYLE_CLASS_GRIP,
 * #GTK_STYLE_CLASS_DOCK,
 * #GTK_STYLE_CLASS_PROGRESSBAR,
 * #GTK_STYLE_CLASS_SPINNER,
 * #GTK_STYLE_CLASS_EXPANDER,
 * #GTK_STYLE_CLASS_SPINBUTTON,
 * #GTK_STYLE_CLASS_NOTEBOOK,
 * #GTK_STYLE_CLASS_VIEW,
 * #GTK_STYLE_CLASS_SIDEBAR,
 * #GTK_STYLE_CLASS_IMAGE,
 * #GTK_STYLE_CLASS_HIGHLIGHT,
 * #GTK_STYLE_CLASS_FRAME,
 * #GTK_STYLE_CLASS_DND,
 * #GTK_STYLE_CLASS_PANE_SEPARATOR,
 * #GTK_STYLE_CLASS_SEPARATOR,
 * #GTK_STYLE_CLASS_INFO,
 * #GTK_STYLE_CLASS_WARNING,
 * #GTK_STYLE_CLASS_QUESTION,
 * #GTK_STYLE_CLASS_ERROR,
 * #GTK_STYLE_CLASS_HORIZONTAL,
 * #GTK_STYLE_CLASS_VERTICAL,
 * #GTK_STYLE_CLASS_TOP,
 * #GTK_STYLE_CLASS_BOTTOM,
 * #GTK_STYLE_CLASS_LEFT,
 * #GTK_STYLE_CLASS_RIGHT,
 * </para>
 * <para>
 * Widgets can also add regions with flags to their context.
 * The regions used by GTK+ widgets are:
 * <informaltable>
 *   <tgroup cols="4">
 *     <thead>
 *       <row>
 *         <entry>Region</entry>
 *         <entry>Flags</entry>
 *         <entry>Macro</entry>
 *         <entry>Used by</entry>
 *       </row>
 *     </thead>
 *     <tbody>
 *       <row>
 *         <entry>row</entry>
 *         <entry>even, odd</entry>
 *         <entry>GTK_STYLE_REGION_ROW</entry>
 *         <entry>#GtkTreeView</entry>
 *       </row>
 *       <row>
 *         <entry>column</entry>
 *         <entry>first, last, sorted</entry>
 *         <entry>GTK_STYLE_REGION_COLUMN</entry>
 *         <entry>#GtkTreeView</entry>
 *       </row>
 *       <row>
 *         <entry>column-header</entry>
 *         <entry></entry>
 *         <entry>GTK_STYLE_REGION_COLUMN_HEADER</entry>
 *         <entry></entry>
 *       </row>
 *       <row>
 *         <entry>tab</entry>
 *         <entry>even, odd, first, last</entry>
 *         <entry>GTK_STYLE_REGION_TAB</entry>
 *         <entry>#GtkNotebook</entry>
 *       </row>
 *     </tbody>
 *   </tgroup>
 * </informaltable>
 * </para>
 * </refsect2>
 * <refsect2 id="gtkstylecontext-custom-styling">
 * <title>Custom styling in UI libraries and applications</title>
 * <para>
 * If you are developing a library with custom #GtkWidget<!-- -->s that
 * render differently than standard components, you may need to add a
 * #GtkStyleProvider yourself with the %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK
 * priority, either a #GtkCssProvider or a custom object implementing the
 * #GtkStyleProvider interface. This way theming engines may still attempt
 * to style your UI elements in a different way if needed so.
 * </para>
 * <para>
 * If you are using custom styling on an applications, you probably want then
 * to make your style information prevail to the theme's, so you must use
 * a #GtkStyleProvider with the %GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
 * priority, keep in mind that the user settings in
 * <filename><replaceable>XDG_CONFIG_HOME</replaceable>/gtk-3.0/gtk.css</filename> will
 * still take precedence over your changes, as it uses the
 * %GTK_STYLE_PROVIDER_PRIORITY_USER priority.
 * </para>
 * <para>
 * If a custom theming engine is needed, you probably want to implement a
 * #GtkStyleProvider yourself so it points to your #GtkThemingEngine
 * implementation, as #GtkCssProvider uses gtk_theming_engine_load()
 * which loads the theming engine module from the standard paths.
 * </para>
 * </refsect2>
 */

/* When these change we do a full restyling. Otherwise we try to figure out
 * if we need to change things. */
#define GTK_STYLE_CONTEXT_RADICAL_CHANGE (GTK_CSS_CHANGE_NAME | GTK_CSS_CHANGE_CLASS | GTK_CSS_CHANGE_SOURCE)
/* When these change we don't clear the cache. This takes more memory but makes
 * things go faster. */
#define GTK_STYLE_CONTEXT_CACHED_CHANGE (GTK_CSS_CHANGE_STATE)

typedef struct GtkStyleInfo GtkStyleInfo;
typedef struct GtkRegion GtkRegion;
typedef struct PropertyValue PropertyValue;
typedef struct StyleData StyleData;

struct GtkRegion
{
  GQuark class_quark;
  GtkRegionFlags flags;
};

struct PropertyValue
{
  GType       widget_type;
  GParamSpec *pspec;
  GtkStateFlags state;
  GValue      value;
};

struct GtkStyleInfo
{
  GtkStyleInfo *next;
  GArray *style_classes;
  GArray *regions;
  GtkJunctionSides junction_sides;
  GtkStateFlags state_flags;
  StyleData *data;
};

struct StyleData
{
  GtkCssComputedValues *store;
  GArray *property_cache;
  guint ref_count;
};

struct _GtkStyleContextPrivate
{
  GdkScreen *screen;

  GtkStyleCascade *cascade;

  GtkStyleContext *animation_list_prev;
  GtkStyleContext *animation_list_next;

  GtkStyleContext *parent;
  GSList *children;
  GtkWidget *widget;            
  GtkWidgetPath *widget_path;
  GHashTable *style_data;
  GtkStyleInfo *info;

  GtkTextDirection direction;

  GtkCssChange relevant_changes;
  GtkCssChange pending_changes;

  guint invalidating_context : 1;
  guint invalid : 1;
};

enum {
  PROP_0,
  PROP_SCREEN,
  PROP_DIRECTION,
  PROP_PARENT
};

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GtkStyleContext *_running_animations = NULL;
guint _running_animations_timer_id = 0;

static void gtk_style_context_finalize (GObject *object);

static void gtk_style_context_impl_set_property (GObject      *object,
                                                 guint         prop_id,
                                                 const GValue *value,
                                                 GParamSpec   *pspec);
static void gtk_style_context_impl_get_property (GObject      *object,
                                                 guint         prop_id,
                                                 GValue       *value,
                                                 GParamSpec   *pspec);
static GtkSymbolicColor *
            gtk_style_context_color_lookup_func (gpointer      contextp,
                                                 const char   *name);


G_DEFINE_TYPE (GtkStyleContext, gtk_style_context, G_TYPE_OBJECT)

static void
gtk_style_context_real_changed (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (priv->widget)
    _gtk_widget_style_context_invalidated (priv->widget);
}

static void
gtk_style_context_class_init (GtkStyleContextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_context_finalize;
  object_class->set_property = gtk_style_context_impl_set_property;
  object_class->get_property = gtk_style_context_impl_get_property;

  klass->changed = gtk_style_context_real_changed;

  signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkStyleContextClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_object_class_install_property (object_class,
                                   PROP_SCREEN,
                                   g_param_spec_object ("screen",
                                                        P_("Screen"),
                                                        P_("The associated GdkScreen"),
                                                        GDK_TYPE_SCREEN,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_DIRECTION,
                                   g_param_spec_enum ("direction",
                                                      P_("Direction"),
                                                      P_("Text direction"),
                                                      GTK_TYPE_TEXT_DIRECTION,
                                                      GTK_TEXT_DIR_LTR,
                                                      GTK_PARAM_READWRITE));
  /**
   * GtkStyleContext:parent:
   *
   * Sets or gets the style context's parent. See gtk_style_context_set_parent()
   * for details.
   *
   * Since: 3.4
   */
  g_object_class_install_property (object_class,
                                   PROP_PARENT,
                                   g_param_spec_object ("parent",
                                                        P_("Parent"),
                                                        P_("The parent style context"),
                                                        GTK_TYPE_STYLE_CONTEXT,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GtkStyleContextPrivate));
}

static StyleData *
style_data_new (void)
{
  StyleData *data;

  data = g_slice_new0 (StyleData);
  data->ref_count = 1;

  return data;
}

static void
clear_property_cache (StyleData *data)
{
  guint i;

  if (!data->property_cache)
    return;

  for (i = 0; i < data->property_cache->len; i++)
    {
      PropertyValue *node = &g_array_index (data->property_cache, PropertyValue, i);

      g_param_spec_unref (node->pspec);
      g_value_unset (&node->value);
    }

  g_array_free (data->property_cache, TRUE);
  data->property_cache = NULL;
}

static StyleData *
style_data_ref (StyleData *style_data)
{
  style_data->ref_count++;

  return style_data;
}

static void
style_data_unref (StyleData *data)
{
  data->ref_count--;

  if (data->ref_count > 0)
    return;

  g_object_unref (data->store);
  clear_property_cache (data);

  g_slice_free (StyleData, data);
}

static gboolean
style_data_is_animating (StyleData *style_data)
{
  return GTK_IS_CSS_ANIMATED_VALUES (style_data->store);
}

static GtkStyleInfo *
style_info_new (void)
{
  GtkStyleInfo *info;

  info = g_slice_new0 (GtkStyleInfo);
  info->style_classes = g_array_new (FALSE, FALSE, sizeof (GQuark));
  info->regions = g_array_new (FALSE, FALSE, sizeof (GtkRegion));

  return info;
}

static void
style_info_set_data (GtkStyleInfo *info,
                     StyleData    *data)
{
  if (info->data == data)
    return;

  if (data)
    style_data_ref (data);

  if (info->data)
    style_data_unref (info->data);

  info->data = data;
}

static void
style_info_free (GtkStyleInfo *info)
{
  style_info_set_data (info, NULL);
  g_array_free (info->style_classes, TRUE);
  g_array_free (info->regions, TRUE);
  g_slice_free (GtkStyleInfo, info);
}

static GtkStyleInfo *
style_info_pop (GtkStyleInfo *info)
{
  GtkStyleInfo *next = info->next;

  style_info_free (info);

  return next;
}

static GtkStyleInfo *
style_info_copy (GtkStyleInfo *info)
{
  GtkStyleInfo *copy;

  copy = style_info_new ();
  g_array_insert_vals (copy->style_classes, 0,
                       info->style_classes->data,
                       info->style_classes->len);

  g_array_insert_vals (copy->regions, 0,
                       info->regions->data,
                       info->regions->len);

  copy->next = info;
  copy->junction_sides = info->junction_sides;
  copy->state_flags = info->state_flags;
  style_info_set_data (copy, info->data);

  return copy;
}

static guint
style_info_hash (gconstpointer elem)
{
  const GtkStyleInfo *info;
  guint i, hash = 0;

  info = elem;

  for (i = 0; i < info->style_classes->len; i++)
    {
      hash += g_array_index (info->style_classes, GQuark, i);
      hash <<= 5;
    }

  for (i = 0; i < info->regions->len; i++)
    {
      GtkRegion *region;

      region = &g_array_index (info->regions, GtkRegion, i);
      hash += region->class_quark;
      hash += region->flags;
      hash <<= 5;
    }

  return hash ^ info->state_flags;
}

static gboolean
style_info_equal (gconstpointer elem1,
                  gconstpointer elem2)
{
  const GtkStyleInfo *info1, *info2;

  info1 = elem1;
  info2 = elem2;

  if (info1->state_flags != info2->state_flags)
    return FALSE;

  if (info1->junction_sides != info2->junction_sides)
    return FALSE;

  if (info1->style_classes->len != info2->style_classes->len)
    return FALSE;

  if (memcmp (info1->style_classes->data,
              info2->style_classes->data,
              info1->style_classes->len * sizeof (GQuark)) != 0)
    return FALSE;

  if (info1->regions->len != info2->regions->len)
    return FALSE;

  if (memcmp (info1->regions->data,
              info2->regions->data,
              info1->regions->len * sizeof (GtkRegion)) != 0)
    return FALSE;

  return TRUE;
}

static void
gtk_style_context_cascade_changed (GtkStyleCascade *cascade,
                                   GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (priv->widget)
    _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_SOURCE);
  else
    gtk_style_context_invalidate (context);
}

static void
gtk_style_context_set_cascade (GtkStyleContext *context,
                               GtkStyleCascade *cascade)
{
  GtkStyleContextPrivate *priv;

  priv = context->priv;

  if (priv->cascade == cascade)
    return;

  if (cascade)
    {
      g_object_ref (cascade);
      g_signal_connect (cascade,
                        "-gtk-private-changed",
                        G_CALLBACK (gtk_style_context_cascade_changed),
                        context);
    }

  if (priv->cascade)
    {
      g_signal_handlers_disconnect_by_func (priv->cascade, 
                                            gtk_style_context_cascade_changed,
                                            context);
      g_object_unref (priv->cascade);
    }

  priv->cascade = cascade;

  if (cascade)
    gtk_style_context_cascade_changed (cascade, context);
}

static void
gtk_style_context_init (GtkStyleContext *style_context)
{
  GtkStyleContextPrivate *priv;

  priv = style_context->priv = G_TYPE_INSTANCE_GET_PRIVATE (style_context,
                                                            GTK_TYPE_STYLE_CONTEXT,
                                                            GtkStyleContextPrivate);

  priv->style_data = g_hash_table_new_full (style_info_hash,
                                            style_info_equal,
                                            (GDestroyNotify) style_info_free,
                                            (GDestroyNotify) style_data_unref);

  priv->direction = GTK_TEXT_DIR_LTR;

  priv->screen = gdk_screen_get_default ();
  priv->relevant_changes = GTK_CSS_CHANGE_ANY;

  /* Create default info store */
  priv->info = style_info_new ();

  gtk_style_context_set_cascade (style_context,
                                 _gtk_style_cascade_get_for_screen (priv->screen));
}

static gboolean
gtk_style_context_do_animations (gpointer unused)
{
  GtkStyleContext *context;

  for (context = _running_animations;
       context != NULL;
       context = context->priv->animation_list_next)
    {
      _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_ANIMATE);
    }

  return TRUE;
}

static gboolean
gtk_style_context_is_animating (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  return priv->animation_list_prev != NULL
      || _running_animations == context;
}

static void
gtk_style_context_stop_animating (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (!gtk_style_context_is_animating (context))
    return;

  if (priv->animation_list_prev == NULL)
    {
      _running_animations = priv->animation_list_next;

      if (_running_animations == NULL)
        {
          /* we were the last animation */
          g_source_remove (_running_animations_timer_id);
          _running_animations_timer_id = 0;
        }
    }
  else
    priv->animation_list_prev->priv->animation_list_next = priv->animation_list_next;

  if (priv->animation_list_next)
    priv->animation_list_next->priv->animation_list_prev = priv->animation_list_prev;

  priv->animation_list_next = NULL;
  priv->animation_list_prev = NULL;
}

static void
gtk_style_context_start_animating (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv = context->priv;

  if (gtk_style_context_is_animating (context))
    return;

  if (_running_animations == NULL)
    {
      _running_animations_timer_id = gdk_threads_add_timeout (25,
                                                              gtk_style_context_do_animations,
                                                              NULL);
      _running_animations = context;
    }
  else
    {
      priv->animation_list_next = _running_animations;
      _running_animations->priv->animation_list_prev = context;
      _running_animations = context;
    }
}

static void
gtk_style_context_finalize (GObject *object)
{
  GtkStyleContextPrivate *priv;
  GtkStyleContext *style_context;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  _gtk_style_context_stop_animations (style_context);

  /* children hold a reference to us */
  g_assert (priv->children == NULL);

  gtk_style_context_set_parent (style_context, NULL);

  gtk_style_context_set_cascade (style_context, NULL);

  if (priv->widget_path)
    gtk_widget_path_free (priv->widget_path);

  g_hash_table_destroy (priv->style_data);

  while (priv->info)
    priv->info = style_info_pop (priv->info);

  G_OBJECT_CLASS (gtk_style_context_parent_class)->finalize (object);
}

static void
gtk_style_context_impl_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkStyleContext *style_context;

  style_context = GTK_STYLE_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_SCREEN:
      gtk_style_context_set_screen (style_context,
                                    g_value_get_object (value));
      break;
    case PROP_DIRECTION:
      gtk_style_context_set_direction (style_context,
                                       g_value_get_enum (value));
      break;
    case PROP_PARENT:
      gtk_style_context_set_parent (style_context,
                                    g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_style_context_impl_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkStyleContext *style_context;
  GtkStyleContextPrivate *priv;

  style_context = GTK_STYLE_CONTEXT (object);
  priv = style_context->priv;

  switch (prop_id)
    {
    case PROP_SCREEN:
      g_value_set_object (value, priv->screen);
      break;
    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
      break;
    case PROP_PARENT:
      g_value_set_object (value, priv->parent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
build_properties (GtkStyleContext *context,
                  StyleData       *style_data,
                  GtkWidgetPath   *path,
                  GtkStateFlags    state)
{
  GtkStyleContextPrivate *priv;
  GtkCssMatcher matcher;
  GtkCssLookup *lookup;

  priv = context->priv;

  lookup = _gtk_css_lookup_new ();

  if (_gtk_css_matcher_init (&matcher, path, state))
    _gtk_style_provider_private_lookup (GTK_STYLE_PROVIDER_PRIVATE (priv->cascade),
                                        &matcher,
                                        lookup);

  style_data->store = _gtk_css_computed_values_new ();
  _gtk_css_lookup_resolve (lookup, context, style_data->store);
  _gtk_css_lookup_free (lookup);
}

static GtkWidgetPath *
create_query_path (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkWidgetPath *path;
  GtkStyleInfo *info;
  guint i, pos;

  priv = context->priv;
  path = priv->widget ? _gtk_widget_create_path (priv->widget) : gtk_widget_path_copy (priv->widget_path);
  pos = gtk_widget_path_length (path) - 1;

  info = priv->info;

  /* Set widget regions */
  for (i = 0; i < info->regions->len; i++)
    {
      GtkRegion *region;

      region = &g_array_index (info->regions, GtkRegion, i);
      gtk_widget_path_iter_add_region (path, pos,
                                       g_quark_to_string (region->class_quark),
                                       region->flags);
    }

  /* Set widget classes */
  for (i = 0; i < info->style_classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (info->style_classes, GQuark, i);
      gtk_widget_path_iter_add_class (path, pos,
                                      g_quark_to_string (quark));
    }

  return path;
}

static StyleData *
style_data_lookup (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkWidgetPath *path;
  GtkStyleInfo *info;
  StyleData *data;

  priv = context->priv;
  info = priv->info;

  /* Current data in use is cached, just return it */
  if (info->data)
    return info->data;

  g_assert (priv->widget != NULL || priv->widget_path != NULL);

  data = g_hash_table_lookup (priv->style_data, info);
  if (data)
    {
      style_info_set_data (info, data);
      return data;
    }

  path = create_query_path (context);

  data = style_data_new ();
  style_info_set_data (info, data);
  g_hash_table_insert (priv->style_data,
                       style_info_copy (info),
                       data);

  build_properties (context, data, path, info->state_flags);

  gtk_widget_path_free (path);

  return data;
}

static StyleData *
style_data_lookup_for_state (GtkStyleContext *context,
                             GtkStateFlags    state)
{
  StyleData *data;

  if (context->priv->info->state_flags == state)
    return style_data_lookup (context);

  gtk_style_context_save (context);
  gtk_style_context_set_state (context, state);
  data = style_data_lookup (context);
  gtk_style_context_restore (context);

  return data;
}

static void
gtk_style_context_set_invalid (GtkStyleContext *context,
                               gboolean         invalid)
{
  GtkStyleContextPrivate *priv;
  
  priv = context->priv;

  if (priv->invalid == invalid)
    return;

  priv->invalid = invalid;

  if (invalid)
    {
      if (priv->parent)
        gtk_style_context_set_invalid (priv->parent, TRUE);
      else if (GTK_IS_RESIZE_CONTAINER (priv->widget))
        _gtk_container_queue_restyle (GTK_CONTAINER (priv->widget));
    }
}

/* returns TRUE if someone called gtk_style_context_save() but hasn't
 * called gtk_style_context_restore() yet.
 * In those situations we don't invalidate the context when somebody
 * changes state/regions/classes.
 */
static gboolean
gtk_style_context_is_saved (GtkStyleContext *context)
{
  return context->priv->info->next != NULL;
}

static void
gtk_style_context_queue_invalidate_internal (GtkStyleContext *context,
                                             GtkCssChange     change)
{
  GtkStyleContextPrivate *priv = context->priv;
  GtkStyleInfo *info = priv->info;

  if (gtk_style_context_is_saved (context))
    {
      style_info_set_data (info, NULL);
    }
  else
    {
      _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_STATE);
      /* XXX: We need to invalidate siblings here somehow */
    }
}

/**
 * gtk_style_context_new:
 *
 * Creates a standalone #GtkStyleContext, this style context
 * won't be attached to any widget, so you may want
 * to call gtk_style_context_set_path() yourself.
 *
 * <note>
 * This function is only useful when using the theming layer
 * separated from GTK+, if you are using #GtkStyleContext to
 * theme #GtkWidget<!-- -->s, use gtk_widget_get_style_context()
 * in order to get a style context ready to theme the widget.
 * </note>
 *
 * Returns: A newly created #GtkStyleContext.
 **/
GtkStyleContext *
gtk_style_context_new (void)
{
  return g_object_new (GTK_TYPE_STYLE_CONTEXT, NULL);
}

void
_gtk_style_context_set_widget (GtkStyleContext *context,
                               GtkWidget       *widget)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (widget == NULL || GTK_IS_WIDGET (widget));

  context->priv->widget = widget;

  _gtk_style_context_stop_animations (context);

  _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_ANY_SELF);
}

/**
 * gtk_style_context_add_provider:
 * @context: a #GtkStyleContext
 * @provider: a #GtkStyleProvider
 * @priority: the priority of the style provider. The lower
 *            it is, the earlier it will be used in the style
 *            construction. Typically this will be in the range
 *            between %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK and
 *            %GTK_STYLE_PROVIDER_PRIORITY_USER
 *
 * Adds a style provider to @context, to be used in style construction.
 *
 * <note><para>If both priorities are the same, A #GtkStyleProvider
 * added through this function takes precedence over another added
 * through gtk_style_context_add_provider_for_screen().</para></note>
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_provider (GtkStyleContext  *context,
                                GtkStyleProvider *provider,
                                guint             priority)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  priv = context->priv;

  if (priv->cascade == _gtk_style_cascade_get_for_screen (priv->screen))
    {
      GtkStyleCascade *new_cascade;
      
      new_cascade = _gtk_style_cascade_new ();
      _gtk_style_cascade_set_parent (new_cascade, priv->cascade);
      _gtk_style_cascade_add_provider (new_cascade, provider, priority);
      gtk_style_context_set_cascade (context, new_cascade);
      g_object_unref (new_cascade);
    }
  else
    {
      _gtk_style_cascade_add_provider (priv->cascade, provider, priority);
    }
}

/**
 * gtk_style_context_remove_provider:
 * @context: a #GtkStyleContext
 * @provider: a #GtkStyleProvider
 *
 * Removes @provider from the style providers list in @context.
 *
 * Since: 3.0
 **/
void
gtk_style_context_remove_provider (GtkStyleContext  *context,
                                   GtkStyleProvider *provider)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  priv = context->priv;

  if (priv->cascade == _gtk_style_cascade_get_for_screen (priv->screen))
    return;

  _gtk_style_cascade_remove_provider (priv->cascade, provider);
}

/**
 * gtk_style_context_reset_widgets:
 * @screen: a #GdkScreen
 *
 * This function recomputes the styles for all widgets under a particular
 * #GdkScreen. This is useful when some global parameter has changed that
 * affects the appearance of all widgets, because when a widget gets a new
 * style, it will both redraw and recompute any cached information about
 * its appearance. As an example, it is used when the color scheme changes
 * in the related #GtkSettings object.
 *
 * Since: 3.0
 **/
void
gtk_style_context_reset_widgets (GdkScreen *screen)
{
  GList *list, *toplevels;

  _gtk_icon_set_invalidate_caches ();

  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc) g_object_ref, NULL);

  for (list = toplevels; list; list = list->next)
    {
      if (gtk_widget_get_screen (list->data) == screen)
        gtk_widget_reset_style (list->data);

      g_object_unref (list->data);
    }

  g_list_free (toplevels);
}

/**
 * gtk_style_context_add_provider_for_screen:
 * @screen: a #GdkScreen
 * @provider: a #GtkStyleProvider
 * @priority: the priority of the style provider. The lower
 *            it is, the earlier it will be used in the style
 *            construction. Typically this will be in the range
 *            between %GTK_STYLE_PROVIDER_PRIORITY_FALLBACK and
 *            %GTK_STYLE_PROVIDER_PRIORITY_USER
 *
 * Adds a global style provider to @screen, which will be used
 * in style construction for all #GtkStyleContext<!-- -->s under
 * @screen.
 *
 * GTK+ uses this to make styling information from #GtkSettings
 * available.
 *
 * <note><para>If both priorities are the same, A #GtkStyleProvider
 * added through gtk_style_context_add_provider() takes precedence
 * over another added through this function.</para></note>
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_provider_for_screen (GdkScreen        *screen,
                                           GtkStyleProvider *provider,
                                           guint             priority)
{
  GtkStyleCascade *cascade;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  cascade = _gtk_style_cascade_get_for_screen (screen);
  _gtk_style_cascade_add_provider (cascade, provider, priority);
}

/**
 * gtk_style_context_remove_provider_for_screen:
 * @screen: a #GdkScreen
 * @provider: a #GtkStyleProvider
 *
 * Removes @provider from the global style providers list in @screen.
 *
 * Since: 3.0
 **/
void
gtk_style_context_remove_provider_for_screen (GdkScreen        *screen,
                                              GtkStyleProvider *provider)
{
  GtkStyleCascade *cascade;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GTK_IS_STYLE_PROVIDER (provider));

  cascade = _gtk_style_cascade_get_for_screen (screen);
  _gtk_style_cascade_remove_provider (cascade, provider);
}

/**
 * gtk_style_context_get_section:
 * @context: a #GtkStyleContext
 * @property: style property name
 *
 * Queries the location in the CSS where @property was defined for the
 * current @context. Note that the state to be queried is taken from
 * gtk_style_context_get_state().
 *
 * If the location is not available, %NULL will be returned. The
 * location might not be available for various reasons, such as the
 * property being overridden, @property not naming a supported CSS
 * property or tracking of definitions being disabled for performance
 * reasons.
 *
 * Shorthand CSS properties cannot be queried for a location and will
 * always return %NULL.
 *
 * Returns: %NULL or the section where value was defined
 **/
GtkCssSection *
gtk_style_context_get_section (GtkStyleContext *context,
                               const gchar     *property)
{
  GtkStyleContextPrivate *priv;
  GtkStyleProperty *prop;
  StyleData *data;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (property != NULL, NULL);

  priv = context->priv;
  g_return_val_if_fail (priv->widget != NULL || priv->widget_path != NULL, NULL);

  prop = _gtk_style_property_lookup (property);
  if (!GTK_IS_CSS_STYLE_PROPERTY (prop))
    return NULL;

  data = style_data_lookup (context);
  return _gtk_css_computed_values_get_section (data->store, _gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (prop)));
}

static GtkCssValue *
gtk_style_context_query_func (guint    id,
                              gpointer values)
{
  return _gtk_css_computed_values_get_value (values, id);
}

/**
 * gtk_style_context_get_property:
 * @context: a #GtkStyleContext
 * @property: style property name
 * @state: state to retrieve the property value for
 * @value: (out) (transfer full):  return location for the style property value
 *
 * Gets a style property from @context for the given state.
 *
 * When @value is no longer needed, g_value_unset() must be called
 * to free any allocated memory.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_property (GtkStyleContext *context,
                                const gchar     *property,
                                GtkStateFlags    state,
                                GValue          *value)
{
  GtkStyleContextPrivate *priv;
  GtkStyleProperty *prop;
  StyleData *data;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property != NULL);
  g_return_if_fail (value != NULL);

  priv = context->priv;
  g_return_if_fail (priv->widget != NULL || priv->widget_path != NULL);

  prop = _gtk_style_property_lookup (property);
  if (prop == NULL)
    {
      g_warning ("Style property \"%s\" is not registered", property);
      return;
    }
  if (_gtk_style_property_get_value_type (prop) == G_TYPE_NONE)
    {
      g_warning ("Style property \"%s\" is not gettable", property);
      return;
    }

  data = style_data_lookup_for_state (context, state);
  _gtk_style_property_query (prop, value, gtk_style_context_query_func, data->store);
}

/**
 * gtk_style_context_get_valist:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the property values for
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several style property values from @context for a given state.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_valist (GtkStyleContext *context,
                              GtkStateFlags    state,
                              va_list          args)
{
  const gchar *property_name;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  property_name = va_arg (args, const gchar *);

  while (property_name)
    {
      gchar *error = NULL;
      GValue value = G_VALUE_INIT;

      gtk_style_context_get_property (context,
                                      property_name,
                                      state,
                                      &value);

      G_VALUE_LCOPY (&value, args, 0, &error);
      g_value_unset (&value);

      if (error)
        {
          g_warning ("Could not get style property \"%s\": %s", property_name, error);
          g_free (error);
          break;
        }

      property_name = va_arg (args, const gchar *);
    }
}

/**
 * gtk_style_context_get:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the property values for
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several style property values from @context for a
 * given state.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get (GtkStyleContext *context,
                       GtkStateFlags    state,
                       ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  va_start (args, state);
  gtk_style_context_get_valist (context, state, args);
  va_end (args);
}

/**
 * gtk_style_context_set_state:
 * @context: a #GtkStyleContext
 * @flags: state to represent
 *
 * Sets the state to be used when rendering with any
 * of the gtk_render_*() functions.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_state (GtkStyleContext *context,
                             GtkStateFlags    flags)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  context->priv->info->state_flags = flags;
  
  gtk_style_context_queue_invalidate_internal (context, GTK_CSS_CHANGE_STATE);
}

/**
 * gtk_style_context_get_state:
 * @context: a #GtkStyleContext
 *
 * Returns the state used when rendering.
 *
 * Returns: the state flags
 *
 * Since: 3.0
 **/
GtkStateFlags
gtk_style_context_get_state (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return context->priv->info->state_flags;
}

/**
 * gtk_style_context_state_is_running:
 * @context: a #GtkStyleContext
 * @state: a widget state
 * @progress: (out): return location for the transition progress
 *
 * Returns %TRUE if there is a transition animation running for the
 * current region (see gtk_style_context_push_animatable_region()).
 *
 * If @progress is not %NULL, the animation progress will be returned
 * there, 0.0 means the state is closest to being unset, while 1.0 means
 * it's closest to being set. This means transition animation will
 * run from 0 to 1 when @state is being set and from 1 to 0 when
 * it's being unset.
 *
 * Returns: %TRUE if there is a running transition animation for @state.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function always returns %FALSE
 **/
gboolean
gtk_style_context_state_is_running (GtkStyleContext *context,
                                    GtkStateType     state,
                                    gdouble         *progress)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);

  return FALSE;
}

/**
 * gtk_style_context_set_path:
 * @context: a #GtkStyleContext
 * @path: a #GtkWidgetPath
 *
 * Sets the #GtkWidgetPath used for style matching. As a
 * consequence, the style will be regenerated to match
 * the new given path.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to call
 * this yourself.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_path (GtkStyleContext *context,
                            GtkWidgetPath   *path)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (path != NULL);

  priv = context->priv;
  g_return_if_fail (priv->widget == NULL);

  if (priv->widget_path)
    {
      gtk_widget_path_free (priv->widget_path);
      priv->widget_path = NULL;
    }

  if (path)
    priv->widget_path = gtk_widget_path_copy (path);

  _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_ANY);
}

/**
 * gtk_style_context_get_path:
 * @context: a #GtkStyleContext
 *
 * Returns the widget path used for style matching.
 *
 * Returns: (transfer none): A #GtkWidgetPath
 *
 * Since: 3.0
 **/
const GtkWidgetPath *
gtk_style_context_get_path (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  priv = context->priv;
  if (priv->widget)
    return gtk_widget_get_path (priv->widget);
  else
    return priv->widget_path;
}

/**
 * gtk_style_context_set_parent:
 * @context: a #GtkStyleContext
 * @parent: (allow-none): the new parent or %NULL
 *
 * Sets the parent style context for @context. The parent style
 * context is used to implement
 * <ulink url="http://www.w3.org/TR/css3-cascade/#inheritance">inheritance</ulink>
 * of properties.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), the parent will be set for you.
 *
 * Since: 3.4
 **/
void
gtk_style_context_set_parent (GtkStyleContext *context,
                              GtkStyleContext *parent)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (parent == NULL || GTK_IS_STYLE_CONTEXT (parent));

  priv = context->priv;

  if (priv->parent == parent)
    return;

  if (parent)
    {
      parent->priv->children = g_slist_prepend (parent->priv->children, context);
      g_object_ref (parent);
      if (priv->invalid)
        gtk_style_context_set_invalid (parent, TRUE);
    }

  if (priv->parent)
    {
      priv->parent->priv->children = g_slist_remove (priv->parent->priv->children, context);
      g_object_unref (priv->parent);
    }

  priv->parent = parent;

  g_object_notify (G_OBJECT (context), "parent");
  _gtk_style_context_queue_invalidate (context, GTK_CSS_CHANGE_ANY_PARENT | GTK_CSS_CHANGE_ANY_SIBLING);
}

/**
 * gtk_style_context_get_parent:
 * @context: a #GtkStyleContext
 *
 * Gets the parent context set via gtk_style_context_set_parent().
 * See that function for details.
 *
 * Returns: (transfer none): the parent context or %NULL
 *
 * Since: 3.4
 **/
GtkStyleContext *
gtk_style_context_get_parent (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  return context->priv->parent;
}

/**
 * gtk_style_context_save:
 * @context: a #GtkStyleContext
 *
 * Saves the @context state, so all modifications done through
 * gtk_style_context_add_class(), gtk_style_context_remove_class(),
 * gtk_style_context_add_region(), gtk_style_context_remove_region()
 * or gtk_style_context_set_junction_sides() can be reverted in one
 * go through gtk_style_context_restore().
 *
 * Since: 3.0
 **/
void
gtk_style_context_save (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  priv->info = style_info_copy (priv->info);
  /* Need to unset animations here because we can not know what style
   * class potential transitions came from once we save().
   */
  if (priv->info->data && style_data_is_animating (priv->info->data))
    style_info_set_data (priv->info, NULL);
}

/**
 * gtk_style_context_restore:
 * @context: a #GtkStyleContext
 *
 * Restores @context state to a previous stage.
 * See gtk_style_context_save().
 *
 * Since: 3.0
 **/
void
gtk_style_context_restore (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  priv->info = style_info_pop (priv->info);

  if (!priv->info)
    {
      g_warning ("Unpaired gtk_style_context_restore() call");

      /* Create default region */
      priv->info = style_info_new ();
    }
}

static gboolean
style_class_find (GArray *array,
                  GQuark  class_quark,
                  guint  *position)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint pos;

  if (position)
    *position = 0;

  if (!array || array->len == 0)
    return FALSE;

  min = 0;
  max = array->len - 1;

  do
    {
      GQuark item;

      mid = (min + max) / 2;
      item = g_array_index (array, GQuark, mid);

      if (class_quark == item)
        {
          found = TRUE;
          pos = mid;
        }
      else if (class_quark > item)
        min = pos = mid + 1;
      else
        {
          max = mid - 1;
          pos = mid;
        }
    }
  while (!found && min <= max);

  if (position)
    *position = pos;

  return found;
}

static gboolean
region_find (GArray *array,
             GQuark  class_quark,
             guint  *position)
{
  gint min, max, mid;
  gboolean found = FALSE;
  guint pos;

  if (position)
    *position = 0;

  if (!array || array->len == 0)
    return FALSE;

  min = 0;
  max = array->len - 1;

  do
    {
      GtkRegion *region;

      mid = (min + max) / 2;
      region = &g_array_index (array, GtkRegion, mid);

      if (region->class_quark == class_quark)
        {
          found = TRUE;
          pos = mid;
        }
      else if (region->class_quark > class_quark)
        min = pos = mid + 1;
      else
        {
          max = mid - 1;
          pos = mid;
        }
    }
  while (!found && min <= max);

  if (position)
    *position = pos;

  return found;
}

/**
 * gtk_style_context_add_class:
 * @context: a #GtkStyleContext
 * @class_name: class name to use in styling
 *
 * Adds a style class to @context, so posterior calls to
 * gtk_style_context_get() or any of the gtk_render_*()
 * functions will make use of this new class for styling.
 *
 * In the CSS file format, a #GtkEntry defining an "entry"
 * class, would be matched by:
 *
 * <programlisting>
 * GtkEntry.entry { ... }
 * </programlisting>
 *
 * While any widget defining an "entry" class would be
 * matched by:
 * <programlisting>
 * .entry { ... }
 * </programlisting>
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  priv = context->priv;
  class_quark = g_quark_from_string (class_name);

  info = priv->info;

  if (!style_class_find (info->style_classes, class_quark, &position))
    {
      g_array_insert_val (info->style_classes, position, class_quark);

      gtk_style_context_queue_invalidate_internal (context, GTK_CSS_CHANGE_CLASS);
    }
}

/**
 * gtk_style_context_remove_class:
 * @context: a #GtkStyleContext
 * @class_name: class name to remove
 *
 * Removes @class_name from @context.
 *
 * Since: 3.0
 **/
void
gtk_style_context_remove_class (GtkStyleContext *context,
                                const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (class_name != NULL);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return;

  priv = context->priv;

  info = priv->info;

  if (style_class_find (info->style_classes, class_quark, &position))
    {
      g_array_remove_index (info->style_classes, position);

      gtk_style_context_queue_invalidate_internal (context, GTK_CSS_CHANGE_CLASS);
    }
}

/**
 * gtk_style_context_has_class:
 * @context: a #GtkStyleContext
 * @class_name: a class name
 *
 * Returns %TRUE if @context currently has defined the
 * given class name
 *
 * Returns: %TRUE if @context has @class_name defined
 *
 * Since: 3.0
 **/
gboolean
gtk_style_context_has_class (GtkStyleContext *context,
                             const gchar     *class_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark class_quark;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (class_name != NULL, FALSE);

  class_quark = g_quark_try_string (class_name);

  if (!class_quark)
    return FALSE;

  priv = context->priv;

  info = priv->info;

  if (style_class_find (info->style_classes, class_quark, NULL))
    return TRUE;

  return FALSE;
}

/**
 * gtk_style_context_list_classes:
 * @context: a #GtkStyleContext
 *
 * Returns the list of classes currently defined in @context.
 *
 * Returns: (transfer container) (element-type utf8): a #GList of
 *          strings with the currently defined classes. The contents
 *          of the list are owned by GTK+, but you must free the list
 *          itself with g_list_free() when you are done with it.
 *
 * Since: 3.0
 **/
GList *
gtk_style_context_list_classes (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GList *classes = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;

  info = priv->info;

  for (i = 0; i < info->style_classes->len; i++)
    {
      GQuark quark;

      quark = g_array_index (info->style_classes, GQuark, i);
      classes = g_list_prepend (classes, (gchar *) g_quark_to_string (quark));
    }

  return classes;
}

/**
 * gtk_style_context_list_regions:
 * @context: a #GtkStyleContext
 *
 * Returns the list of regions currently defined in @context.
 *
 * Returns: (transfer container) (element-type utf8): a #GList of
 *          strings with the currently defined regions. The contents
 *          of the list are owned by GTK+, but you must free the list
 *          itself with g_list_free() when you are done with it.
 *
 * Since: 3.0
 **/
GList *
gtk_style_context_list_regions (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GList *classes = NULL;
  guint i;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;

  info = priv->info;

  for (i = 0; i < info->regions->len; i++)
    {
      GtkRegion *region;
      const gchar *class_name;

      region = &g_array_index (info->regions, GtkRegion, i);

      class_name = g_quark_to_string (region->class_quark);
      classes = g_list_prepend (classes, (gchar *) class_name);
    }

  return classes;
}

gboolean
_gtk_style_context_check_region_name (const gchar *str)
{
  g_return_val_if_fail (str != NULL, FALSE);

  if (!g_ascii_islower (str[0]))
    return FALSE;

  while (*str)
    {
      if (*str != '-' &&
          !g_ascii_islower (*str))
        return FALSE;

      str++;
    }

  return TRUE;
}

/**
 * gtk_style_context_add_region:
 * @context: a #GtkStyleContext
 * @region_name: region name to use in styling
 * @flags: flags that apply to the region
 *
 * Adds a region to @context, so posterior calls to
 * gtk_style_context_get() or any of the gtk_render_*()
 * functions will make use of this new region for styling.
 *
 * In the CSS file format, a #GtkTreeView defining a "row"
 * region, would be matched by:
 *
 * <programlisting>
 * GtkTreeView row { ... }
 * </programlisting>
 *
 * Pseudo-classes are used for matching @flags, so the two
 * following rules:
 * <programlisting>
 * GtkTreeView row:nth-child(even) { ... }
 * GtkTreeView row:nth-child(odd) { ... }
 * </programlisting>
 *
 * would apply to even and odd rows, respectively.
 *
 * <note><para>Region names must only contain lowercase letters
 * and '-', starting always with a lowercase letter.</para></note>
 *
 * Since: 3.0
 **/
void
gtk_style_context_add_region (GtkStyleContext *context,
                              const gchar     *region_name,
                              GtkRegionFlags   flags)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark region_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_name != NULL);
  g_return_if_fail (_gtk_style_context_check_region_name (region_name));

  priv = context->priv;
  region_quark = g_quark_from_string (region_name);

  info = priv->info;

  if (!region_find (info->regions, region_quark, &position))
    {
      GtkRegion region;

      region.class_quark = region_quark;
      region.flags = flags;

      g_array_insert_val (info->regions, position, region);

      gtk_style_context_queue_invalidate_internal (context, GTK_CSS_CHANGE_REGION);
    }
}

/**
 * gtk_style_context_remove_region:
 * @context: a #GtkStyleContext
 * @region_name: region name to unset
 *
 * Removes a region from @context.
 *
 * Since: 3.0
 **/
void
gtk_style_context_remove_region (GtkStyleContext *context,
                                 const gchar     *region_name)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark region_quark;
  guint position;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_name != NULL);

  region_quark = g_quark_try_string (region_name);

  if (!region_quark)
    return;

  priv = context->priv;

  info = priv->info;

  if (region_find (info->regions, region_quark, &position))
    {
      g_array_remove_index (info->regions, position);

      gtk_style_context_queue_invalidate_internal (context, GTK_CSS_CHANGE_REGION);
    }
}

/**
 * gtk_style_context_has_region:
 * @context: a #GtkStyleContext
 * @region_name: a region name
 * @flags_return: (out) (allow-none): return location for region flags
 *
 * Returns %TRUE if @context has the region defined.
 * If @flags_return is not %NULL, it is set to the flags
 * affecting the region.
 *
 * Returns: %TRUE if region is defined
 *
 * Since: 3.0
 **/
gboolean
gtk_style_context_has_region (GtkStyleContext *context,
                              const gchar     *region_name,
                              GtkRegionFlags  *flags_return)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;
  GQuark region_quark;
  guint position;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (region_name != NULL, FALSE);

  if (flags_return)
    *flags_return = 0;

  region_quark = g_quark_try_string (region_name);

  if (!region_quark)
    return FALSE;

  priv = context->priv;

  info = priv->info;

  if (region_find (info->regions, region_quark, &position))
    {
      if (flags_return)
        {
          GtkRegion *region;

          region = &g_array_index (info->regions, GtkRegion, position);
          *flags_return = region->flags;
        }
      return TRUE;
    }

  return FALSE;
}

static gint
style_property_values_cmp (gconstpointer bsearch_node1,
                           gconstpointer bsearch_node2)
{
  const PropertyValue *val1 = bsearch_node1;
  const PropertyValue *val2 = bsearch_node2;

  if (val1->widget_type != val2->widget_type)
    return val1->widget_type < val2->widget_type ? -1 : 1;

  if (val1->pspec != val2->pspec)
    return val1->pspec < val2->pspec ? -1 : 1;

  if (val1->state != val2->state)
    return val1->state < val2->state ? -1 : 1;

  return 0;
}

GtkCssValue *
_gtk_style_context_peek_property (GtkStyleContext *context,
                                  guint            property_id)
{
  StyleData *data = style_data_lookup (context);

  return _gtk_css_computed_values_get_value (data->store, property_id);
}

double
_gtk_style_context_get_number (GtkStyleContext *context,
                               guint            property_id,
                               double           one_hundred_percent)
{
  GtkCssValue *value;
  
  value = _gtk_style_context_peek_property (context, property_id);
  return _gtk_css_number_value_get (value, one_hundred_percent);
}

const GValue *
_gtk_style_context_peek_style_property (GtkStyleContext *context,
                                        GType            widget_type,
                                        GtkStateFlags    state,
                                        GParamSpec      *pspec)
{
  GtkStyleContextPrivate *priv;
  PropertyValue *pcache, key = { 0 };
  StyleData *data;
  guint i;

  priv = context->priv;

  data = style_data_lookup_for_state (context, state);

  key.widget_type = widget_type;
  key.state = state;
  key.pspec = pspec;

  /* need value cache array */
  if (!data->property_cache)
    data->property_cache = g_array_new (FALSE, FALSE, sizeof (PropertyValue));
  else
    {
      pcache = bsearch (&key,
                        data->property_cache->data, data->property_cache->len,
                        sizeof (PropertyValue), style_property_values_cmp);
      if (pcache)
        return &pcache->value;
    }

  i = 0;
  while (i < data->property_cache->len &&
         style_property_values_cmp (&key, &g_array_index (data->property_cache, PropertyValue, i)) >= 0)
    i++;

  g_array_insert_val (data->property_cache, i, key);
  pcache = &g_array_index (data->property_cache, PropertyValue, i);

  /* cache miss, initialize value type, then set contents */
  g_param_spec_ref (pcache->pspec);
  g_value_init (&pcache->value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  if (priv->widget || priv->widget_path)
    {
      GtkWidgetPath *widget_path = priv->widget ? _gtk_widget_create_path (priv->widget) : priv->widget_path;

      if (gtk_style_provider_get_style_property (GTK_STYLE_PROVIDER (priv->cascade),
                                                 widget_path,
                                                 state, pspec, &pcache->value))
        {
          /* Resolve symbolic colors to GdkColor/GdkRGBA */
          if (G_VALUE_TYPE (&pcache->value) == GTK_TYPE_SYMBOLIC_COLOR)
            {
              GtkSymbolicColor *color;
              GdkRGBA rgba;

              color = g_value_dup_boxed (&pcache->value);

              g_value_unset (&pcache->value);

              if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_RGBA)
                g_value_init (&pcache->value, GDK_TYPE_RGBA);
              else
                g_value_init (&pcache->value, GDK_TYPE_COLOR);

              if (_gtk_style_context_resolve_color (context, color, &rgba))
                {
                  if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_RGBA)
                    g_value_set_boxed (&pcache->value, &rgba);
                  else
                    {
                      GdkColor rgb;

                      rgb.red = rgba.red * 65535. + 0.5;
                      rgb.green = rgba.green * 65535. + 0.5;
                      rgb.blue = rgba.blue * 65535. + 0.5;

                      g_value_set_boxed (&pcache->value, &rgb);
                    }
                }
              else
                g_param_value_set_default (pspec, &pcache->value);

              gtk_symbolic_color_unref (color);
            }

          if (priv->widget)
            gtk_widget_path_free (widget_path);

          return &pcache->value;
        }

      if (priv->widget)
        gtk_widget_path_free (widget_path);
    }

  /* not supplied by any provider, revert to default */
  g_param_value_set_default (pspec, &pcache->value);

  return &pcache->value;
}

/**
 * gtk_style_context_get_style_property:
 * @context: a #GtkStyleContext
 * @property_name: the name of the widget style property
 * @value: Return location for the property value
 *
 * Gets the value for a widget style property.
 *
 * When @value is no longer needed, g_value_unset() must be called
 * to free any allocated memory.
 **/
void
gtk_style_context_get_style_property (GtkStyleContext *context,
                                      const gchar     *property_name,
                                      GValue          *value)
{
  GtkStyleContextPrivate *priv;
  GtkWidgetClass *widget_class;
  GtkStateFlags state;
  GParamSpec *pspec;
  const GValue *peek_value;
  GType widget_type;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (value != NULL);

  priv = context->priv;

  if (priv->widget)
    {
      widget_type = G_OBJECT_TYPE (priv->widget);
    }
  else
    {
      if (!priv->widget_path)
        return;

      widget_type = gtk_widget_path_get_object_type (priv->widget_path);

      if (!g_type_is_a (widget_type, GTK_TYPE_WIDGET))
        {
          g_warning ("%s: can't get style properties for non-widget class `%s'",
                     G_STRLOC,
                     g_type_name (widget_type));
          return;
        }
    }

  widget_class = g_type_class_ref (widget_type);
  pspec = gtk_widget_class_find_style_property (widget_class, property_name);
  g_type_class_unref (widget_class);

  if (!pspec)
    {
      g_warning ("%s: widget class `%s' has no style property named `%s'",
                 G_STRLOC,
                 g_type_name (widget_type),
                 property_name);
      return;
    }

  state = gtk_style_context_get_state (context);
  peek_value = _gtk_style_context_peek_style_property (context, widget_type,
                                                       state, pspec);

  if (G_VALUE_TYPE (value) == G_VALUE_TYPE (peek_value))
    g_value_copy (peek_value, value);
  else if (g_value_type_transformable (G_VALUE_TYPE (peek_value), G_VALUE_TYPE (value)))
    g_value_transform (peek_value, value);
  else
    g_warning ("can't retrieve style property `%s' of type `%s' as value of type `%s'",
               pspec->name,
               G_VALUE_TYPE_NAME (peek_value),
               G_VALUE_TYPE_NAME (value));
}

/**
 * gtk_style_context_get_style_valist:
 * @context: a #GtkStyleContext
 * @args: va_list of property name/return location pairs, followed by %NULL
 *
 * Retrieves several widget style properties from @context according to the
 * current style.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_style_valist (GtkStyleContext *context,
                                    va_list          args)
{
  GtkStyleContextPrivate *priv;
  const gchar *prop_name;
  GtkStateFlags state;
  GType widget_type;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  prop_name = va_arg (args, const gchar *);
  priv = context->priv;

  if (priv->widget)
    {
      widget_type = G_OBJECT_TYPE (priv->widget);
    }
  else
    {
      if (!priv->widget_path)
        return;

      widget_type = gtk_widget_path_get_object_type (priv->widget_path);

      if (!g_type_is_a (widget_type, GTK_TYPE_WIDGET))
        {
          g_warning ("%s: can't get style properties for non-widget class `%s'",
                     G_STRLOC,
                     g_type_name (widget_type));
          return;
        }
    }

  state = gtk_style_context_get_state (context);

  while (prop_name)
    {
      GtkWidgetClass *widget_class;
      GParamSpec *pspec;
      const GValue *peek_value;
      gchar *error;

      widget_class = g_type_class_ref (widget_type);
      pspec = gtk_widget_class_find_style_property (widget_class, prop_name);
      g_type_class_unref (widget_class);

      if (!pspec)
        {
          g_warning ("%s: widget class `%s' has no style property named `%s'",
                     G_STRLOC,
                     g_type_name (widget_type),
                     prop_name);
          break;
        }

      peek_value = _gtk_style_context_peek_style_property (context, widget_type,
                                                           state, pspec);

      G_VALUE_LCOPY (peek_value, args, 0, &error);

      if (error)
        {
          g_warning ("can't retrieve style property `%s' of type `%s': %s",
                     pspec->name,
                     G_VALUE_TYPE_NAME (peek_value),
                     error);
          g_free (error);
          break;
        }

      prop_name = va_arg (args, const gchar *);
    }
}

/**
 * gtk_style_context_get_style:
 * @context: a #GtkStyleContext
 * @...: property name /return value pairs, followed by %NULL
 *
 * Retrieves several widget style properties from @context according to the
 * current style.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_style (GtkStyleContext *context,
                             ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  va_start (args, context);
  gtk_style_context_get_style_valist (context, args);
  va_end (args);
}


/**
 * gtk_style_context_lookup_icon_set:
 * @context: a #GtkStyleContext
 * @stock_id: an icon name
 *
 * Looks up @stock_id in the icon factories associated to @context and
 * the default icon factory, returning an icon set if found, otherwise
 * %NULL.
 *
 * Returns: (transfer none): The looked  up %GtkIconSet, or %NULL
 **/
GtkIconSet *
gtk_style_context_lookup_icon_set (GtkStyleContext *context,
                                   const gchar     *stock_id)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (stock_id != NULL, NULL);

  priv = context->priv;
  g_return_val_if_fail (priv->widget != NULL || priv->widget_path != NULL, NULL);

  return gtk_icon_factory_lookup_default (stock_id);
}

/**
 * gtk_style_context_set_screen:
 * @context: a #GtkStyleContext
 * @screen: a #GdkScreen
 *
 * Attaches @context to the given screen.
 *
 * The screen is used to add style information from 'global' style
 * providers, such as the screens #GtkSettings instance.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to
 * call this yourself.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_screen (GtkStyleContext *context,
                              GdkScreen       *screen)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_SCREEN (screen));

  priv = context->priv;
  if (priv->screen == screen)
    return;

  if (priv->cascade == _gtk_style_cascade_get_for_screen (priv->screen))
    {
      gtk_style_context_set_cascade (context, _gtk_style_cascade_get_for_screen (screen));
    }
  else
    {
      _gtk_style_cascade_set_parent (priv->cascade, _gtk_style_cascade_get_for_screen (screen));
    }

  priv->screen = screen;

  g_object_notify (G_OBJECT (context), "screen");

  gtk_style_context_invalidate (context);
}

/**
 * gtk_style_context_get_screen:
 * @context: a #GtkStyleContext
 *
 * Returns the #GdkScreen to which @context is attached.
 *
 * Returns: (transfer none): a #GdkScreen.
 **/
GdkScreen *
gtk_style_context_get_screen (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;
  return priv->screen;
}

/**
 * gtk_style_context_set_direction:
 * @context: a #GtkStyleContext
 * @direction: the new direction.
 *
 * Sets the reading direction for rendering purposes.
 *
 * If you are using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to
 * call this yourself.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_direction (GtkStyleContext  *context,
                                 GtkTextDirection  direction)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;
  priv->direction = direction;

  g_object_notify (G_OBJECT (context), "direction");
}

/**
 * gtk_style_context_get_direction:
 * @context: a #GtkStyleContext
 *
 * Returns the widget direction used for rendering.
 *
 * Returns: the widget direction
 *
 * Since: 3.0
 **/
GtkTextDirection
gtk_style_context_get_direction (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), GTK_TEXT_DIR_LTR);

  priv = context->priv;
  return priv->direction;
}

/**
 * gtk_style_context_set_junction_sides:
 * @context: a #GtkStyleContext
 * @sides: sides where rendered elements are visually connected to
 *     other elements
 *
 * Sets the sides where rendered elements (mostly through
 * gtk_render_frame()) will visually connect with other visual elements.
 *
 * This is merely a hint that may or may not be honored
 * by theming engines.
 *
 * Container widgets are expected to set junction hints as appropriate
 * for their children, so it should not normally be necessary to call
 * this function manually.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_junction_sides (GtkStyleContext  *context,
                                      GtkJunctionSides  sides)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  context->priv->info->junction_sides = sides;
}

/**
 * gtk_style_context_get_junction_sides:
 * @context: a #GtkStyleContext
 *
 * Returns the sides where rendered elements connect visually with others.
 *
 * Returns: the junction sides
 *
 * Since: 3.0
 **/
GtkJunctionSides
gtk_style_context_get_junction_sides (GtkStyleContext *context)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), 0);

  return context->priv->info->junction_sides;
}

static GtkSymbolicColor *
gtk_style_context_color_lookup_func (gpointer    contextp,
                                     const char *name)
{
  GtkStyleContext *context = contextp;

  return _gtk_style_provider_private_get_color (GTK_STYLE_PROVIDER_PRIVATE (context->priv->cascade), name);
}

GtkCssValue *
_gtk_style_context_resolve_color_value (GtkStyleContext  *context,
                                        GtkCssValue      *current,
					GtkCssValue      *color)
{
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (current != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  return _gtk_symbolic_color_resolve_full ((GtkSymbolicColor *) color,
                                           current,
                                           gtk_style_context_color_lookup_func,
                                           context);
}


gboolean
_gtk_style_context_resolve_color (GtkStyleContext  *context,
                                  GtkSymbolicColor *color,
                                  GdkRGBA          *result)
{
  GtkCssValue *val;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color != NULL, FALSE);
  g_return_val_if_fail (result != NULL, FALSE);

  val = _gtk_symbolic_color_resolve_full (color,
                                          _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR),
					  gtk_style_context_color_lookup_func,
					  context);
  if (val == NULL)
    return FALSE;

  *result = *_gtk_css_rgba_value_get_rgba (val);
  _gtk_css_value_unref (val);
  return TRUE;
}

/**
 * gtk_style_context_lookup_color:
 * @context: a #GtkStyleContext
 * @color_name: color name to lookup
 * @color: (out): Return location for the looked up color
 *
 * Looks up and resolves a color name in the @context color map.
 *
 * Returns: %TRUE if @color_name was found and resolved, %FALSE otherwise
 **/
gboolean
gtk_style_context_lookup_color (GtkStyleContext *context,
                                const gchar     *color_name,
                                GdkRGBA         *color)
{
  GtkSymbolicColor *sym_color;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), FALSE);
  g_return_val_if_fail (color_name != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  sym_color = gtk_style_context_color_lookup_func (context, color_name);
  if (sym_color == NULL)
    return FALSE;

  return _gtk_style_context_resolve_color (context, sym_color, color);
}

/**
 * gtk_style_context_notify_state_change:
 * @context: a #GtkStyleContext
 * @window: a #GdkWindow
 * @region_id: (allow-none): animatable region to notify on, or %NULL.
 *     See gtk_style_context_push_animatable_region()
 * @state: state to trigger transition for
 * @state_value: %TRUE if @state is the state we are changing to,
 *     %FALSE if we are changing away from it
 *
 * Notifies a state change on @context, so if the current style makes use
 * of transition animations, one will be started so all rendered elements
 * under @region_id are animated for state @state being set to value
 * @state_value.
 *
 * The @window parameter is used in order to invalidate the rendered area
 * as the animation runs, so make sure it is the same window that is being
 * rendered on by the gtk_render_*() functions.
 *
 * If @region_id is %NULL, all rendered elements using @context will be
 * affected by this state transition.
 *
 * As a practical example, a #GtkButton notifying a state transition on
 * the prelight state:
 * <programlisting>
 * gtk_style_context_notify_state_change (context,
 *                                        gtk_widget_get_window (widget),
 *                                        NULL,
 *                                        GTK_STATE_PRELIGHT,
 *                                        button->in_button);
 * </programlisting>
 *
 * Can be handled in the CSS file like this:
 * <programlisting>
 * GtkButton {
 *     background-color: &num;f00
 * }
 *
 * GtkButton:hover {
 *     background-color: &num;fff;
 *     transition: 200ms linear
 * }
 * </programlisting>
 *
 * This combination will animate the button background from red to white
 * if a pointer enters the button, and back to red if the pointer leaves
 * the button.
 *
 * Note that @state is used when finding the transition parameters, which
 * is why the style places the transition under the :hover pseudo-class.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_notify_state_change (GtkStyleContext *context,
                                       GdkWindow       *window,
                                       gpointer         region_id,
                                       GtkStateType     state,
                                       gboolean         state_value)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (state > GTK_STATE_NORMAL && state <= GTK_STATE_FOCUSED);
  g_return_if_fail (context->priv->widget != NULL || context->priv->widget_path != NULL);
}

/**
 * gtk_style_context_cancel_animations:
 * @context: a #GtkStyleContext
 * @region_id: (allow-none): animatable region to stop, or %NULL.
 *     See gtk_style_context_push_animatable_region()
 *
 * Stops all running animations for @region_id and all animatable
 * regions underneath.
 *
 * A %NULL @region_id will stop all ongoing animations in @context,
 * when dealing with a #GtkStyleContext obtained through
 * gtk_widget_get_style_context(), this is normally done for you
 * in all circumstances you would expect all widget to be stopped,
 * so this should be only used in complex widgets with different
 * animatable regions.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_cancel_animations (GtkStyleContext *context,
                                     gpointer         region_id)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
}

/**
 * gtk_style_context_scroll_animations:
 * @context: a #GtkStyleContext
 * @window: a #GdkWindow used previously in
 *          gtk_style_context_notify_state_change()
 * @dx: Amount to scroll in the X axis
 * @dy: Amount to scroll in the Y axis
 *
 * This function is analogous to gdk_window_scroll(), and
 * should be called together with it so the invalidation
 * areas for any ongoing animation are scrolled together
 * with it.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_scroll_animations (GtkStyleContext *context,
                                     GdkWindow       *window,
                                     gint             dx,
                                     gint             dy)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));
}

/**
 * gtk_style_context_push_animatable_region:
 * @context: a #GtkStyleContext
 * @region_id: unique identifier for the animatable region
 *
 * Pushes an animatable region, so all further gtk_render_*() calls between
 * this call and the following gtk_style_context_pop_animatable_region()
 * will potentially show transition animations for this region if
 * gtk_style_context_notify_state_change() is called for a given state,
 * and the current theme/style defines transition animations for state
 * changes.
 *
 * The @region_id used must be unique in @context so the theming engine
 * can uniquely identify rendered elements subject to a state transition.
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_push_animatable_region (GtkStyleContext *context,
                                          gpointer         region_id)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (region_id != NULL);
}

/**
 * gtk_style_context_pop_animatable_region:
 * @context: a #GtkStyleContext
 *
 * Pops an animatable region from @context.
 * See gtk_style_context_push_animatable_region().
 *
 * Since: 3.0
 *
 * Deprecated: 3.6: This function does nothing.
 **/
void
gtk_style_context_pop_animatable_region (GtkStyleContext *context)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
}

static void
gtk_style_context_clear_cache (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  GtkStyleInfo *info;

  priv = context->priv;

  for (info = priv->info; info; info = info->next)
    {
      style_info_set_data (info, NULL);
    }
  g_hash_table_remove_all (priv->style_data);
}

static void
gtk_style_context_do_invalidate (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  /* Avoid reentrancy */
  if (priv->invalidating_context)
    return;

  priv->invalidating_context = TRUE;

  g_signal_emit (context, signals[CHANGED], 0);

  priv->invalidating_context = FALSE;
}

void
_gtk_style_context_stop_animations (GtkStyleContext *context)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  if (!gtk_style_context_is_animating (context))
    return;

  style_info_set_data (context->priv->info, NULL);

  gtk_style_context_stop_animating (context);
}

static GtkBitmask *
gtk_style_context_update_animations (GtkStyleContext *context,
                                     gint64           timestamp)
{
  GtkBitmask *differences;
  StyleData *style_data;
  
  style_data = style_data_lookup (context);

  differences = _gtk_css_animated_values_advance (GTK_CSS_ANIMATED_VALUES (style_data->store),
                                                  timestamp);

  if (_gtk_css_animated_values_is_finished (GTK_CSS_ANIMATED_VALUES (style_data->store)))
    _gtk_style_context_stop_animations (context);

  return differences;
}

static gboolean
gtk_style_context_should_animate (GtkStyleContext *context)
{
  GtkStyleContextPrivate *priv;
  gboolean animate;

  priv = context->priv;

  if (priv->widget == NULL)
    return FALSE;

  if (!gtk_widget_get_mapped (priv->widget))
    return FALSE;

  g_object_get (gtk_widget_get_settings (context->priv->widget),
                "gtk-enable-animations", &animate,
                NULL);

  return animate;
}

static void
gtk_style_context_start_animations (GtkStyleContext      *context,
                                    GtkCssComputedValues *previous,
                                    gint64                timestamp)
{
  StyleData *animated;

  if (!gtk_style_context_should_animate (context))
    {
      gtk_style_context_stop_animating (context);
      return;
    }

  animated = style_data_new ();
  animated->store = _gtk_css_animated_values_new (style_data_lookup (context)->store,
                                                  previous,
                                                  timestamp);

  if (_gtk_css_animated_values_is_finished (GTK_CSS_ANIMATED_VALUES (animated->store)))
    {
      style_data_unref (animated);
      gtk_style_context_stop_animating (context);
      return;
    }

  style_info_set_data (context->priv->info, animated);
  style_data_unref (animated);
  gtk_style_context_start_animating (context);
}

void
_gtk_style_context_validate (GtkStyleContext *context,
                             gint64           timestamp,
                             GtkCssChange     change)
{
  GtkStyleContextPrivate *priv;
  GtkBitmask *changes;
  GSList *list;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  priv = context->priv;

  change |= priv->pending_changes;

  if (!priv->invalid && change == 0)
    return;

  priv->pending_changes = 0;
  gtk_style_context_set_invalid (context, FALSE);

  /* Try to avoid invalidating if we can */
  if (change & GTK_STYLE_CONTEXT_RADICAL_CHANGE)
    {
      priv->relevant_changes = GTK_CSS_CHANGE_ANY;
    }
  else
    {
      if (priv->relevant_changes == GTK_CSS_CHANGE_ANY)
        {
          GtkWidgetPath *path;
          GtkCssMatcher matcher;

          path = create_query_path (context);
          if (_gtk_css_matcher_init (&matcher, path, priv->info->state_flags))
            priv->relevant_changes = _gtk_style_provider_private_get_change (GTK_STYLE_PROVIDER_PRIVATE (priv->cascade),
                                                                             &matcher);
          else
            priv->relevant_changes = 0;

          priv->relevant_changes &= ~GTK_STYLE_CONTEXT_RADICAL_CHANGE;

          gtk_widget_path_unref (path);
        }
    }

  if (priv->relevant_changes & change)
    {
      GtkStyleInfo *info = priv->info;
      StyleData *current;

      if (info->data)
        current = style_data_ref (info->data);
      else
        current = NULL;

      if ((priv->relevant_changes & change) & ~GTK_STYLE_CONTEXT_CACHED_CHANGE)
        gtk_style_context_clear_cache (context);
      else
        style_info_set_data (info, NULL);

      if (current)
        {
          StyleData *data;

          gtk_style_context_start_animations (context, current->store, timestamp);
          change &= ~GTK_CSS_CHANGE_ANIMATE;

          data = style_data_lookup (context);

          changes = _gtk_css_computed_values_get_difference (data->store, current->store);

          style_data_unref (current);
        }
      else
        {
          changes = _gtk_bitmask_new ();
          changes = _gtk_bitmask_invert_range (changes, 0, _gtk_css_style_property_get_n_properties ());
        }
    }
  else
    changes = _gtk_bitmask_new ();

  if (change & GTK_CSS_CHANGE_ANIMATE &&
      gtk_style_context_is_animating (context))
    {
      GtkBitmask *animation_changes;

      animation_changes = gtk_style_context_update_animations (context, timestamp);
      changes = _gtk_bitmask_union (changes, animation_changes);
      _gtk_bitmask_free (animation_changes);
    }

  if (!_gtk_bitmask_is_empty (changes))
    gtk_style_context_do_invalidate (context);

  change = _gtk_css_change_for_child (change);
  for (list = priv->children; list; list = list->next)
    {
      _gtk_style_context_validate (list->data, timestamp, change);
    }

  _gtk_bitmask_free (changes);
}

void
_gtk_style_context_queue_invalidate (GtkStyleContext *context,
                                     GtkCssChange     change)
{
  GtkStyleContextPrivate *priv;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (change != 0);

  priv = context->priv;

  if (priv->widget == NULL && priv->widget_path == NULL)
    return;

  priv->pending_changes |= change;
  gtk_style_context_set_invalid (context, TRUE);
}

/**
 * gtk_style_context_invalidate:
 * @context: a #GtkStyleContext.
 *
 * Invalidates @context style information, so it will be reconstructed
 * again.
 *
 * If you're using a #GtkStyleContext returned from
 * gtk_widget_get_style_context(), you do not need to
 * call this yourself.
 *
 * Since: 3.0
 **/
void
gtk_style_context_invalidate (GtkStyleContext *context)
{
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_clear_cache (context);
  gtk_style_context_do_invalidate (context);
}

/**
 * gtk_style_context_set_background:
 * @context: a #GtkStyleContext
 * @window: a #GdkWindow
 *
 * Sets the background of @window to the background pattern or
 * color specified in @context for its current state.
 *
 * Since: 3.0
 **/
void
gtk_style_context_set_background (GtkStyleContext *context,
                                  GdkWindow       *window)
{
  GtkStateFlags state;
  cairo_pattern_t *pattern;
  GdkRGBA *color;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (GDK_IS_WINDOW (window));

  state = gtk_style_context_get_state (context);
  gtk_style_context_get (context, state,
                         "background-image", &pattern,
                         NULL);
  if (pattern)
    {
      gdk_window_set_background_pattern (window, pattern);
      cairo_pattern_destroy (pattern);
      return;
    }

  gtk_style_context_get (context, state,
                         "background-color", &color,
                         NULL);
  if (color)
    {
      gdk_window_set_background_rgba (window, color);
      gdk_rgba_free (color);
    }
}

/**
 * gtk_style_context_get_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
 * @color: (out): return value for the foreground color
 *
 * Gets the foreground color for a given state.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_color (GtkStyleContext *context,
                             GtkStateFlags    state,
                             GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_background_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
 * @color: (out): return value for the background color
 *
 * Gets the background color for a given state.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_background_color (GtkStyleContext *context,
                                        GtkStateFlags    state,
                                        GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "background-color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_border_color:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the color for
 * @color: (out): return value for the border color
 *
 * Gets the border color for a given state.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_border_color (GtkStyleContext *context,
                                    GtkStateFlags    state,
                                    GdkRGBA         *color)
{
  GdkRGBA *c;

  g_return_if_fail (color != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "border-color", &c,
                         NULL);

  *color = *c;
  gdk_rgba_free (c);
}

/**
 * gtk_style_context_get_border:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the border for
 * @border: (out): return value for the border settings
 *
 * Gets the border for a given state as a #GtkBorder.
 * See %GTK_STYLE_PROPERTY_BORDER_WIDTH.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_border (GtkStyleContext *context,
                              GtkStateFlags    state,
                              GtkBorder       *border)
{
  int top, left, bottom, right;

  g_return_if_fail (border != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "border-top-width", &top,
                         "border-left-width", &left,
                         "border-bottom-width", &bottom,
                         "border-right-width", &right,
                         NULL);

  border->top = top;
  border->left = left;
  border->bottom = bottom;
  border->right = right;
}

/**
 * gtk_style_context_get_padding:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the padding for
 * @padding: (out): return value for the padding settings
 *
 * Gets the padding for a given state as a #GtkBorder.
 * See %GTK_STYLE_PROPERTY_PADDING.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_padding (GtkStyleContext *context,
                               GtkStateFlags    state,
                               GtkBorder       *padding)
{
  int top, left, bottom, right;

  g_return_if_fail (padding != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "padding-top", &top,
                         "padding-left", &left,
                         "padding-bottom", &bottom,
                         "padding-right", &right,
                         NULL);

  padding->top = top;
  padding->left = left;
  padding->bottom = bottom;
  padding->right = right;
}

/**
 * gtk_style_context_get_margin:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the border for
 * @margin: (out): return value for the margin settings
 *
 * Gets the margin for a given state as a #GtkBorder.
 * See %GTK_STYLE_PROPERTY_MARGIN.
 *
 * Since: 3.0
 **/
void
gtk_style_context_get_margin (GtkStyleContext *context,
                              GtkStateFlags    state,
                              GtkBorder       *margin)
{
  int top, left, bottom, right;

  g_return_if_fail (margin != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_style_context_get (context,
                         state,
                         "margin-top", &top,
                         "margin-left", &left,
                         "margin-bottom", &bottom,
                         "margin-right", &right,
                         NULL);

  margin->top = top;
  margin->left = left;
  margin->bottom = bottom;
  margin->right = right;
}

/**
 * gtk_style_context_get_font:
 * @context: a #GtkStyleContext
 * @state: state to retrieve the font for
 *
 * Returns the font description for a given state. The returned
 * object is const and will remain valid until the
 * #GtkStyleContext::changed signal happens.
 *
 * Returns: (transfer none): the #PangoFontDescription for the given
 *          state.  This object is owned by GTK+ and should not be
 *          freed.
 *
 * Since: 3.0
 **/
const PangoFontDescription *
gtk_style_context_get_font (GtkStyleContext *context,
                            GtkStateFlags    state)
{
  GtkStyleContextPrivate *priv;
  StyleData *data;
  PangoFontDescription *description;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  priv = context->priv;
  g_return_val_if_fail (priv->widget != NULL || priv->widget_path != NULL, NULL);

  data = style_data_lookup_for_state (context, state);

  /* Yuck, fonts are created on-demand but we don't return a ref.
   * Do bad things to achieve this requirement */
  description = g_object_get_data (G_OBJECT (data->store), "font-cache-for-get_font");
  if (description == NULL)
    {
      gtk_style_context_get (context, state, "font", &description, NULL);
      g_object_set_data_full (G_OBJECT (data->store),
                              "font-cache-for-get_font",
                              description,
                              (GDestroyNotify) pango_font_description_free);
    }
  return description;
}

static void
get_cursor_color (GtkStyleContext *context,
                  gboolean         primary,
                  GdkRGBA         *color)
{
  GdkColor *style_color;

  gtk_style_context_get_style (context,
                               primary ? "cursor-color" : "secondary-cursor-color",
                               &style_color,
                               NULL);

  if (style_color)
    {
      color->red = style_color->red / 65535.0;
      color->green = style_color->green / 65535.0;
      color->blue = style_color->blue / 65535.0;
      color->alpha = 1;

      gdk_color_free (style_color);
    }
  else
    {
      gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, color);

      if (!primary)
      {
        GdkRGBA bg;

        gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);

        color->red = (color->red + bg.red) * 0.5;
        color->green = (color->green + bg.green) * 0.5;
        color->blue = (color->blue + bg.blue) * 0.5;
      }
    }
}

void
_gtk_style_context_get_cursor_color (GtkStyleContext *context,
                                     GdkRGBA         *primary_color,
                                     GdkRGBA         *secondary_color)
{
  if (primary_color)
    get_cursor_color (context, TRUE, primary_color);

  if (secondary_color)
    get_cursor_color (context, FALSE, secondary_color);
}

/* Paint methods */

/**
 * gtk_render_check:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a checkmark (as in a #GtkCheckButton).
 *
 * The %GTK_STATE_FLAG_ACTIVE state determines whether the check is
 * on or off, and %GTK_STATE_FLAG_INCONSISTENT determines whether it
 * should be marked as undefined.
 *
 * <example>
 * <title>Typical checkmark rendering</title>
 * <inlinegraphic fileref="checks.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_check (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);


  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_check (engine, cr,
                              x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_option:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an option mark (as in a #GtkRadioButton), the %GTK_STATE_FLAG_ACTIVE
 * state will determine whether the option is on or off, and
 * %GTK_STATE_FLAG_INCONSISTENT whether it should be marked as undefined.
 *
 * <example>
 * <title>Typical option mark rendering</title>
 * <inlinegraphic fileref="options.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_option (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_option (engine, cr,
                               x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_arrow:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @angle: arrow angle from 0 to 2 * %G_PI, being 0 the arrow pointing to the north
 * @x: X origin of the render area
 * @y: Y origin of the render area
 * @size: square side for render area
 *
 * Renders an arrow pointing to @angle.
 *
 * <example>
 * <title>Typical arrow rendering at 0, 1&solidus;2 &pi;, &pi; and 3&solidus;2 &pi;</title>
 * <inlinegraphic fileref="arrows.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_arrow (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          angle,
                  gdouble          x,
                  gdouble          y,
                  gdouble          size)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (size <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_ARROW);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_arrow (engine, cr,
                              angle, x, y, size);

  gtk_style_context_restore (context);
  cairo_restore (cr);
}

/**
 * gtk_render_background:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders the background of an element.
 *
 * <example>
 * <title>Typical background rendering, showing the effect of
 * <parameter>background-image</parameter>,
 * <parameter>border-width</parameter> and
 * <parameter>border-radius</parameter></title>
 * <inlinegraphic fileref="background.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0.
 **/
void
gtk_render_background (GtkStyleContext *context,
                       cairo_t         *cr,
                       gdouble          x,
                       gdouble          y,
                       gdouble          width,
                       gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_background (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_frame:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a frame around the rectangle defined by @x, @y, @width, @height.
 *
 * <example>
 * <title>Examples of frame rendering, showing the effect of
 * <parameter>border-image</parameter>,
 * <parameter>border-color</parameter>,
 * <parameter>border-width</parameter>,
 * <parameter>border-radius</parameter> and
 * junctions</title>
 * <inlinegraphic fileref="frames.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_frame (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_frame (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_expander:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an expander (as used in #GtkTreeView and #GtkExpander) in the area
 * defined by @x, @y, @width, @height. The state %GTK_STATE_FLAG_ACTIVE
 * determines whether the expander is collapsed or expanded.
 *
 * <example>
 * <title>Typical expander rendering</title>
 * <inlinegraphic fileref="expanders.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_expander (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_expander (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_focus:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a focus indicator on the rectangle determined by @x, @y, @width, @height.
 * <example>
 * <title>Typical focus rendering</title>
 * <inlinegraphic fileref="focus.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_focus (GtkStyleContext *context,
                  cairo_t         *cr,
                  gdouble          x,
                  gdouble          y,
                  gdouble          width,
                  gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_focus (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_layout:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin
 * @y: Y origin
 * @layout: the #PangoLayout to render
 *
 * Renders @layout on the coordinates @x, @y
 *
 * Since: 3.0
 **/
void
gtk_render_layout (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   PangoLayout     *layout)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;
  PangoRectangle extents;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (cr != NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  pango_layout_get_extents (layout, &extents, NULL);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_layout (engine, cr, x, y, layout);

  cairo_restore (cr);
}

/**
 * gtk_render_line:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x0: X coordinate for the origin of the line
 * @y0: Y coordinate for the origin of the line
 * @x1: X coordinate for the end of the line
 * @y1: Y coordinate for the end of the line
 *
 * Renders a line from (x0, y0) to (x1, y1).
 *
 * Since: 3.0
 **/
void
gtk_render_line (GtkStyleContext *context,
                 cairo_t         *cr,
                 gdouble          x0,
                 gdouble          y0,
                 gdouble          x1,
                 gdouble          y1)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_line (engine, cr, x0, y0, x1, y1);

  cairo_restore (cr);
}

/**
 * gtk_render_slider:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @orientation: orientation of the slider
 *
 * Renders a slider (as in #GtkScale) in the rectangle defined by @x, @y,
 * @width, @height. @orientation defines whether the slider is vertical
 * or horizontal.
 *
 * <example>
 * <title>Typical slider rendering</title>
 * <inlinegraphic fileref="sliders.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_slider (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height,
                   GtkOrientation   orientation)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_slider (engine, cr, x, y, width, height, orientation);

  cairo_restore (cr);
}

/**
 * gtk_render_frame_gap:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @gap_side: side where the gap is
 * @xy0_gap: initial coordinate (X or Y depending on @gap_side) for the gap
 * @xy1_gap: end coordinate (X or Y depending on @gap_side) for the gap
 *
 * Renders a frame around the rectangle defined by (@x, @y, @width, @height),
 * leaving a gap on one side. @xy0_gap and @xy1_gap will mean X coordinates
 * for %GTK_POS_TOP and %GTK_POS_BOTTOM gap sides, and Y coordinates for
 * %GTK_POS_LEFT and %GTK_POS_RIGHT.
 *
 * <example>
 * <title>Typical rendering of a frame with a gap</title>
 * <inlinegraphic fileref="frame-gap.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_frame_gap (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side,
                      gdouble          xy0_gap,
                      gdouble          xy1_gap)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (xy0_gap <= xy1_gap);
  g_return_if_fail (xy0_gap >= 0);

  if (width <= 0 || height <= 0)
    return;

  if (gap_side == GTK_POS_LEFT ||
      gap_side == GTK_POS_RIGHT)
    g_return_if_fail (xy1_gap <= height);
  else
    g_return_if_fail (xy1_gap <= width);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_frame_gap (engine, cr,
                                  x, y, width, height, gap_side,
                                  xy0_gap, xy1_gap);

  cairo_restore (cr);
}

/**
 * gtk_render_extension:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 * @gap_side: side where the gap is
 *
 * Renders a extension (as in a #GtkNotebook tab) in the rectangle
 * defined by @x, @y, @width, @height. The side where the extension
 * connects to is defined by @gap_side.
 *
 * <example>
 * <title>Typical extension rendering</title>
 * <inlinegraphic fileref="extensions.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_extension (GtkStyleContext *context,
                      cairo_t         *cr,
                      gdouble          x,
                      gdouble          y,
                      gdouble          width,
                      gdouble          height,
                      GtkPositionType  gap_side)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_extension (engine, cr, x, y, width, height, gap_side);

  cairo_restore (cr);
}

/**
 * gtk_render_handle:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders a handle (as in #GtkHandleBox, #GtkPaned and
 * #GtkWindow<!-- -->'s resize grip), in the rectangle
 * determined by @x, @y, @width, @height.
 *
 * <example>
 * <title>Handles rendered for the paned and grip classes</title>
 * <inlinegraphic fileref="handles.png" format="PNG"/>
 * </example>
 *
 * Since: 3.0
 **/
void
gtk_render_handle (GtkStyleContext *context,
                   cairo_t         *cr,
                   gdouble          x,
                   gdouble          y,
                   gdouble          width,
                   gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_handle (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_activity:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Renders an activity area (Such as in #GtkSpinner or the
 * fill line in #GtkRange), the state %GTK_STATE_FLAG_ACTIVE
 * determines whether there is activity going on.
 *
 * Since: 3.0
 **/
void
gtk_render_activity (GtkStyleContext *context,
                     cairo_t         *cr,
                     gdouble          x,
                     gdouble          y,
                     gdouble          width,
                     gdouble          height)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  if (width <= 0 || height <= 0)
    return;

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_activity (engine, cr, x, y, width, height);

  cairo_restore (cr);
}

/**
 * gtk_render_icon_pixbuf:
 * @context: a #GtkStyleContext
 * @source: the #GtkIconSource specifying the icon to render
 * @size: (type int): the size to render the icon at. A size of (GtkIconSize) -1
 *        means render at the size of the source and don't scale.
 *
 * Renders the icon specified by @source at the given @size, returning the result
 * in a pixbuf.
 *
 * Returns: (transfer full): a newly-created #GdkPixbuf containing the rendered icon
 *
 * Since: 3.0
 **/
GdkPixbuf *
gtk_render_icon_pixbuf (GtkStyleContext     *context,
                        const GtkIconSource *source,
                        GtkIconSize          size)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);
  g_return_val_if_fail (size > GTK_ICON_SIZE_INVALID || size == -1, NULL);
  g_return_val_if_fail (source != NULL, NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  _gtk_theming_engine_set_context (engine, context);
  return engine_class->render_icon_pixbuf (engine, source, size);
}

/**
 * gtk_render_icon:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @pixbuf: a #GdkPixbuf containing the icon to draw
 * @x: X position for the @pixbuf
 * @y: Y position for the @pixbuf
 *
 * Renders the icon in @pixbuf at the specified @x and @y coordinates.
 *
 * Since: 3.2
 **/
void
gtk_render_icon (GtkStyleContext *context,
                 cairo_t         *cr,
                 GdkPixbuf       *pixbuf,
                 gdouble          x,
                 gdouble          y)
{
  GtkThemingEngineClass *engine_class;
  GtkThemingEngine *engine;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);

  engine = _gtk_css_engine_value_get_engine (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_ENGINE));
  engine_class = GTK_THEMING_ENGINE_GET_CLASS (engine);

  cairo_save (cr);

  _gtk_theming_engine_set_context (engine, context);
  engine_class->render_icon (engine, cr, pixbuf, x, y);

  cairo_restore (cr);
}

static void
draw_insertion_cursor (GtkStyleContext *context,
                       cairo_t         *cr,
                       gdouble          x,
                       gdouble          y,
                       gdouble          height,
                       gboolean         is_primary,
                       PangoDirection   direction,
                       gboolean         draw_arrow)

{
  GdkRGBA primary_color;
  GdkRGBA secondary_color;
  gfloat cursor_aspect_ratio;
  gint stem_width;
  gint offset;

  cairo_save (cr);

  _gtk_style_context_get_cursor_color (context, &primary_color, &secondary_color);
  gdk_cairo_set_source_rgba (cr, is_primary ? &primary_color : &secondary_color);

  /* When changing the shape or size of the cursor here,
   * propagate the changes to gtktextview.c:text_window_invalidate_cursors().
   */

  gtk_style_context_get_style (context,
                               "cursor-aspect-ratio", &cursor_aspect_ratio,
                               NULL);

  stem_width = height * cursor_aspect_ratio + 1;

  /* put (stem_width % 2) on the proper side of the cursor */
  if (direction == PANGO_DIRECTION_LTR)
    offset = stem_width / 2;
  else
    offset = stem_width - stem_width / 2;

  cairo_rectangle (cr, x - offset, y, stem_width, height);
  cairo_fill (cr);

  if (draw_arrow)
    {
      gint arrow_width;
      gint ax, ay;

      arrow_width = stem_width + 1;

      if (direction == PANGO_DIRECTION_RTL)
        {
          ax = x - offset - 1;
          ay = y + height - arrow_width * 2 - arrow_width + 1;

          cairo_move_to (cr, ax, ay + 1);
          cairo_line_to (cr, ax - arrow_width, ay + arrow_width);
          cairo_line_to (cr, ax, ay + 2 * arrow_width);
          cairo_fill (cr);
        }
      else if (direction == PANGO_DIRECTION_LTR)
        {
          ax = x + stem_width - offset;
          ay = y + height - arrow_width * 2 - arrow_width + 1;

          cairo_move_to (cr, ax, ay + 1);
          cairo_line_to (cr, ax + arrow_width, ay + arrow_width);
          cairo_line_to (cr, ax, ay + 2 * arrow_width);
          cairo_fill (cr);
        }
      else
        g_assert_not_reached();
    }

  cairo_restore (cr);
}

/**
 * gtk_render_insertion_cursor:
 * @context: a #GtkStyleContext
 * @cr: a #cairo_t
 * @x: X origin
 * @y: Y origin
 * @layout: the #PangoLayout of the text
 * @index: the index in the #PangoLayout
 * @direction: the #PangoDirection of the text
 *
 * Draws a text caret on @cr at the specified index of @layout.
 *
 * Since: 3.4
 **/
void
gtk_render_insertion_cursor (GtkStyleContext *context,
                             cairo_t         *cr,
                             gdouble          x,
                             gdouble          y,
                             PangoLayout     *layout,
                             int              index,
                             PangoDirection   direction)
{
  GtkStyleContextPrivate *priv;
  gboolean split_cursor;
  PangoRectangle strong_pos, weak_pos;
  PangoRectangle *cursor1, *cursor2;
  PangoDirection keymap_direction;
  PangoDirection direction2;

  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (PANGO_IS_LAYOUT (layout));
  g_return_if_fail (index >= 0);

  priv = context->priv;

  g_object_get (gtk_settings_get_for_screen (priv->screen),
                "gtk-split-cursor", &split_cursor,
                NULL);

  keymap_direction = gdk_keymap_get_direction (gdk_keymap_get_for_display (gdk_screen_get_display (priv->screen)));

  pango_layout_get_cursor_pos (layout, index, &strong_pos, &weak_pos);

  direction2 = PANGO_DIRECTION_NEUTRAL;

  if (split_cursor)
    {
      cursor1 = &strong_pos;

      if (strong_pos.x != weak_pos.x || strong_pos.y != weak_pos.y)
        {
          direction2 = (direction == PANGO_DIRECTION_LTR) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
          cursor2 = &weak_pos;
        }
    }
  else
    {
      if (keymap_direction == direction)
        cursor1 = &strong_pos;
      else
        cursor1 = &weak_pos;
    }

  draw_insertion_cursor (context,
                         cr,
                         x + PANGO_PIXELS (cursor1->x),
                         y + PANGO_PIXELS (cursor1->y),
                         PANGO_PIXELS (cursor1->height),
                         TRUE,
                         direction,
                         direction2 != PANGO_DIRECTION_NEUTRAL);

  if (direction2 != PANGO_DIRECTION_NEUTRAL)
    {
      draw_insertion_cursor (context,
                             cr,
                             x + PANGO_PIXELS (cursor2->x),
                             y + PANGO_PIXELS (cursor2->y),
                             PANGO_PIXELS (cursor2->height),
                             FALSE,
                             direction2,
                             TRUE);
    }
}

/**
 * gtk_draw_insertion_cursor:
 * @widget:  a #GtkWidget
 * @cr: cairo context to draw to
 * @location: location where to draw the cursor (@location->width is ignored)
 * @is_primary: if the cursor should be the primary cursor color.
 * @direction: whether the cursor is left-to-right or
 *             right-to-left. Should never be #GTK_TEXT_DIR_NONE
 * @draw_arrow: %TRUE to draw a directional arrow on the
 *        cursor. Should be %FALSE unless the cursor is split.
 *
 * Draws a text caret on @cr at @location. This is not a style function
 * but merely a convenience function for drawing the standard cursor shape.
 *
 * Since: 3.0
 * Deprecated: 3.4: Use gtk_render_insertion_cursor() instead.
 */
void
gtk_draw_insertion_cursor (GtkWidget          *widget,
                           cairo_t            *cr,
                           const GdkRectangle *location,
                           gboolean            is_primary,
                           GtkTextDirection    direction,
                           gboolean            draw_arrow)
{
  GtkStyleContext *context;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (location != NULL);
  g_return_if_fail (direction != GTK_TEXT_DIR_NONE);

  context = gtk_widget_get_style_context (widget);

  draw_insertion_cursor (context, cr,
                         location->x, location->y, location->height,
                         is_primary,
                         (direction == GTK_TEXT_DIR_RTL) ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR,
                         draw_arrow);
}

static AtkAttributeSet *
add_attribute (AtkAttributeSet  *attributes,
               AtkTextAttribute  attr,
               const gchar      *value)
{
  AtkAttribute *at;

  at = g_new (AtkAttribute, 1);
  at->name = g_strdup (atk_text_attribute_get_name (attr));
  at->value = g_strdup (value);

  return g_slist_prepend (attributes, at);
}

/*
 * _gtk_style_context_get_attributes:
 * @attributes: a #AtkAttributeSet to add attributes to
 * @context: the #GtkStyleContext to get attributes from
 * @flags: the state to use with @context
 *
 * Adds the foreground and background color from @context to
 * @attributes, after translating them to ATK attributes.
 *
 * This is a convenience function that can be used in
 * implementing the #AtkText interface in widgets.
 *
 * Returns: the modified #AtkAttributeSet
 */
AtkAttributeSet *
_gtk_style_context_get_attributes (AtkAttributeSet *attributes,
                                   GtkStyleContext *context,
                                   GtkStateFlags    flags)
{
  GdkRGBA color;
  gchar *value;

  gtk_style_context_get_background_color (context, flags, &color);
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) ceil (color.red * 65536 - color.red),
                           (guint) ceil (color.green * 65536 - color.green),
                           (guint) ceil (color.blue * 65536 - color.blue));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_BG_COLOR, value);
  g_free (value);

  gtk_style_context_get_color (context, flags, &color);
  value = g_strdup_printf ("%u,%u,%u",
                           (guint) ceil (color.red * 65536 - color.red),
                           (guint) ceil (color.green * 65536 - color.green),
                           (guint) ceil (color.blue * 65536 - color.blue));
  attributes = add_attribute (attributes, ATK_TEXT_ATTR_FG_COLOR, value);
  g_free (value);

  return attributes;
}
