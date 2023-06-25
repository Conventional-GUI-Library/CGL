/*
 * Copyright © 2010 Intel Corporation
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <wayland-egl.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include "gdkwayland.h"
#include "gdkdisplay.h"
#include "gdkdisplay-wayland.h"
#include "gdkscreen.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager.h"
#include "gdkkeysprivate.h"
#include "gdkprivate-wayland.h"

G_DEFINE_TYPE (GdkDisplayWayland, _gdk_display_wayland, GDK_TYPE_DISPLAY)

static void
gdk_input_init (GdkDisplay *display)
{
  GdkDisplayWayland *display_wayland;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GList *list, *l;

  display_wayland = GDK_DISPLAY_WAYLAND (display);
  device_manager = gdk_display_get_device_manager (display);

  /* For backwards compatibility, just add
   * floating devices that are not keyboards.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING);

  for (l = list; l; l = l->next)
    {
      device = l->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
	continue;

      display_wayland->input_devices = g_list_prepend (display_wayland->input_devices, l->data);
    }

  g_list_free (list);

  /* Now set "core" pointer to the first
   * master device that is a pointer.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (l = list; l; l = l->next)
    {
      device = list->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
	continue;

      display->core_pointer = device;
      break;
    }

  /* Add the core pointer to the devices list */
  display_wayland->input_devices = g_list_prepend (display_wayland->input_devices, display->core_pointer);

  g_list_free (list);
}

static void
shell_handle_configure(void *data, struct wl_shell *shell,
		       uint32_t time, uint32_t edges,
		       struct wl_surface *surface,
		       int32_t width, int32_t height)
{
  GdkWindow *window;
  GdkDisplay *display;
  GdkEvent *event;

  window = wl_surface_get_user_data(surface);

  display = gdk_window_get_display (window);

  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.window = window;
  event->configure.send_event = FALSE;
  event->configure.width = width;
  event->configure.height = height;

  _gdk_window_update_size (window);
  _gdk_wayland_window_update_size (window, width, height, edges);

  g_object_ref(window);

  _gdk_wayland_display_deliver_event (display, event);
}

static const struct wl_shell_listener shell_listener = {
  shell_handle_configure,
};

static void
output_handle_geometry(void *data,
		       struct wl_output *wl_output,
		       int x, int y, int physical_width, int physical_height,
		       int subpixel, const char *make, const char *model)
{
  /*
    g_signal_emit_by_name (screen, "monitors-changed");
    g_signal_emit_by_name (screen, "size-changed");
  */
}
static void
display_handle_mode(void *data,
                   struct wl_output *wl_output,
                   uint32_t flags,
                   int width,
                   int height,
                   int refresh)
{
}

static void
compositor_handle_visual(void *data,
			 struct wl_compositor *compositor,
			 uint32_t id, uint32_t token)
{
	GdkDisplayWayland *d = data;

	switch (token) {
	case WL_COMPOSITOR_VISUAL_ARGB32:
		d->argb_visual = wl_visual_create(d->wl_display, id, 1);
		break;
	case WL_COMPOSITOR_VISUAL_PREMULTIPLIED_ARGB32:
		d->premultiplied_argb_visual =
			wl_visual_create(d->wl_display, id, 1);
		break;
	case WL_COMPOSITOR_VISUAL_XRGB32:
		d->rgb_visual = wl_visual_create(d->wl_display, id, 1);
		break;
	}
}

static const struct wl_compositor_listener compositor_listener = {
	compositor_handle_visual,
};

static const struct wl_output_listener output_listener = {
	output_handle_geometry,
	display_handle_mode
};

static void
gdk_display_handle_global(struct wl_display *display, uint32_t id,
			  const char *interface, uint32_t version, void *data)
{
  GdkDisplayWayland *display_wayland = data;
  GdkDisplay *gdk_display = GDK_DISPLAY_OBJECT (data);
  struct wl_input_device *input;

  if (strcmp(interface, "wl_compositor") == 0) {
    display_wayland->compositor = wl_compositor_create(display, id, 1);
    wl_compositor_add_listener(display_wayland->compositor,
					   &compositor_listener, display_wayland);
  } else if (strcmp(interface, "wl_shm") == 0) {
    display_wayland->shm = wl_shm_create(display, id, 1);
  } else if (strcmp(interface, "wl_shell") == 0) {
    display_wayland->shell = wl_shell_create(display, id, 1);
    wl_shell_add_listener(display_wayland->shell,
			  &shell_listener, display_wayland);
  } else if (strcmp(interface, "wl_output") == 0) {
    display_wayland->output = wl_output_create(display, id, 1);
    wl_output_add_listener(display_wayland->output,
			   &output_listener, display_wayland);
  } else if (strcmp(interface, "wl_input_device") == 0) {
    input = wl_input_device_create(display, id, 1);
    _gdk_wayland_device_manager_add_device (gdk_display->device_manager,
					    input);
  }
}

