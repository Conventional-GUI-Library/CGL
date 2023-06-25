/* GDK - The GIMP Drawing Kit
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * Private uninstalled header defining things local to X windowing code
 */

#ifndef __GDK_PRIVATE_WAYLAND_H__
#define __GDK_PRIVATE_WAYLAND_H__

#include <gdk/gdkcursor.h>
#include <gdk/gdkprivate.h>
#include <gdk/wayland/gdkdisplay-wayland.h>

#include "gdkinternals.h"

#include "config.h"

#define GDK_SCREEN_DISPLAY(screen)    (GDK_SCREEN_WAYLAND (screen)->display)
#define GDK_WINDOW_SCREEN(win)	      (gdk_window_get_screen (win))
#define GDK_WINDOW_DISPLAY(win)       (GDK_SCREEN_WAYLAND (GDK_WINDOW_SCREEN (win))->display)
#define GDK_WINDOW_IS_WAYLAND(win)    (GDK_IS_WINDOW_IMPL_WAYLAND (((GdkWindow *)win)->impl))

GType _gdk_wayland_window_get_type    (void);
void  _gdk_wayland_window_update_size (GdkWindow *window,
				       int32_t width,
				       int32_t height,
				       uint32_t edges);

GdkKeymap *_gdk_wayland_keymap_new (GdkDisplay *display);
struct xkb_desc *_gdk_wayland_keymap_get_xkb_desc (GdkKeymap *keymap);

GdkCursor *_gdk_wayland_display_get_cursor_for_type (GdkDisplay    *display,
						     GdkCursorType  cursor_type);
GdkCursor *_gdk_wayland_display_get_cursor_for_name (GdkDisplay  *display,
						     const gchar *name);
GdkCursor *_gdk_wayland_display_get_cursor_for_pixbuf (GdkDisplay *display,
						       GdkPixbuf  *pixbuf,
						       gint        x,
						       gint        y);
void       _gdk_wayland_display_get_default_cursor_size (GdkDisplay *display,
							 guint       *width,
							 guint       *height);
void       _gdk_wayland_display_get_maximal_cursor_size (GdkDisplay *display,
							 guint       *width,
							 guint       *height);
gboolean   _gdk_wayland_display_supports_cursor_alpha (GdkDisplay *display);
gboolean   _gdk_wayland_display_supports_cursor_color (GdkDisplay *display);

struct wl_buffer *_gdk_wayland_cursor_get_buffer (GdkCursor *cursor,
						  int       *x,
						  int       *y);

GdkDragProtocol _gdk_wayland_window_get_drag_protocol (GdkWindow *window,
						       GdkWindow **target);

void            _gdk_wayland_window_register_dnd (GdkWindow *window);
GdkDragContext *_gdk_wayland_window_drag_begin (GdkWindow *window,
						GdkDevice *device,
						GList     *targets);

void _gdk_wayland_display_create_window_impl (GdkDisplay    *display,
					      GdkWindow     *window,
					      GdkWindow     *real_parent,
					      GdkScreen     *screen,
					      GdkEventMask   event_mask,
					      GdkWindowAttr *attributes,
					      gint           attributes_mask);

GdkKeymap *_gdk_wayland_display_get_keymap (GdkDisplay *display);

GdkWindow *_gdk_wayland_display_get_selection_owner (GdkDisplay *display,
						 GdkAtom     selection);
gboolean   _gdk_wayland_display_set_selection_owner (GdkDisplay *display,
						     GdkWindow  *owner,
						     GdkAtom     selection,
						     guint32     time,
						     gboolean    send_event);
void       _gdk_wayland_display_send_selection_notify (GdkDisplay *dispay,
						       GdkWindow        *requestor,
						       GdkAtom          selection,
						       GdkAtom          target,
						       GdkAtom          property,
						       guint32          time);
gint       _gdk_wayland_display_get_selection_property (GdkDisplay  *display,
							GdkWindow   *requestor,
							guchar     **data,
							GdkAtom     *ret_type,
							gint        *ret_format);
void       _gdk_wayland_display_convert_selection (GdkDisplay *display,
						   GdkWindow  *requestor,
						   GdkAtom     selection,
						   GdkAtom     target,
						   guint32     time);
gint        _gdk_wayland_display_text_property_to_utf8_list (GdkDisplay    *display,
							     GdkAtom        encoding,
							     gint           format,
							     const guchar  *text,
							     gint           length,
							     gchar       ***list);
gchar *     _gdk_wayland_display_utf8_to_string_target (GdkDisplay  *display,
							const gchar *str);

GdkDeviceManager *_gdk_wayland_device_manager_new (GdkDisplay *display);
void              _gdk_wayland_device_manager_add_device (GdkDeviceManager *device_manager,
							  struct wl_input_device *device);
struct wl_input_device *_gdk_wayland_device_get_device (GdkDevice *device);

void     _gdk_wayland_display_deliver_event (GdkDisplay *display, GdkEvent *event);
GSource *_gdk_wayland_display_event_source_new (GdkDisplay *display);
void     _gdk_wayland_display_queue_events (GdkDisplay *display);
void     _gdk_wayland_display_flush (GdkDisplay *display, GSource *source);

GdkAppLaunchContext *_gdk_wayland_display_get_app_launch_context (GdkDisplay *display);

GdkDisplay *_gdk_wayland_display_open (const gchar *display_name);
void        _gdk_wayland_display_make_default (GdkDisplay *display);

GdkWindow *_gdk_wayland_screen_create_root_window (GdkScreen *screen,
						   int width,
						   int height);

GdkScreen *_gdk_wayland_screen_new (GdkDisplay *display);

void _gdk_wayland_display_manager_add_display (GdkDisplayManager *manager,
					       GdkDisplay        *display);
void _gdk_wayland_display_manager_remove_display (GdkDisplayManager *manager,
						  GdkDisplay        *display);

#endif /* __GDK_PRIVATE_WAYLAND_H__ */
