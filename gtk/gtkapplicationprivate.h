/*
 * Copyright © 2011, 2013 Canonical Limited
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


#ifndef __GTK_APPLICATION_PRIVATE_H__
#define __GTK_APPLICATION_PRIVATE_H__

#include "gtkapplicationwindow.h"
#include "gtkwindowprivate.h"

#include "gtkactionmuxer.h"

G_GNUC_INTERNAL
void                    gtk_application_window_set_id                   (GtkApplicationWindow     *window,
                                                                         guint                     id);
G_GNUC_INTERNAL
GActionGroup *          gtk_application_window_get_action_group         (GtkApplicationWindow     *window);
G_GNUC_INTERNAL
void                    gtk_application_handle_window_realize           (GtkApplication           *application,
                                                                         GtkWindow                *window);
G_GNUC_INTERNAL
void                    gtk_application_handle_window_map               (GtkApplication           *application,
                                                                         GtkWindow                *window);
G_GNUC_INTERNAL
GtkActionMuxer *        gtk_application_get_parent_muxer_for_window     (GtkWindow                *window);

G_GNUC_INTERNAL
gboolean                gtk_application_activate_accel                  (GtkApplication           *application,
                                                                         GActionGroup             *action_group,
                                                                         guint                     key,
                                                                         GdkModifierType           modifier);

G_GNUC_INTERNAL
void                    gtk_application_foreach_accel_keys              (GtkApplication           *application,
                                                                         GtkWindow                *window,
                                                                         GtkWindowKeysForeachFunc  callback,
                                                                         gpointer                  user_data);
G_GNUC_INTERNAL
GtkActionMuxer *        gtk_application_get_action_muxer                (GtkApplication           *application);
G_GNUC_INTERNAL
void                    gtk_application_insert_action_group             (GtkApplication           *application,
                                                                         const gchar              *name,
                                                                         GActionGroup             *action_group);


#define GTK_TYPE_APPLICATION_IMPL                           (gtk_application_impl_get_type ())
#define GTK_APPLICATION_IMPL_CLASS(class)                   (G_TYPE_CHECK_CLASS_CAST ((class),                     \
                                                             GTK_TYPE_APPLICATION_IMPL,                            \
                                                             GtkApplicationImplClass))
#define GTK_APPLICATION_IMPL_GET_CLASS(obj)                 (G_TYPE_INSTANCE_GET_CLASS ((obj),                     \
                                                             GTK_TYPE_APPLICATION_IMPL,                            \
                                                             GtkApplicationImplClass))

typedef struct
{
  GObject parent_instance;
  GtkApplication *application;
  GdkDisplay *display;
} GtkApplicationImpl;

typedef struct
{
  GObjectClass parent_class;

  void        (* startup)                   (GtkApplicationImpl          *impl,
                                             gboolean                     register_session);
  void        (* shutdown)                  (GtkApplicationImpl          *impl);

  void        (* before_emit)               (GtkApplicationImpl          *impl,
                                             GVariant                    *platform_data);

  void        (* window_added)              (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);
  void        (* window_removed)            (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);
  void        (* active_window_changed)     (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);
  void        (* handle_window_realize)     (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);
  void        (* handle_window_map)         (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window);

  void        (* set_app_menu)              (GtkApplicationImpl          *impl,
                                             GMenuModel                  *app_menu);
  void        (* set_menubar)               (GtkApplicationImpl          *impl,
                                             GMenuModel                  *menubar);

  guint       (* inhibit)                   (GtkApplicationImpl          *impl,
                                             GtkWindow                   *window,
                                             GtkApplicationInhibitFlags   flags,
                                             const gchar                 *reason);
  void        (* uninhibit)                 (GtkApplicationImpl          *impl,
                                             guint                        cookie);
  gboolean    (* is_inhibited)              (GtkApplicationImpl          *impl,
                                             GtkApplicationInhibitFlags   flags);


} GtkApplicationImplClass;

#define GTK_TYPE_APPLICATION_IMPL_DBUS                      (gtk_application_impl_dbus_get_type ())
#define GTK_APPLICATION_IMPL_DBUS_CLASS(class)              (G_TYPE_CHECK_CLASS_CAST ((class),                     \
                                                             GTK_TYPE_APPLICATION_IMPL_DBUS,                       \
                                                             GtkApplicationImplDBusClass))
#define GTK_APPLICATION_IMPL_DBUS_GET_CLASS(obj)            (G_TYPE_INSTANCE_GET_CLASS ((obj),                     \
                                                             GTK_TYPE_APPLICATION_IMPL_DBUS,                       \
                                                             GtkApplicationImplDBusClass))

typedef struct
{
  GtkApplicationImpl impl;

  GDBusConnection *session;

  const gchar     *application_id;
  const gchar     *unique_name;
  const gchar     *object_path;

  gchar           *app_menu_path;
  guint            app_menu_id;

  gchar           *menubar_path;
  guint            menubar_id;

  /* Session management... */
  gchar           *app_id; /* actually prgname... */
  GDBusProxy      *sm_proxy;
  GDBusProxy      *client_proxy;
  gchar           *client_path;
} GtkApplicationImplDBus;