static gboolean
gdk_display_init_egl(GdkDisplay *display)
{
  GdkDisplayWayland *display_wayland = GDK_DISPLAY_WAYLAND (display);
  EGLint major, minor, i;
  void *p;

  static const struct { const char *f; unsigned int offset; }
  extension_functions[] = {
    { "glEGLImageTargetTexture2DOES", offsetof(GdkDisplayWayland, image_target_texture_2d) },
    { "eglCreateImageKHR", offsetof(GdkDisplayWayland, create_image) },
    { "eglDestroyImageKHR", offsetof(GdkDisplayWayland, destroy_image) }
  };

  display_wayland->egl_display =
    eglGetDisplay(display_wayland->wl_display);
  if (!eglInitialize(display_wayland->egl_display, &major, &minor)) {
    fprintf(stderr, "failed to initialize display\n");
    return FALSE;
  }

  eglBindAPI(EGL_OPENGL_API);

  display_wayland->egl_context =
    eglCreateContext(display_wayland->egl_display, NULL, EGL_NO_CONTEXT, NULL);
  if (display_wayland->egl_context == NULL) {
    fprintf(stderr, "failed to create context\n");
    return FALSE;
  }

  if (!eglMakeCurrent(display_wayland->egl_display,
		      NULL, NULL, display_wayland->egl_context)) {
    fprintf(stderr, "faile to make context current\n");
    return FALSE;
  }

  display_wayland->cairo_device =
    cairo_egl_device_create(display_wayland->egl_display,
			    display_wayland->egl_context);
  if (cairo_device_status (display_wayland->cairo_device) != CAIRO_STATUS_SUCCESS) {
    fprintf(stderr, "failed to get cairo drm device\n");
    return FALSE;
  }

  for (i = 0; i < G_N_ELEMENTS(extension_functions); i++) {
    p = eglGetProcAddress(extension_functions[i].f);
    *(void **) ((char *) display_wayland + extension_functions[i].offset) = p;
    if (p == NULL) {
      fprintf(stderr, "failed to look up %s\n", extension_functions[i].f);
      return FALSE;
    }
  }

  return TRUE;
}

GdkDisplay *
_gdk_wayland_display_open (const gchar *display_name)
{
  struct wl_display *wl_display;
  GdkDisplay *display;
  GdkDisplayWayland *display_wayland;

  wl_display = wl_display_connect(display_name);
  if (!wl_display)
    return NULL;

  display = g_object_new (GDK_TYPE_DISPLAY_WAYLAND, NULL);
  display_wayland = GDK_DISPLAY_WAYLAND (display);

  display_wayland->wl_display = wl_display;

  display_wayland->screen = _gdk_wayland_screen_new (display);

  display->device_manager = _gdk_wayland_device_manager_new (display);

  /* Set up listener so we'll catch all events. */
  wl_display_add_global_listener(display_wayland->wl_display,
				 gdk_display_handle_global, display_wayland);

  gdk_display_init_egl(display);

  display_wayland->event_source =
    _gdk_wayland_display_event_source_new (display);

  gdk_input_init (display);

  g_signal_emit_by_name (display, "opened");
  g_signal_emit_by_name (gdk_display_manager_get(), "display_opened", display);

  return display;
}

static void
gdk_wayland_display_dispose (GObject *object)
{
  GdkDisplayWayland *display_wayland = GDK_DISPLAY_WAYLAND (object);

  _gdk_wayland_display_manager_remove_display (gdk_display_manager_get (),
					       GDK_DISPLAY (display_wayland));
  g_list_foreach (display_wayland->input_devices,
		  (GFunc) g_object_run_dispose, NULL);

  _gdk_screen_close (display_wayland->screen);

  if (display_wayland->event_source)
    {
      g_source_destroy (display_wayland->event_source);
      g_source_unref (display_wayland->event_source);
      display_wayland->event_source = NULL;
    }

  eglTerminate(display_wayland->egl_display);

  G_OBJECT_CLASS (_gdk_display_wayland_parent_class)->dispose (object);
}

