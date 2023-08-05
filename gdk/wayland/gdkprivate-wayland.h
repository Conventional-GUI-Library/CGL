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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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
#include <gdk/wayland/gdkwayland.h>
#include <gdk/wayland/gdkdisplay-wayland.h>

#include <xkbcommon/xkbcommon.h>

#include "gdkinternals.h"

#include "config.h"

#define GDK_WINDOW_IS_WAYLAND(win)    (GDK_IS_WINDOW_IMPL_WAYLAND (((GdkWindow *)win)->impl))

GdkKeymap *_gdk_wayland_keymap_new (void);
void       _gdk_wayland_keymap_update_from_fd (GdkKeymap *keymap,
                                               uint32_t   format,
                                               uint32_t   fd,
                                               uint32_t   size);
struct xkb_state *_gdk_wayland_keymap_get_xkb_state (GdkKeymap *keymap);
struct xkb_keymap *_gdk_wayland_keymap_get_xkb_keymap (GdkKeymap *keymap);
gboolean           _gdk_wayland_keymap_key_is_modifier (GdkKeymap *keymap,
                                                        guint      keycode);

void       _gdk_wayland_display_finalize_cursors (GdkWaylandDisplay *display);
void       _gdk_wayland_display_update_cursors (GdkWaylandDisplay      *display,
                                                struct wl_cursor_theme *theme);

GdkCursor *_gdk_wayland_display_get_cursor_for_type (GdkDisplay    *display,
						     GdkCursorType  cursor_type);
GdkCursor *_gdk_wayland_display_get_cursor_for_name (GdkDisplay  *display,
						     const gchar *name);
GdkCursor *_gdk_wayland_display_get_cursor_for_surface (GdkDisplay *display,
							cairo_surface_t *surface,
							gdouble     x,
							gdouble     y);
void       _gdk_wayland_display_get_default_cursor_size (GdkDisplay *display,
							 guint       *width,
							 guint       *height);
void       _gdk_wayland_display_get_maximal_cursor_size (GdkDisplay *display,
							 guint       *width,
							 guint       *height);
gboolean   _gdk_wayland_display_supports_cursor_alpha (GdkDisplay *display);
gboolean   _gdk_wayland_display_supports_cursor_color (GdkDisplay *display);

struct wl_buffer *_gdk_wayland_cursor_get_buffer (GdkCursor *cursor,
                                                  guint      image_index,
                                                  int       *hotspot_x,
                                                  int       *hotspot_y,
                                                  int       *w,
                                                  int       *h,
						  int       *scale);
guint      _gdk_wayland_cursor_get_next_image_index (GdkCursor *cursor,
                                                     guint      current_image_index,
                                                     guint     *next_image_delay);

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
void              _gdk_wayland_device_manager_add_seat (GdkDeviceManager *device_manager,
                                                        guint32           id,
						        struct wl_seat   *seat);
void              _gdk_wayland_device_manager_remove_seat (GdkDeviceManager *device_manager,
                                                           guint32           id);

typedef struct _GdkWaylandDeviceData GdkWaylandDeviceData;

GdkKeymap *_gdk_wayland_device_get_keymap (GdkDevice *device);
uint32_t _gdk_wayland_device_get_button_press_serial(GdkWaylandDeviceData *device);

void     _gdk_wayland_display_deliver_event (GdkDisplay *display, GdkEvent *event);
GSource *_gdk_wayland_display_event_source_new (GdkDisplay *display);
void     _gdk_wayland_display_queue_events (GdkDisplay *display);

GdkAppLaunchContext *_gdk_wayland_display_get_app_launch_context (GdkDisplay *display);

GdkDisplay *_gdk_wayland_display_open (const gchar *display_name);

GdkWindow *_gdk_wayland_screen_create_root_window (GdkScreen *screen,
						   int width,
						   int height);

GdkScreen *_gdk_wayland_screen_new (GdkDisplay *display);
void _gdk_wayland_screen_add_output (GdkScreen        *screen,
                                     guint32           id,
                                     struct wl_output *output,
				     guint32           version);
void _gdk_wayland_screen_remove_output (GdkScreen *screen,
                                        guint32 id);
int _gdk_wayland_screen_get_output_refresh_rate (GdkScreen        *screen,
                                                 struct wl_output *output);
guint32 _gdk_wayland_screen_get_output_scale (GdkScreen        *screen,
					      struct wl_output *output);

void _gdk_wayland_screen_set_has_gtk_shell (GdkScreen       *screen);

void _gdk_wayland_window_set_device_grabbed (GdkWindow      *window,
                                             GdkDevice      *device,
                                             struct wl_seat *seat,
                                             guint32         time_);

guint32 _gdk_wayland_display_get_serial (GdkWaylandDisplay *wayland_display);
void _gdk_wayland_display_update_serial (GdkWaylandDisplay *wayland_display, guint32 serial);

struct wl_shm_pool * _create_shm_pool (struct wl_shm *shm,
                                      int             width,
                                      int             height,
                                      size_t         *buf_length,
                                      void          **data_out);

#endif /* __GDK_PRIVATE_WAYLAND_H__ */
