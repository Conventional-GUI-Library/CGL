/*
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplication.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>

#include "gtkapplicationprivate.h"
#include "gtkclipboard.h"
#include "gtkmarshalers.h"
#include "gtkmain.h"
#include "gtkrecentmanager.h"
#include "gtkaccelmapprivate.h"
#include "gactionmuxer.h"
#include "gtkintl.h"

#ifdef GDK_WINDOWING_QUARTZ
#include "gtkquartz-menu.h"
#import <Cocoa/Cocoa.h>
#include <Carbon/Carbon.h>
#include "gtkmessagedialog.h"
#endif

#include <gdk/gdk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

/**
 * SECTION:gtkapplication
 * @title: GtkApplication
 * @short_description: Application class
 *
 * #GtkApplication is a class that handles many important aspects
 * of a GTK+ application in a convenient fashion, without enforcing
 * a one-size-fits-all application model.
 *
 * Currently, GtkApplication handles GTK+ initialization, application
 * uniqueness, session management, provides some basic scriptability and
 * desktop shell integration by exporting actions and menus and manages a
 * list of toplevel windows whose life-cycle is automatically tied to the
 * life-cycle of your application.
 *
 * While GtkApplication works fine with plain #GtkWindows, it is recommended
 * to use it together with #GtkApplicationWindow.
 *
 * To set an app menu on a GtkApplication, use g_application_set_app_menu().
 * The #GMenuModel that this function expects is usually constructed using
 * #GtkBuilder, as seen in the following example. To set a menubar that will
 * be automatically picked up by #GApplicationWindows, use
 * g_application_set_menubar().
 *
 * <example id="gtkapplication"><title>A simple application</title>
 * <programlisting>
 * <xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../examples/bloatpad.c">
 *  <xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback>
 * </xi:include>
 * </programlisting>
 * </example>
 *
 * GtkApplication optionally registers with a session manager
 * of the users session (if you set the #GtkApplication:register-session
 * property) and offers various functionality related to the session
 * life-cycle.
 *
 * An application can be informed when the session is about to end
 * by connecting to the #GtkApplication::quit signal.
 *
 * An application can request the session to be ended by calling
 * gtk_application_end_session().
 *
 * An application can block various ways to end the session with
 * the gtk_application_inhibit() function. Typical use cases for
 * this kind of inhibiting are long-running, uninterruptible operations,
 * such as burning a CD or performing a disk backup. The session
 * manager may not honor the inhibitor, but it can be expected to
 * inform the user about the negative consequences of ending the
 * session while inhibitors are present.
 */

enum {
  WINDOW_ADDED,
  WINDOW_REMOVED,
  QUIT,
  LAST_SIGNAL
};

static guint gtk_application_signals[LAST_SIGNAL];

enum {
  PROP_ZERO,
  PROP_REGISTER_SESSION,
  PROP_APP_MENU,
  PROP_MENUBAR
};

G_DEFINE_TYPE (GtkApplication, gtk_application, G_TYPE_APPLICATION)

struct _GtkApplicationPrivate
{
  GList *windows;

  gboolean register_session;

  GMenuModel      *app_menu;
  GMenuModel      *menubar;

#ifdef GDK_WINDOWING_X11
  GDBusConnection *session_bus;
  const gchar     *application_id;
  gchar           *object_path;

  gchar           *app_menu_path;
  guint            app_menu_id;

  gchar           *menubar_path;
  guint            menubar_id;

  guint next_id;

  GDBusProxy *sm_proxy;
  GDBusProxy *client_proxy;
  gchar *app_id;
  gchar *client_path;
#endif

#ifdef GDK_WINDOWING_QUARTZ
  GActionMuxer *muxer;
  GMenu *combined;

  GSList *inhibitors;
  gint quit_inhibit;
  guint next_cookie;
#endif
};

#ifdef GDK_WINDOWING_X11
static void
gtk_application_x11_publish_menu (GtkApplication  *application,
                                  const gchar     *type,
                                  GMenuModel      *model,
                                  guint           *id,
                                  gchar          **path)
{
  gint i;

  /* unexport any existing menu */
  if (*id)
    {
      g_dbus_connection_unexport_menu_model (application->priv->session_bus, *id);
      g_free (*path);
      *path = NULL;
      *id = 0;
    }

  /* export the new menu, if there is one */
  if (model != NULL)
    {
      /* try getting the preferred name */
      *path = g_strconcat (application->priv->object_path, "/menus/", type, NULL);
      *id = g_dbus_connection_export_menu_model (application->priv->session_bus, *path, model, NULL);

      /* keep trying until we get a working name... */
      for (i = 0; *id == 0; i++)
        {
          g_free (*path);
          *path = g_strdup_printf ("%s/menus/%s%d", application->priv->object_path, type, i);
          *id = g_dbus_connection_export_menu_model (application->priv->session_bus, *path, model, NULL);
        }
    }
}

static void
gtk_application_set_app_menu_x11 (GtkApplication *application,
                                  GMenuModel     *app_menu)
{
  gtk_application_x11_publish_menu (application, "appmenu", app_menu,
                                    &application->priv->app_menu_id,
                                    &application->priv->app_menu_path);
}

static void
gtk_application_set_menubar_x11 (GtkApplication *application,
                                 GMenuModel     *menubar)
{
  gtk_application_x11_publish_menu (application, "menubar", menubar,
                                    &application->priv->menubar_id,
                                    &application->priv->menubar_path);
}

static void
gtk_application_window_added_x11 (GtkApplication *application,
                                  GtkWindow      *window)
{
  if (application->priv->session_bus == NULL)
    return;

  if (GTK_IS_APPLICATION_WINDOW (window))
    {
      GtkApplicationWindow *app_window = GTK_APPLICATION_WINDOW (window);
      gboolean success;

      /* GtkApplicationWindow associates with us when it is first created,
       * so surely it's not realized yet...
       */
      g_assert (!gtk_widget_get_realized (GTK_WIDGET (window)));

      do
        {
          gchar *window_path;
          guint window_id;

          window_id = application->priv->next_id++;
          window_path = g_strdup_printf ("%s/window/%d", application->priv->object_path, window_id);
          success = gtk_application_window_publish (app_window, application->priv->session_bus, window_path);
          g_free (window_path);
        }
      while (!success);
    }
}