static void
gdk_wayland_display_finalize (GObject *object)
{
  GdkDisplayWayland *display_wayland = GDK_DISPLAY_WAYLAND (object);

  /* Keymap */
  if (display_wayland->keymap)
    g_object_unref (display_wayland->keymap);

  /* input GdkDevice list */
  g_list_foreach (display_wayland->input_devices, (GFunc) g_object_unref, NULL);
  g_list_free (display_wayland->input_devices);

  g_object_unref (display_wayland->screen);

  g_free (display_wayland->startup_notification_id);

  G_OBJECT_CLASS (_gdk_display_wayland_parent_class)->finalize (object);
}

static const gchar *
gdk_wayland_display_get_name (GdkDisplay *display)
{
  return "Wayland";
}

static gint
gdk_wayland_display_get_n_screens (GdkDisplay *display)
{
  return 1;
}

static GdkScreen *
gdk_wayland_display_get_screen (GdkDisplay *display, 
				gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return GDK_DISPLAY_WAYLAND (display)->screen;
}

static GdkScreen *
gdk_wayland_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_WAYLAND (display)->screen;
}

static void
gdk_wayland_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

static void
sync_callback(void *data)
{
  gboolean *done = data;

  *done = TRUE;
}

static void
gdk_wayland_display_sync (GdkDisplay *display)
{
  GdkDisplayWayland *display_wayland;
  gboolean done;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_wayland = GDK_DISPLAY_WAYLAND (display);

  wl_display_sync_callback(display_wayland->wl_display, sync_callback, &done);
  wl_display_iterate(display_wayland->wl_display, WL_DISPLAY_WRITABLE);
  while (!done)
    wl_display_iterate(display_wayland->wl_display, WL_DISPLAY_READABLE);
}

static void
gdk_wayland_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (!display->closed)
    _gdk_wayland_display_flush (display,
				GDK_DISPLAY_WAYLAND (display)->event_source);
}

static gboolean
gdk_wayland_display_has_pending (GdkDisplay *display)
{
  return FALSE;
}

static GdkWindow *
gdk_wayland_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}


static gboolean
gdk_wayland_display_supports_selection_notification (GdkDisplay *display)
{
  return TRUE;
}

static gboolean
gdk_wayland_display_request_selection_notification (GdkDisplay *display,
						    GdkAtom     selection)

{
    return FALSE;
}

static gboolean
gdk_wayland_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_wayland_display_store_clipboard (GdkDisplay    *display,
				     GdkWindow     *clipboard_window,
				     guint32        time_,
				     const GdkAtom *targets,
				     gint           n_targets)
{
}

static gboolean
gdk_wayland_display_supports_shapes (GdkDisplay *display)
{
  return TRUE;
}

static gboolean
gdk_wayland_display_supports_input_shapes (GdkDisplay *display)
{
  return TRUE;
}

static gboolean
gdk_wayland_display_supports_composite (GdkDisplay *display)
{
  return TRUE;
}

static GList *
gdk_wayland_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_DISPLAY_WAYLAND (display)->input_devices;
}

static void
gdk_wayland_display_before_process_all_updates (GdkDisplay *display)
{
}

static void
gdk_wayland_display_after_process_all_updates (GdkDisplay *display)
{
  /* Post the damage here instead? */
}

static gulong
gdk_wayland_display_get_next_serial (GdkDisplay *display)
{
  return 0;
}

void
_gdk_wayland_display_make_default (GdkDisplay *display)
{
}

/**
 * gdk_wayland_display_broadcast_startup_message:
 * @display: a #GdkDisplay
 * @message_type: startup notification message type ("new", "change",
 * or "remove")
 * @...: a list of key/value pairs (as strings), terminated by a
 * %NULL key. (A %NULL value for a key will cause that key to be
 * skipped in the output.)
 *
 * Sends a startup notification message of type @message_type to
 * @display. 
 *
 * This is a convenience function for use by code that implements the
 * freedesktop startup notification specification. Applications should
 * not normally need to call it directly. See the <ulink
 * url="http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt">Startup
 * Notification Protocol specification</ulink> for
 * definitions of the message types and keys that can be used.
 *
 * Since: 2.12
 **/
void
gdk_wayland_display_broadcast_startup_message (GdkDisplay *display,
					       const char *message_type,
					       ...)
{
  GString *message;
  va_list ap;
  const char *key, *value, *p;

  message = g_string_new (message_type);
  g_string_append_c (message, ':');

  va_start (ap, message_type);
  while ((key = va_arg (ap, const char *)))
    {
      value = va_arg (ap, const char *);
      if (!value)
	continue;

      g_string_append_printf (message, " %s=\"", key);
      for (p = value; *p; p++)
	{
	  switch (*p)
	    {
	    case ' ':
	    case '"':
	    case '\\':
	      g_string_append_c (message, '\\');
	      break;
	    }

	  g_string_append_c (message, *p);
	}
      g_string_append_c (message, '\"');
    }
  va_end (ap);

  printf ("startup message: %s\n", message->str);

  g_string_free (message, TRUE);
}

