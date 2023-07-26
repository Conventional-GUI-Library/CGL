/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdkconfig.h"
#include "gdkdisplaymanagerprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkinternals.h"
#include "gdkkeysprivate.h"
#include "gdkmarshalers.h"
#include "gdkintl.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#include "x11/gdkprivate-x11.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
/* When the gdk_quartz_display_open function is removed We can
 * immediately include gdkquartzdisplaymanager.h here instead of
 * gdkprivate-quartz.h so that we won't have to enable -xobjective-c
 * for the "generic" GDK source code.
 * #include "quartz/gdkquartzdisplaymanager.h"
 */
#include "quartz/gdkprivate-quartz.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadwaydisplaymanager.h"
#include "broadway/gdkprivate-broadway.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#include "win32/gdkprivate-win32.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#include "wayland/gdkprivate-wayland.h"
#endif

/**
 * SECTION:gdkdisplaymanager
 * @Short_description: Maintains a list of all open GdkDisplays
 * @Title: GdkDisplayManager
 *
 * The purpose of the #GdkDisplayManager singleton object is to offer
 * notification when displays appear or disappear or the default display
 * changes.
 *
 * You can use gdk_display_manager_get() to obtain the GdkDisplayManager
 * singleton, but that should be rarely necessary. Typically, initializing
 * GTK+ opens a display that you can work with without ever accessing the
 * GdkDisplayManager.
 *
 * The GDK library can be built with support for multiple backends.
 * The GdkDisplayManager object determines which backend is used
 * at runtime.
 *
 * When writing backend-specific code that is supposed to work with
 * multiple GDK backends, you have to consider both compile time and
 * runtime. At compile time, use the #GDK_WINDOWING_X11, #GDK_WINDOWING_WIN32
 * macros, etc. to find out which backends are present in the GDK library
 * you are building your application against. At runtime, use type-check
 * macros like GDK_IS_X11_DISPLAY() to find out which backend is in use:
 *
 * <example id="backend-specific">
 * <title>Backend-specific code</title>
 * <programlisting>
 * #ifdef GDK_WINDOWING_X11
 *   if (GDK_IS_X11_DISPLAY (display))
 *     {
 *       /&ast; make X11-specific calls here &ast;/
 *     }
 *   else
 * #endif
 * #ifdef GDK_WINDOWING_QUARTZ
 *   if (GDK_IS_QUARTZ_DISPLAY (display))
 *     {
 *       /&ast; make Quartz-specific calls here &ast;/
*     }
 *   else
 * #endif
 *   g_error ("Unsupported GDK backend");
 * </programlisting>
 * </example>
 */


enum {
  PROP_0,
  PROP_DEFAULT_DISPLAY
};

enum {
  DISPLAY_OPENED,
  LAST_SIGNAL
};

static void gdk_display_manager_class_init   (GdkDisplayManagerClass *klass);
static void gdk_display_manager_set_property (GObject                *object,
                                              guint                   prop_id,
                                              const GValue           *value,
                                              GParamSpec             *pspec);
static void gdk_display_manager_get_property (GObject                *object,
                                              guint                   prop_id,
                                              GValue                 *value,
                                              GParamSpec             *pspec);

static guint signals[LAST_SIGNAL] = { 0 };

static void g_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkDisplayManager, gdk_display_manager, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

static gboolean
gdk_display_manager_initable_init (GInitable     *initable,
                                   GCancellable  *cancellable,
                                   GError       **error)
{
  return TRUE;
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = gdk_display_manager_initable_init;
}