static void
gtk_application_window_removed_x11 (GtkApplication *application,
                                    GtkWindow      *window)
{
  if (application->priv->session_bus == NULL)
    return;

  if (GTK_IS_APPLICATION_WINDOW (window))
    gtk_application_window_unpublish (GTK_APPLICATION_WINDOW (window));
}

static gchar *
object_path_from_appid (const gchar *appid)
{
  gchar *appid_path, *iter;

  appid_path = g_strconcat ("/", appid, NULL);
  for (iter = appid_path; *iter; iter++)
    {
      if (*iter == '.')
        *iter = '/';

      if (*iter == '-')
        *iter = '_';
    }

  return appid_path;
}

static void gtk_application_startup_session_dbus (GtkApplication *app);

static void
gtk_application_startup_x11 (GtkApplication *application)
{
  const gchar *application_id;

  application_id = g_application_get_application_id (G_APPLICATION (application));
  application->priv->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  application->priv->object_path = object_path_from_appid (application_id);

  gtk_application_startup_session_dbus (GTK_APPLICATION (application));
}

static void
gtk_application_shutdown_x11 (GtkApplication *application)
{
  g_free (application->priv->object_path);
  application->priv->object_path = NULL;
  g_clear_object (&application->priv->session_bus);

  g_clear_object (&application->priv->sm_proxy);
  g_clear_object (&application->priv->client_proxy);
  g_free (application->priv->app_id);
  g_free (application->priv->client_path);
}

const gchar *
gtk_application_get_dbus_object_path (GtkApplication *application)
{
  return application->priv->object_path;
}

const gchar *
gtk_application_get_app_menu_object_path (GtkApplication *application)
{
  return application->priv->app_menu_path;
}

const gchar *
gtk_application_get_menubar_object_path (GtkApplication *application)
{
  return application->priv->menubar_path;
}

#endif

#ifdef GDK_WINDOWING_QUARTZ

typedef struct {
  guint cookie;
  GtkApplicationInhibitFlags flags;
  char *reason;
  GtkWindow *window;
} GtkApplicationQuartzInhibitor;

static void
gtk_application_quartz_inhibitor_free (GtkApplicationQuartzInhibitor *inhibitor)
{
  g_free (inhibitor->reason);
  g_clear_object (&inhibitor->window);
  g_slice_free (GtkApplicationQuartzInhibitor, inhibitor);
}

static void
gtk_application_menu_changed_quartz (GObject    *object,
                                     GParamSpec *pspec,
                                     gpointer    user_data)
{
  GtkApplication *application = GTK_APPLICATION (object);
  GMenu *combined;

  combined = g_menu_new ();
  g_menu_append_submenu (combined, "Application", gtk_application_get_app_menu (application));
  g_menu_append_section (combined, NULL, gtk_application_get_menubar (application));

  gtk_quartz_set_main_menu (G_MENU_MODEL (combined), G_ACTION_OBSERVABLE (application->priv->muxer));
}

static void gtk_application_startup_session_quartz (GtkApplication *app);

static void
gtk_application_startup_quartz (GtkApplication *application)
{
  [NSApp finishLaunching];

  application->priv->muxer = g_action_muxer_new ();
  g_action_muxer_insert (application->priv->muxer, "app", G_ACTION_GROUP (application));

  g_signal_connect (application, "notify::app-menu", G_CALLBACK (gtk_application_menu_changed_quartz), NULL);
  g_signal_connect (application, "notify::menubar", G_CALLBACK (gtk_application_menu_changed_quartz), NULL);
  gtk_application_menu_changed_quartz (G_OBJECT (application), NULL, NULL);

  gtk_application_startup_session_quartz (application);
}

static void
gtk_application_shutdown_quartz (GtkApplication *application)
{
  g_signal_handlers_disconnect_by_func (application, gtk_application_menu_changed_quartz, NULL);

  g_object_unref (application->priv->muxer);
  application->priv->muxer = NULL;

  g_slist_free_full (application->priv->inhibitors,
		     (GDestroyNotify) gtk_application_quartz_inhibitor_free);
  application->priv->inhibitors = NULL;
}

static void
gtk_application_focus_changed (GtkApplication *application,
                               GtkWindow      *window)
{
  if (G_IS_ACTION_GROUP (window))
    g_action_muxer_insert (application->priv->muxer, "win", G_ACTION_GROUP (window));
  else
    g_action_muxer_remove (application->priv->muxer, "win");
}
#endif

static gboolean
gtk_application_focus_in_event_cb (GtkWindow      *window,
                                   GdkEventFocus  *event,
                                   GtkApplication *application)
{
  GtkApplicationPrivate *priv = application->priv;
  GList *link;

  /* Keep the window list sorted by most-recently-focused. */
  link = g_list_find (priv->windows, window);
  if (link != NULL && link != priv->windows)
    {
      priv->windows = g_list_remove_link (priv->windows, link);
      priv->windows = g_list_concat (link, priv->windows);
    }

#ifdef GDK_WINDOWING_QUARTZ
  gtk_application_focus_changed (application, window);
#endif

  return FALSE;
}

static void
gtk_application_startup (GApplication *application)
{
  G_APPLICATION_CLASS (gtk_application_parent_class)
    ->startup (application);

  gtk_init (0, 0);

#ifdef GDK_WINDOWING_X11
  gtk_application_startup_x11 (GTK_APPLICATION (application));
#endif

#ifdef GDK_WINDOWING_QUARTZ
  gtk_application_startup_quartz (GTK_APPLICATION (application));
#endif
}