static void
gdk_wayland_display_notify_startup_complete (GdkDisplay  *display,
					     const gchar *startup_id)
{
  gdk_wayland_display_broadcast_startup_message (display, "remove",
						 "ID", startup_id,
						 NULL);
}

static void
gdk_wayland_display_event_data_copy (GdkDisplay     *display,
				     const GdkEvent *src,
				     GdkEvent       *dst)
{
}

static void
gdk_wayland_display_event_data_free (GdkDisplay *display,
				     GdkEvent   *event)
{
}

static GdkKeymap *
gdk_wayland_display_get_keymap (GdkDisplay *display)
{
  GdkDisplayWayland *display_wayland;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  display_wayland = GDK_DISPLAY_WAYLAND (display);

  if (!display_wayland->keymap)
    display_wayland->keymap = _gdk_wayland_keymap_new (display);

  return display_wayland->keymap;
}

static void
gdk_wayland_display_push_error_trap (GdkDisplay *display)
{
}

static gint
gdk_wayland_display_pop_error_trap (GdkDisplay *display,
				    gboolean    ignored)
{
  return 0;
}

static void
_gdk_display_wayland_class_init (GdkDisplayWaylandClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_wayland_display_dispose;
  object_class->finalize = gdk_wayland_display_finalize;

  display_class->window_type = _gdk_wayland_window_get_type ();
  display_class->get_name = gdk_wayland_display_get_name;
  display_class->get_n_screens = gdk_wayland_display_get_n_screens;
  display_class->get_screen = gdk_wayland_display_get_screen;
  display_class->get_default_screen = gdk_wayland_display_get_default_screen;
  display_class->beep = gdk_wayland_display_beep;
  display_class->sync = gdk_wayland_display_sync;
  display_class->flush = gdk_wayland_display_flush;
  display_class->has_pending = gdk_wayland_display_has_pending;
  display_class->queue_events = _gdk_wayland_display_queue_events;
  display_class->get_default_group = gdk_wayland_display_get_default_group;
  display_class->supports_selection_notification = gdk_wayland_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_wayland_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_wayland_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_wayland_display_store_clipboard;
  display_class->supports_shapes = gdk_wayland_display_supports_shapes;
  display_class->supports_input_shapes = gdk_wayland_display_supports_input_shapes;
  display_class->supports_composite = gdk_wayland_display_supports_composite;
  display_class->list_devices = gdk_wayland_display_list_devices;
  display_class->get_app_launch_context = _gdk_wayland_display_get_app_launch_context;
  display_class->get_default_cursor_size = _gdk_wayland_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_wayland_display_get_maximal_cursor_size;
  display_class->get_cursor_for_type = _gdk_wayland_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_wayland_display_get_cursor_for_name;
  display_class->get_cursor_for_pixbuf = _gdk_wayland_display_get_cursor_for_pixbuf;
  display_class->supports_cursor_alpha = _gdk_wayland_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_wayland_display_supports_cursor_color;
  display_class->before_process_all_updates = gdk_wayland_display_before_process_all_updates;
  display_class->after_process_all_updates = gdk_wayland_display_after_process_all_updates;
  display_class->get_next_serial = gdk_wayland_display_get_next_serial;
  display_class->notify_startup_complete = gdk_wayland_display_notify_startup_complete;
  display_class->event_data_copy = gdk_wayland_display_event_data_copy;
  display_class->event_data_free = gdk_wayland_display_event_data_free;
  display_class->create_window_impl = _gdk_wayland_display_create_window_impl;
  display_class->get_keymap = gdk_wayland_display_get_keymap;
  display_class->push_error_trap = gdk_wayland_display_push_error_trap;
  display_class->pop_error_trap = gdk_wayland_display_pop_error_trap;
  display_class->get_selection_owner = _gdk_wayland_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_wayland_display_set_selection_owner;
  display_class->send_selection_notify = _gdk_wayland_display_send_selection_notify;
  display_class->get_selection_property = _gdk_wayland_display_get_selection_property;
  display_class->convert_selection = _gdk_wayland_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_wayland_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_wayland_display_utf8_to_string_target;
}

static void
_gdk_display_wayland_init (GdkDisplayWayland *display)
{
  _gdk_wayland_display_manager_add_display (gdk_display_manager_get (),
					    GDK_DISPLAY (display));
}
