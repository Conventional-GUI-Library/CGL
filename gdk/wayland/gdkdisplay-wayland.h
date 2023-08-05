/*
 * gdkdisplay-wayland.h
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_WAYLAND_DISPLAY__
#define __GDK_WAYLAND_DISPLAY__

#include <config.h>
#include <stdint.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <gdk/wayland/gtk-shell-client-protocol.h>
#include <gdk/wayland/xdg-shell-client-protocol.h>

#include <glib.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkinternals.h>
#include <gdk/gdk.h>		/* For gdk_get_program_class() */

#include "gdkdisplayprivate.h"

G_BEGIN_DECLS

struct _GdkWaylandDisplay
{
  GdkDisplay parent_instance;
  GdkScreen *screen;

  /* input GdkDevice list */
  GList *input_devices;

  /* Startup notification */
  gchar *startup_notification_id;

  /* Time of most recent user interaction and most recent serial */
  gulong user_time;
  guint32 serial;

  /* Wayland fields below */
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct xdg_shell *xdg_shell;
  struct gtk_shell *gtk_shell;
  struct wl_input_device *input_device;
  struct wl_data_device_manager *data_device_manager;

  struct wl_cursor_theme *cursor_theme;
  GSList *cursor_cache;

  GSource *event_source;

  int init_ref_count;
  int compositor_version;

  struct xkb_context *xkb_context;
};

struct _GdkWaylandDisplayClass
{
  GdkDisplayClass parent_class;
};

G_END_DECLS

#endif				/* __GDK_WAYLAND_DISPLAY__ */