static void
gtk_application_shutdown (GApplication *application)
{
#ifdef GDK_WINDOWING_X11
  gtk_application_shutdown_x11 (GTK_APPLICATION (application));
#endif

#ifdef GDK_WINDOWING_QUARTZ
  gtk_application_shutdown_quartz (GTK_APPLICATION (application));
#endif

  /* Try storing all clipboard data we have */
  _gtk_clipboard_store_all ();

  /* Synchronize the recent manager singleton */
  _gtk_recent_manager_sync ();

  G_APPLICATION_CLASS (gtk_application_parent_class)
    ->shutdown (application);
}

static void
gtk_application_add_platform_data (GApplication    *application,
                                   GVariantBuilder *builder)
{
  const gchar *startup_id;

  startup_id = getenv ("DESKTOP_STARTUP_ID");
  
  if (startup_id && g_utf8_validate (startup_id, -1, NULL))
    g_variant_builder_add (builder, "{sv}", "desktop-startup-id",
                           g_variant_new_string (startup_id));
}

static void
gtk_application_before_emit (GApplication *application,
                             GVariant     *platform_data)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  gdk_threads_enter ();

  g_variant_iter_init (&iter, platform_data);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    {
#ifdef GDK_WINDOWING_X11
      if (strcmp (key, "desktop-startup-id") == 0)
        {
          GdkDisplay *display;
          const gchar *id;

          display = gdk_display_get_default ();
          id = g_variant_get_string (value, NULL);
          if (GDK_IS_X11_DISPLAY (display))
            gdk_x11_display_set_startup_notification_id (display, id);
       }
#endif
    }
}

static void
gtk_application_after_emit (GApplication *application,
                            GVariant     *platform_data)
{
  gdk_notify_startup_complete ();

  gdk_threads_leave ();
}

static void
gtk_application_init (GtkApplication *application)
{
  application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                   GTK_TYPE_APPLICATION,
                                                   GtkApplicationPrivate);
}

static void
gtk_application_window_added (GtkApplication *application,
                              GtkWindow      *window)
{
  GtkApplicationPrivate *priv = application->priv;

  priv->windows = g_list_prepend (priv->windows, window);
  gtk_window_set_application (window, application);
  g_application_hold (G_APPLICATION (application));

  g_signal_connect (window, "focus-in-event",
                    G_CALLBACK (gtk_application_focus_in_event_cb),
                    application);

#ifdef GDK_WINDOWING_X11
  gtk_application_window_added_x11 (application, window);
#endif
}

static void
gtk_application_window_removed (GtkApplication *application,
                                GtkWindow      *window)
{
  GtkApplicationPrivate *priv = application->priv;

#ifdef GDK_WINDOWING_X11
  gtk_application_window_removed_x11 (application, window);
#endif

  g_signal_handlers_disconnect_by_func (window,
                                        gtk_application_focus_in_event_cb,
                                        application);

  g_application_release (G_APPLICATION (application));
  priv->windows = g_list_remove (priv->windows, window);
  gtk_window_set_application (window, NULL);
}