static void
gdk_display_manager_class_init (GdkDisplayManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_display_manager_set_property;
  object_class->get_property = gdk_display_manager_get_property;

  /**
   * GdkDisplayManager::display-opened:
   * @manager: the object on which the signal is emitted
   * @display: the opened display
   *
   * The ::display-opened signal is emitted when a display is opened.
   *
   * Since: 2.2
   */
  signals[DISPLAY_OPENED] =
    g_signal_new (g_intern_static_string ("display-opened"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDisplayManagerClass, display_opened),
                  NULL, NULL,
                  _gdk_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1,
                  GDK_TYPE_DISPLAY);

  g_object_class_install_property (object_class,
                                   PROP_DEFAULT_DISPLAY,
                                   g_param_spec_object ("default-display",
                                                        P_("Default Display"),
                                                        P_("The default display for GDK"),
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE|G_PARAM_STATIC_NAME|
                                                        G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

static void
gdk_display_manager_init (GdkDisplayManager *manager)
{
}

static void
gdk_display_manager_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DEFAULT_DISPLAY:
      gdk_display_manager_set_default_display (GDK_DISPLAY_MANAGER (object),
                                               g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_display_manager_get_property (GObject      *object,
                                  guint         prop_id,
                                  GValue       *value,
                                  GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_DEFAULT_DISPLAY:
      g_value_set_object (value,
                          gdk_display_manager_get_default_display (GDK_DISPLAY_MANAGER (object)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static const gchar *allowed_backends;

/**
 * gdk_set_allowed_backends:
 * @backends: a comma-separated list of backends
 *
 * Sets a list of backends that GDK should try to use.
 *
 * This can be be useful if your application does not
 * work with certain GDK backends.
 *
 * By default, GDK tries all included backends.
 *
 * For example,
 * <programlisting>
 * gdk_set_allowed_backends ("wayland,quartz,*");
 * </programlisting>
 * instructs GDK to try the Wayland backend first,
 * followed by the Quartz backend, and then all
 * others.
 *
 * If the <envvar>GDK_BACKEND</envvar> environment variable
 * is set, it determines what backends are tried in what
 * order, while still respecting the set of allowed backends
 * that are specified by this function.
 *
 * The possible backend names are x11, win32, quartz,
 * broadway, wayland. You can also include a * in the
 * list to try all remaining backends.
 *
 * This call must happen prior to gdk_display_open(),
 * gtk_init(), gtk_init_with_args() or gtk_init_check()
 * in order to take effect.
 *
 * Since: 3.10
 */
void
gdk_set_allowed_backends (const gchar *backends)
{
  allowed_backends = g_strdup (backends);
}

static GdkDisplayManager *manager = NULL;

/**
 * gdk_display_manager_get:
 *
 * Gets the singleton #GdkDisplayManager object.
 *
 * When called for the first time, this function consults the
 * <envar>GDK_BACKEND</envar> environment variable to find out which
 * of the supported GDK backends to use (in case GDK has been compiled
 * with multiple backends). Applications can use gdk_set_allowed_backends()
 * to limit what backends can be used.
 *
 * Returns: (transfer none): The global #GdkDisplayManager singleton;
 *     gdk_parse_args(), gdk_init(), or gdk_init_check() must have
 *     been called first.
 *
 * Since: 2.2
 **/
GdkDisplayManager*
gdk_display_manager_get (void)
{
  gdk_display_manager_peek ();

  if (manager == NULL)
    g_error ("No GDK backend found (%s)", allowed_backends);

  return manager;
}

typedef struct _GdkBackend GdkBackend;

struct _GdkBackend {
  const char *name;
  GType (* get_backend_type) (void);
  GdkDisplay * (* open_display) (const char *name);
};

static GdkBackend gdk_backends[] = {
#ifdef GDK_WINDOWING_QUARTZ
  { "quartz",   gdk_quartz_display_manager_get_type,    _gdk_quartz_display_open },
#endif
#ifdef GDK_WINDOWING_WIN32
  { "win32",    gdk_win32_display_manager_get_type,     _gdk_win32_display_open },
#endif
#ifdef GDK_WINDOWING_X11
  { "x11",      gdk_x11_display_manager_get_type,       _gdk_x11_display_open },
#endif
#ifdef GDK_WINDOWING_WAYLAND
  { "wayland",  gdk_wayland_display_manager_get_type,   _gdk_wayland_display_open },
#endif
#ifdef GDK_WINDOWING_BROADWAY
  { "broadway", gdk_broadway_display_manager_get_type,  _gdk_broadway_display_open },
#endif
  /* NULL-terminating this array so we can use commas above */
  { NULL, NULL }
};

/**
 * gdk_display_manager_peek:
 *
 * Gets the singleton #GdkDisplayManager object. If GDK could
 * not be initialized, %NULL is returned.
 *
 * Returns: (transfer none): The global #GdkDisplayManager singleton,
 *     or %NULL if GDK could not be initialized. gdk_parse_args(),
 *     gdk_init(), or gdk_init_check() must have been called first
 *
 * Since: 3.10
 */
GdkDisplayManager *
gdk_display_manager_peek (void)
{
  if (manager == NULL)
    {
      const gchar *backend_list;
      gchar **backends;
      gint i, j;
      gboolean allow_any;

      if (allowed_backends == NULL)
        allowed_backends = "*";
      allow_any = strstr (allowed_backends, "*") != NULL;

      backend_list = g_getenv ("GDK_BACKEND");
      if (backend_list == NULL)
        backend_list = allowed_backends;
      backends = g_strsplit (backend_list, ",", 0);

      for (i = 0; manager == NULL && backends[i] != NULL; i++)
        {
          const gchar *backend = backends[i];
          gboolean any = g_str_equal (backend, "*");

          if (!allow_any && !any && !strstr (allowed_backends, backend))
            continue;

          for (j = 0; gdk_backends[j].name != NULL; j++)
            {
              if ((any && allow_any) ||
                  (any && strstr (allowed_backends, gdk_backends[j].name)) ||
                  g_str_equal (backend, gdk_backends[j].name))
                {
                  GDK_NOTE (MISC, g_message ("Trying %s backend", gdk_backends[j].name));
                  manager = g_initable_new (gdk_backends[j].get_backend_type (), NULL, NULL, NULL);
                  if (manager)
                    break;
                }
            }
        }
      g_strfreev (backends);
    }

  return manager;
}

/**
 * gdk_display_manager_get_default_display:
 * @manager: a #GdkDisplayManager
 *
 * Gets the default #GdkDisplay.
 *
 * Returns: (transfer none): a #GdkDisplay, or %NULL
 *     if there is no default display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_manager_get_default_display (GdkDisplayManager *manager)
{
  return manager->default_display;
}

/**
 * gdk_display_get_default:
 *
 * Gets the default #GdkDisplay. This is a convenience
 * function for
 * <literal>gdk_display_manager_get_default_display (gdk_display_manager_get ())</literal>.
 *
 * Returns: (transfer none): a #GdkDisplay, or %NULL if there is no default
 *   display.
 *
 * Since: 2.2
 */
GdkDisplay *
gdk_display_get_default (void)
{
  return gdk_display_manager_get_default_display (gdk_display_manager_get ());
}

/**
 * gdk_screen_get_default:
 *
 * Gets the default screen for the default display. (See
 * gdk_display_get_default ()).
 *
 * Returns: (transfer none): a #GdkScreen, or %NULL if there is no default display.
 *
 * Since: 2.2
 */
GdkScreen *
gdk_screen_get_default (void)
{
  GdkDisplay *default_display;

  default_display = gdk_display_get_default ();

  if (default_display)
    return gdk_display_get_default_screen (default_display);
  else
    return NULL;
}

/**
 * gdk_display_manager_set_default_display:
 * @manager: a #GdkDisplayManager
 * @display: a #GdkDisplay
 * 
 * Sets @display as the default display.
 *
 * Since: 2.2
 **/
void
gdk_display_manager_set_default_display (GdkDisplayManager *manager,
                                         GdkDisplay        *display)
{
  manager->default_display = display;

  if (display)
    GDK_DISPLAY_GET_CLASS (display)->make_default (display);

  g_object_notify (G_OBJECT (manager), "default-display");
}

/**
 * gdk_display_manager_list_displays:
 * @manager: a #GdkDisplayManager
 *
 * List all currently open displays.
 *
 * Return value: (transfer container) (element-type GdkDisplay): a newly
 *     allocated #GSList of #GdkDisplay objects. Free with g_slist_free()
 *     when you are done with it.
 *
 * Since: 2.2
 **/
GSList *
gdk_display_manager_list_displays (GdkDisplayManager *manager)
{
  return g_slist_copy (manager->displays);
}

/**
 * gdk_display_manager_open_display:
 * @manager: a #GdkDisplayManager
 * @name: the name of the display to open
 *
 * Opens a display.
 *
 * Return value: (transfer none): a #GdkDisplay, or %NULL
 *     if the display could not be opened
 *
 * Since: 3.0
 */
GdkDisplay *
gdk_display_manager_open_display (GdkDisplayManager *manager,
                                  const gchar       *name)
{
  guint i;

  for (i = 0; gdk_backends[i].name != NULL; i++)
    {
      if (G_OBJECT_TYPE (manager) == gdk_backends[i].get_backend_type ())
        {
          return gdk_backends[i].open_display (name);
        }
    }

  g_assert_not_reached ();

  return NULL;
}

void
_gdk_display_manager_add_display (GdkDisplayManager *manager,
                                  GdkDisplay        *display)
{
  if (manager->displays == NULL)
    gdk_display_manager_set_default_display (manager, display);

  manager->displays = g_slist_prepend (manager->displays, display);

  g_signal_emit (manager, signals[DISPLAY_OPENED], 0, display);
}

/* NB: This function can be called multiple times per display. */
void
_gdk_display_manager_remove_display (GdkDisplayManager *manager,
                                     GdkDisplay        *display)
{
  manager->displays = g_slist_remove (manager->displays, display);

  if (manager->default_display == display)
    {
      if (manager->displays)
        gdk_display_manager_set_default_display (manager, manager->displays->data);
      else
        gdk_display_manager_set_default_display (manager, NULL);
    }
}