typedef struct
{
  GtkApplicationImplClass parent_class;

  /* returns floating */
  GVariant *  (* get_window_system_id)      (GtkApplicationImplDBus      *dbus,
                                             GtkWindow                   *window);
} GtkApplicationImplDBusClass;

G_GNUC_INTERNAL
GType                   gtk_application_impl_get_type                   (void);
G_GNUC_INTERNAL
GType                   gtk_application_impl_dbus_get_type              (void);
G_GNUC_INTERNAL
GType                   gtk_application_impl_x11_get_type               (void);
G_GNUC_INTERNAL
GType                   gtk_application_impl_wayland_get_type           (void);
G_GNUC_INTERNAL
GType                   gtk_application_impl_quartz_get_type            (void);

G_GNUC_INTERNAL
GtkApplicationImpl *    gtk_application_impl_new                        (GtkApplication              *application,
                                                                         GdkDisplay                  *display);
G_GNUC_INTERNAL
void                    gtk_application_impl_startup                    (GtkApplicationImpl          *impl,
                                                                         gboolean                     register_sesion);
G_GNUC_INTERNAL
void                    gtk_application_impl_shutdown                   (GtkApplicationImpl          *impl);
G_GNUC_INTERNAL
void                    gtk_application_impl_before_emit                (GtkApplicationImpl          *impl,
                                                                         GVariant                    *platform_data);
G_GNUC_INTERNAL
void                    gtk_application_impl_window_added               (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
G_GNUC_INTERNAL
void                    gtk_application_impl_window_removed             (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
G_GNUC_INTERNAL
void                    gtk_application_impl_active_window_changed      (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
G_GNUC_INTERNAL
void                    gtk_application_impl_handle_window_realize      (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
G_GNUC_INTERNAL
void                    gtk_application_impl_handle_window_map          (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window);
G_GNUC_INTERNAL
void                    gtk_application_impl_set_app_menu               (GtkApplicationImpl          *impl,
                                                                         GMenuModel                  *app_menu);
G_GNUC_INTERNAL
void                    gtk_application_impl_set_menubar                (GtkApplicationImpl          *impl,
                                                                         GMenuModel                  *menubar);
G_GNUC_INTERNAL
guint                   gtk_application_impl_inhibit                    (GtkApplicationImpl          *impl,
                                                                         GtkWindow                   *window,
                                                                         GtkApplicationInhibitFlags   flags,
                                                                         const gchar                 *reason);
G_GNUC_INTERNAL
void                    gtk_application_impl_uninhibit                  (GtkApplicationImpl          *impl,
                                                                         guint                        cookie);
G_GNUC_INTERNAL
gboolean                gtk_application_impl_is_inhibited               (GtkApplicationImpl          *impl,
                                                                         GtkApplicationInhibitFlags   flags);

G_GNUC_INTERNAL
gchar *                 gtk_application_impl_dbus_get_window_path       (GtkApplicationImplDBus      *dbus,
                                                                         GtkWindow                   *window);

G_GNUC_INTERNAL
void                    gtk_application_impl_quartz_setup_menu          (GMenuModel                  *model,
                                                                         GtkActionMuxer              *muxer);

#endif /* __GTK_APPLICATION_PRIVATE_H__ */