static void
extract_accel_from_menu_item (GMenuModel     *model,
                              gint            item,
                              GtkApplication *app)
{
  GMenuAttributeIter *iter;
  const gchar *key;
  GVariant *value;
  const gchar *accel = NULL;
  const gchar *action = NULL;
  GVariant *target = NULL;

  iter = g_menu_model_iterate_item_attributes (model, item);
  while (g_menu_attribute_iter_get_next (iter, &key, &value))
    {
      if (g_str_equal (key, "action") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        action = g_variant_get_string (value, NULL);
      else if (g_str_equal (key, "accel") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        accel = g_variant_get_string (value, NULL);
      else if (g_str_equal (key, "target"))
        target = g_variant_ref (value);
      g_variant_unref (value);
    }
  g_object_unref (iter);

  if (accel && action)
    gtk_application_add_accelerator (app, accel, action, target);

  if (target)
    g_variant_unref (target);
}

static void
extract_accels_from_menu (GMenuModel     *model,
                          GtkApplication *app)
{
  gint i;
  GMenuLinkIter *iter;
  const gchar *key;
  GMenuModel *m;

  for (i = 0; i < g_menu_model_get_n_items (model); i++)
    {
      extract_accel_from_menu_item (model, i, app);

      iter = g_menu_model_iterate_item_links (model, i);
      while (g_menu_link_iter_get_next (iter, &key, &m))
        {
          extract_accels_from_menu (m, app);
          g_object_unref (m);
        }
      g_object_unref (iter);
    }
}

static void
gtk_application_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_REGISTER_SESSION:
      g_value_set_boolean (value, application->priv->register_session);
      break;

    case PROP_APP_MENU:
      g_value_set_object (value, gtk_application_get_app_menu (application));
      break;

    case PROP_MENUBAR:
      g_value_set_object (value, gtk_application_get_menubar (application));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_application_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkApplication *application = GTK_APPLICATION (object);

  switch (prop_id)
    {
    case PROP_REGISTER_SESSION:
      application->priv->register_session = g_value_get_boolean (value);
      break;

    case PROP_APP_MENU:
      gtk_application_set_app_menu (application, g_value_get_object (value));
      break;

    case PROP_MENUBAR:
      gtk_application_set_menubar (application, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_application_quit (GtkApplication *app)
{
  /* we are asked to quit, so don't linger */
  g_application_set_inactivity_timeout (G_APPLICATION (app), 0);
}

static void
gtk_application_finalize (GObject *object)
{
  GtkApplication *application = GTK_APPLICATION (object);

  g_clear_object (&application->priv->app_menu);
  g_clear_object (&application->priv->menubar);

  G_OBJECT_CLASS (gtk_application_parent_class)
    ->finalize (object);
}

static void
gtk_application_class_init (GtkApplicationClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (class);

  object_class->get_property = gtk_application_get_property;
  object_class->set_property = gtk_application_set_property;
  object_class->finalize = gtk_application_finalize;

  application_class->add_platform_data = gtk_application_add_platform_data;
  application_class->before_emit = gtk_application_before_emit;
  application_class->after_emit = gtk_application_after_emit;
  application_class->startup = gtk_application_startup;
  application_class->shutdown = gtk_application_shutdown;

  class->window_added = gtk_application_window_added;
  class->window_removed = gtk_application_window_removed;
  class->quit = gtk_application_quit;

  g_type_class_add_private (class, sizeof (GtkApplicationPrivate));

  /**
   * GtkApplication::window-added:
   * @application: the #GtkApplication which emitted the signal
   * @window: the newly-added #GtkWindow
   *
   * Emitted when a #GtkWindow is added to @application through
   * gtk_application_add_window().
   *
   * Since: 3.2
   */
  gtk_application_signals[WINDOW_ADDED] =
    g_signal_new ("window-added", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, window_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_WINDOW);

  /**
   * GtkApplication::window-removed:
   * @application: the #GtkApplication which emitted the signal
   * @window: the #GtkWindow that is being removed
   *
   * Emitted when a #GtkWindow is removed from @application,
   * either as a side-effect of being destroyed or explicitly
   * through gtk_application_remove_window().
   *
   * Since: 3.2
   */
  gtk_application_signals[WINDOW_REMOVED] =
    g_signal_new ("window-removed", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, window_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GTK_TYPE_WINDOW);

  /**
   * GtkApplication::quit:
   * @application: the #GtkApplication
   *
   * Emitted when the session manager wants the application to quit
   * (generally because the user is logging out). The application
   * should exit as soon as possible after receiving this signal; if
   * it does not, the session manager may choose to forcibly kill it.
   *
   * Normally, an application would only be sent a ::quit if there
   * are no inhibitors (see gtk_application_inhibit()).
   * However, this is not guaranteed; in some situations the
   * session manager may decide to end the session without giving
   * applications a chance to object.
   *
   * To receive this signal, you need to set the
   * #GtkApplication:register-session property
   * when creating the application object.
   *
   * Since: 3.4
   */
  gtk_application_signals[QUIT] =
    g_signal_new ("quit", GTK_TYPE_APPLICATION, G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkApplicationClass, quit),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GtkApplication:register-session:
   *
   * Set this property to %TRUE to register with the session manager
   * and receive the #GtkApplication::quit signal when the session
   * is about to end.
   *
   * Since: 3.4
   */
  g_object_class_install_property (object_class, PROP_REGISTER_SESSION,
    g_param_spec_boolean ("register-session",
                          P_("Register session"),
                          P_("Register with the session manager"),
                          FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_APP_MENU,
    g_param_spec_object ("app-menu",
                         P_("Application menu"),
                         P_("The GMenuModel for the application menu"),
                         G_TYPE_MENU_MODEL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_MENUBAR,
    g_param_spec_object ("menubar",
                         P_("Menubar"),
                         P_("The GMenuModel for the menubar"),
                         G_TYPE_MENU_MODEL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * gtk_application_new:
 * @application_id: the application id
 * @flags: the application flags
 *
 * Creates a new #GtkApplication instance.
 *
 * This function calls g_type_init() for you. gtk_init() is called
 * as soon as the application gets registered as the primary instance.
 *
 * Concretely, gtk_init() is called in the default handler for the
 * startup() signal. Therefore, #GtkApplication subclasses should
 * chain up in their startup() handler before using any GTK+ API.
 *
 * Note that commandline arguments are not passed to gtk_init().
 * All GTK+ functionality that is available via commandline arguments
 * can also be achieved by setting suitable environment variables
 * such as <envar>G_DEBUG</envar>, so this should not be a big
 * problem. If you absolutely must support GTK+ commandline arguments,
 * you can explicitly call gtk_init() before creating the application
 * instance.
 *
 * The application id must be valid. See g_application_id_is_valid().
 *
 * Returns: a new #GtkApplication instance
 *
 * Since: 3.0
 */
GtkApplication *
gtk_application_new (const gchar       *application_id,
                     GApplicationFlags  flags)
{
  g_return_val_if_fail (g_application_id_is_valid (application_id), NULL);

  g_type_init ();

  return g_object_new (GTK_TYPE_APPLICATION,
                       "application-id", application_id,
                       "flags", flags,
                       NULL);
}

/**
 * gtk_application_add_window:
 * @application: a #GtkApplication
 * @window: a #GtkWindow
 *
 * Adds a window from @application.
 *
 * This call is equivalent to setting the #GtkWindow:application
 * property of @window to @application.
 *
 * Normally, the connection between the application and the window
 * will remain until the window is destroyed, but you can explicitly
 * remove it with gtk_application_remove_window().
 *
 * GTK+ will keep the application running as long as it has
 * any windows.
 *
 * Since: 3.0
 **/
void
gtk_application_add_window (GtkApplication *application,
                            GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (!g_list_find (application->priv->windows, window))
    g_signal_emit (application,
                   gtk_application_signals[WINDOW_ADDED], 0, window);
}

/**
 * gtk_application_remove_window:
 * @application: a #GtkApplication
 * @window: a #GtkWindow
 *
 * Remove a window from @application.
 *
 * If @window belongs to @application then this call is equivalent to
 * setting the #GtkWindow:application property of @window to
 * %NULL.
 *
 * The application may stop running as a result of a call to this
 * function.
 *
 * Since: 3.0
 **/
void
gtk_application_remove_window (GtkApplication *application,
                               GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (g_list_find (application->priv->windows, window))
    g_signal_emit (application,
                   gtk_application_signals[WINDOW_REMOVED], 0, window);
}

/**
 * gtk_application_get_windows:
 * @application: a #GtkApplication
 *
 * Gets a list of the #GtkWindows associated with @application.
 *
 * The list is sorted by most recently focused window, such that the first
 * element is the currently focused window. (Useful for choosing a parent
 * for a transient window.)
 *
 * The list that is returned should not be modified in any way. It will
 * only remain valid until the next focus change or window creation or
 * deletion.
 *
 * Returns: (element-type GtkWindow) (transfer none): a #GList of #GtkWindow
 *
 * Since: 3.0
 **/
GList *
gtk_application_get_windows (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);

  return application->priv->windows;
}

/**
 * gtk_application_add_accelerator:
 * @application: a #GtkApplication
 * @accelerator: accelerator string
 * @action_name: the name of the action to activate
 * @parameter: (allow-none): parameter to pass when activating the action,
 *   or %NULL if the action does not accept an activation parameter
 *
 * Installs an accelerator that will cause the named action
 * to be activated when the key combination specificed by @accelerator
 * is pressed.
 *
 * @accelerator must be a string that can be parsed by
 * gtk_accelerator_parse(), e.g. "<Primary>q" or "<Control><Alt>p".
 *
 * @action_name must be the name of an action as it would be used
 * in the app menu, i.e. actions that have been added to the application
 * are referred to with an "app." prefix, and window-specific actions
 * with a "win." prefix.
 *
 * GtkApplication also extracts accelerators out of 'accel' attributes
 * in the #GMenuModels passed to gtk_application_set_app_menu() and
 * gtk_application_set_menubar(), which is usually more convenient
 * than calling this function for each accelerator.
 *
 * Since: 3.4
 */
void
gtk_application_add_accelerator (GtkApplication *application,
                                 const gchar    *accelerator,
                                 const gchar    *action_name,
                                 GVariant       *parameter)
{
  gchar *accel_path;
  guint accel_key;
  GdkModifierType accel_mods;

  g_return_if_fail (GTK_IS_APPLICATION (application));

  /* Call this here, since gtk_init() is only getting called in startup() */
  _gtk_accel_map_init ();

  gtk_accelerator_parse (accelerator, &accel_key, &accel_mods);

  if (accel_key == 0)
    {
      g_warning ("Failed to parse accelerator: '%s'\n", accelerator);
      return;
    }

  accel_path = _gtk_accel_path_for_action (action_name, parameter);

  if (gtk_accel_map_lookup_entry (accel_path, NULL))
    gtk_accel_map_change_entry (accel_path, accel_key, accel_mods, TRUE);
  else
    gtk_accel_map_add_entry (accel_path, accel_key, accel_mods);

  g_free (accel_path);
}

/**
 * gtk_application_remove_accelerator:
 * @application: a #GtkApplication
 * @action_name: the name of the action to activate
 * @parameter: (allow-none): parameter to pass when activating the action,
 *   or %NULL if the action does not accept an activation parameter
 *
 * Removes an accelerator that has been previously added
 * with gtk_application_add_accelerator().
 *
 * Since: 3.4
 */
void
gtk_application_remove_accelerator (GtkApplication *application,
                                    const gchar    *action_name,
                                    GVariant       *parameter)
{
  gchar *accel_path;

  g_return_if_fail (GTK_IS_APPLICATION (application));

  accel_path = _gtk_accel_path_for_action (action_name, parameter);

  if (!gtk_accel_map_lookup_entry (accel_path, NULL))
    {
      g_warning ("No accelerator found for '%s'\n", accel_path);
      g_free (accel_path);
      return;
    }

  gtk_accel_map_change_entry (accel_path, 0, 0, FALSE);
  g_free (accel_path);
}

/**
 * gtk_application_set_app_menu:
 * @application: a #GtkApplication
 * @app_menu: (allow-none): a #GMenuModel, or %NULL
 *
 * Sets or unsets the application menu for @application.
 *
 * The application menu is a single menu containing items that typically
 * impact the application as a whole, rather than acting on a specific
 * window or document.  For example, you would expect to see
 * "Preferences" or "Quit" in an application menu, but not "Save" or
 * "Print".
 *
 * If supported, the application menu will be rendered by the desktop
 * environment.
 *
 * Since: 3.4
 */
void
gtk_application_set_app_menu (GtkApplication *application,
                              GMenuModel     *app_menu)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (app_menu != application->priv->app_menu)
    {
      if (application->priv->app_menu != NULL)
       g_object_unref (application->priv->app_menu);

     application->priv->app_menu = app_menu;

      if (application->priv->app_menu != NULL)
     g_object_ref (application->priv->app_menu);

      extract_accels_from_menu (app_menu, application);

#ifdef GDK_WINDOWING_X11
      gtk_application_set_app_menu_x11 (application, app_menu);
#endif

      g_object_notify (G_OBJECT (application), "app-menu");
    }
}

/**
 * gtk_application_get_app_menu:
 * @application: a #GtkApplication
 *
 * Returns the menu model that has been set with
 * gtk_application_set_app_menu().
 *
 * Returns: the application menu of @application
 *
 * Since: 3.4
 */
GMenuModel *
gtk_application_get_app_menu (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
  return application->priv->app_menu;
}

/**
 * gtk_application_set_menubar:
 * @application: a #GtkApplication
 * @menubar: (allow-none): a #GMenuModel, or %NULL
 *
 * Sets or unsets the menubar for windows of @application.
 *
 * This is a menubar in the traditional sense.
 *
 * Depending on the desktop environment, this may appear at the top of
 * each window, or at the top of the screen.  In some environments, if
 * both the application menu and the menubar are set, the application
 * menu will be presented as if it were the first item of the menubar.
 * Other environments treat the two as completely separate -- for
 * example, the application menu may be rendered by the desktop shell
 * while the menubar (if set) remains in each individual window.
 *
 * Since: 3.4
 */
void
gtk_application_set_menubar (GtkApplication *application,
                             GMenuModel     *menubar)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));

  if (menubar != application->priv->menubar)
    {
      if (application->priv->menubar != NULL)
        g_object_unref (application->priv->menubar);

      application->priv->menubar = menubar;

      if (application->priv->menubar != NULL)
        g_object_ref (application->priv->menubar);

     extract_accels_from_menu (menubar, application);

#ifdef GDK_WINDOWING_X11
     gtk_application_set_menubar_x11 (application, menubar);
#endif

      g_object_notify (G_OBJECT (application), "menubar");
    }
}

/**
 * gtk_application_get_menubar:
 * @application: a #GtkApplication
 *
 * Returns the menu model that has been set with
 * gtk_application_set_menubar().
 *
 * Returns: the menubar for windows of @application
 *
 * Since: 3.4
 */
GMenuModel *
gtk_application_get_menubar (GtkApplication *application)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), NULL);
 
  return application->priv->menubar;
}

#if defined(GDK_WINDOWING_X11)

/* D-Bus Session Management
 *
 * The protocol and the D-Bus API are described here:
 * http://live.gnome.org/SessionManagement/GnomeSession
 * http://people.gnome.org/~mccann/gnome-session/docs/gnome-session.html
 */

static void
unregister_client (GtkApplication *app)
{
  GError *error = NULL;

  g_debug ("Unregistering client");

  g_dbus_proxy_call_sync (app->priv->sm_proxy,
                          "UnregisterClient",
                          g_variant_new ("(o)", app->priv->client_path),
                          G_DBUS_CALL_FLAGS_NONE,
                          G_MAXINT,
                          NULL,
                          &error);

  if (error)
    {
      g_warning ("Failed to unregister client: %s", error->message);
      g_error_free (error);
    }

  g_clear_object (&app->priv->client_proxy);

  g_free (app->priv->client_path);
  app->priv->client_path = NULL;
}

static void
gtk_application_quit_response (GtkApplication *application,
                               gboolean        will_quit,
                               const gchar    *reason)
{
  g_debug ("Calling EndSessionResponse %d '%s'", will_quit, reason);

  g_dbus_proxy_call (application->priv->client_proxy,
                     "EndSessionResponse",
                     g_variant_new ("(bs)", will_quit, reason ? reason : ""),
                     G_DBUS_CALL_FLAGS_NONE,
                     G_MAXINT,
                     NULL, NULL, NULL);
}
static void
client_proxy_signal (GDBusProxy     *proxy,
                     const gchar    *sender_name,
                     const gchar    *signal_name,
                     GVariant       *parameters,
                     GtkApplication *app)
{
  if (strcmp (signal_name, "QueryEndSession") == 0)
    {
      g_debug ("Received QueryEndSession");
      gtk_application_quit_response (app, TRUE, NULL);
    }
  else if (strcmp (signal_name, "CancelEndSession") == 0)
    {
      g_debug ("Received CancelEndSession");
    }
  else if (strcmp (signal_name, "EndSession") == 0)
    {
      g_debug ("Received EndSession");
      gtk_application_quit_response (app, TRUE, NULL);
      unregister_client (app);
      g_signal_emit (app, gtk_application_signals[QUIT], 0);
    }
  else if (strcmp (signal_name, "Stop") == 0)
    {
      g_debug ("Received Stop");
      unregister_client (app);
      g_signal_emit (app, gtk_application_signals[QUIT], 0);
    }
}

static void
gtk_application_startup_session_dbus (GtkApplication *app)
{
  static gchar *client_id;
  GError *error = NULL;
  GVariant *res;

  if (app->priv->session_bus == NULL)
    return;

  if (client_id == NULL)
    {
      const gchar *desktop_autostart_id;

      desktop_autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
      /* Unset DESKTOP_AUTOSTART_ID in order to avoid child processes to
       * use the same client id.
       */
      g_unsetenv ("DESKTOP_AUTOSTART_ID");
      client_id = g_strdup (desktop_autostart_id ? desktop_autostart_id : "");
    }

  g_debug ("Connecting to session manager");

  app->priv->sm_proxy = g_dbus_proxy_new_sync (app->priv->session_bus,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                               G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                               NULL,
                                               "org.gnome.SessionManager",
                                               "/org/gnome/SessionManager",
                                               "org.gnome.SessionManager",
                                               NULL,
                                               &error);
  if (error)
    {
      g_warning ("Failed to get a session proxy: %s", error->message);
      g_error_free (error);
      return;
    }

  /* FIXME: should we reuse the D-Bus application id here ? */
  app->priv->app_id = g_strdup (g_get_prgname ());

  if (!app->priv->register_session)
    return;

  g_debug ("Registering client '%s' '%s'", app->priv->app_id, client_id);

  res = g_dbus_proxy_call_sync (app->priv->sm_proxy,
                                "RegisterClient",
                                g_variant_new ("(ss)", app->priv->app_id, client_id),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);

  if (error)
    {
      g_warning ("Failed to register client: %s", error->message);
      g_error_free (error);
      g_clear_object (&app->priv->sm_proxy);
      return;
    }

  g_variant_get (res, "(o)", &app->priv->client_path);
  g_variant_unref (res);

  g_debug ("Registered client at '%s'", app->priv->client_path);

  app->priv->client_proxy = g_dbus_proxy_new_sync (app->priv->session_bus, 0,
                                                   NULL,
                                                   "org.gnome.SessionManager",
                                                   app->priv->client_path,
                                                   "org.gnome.SessionManager.ClientPrivate",
                                                   NULL,
                                                   &error);
  if (error)
    {
      g_warning ("Failed to get client proxy: %s", error->message);
      g_error_free (error);
      g_clear_object (&app->priv->sm_proxy);
      g_free (app->priv->client_path);
      app->priv->client_path = NULL;
      return;
    }

  g_signal_connect (app->priv->client_proxy, "g-signal", G_CALLBACK (client_proxy_signal), app);
}



/**
 * GtkApplicationInhibitFlags:
 * @GTK_APPLICATION_INHIBIT_LOGOUT: Inhibit ending the user session
 *     by logging out or by shutting down the computer
 * @GTK_APPLICATION_INHIBIT_SWITCH: Inhibit user switching
 * @GTK_APPLICATION_INHIBIT_SUSPEND: Inhibit suspending the
 *     session or computer
 * @GTK_APPLICATION_INHIBIT_IDLE: Inhibit the session being
 *     marked as idle (and possibly locked)
 *
 * Types of user actions that may be blocked by gtk_application_inhibit().
 *
 * Since: 3.4
 */

/**
 * gtk_application_inhibit:
 * @application: the #GApplication
 * @window: (allow-none): a #GtkWindow, or %NULL
 * @flags: what types of actions should be inhibited
 * @reason: (allow-none): a short, human-readable string that explains
 *     why these operations are inhibited
 *
 * Inform the session manager that certain types of actions should be
 * inhibited. This is not guaranteed to work on all platforms and for
 * all types of actions.
 *
 * Applications should invoke this method when they begin an operation
 * that should not be interrupted, such as creating a CD or DVD. The
 * types of actions that may be blocked are specified by the @flags
 * parameter. When the application completes the operation it should
 * call g_application_uninhibit() to remove the inhibitor. Note that
 * an application can have multiple inhibitors, and all of the must
 * be individually removed. Inhibitors are also cleared when the
 * application exits.
 *
 * Applications should not expect that they will always be able to block
 * the action. In most cases, users will be given the option to force
 * the action to take place.
 *
 * Reasons should be short and to the point.
 *
 * If @window is given, the session manager may point the user to
 * this window to find out more about why the action is inhibited.
 *
 * Returns: A non-zero cookie that is used to uniquely identify this
 *     request. It should be used as an argument to g_application_uninhibit()
 *     in order to remove the request. If the platform does not support
 *     inhibiting or the request failed for some reason, 0 is returned.
 *
 * Since: 3.4
 */
guint
gtk_application_inhibit (GtkApplication             *application,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const gchar                *reason)
{
  GVariant *res;
  GError *error = NULL;
  guint cookie;
  guint xid;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), 0);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), 0);
  g_return_val_if_fail (application->priv->sm_proxy != NULL, 0);

  if (window != NULL)
    xid = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (window)));
  else
    xid = 0;

  res = g_dbus_proxy_call_sync (application->priv->sm_proxy,
                                "Inhibit",
                                g_variant_new ("(susu)",
                                               application->priv->app_id,
                                               xid,
                                               reason,
                                               flags),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);
 if (error)
    {
      g_warning ("Calling Inhibit failed: %s", error->message);
      g_error_free (error);
      return 0;
    }

  g_variant_get (res, "(u)", &cookie);
  g_variant_unref (res);

  return cookie;
}

/**
 * gtk_application_uninhibit:
 * @application: the #GApplication
 * @cookie: a cookie that was returned by g_application_inhibit()
 *
 * Removes an inhibitor that has been established with g_application_inhibit().
 * Inhibitors are also cleared when the application exits.
 *
 * Since: 3.4
 */
void
gtk_application_uninhibit (GtkApplication *application,
                           guint           cookie)
{
  g_return_if_fail (GTK_IS_APPLICATION (application));
  g_return_if_fail (!g_application_get_is_remote (G_APPLICATION (application)));
  g_return_if_fail (application->priv->sm_proxy != NULL);

  g_dbus_proxy_call (application->priv->sm_proxy,
                     "Uninhibit",
                     g_variant_new ("(u)", cookie),
                     G_DBUS_CALL_FLAGS_NONE,
                     G_MAXINT,
                     NULL, NULL, NULL);
}

/**
 * gtk_application_is_inhibited:
 * @application: the #GApplication
 * @flags: what types of actions should be queried
 *
 * Determines if any of the actions specified in @flags are
 * currently inhibited (possibly by another application).
 *
 * Returns: %TRUE if any of the actions specified in @flags are inhibited
 *
 * Since: 3.4
 */
gboolean
gtk_application_is_inhibited (GtkApplication             *application,
                              GtkApplicationInhibitFlags  flags)
{
  GVariant *res;
  GError *error = NULL;
  gboolean inhibited;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), FALSE);
  g_return_val_if_fail (application->priv->sm_proxy != NULL, FALSE);

  res = g_dbus_proxy_call_sync (application->priv->sm_proxy,
                                "IsInhibited",
                                g_variant_new ("(u)", flags),
                                G_DBUS_CALL_FLAGS_NONE,
                                G_MAXINT,
                                NULL,
                                &error);
  if (error)
    {
      g_warning ("Calling IsInhibited failed: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  g_variant_get (res, "(b)", &inhibited);
  g_variant_unref (res);

  return inhibited;
}

/**
 * GtkApplicationEndSessionStyle:
 * @GTK_APPLICATION_LOGOUT: End the session by logging out
 * @GTK_APPLICATION_REBOOT: Restart the computer
 * @GTK_APPLICATION_SHUTDOWN: Shut the computer down
 *
 * Different ways to end a user session, for use with
 * gtk_application_end_session().
 */

/**
 * gtk_application_end_session:
 * @application: the #GtkApplication
 * @style: the desired kind of session end
 * @request_confirmation: whether or not the user should get a chance
 *     to confirm the action
 *
 * Requests that the session manager end the current session.
 * @style indicates how the session should be ended, and
 * @request_confirmation indicates whether or not the user should be
 * given a chance to confirm the action. Both of these parameters are
 * merely hints though; the session manager may choose to ignore them.
 *
 * Return value: %TRUE if the request was sent; %FALSE if it could not
 *     be sent (eg, because it could not connect to the session manager)
 *
 * Since: 3.4
 */
gboolean
gtk_application_end_session (GtkApplication                *application,
                             GtkApplicationEndSessionStyle  style,
                             gboolean                       request_confirmation)
{
  g_return_val_if_fail (GTK_IS_APPLICATION (application), FALSE);
  g_return_val_if_fail (!g_application_get_is_remote (G_APPLICATION (application)), FALSE);
  g_return_val_if_fail (application->priv->sm_proxy != NULL, FALSE);

  switch (style)
    {
    case GTK_APPLICATION_LOGOUT:
      g_dbus_proxy_call (application->priv->sm_proxy,
                         "Logout",
                         g_variant_new ("(u)", request_confirmation ? 0 : 1),
                         G_DBUS_CALL_FLAGS_NONE,
                         G_MAXINT,
                         NULL, NULL, NULL);
      break;
    case GTK_APPLICATION_REBOOT:
    case GTK_APPLICATION_SHUTDOWN:
      g_dbus_proxy_call (application->priv->sm_proxy,
                         "Shutdown",
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         G_MAXINT,
                         NULL, NULL, NULL);
      break;
    }

  return TRUE;
}

#elif defined(GDK_WINDOWING_QUARTZ)

/* OS X implementation copied from EggSMClient, but simplified since
 * it doesn't need to interact with the user.
 */

static gboolean
idle_will_quit (gpointer data)
{
  GtkApplication *app = data;

  if (app->priv->quit_inhibit == 0)
    g_signal_emit (app, gtk_application_signals[QUIT], 0);
  else
    {
      GtkApplicationQuartzInhibitor *inhibitor;
      GSList *iter;
      GtkWidget *dialog;

      for (iter = app->priv->inhibitors; iter; iter = iter->next)
	{
	  inhibitor = iter->data;
	  if (inhibitor->flags & GTK_APPLICATION_INHIBIT_LOGOUT)
	    break;
        }
      g_assert (inhibitor != NULL);

      dialog = gtk_message_dialog_new (inhibitor->window,
				       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("%s cannot quit at this time:\n\n%s"),
				       g_get_application_name (),
				       inhibitor->reason);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }

  return G_SOURCE_REMOVE;
}

static pascal OSErr
quit_requested (const AppleEvent *aevt,
                AppleEvent       *reply,
                long              refcon)
{
  GtkApplication *app = GSIZE_TO_POINTER ((gsize)refcon);

  /* Don't emit the "quit" signal immediately, since we're
   * called from a weird point in the guts of gdkeventloop-quartz.c
   */
  g_idle_add_full (G_PRIORITY_DEFAULT, idle_will_quit, app, NULL);

  return app->priv->quit_inhibit == 0 ? noErr : userCanceledErr;
}

static void
gtk_application_startup_session_quartz (GtkApplication *app)
{
  if (app->priv->register_session)
    AEInstallEventHandler (kCoreEventClass, kAEQuitApplication,
                           NewAEEventHandlerUPP (quit_requested),
                           (long)GPOINTER_TO_SIZE (app), false);
}

guint
gtk_application_inhibit (GtkApplication             *application,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const gchar                *reason)
{
  GtkApplicationQuartzInhibitor *inhibitor;

  g_return_val_if_fail (GTK_IS_APPLICATION (application), 0);
  g_return_val_if_fail (flags != 0, 0);

  inhibitor = g_slice_new (GtkApplicationQuartzInhibitor);
  inhibitor->cookie = ++application->priv->next_cookie;
  inhibitor->flags = flags;
  inhibitor->reason = g_strdup (reason);
  inhibitor->window = window ? g_object_ref (window) : NULL;

  application->priv->inhibitors = g_slist_prepend (application->priv->inhibitors, inhibitor);

  if (flags & GTK_APPLICATION_INHIBIT_LOGOUT)
    application->priv->quit_inhibit++;

  return inhibitor->cookie;
}

void
gtk_application_uninhibit (GtkApplication *application,
                           guint           cookie)
{
  GSList *iter;

  for (iter = application->priv->inhibitors; iter; iter = iter->next)
    {
      GtkApplicationQuartzInhibitor *inhibitor = iter->data;

      if (inhibitor->cookie == cookie)
	{
	  if (inhibitor->flags & GTK_APPLICATION_INHIBIT_LOGOUT)
	    application->priv->quit_inhibit--;
	  gtk_application_quartz_inhibitor_free (inhibitor);
	  application->priv->inhibitors = g_slist_delete_link (application->priv->inhibitors, iter);
	  return;
	}
    }

  g_warning ("Invalid inhibitor cookie");
}

gboolean
gtk_application_is_inhibited (GtkApplication             *application,
                              GtkApplicationInhibitFlags  flags)
{
  if (flags & GTK_APPLICATION_INHIBIT_LOGOUT)
    return application->priv->quit_inhibit > 0;

  return FALSE;
}

gboolean
gtk_application_end_session (GtkApplication                *application,
                             GtkApplicationEndSessionStyle  style,
                             gboolean                       request_confirmation)
{
  static const ProcessSerialNumber loginwindow_psn = { 0, kSystemProcess };
  AppleEvent event = { typeNull, NULL };
  AppleEvent reply = { typeNull, NULL };
  AEAddressDesc target;
  AEEventID id;
  OSErr err;

  switch (style)
    {
    case GTK_APPLICATION_LOGOUT:
      id = request_confirmation ? kAELogOut : kAEReallyLogOut;
      break;
    case GTK_APPLICATION_REBOOT:
      id = request_confirmation ? kAEShowRestartDialog : kAERestart;
      break;
    case GTK_APPLICATION_SHUTDOWN:
      id = request_confirmation ? kAEShowShutdownDialog : kAEShutDown;
      break;
    }

  err = AECreateDesc (typeProcessSerialNumber, &loginwindow_psn,
                      sizeof (loginwindow_psn), &target);
  if (err != noErr)
    {
      g_warning ("Could not create descriptor for loginwindow: %d", err);
      return FALSE;
    }

  err = AECreateAppleEvent (kCoreEventClass, id, &target,
                            kAutoGenerateReturnID, kAnyTransactionID,
                            &event);
  AEDisposeDesc (&target);
  if (err != noErr)
    {
      g_warning ("Could not create logout AppleEvent: %d", err);
      return FALSE;
    }

  err = AESend (&event, &reply, kAENoReply, kAENormalPriority,
                kAEDefaultTimeout, NULL, NULL);
  AEDisposeDesc (&event);
 if (err == noErr)
    AEDisposeDesc (&reply);

  return err == noErr;
}

#else

/* Trivial implementation.
 *
 * For the inhibit API on Windows, see
 * http://msdn.microsoft.com/en-us/library/ms700677%28VS.85%29.aspx
 */

guint
gtk_application_inhibit (GtkApplication             *application,
                         GtkWindow                  *window,
                         GtkApplicationInhibitFlags  flags,
                         const gchar                *reason)
{
  return 0;
}

void
gtk_application_uninhibit (GtkApplication *application,
                           guint           cookie)
{
}

gboolean
gtk_application_is_inhibited (GtkApplication             *application,
                              GtkApplicationInhibitFlags  flags)
{
  return FALSE;
}

gboolean
gtk_application_end_session (GtkApplication                *application,
                             GtkApplicationEndSessionStyle  style,
                             gboolean                       request_confirmation)
{
  return FALSE;
}

#endif
