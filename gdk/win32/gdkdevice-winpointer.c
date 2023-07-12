/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 the GTK team
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

#include <gdk/gdkwindow.h>

#include <windows.h>

#include "gdkwin32.h"
#include "gdkdevice-winpointer.h"

G_DEFINE_TYPE (GdkDeviceWinpointer, gdk_device_winpointer, GDK_TYPE_DEVICE)

static GdkModifierType
get_keyboard_mask (void)
{
  GdkModifierType mask;
  BYTE kbd[256];

  GetKeyboardState (kbd);
  mask = 0;
  if (kbd[VK_SHIFT] & 0x80)
    mask |= GDK_SHIFT_MASK;
  if (kbd[VK_CAPITAL] & 0x80)
    mask |= GDK_LOCK_MASK;
  if (kbd[VK_CONTROL] & 0x80)
    mask |= GDK_CONTROL_MASK;
  if (kbd[VK_MENU] & 0x80)
    mask |= GDK_MOD1_MASK;

  return mask;
}

static gboolean
gdk_device_winpointer_get_history (GdkDevice      *device,
                                   GdkWindow      *window,
                                   guint32         start,
                                   guint32         stop,
                                   GdkTimeCoord ***events,
                                   gint           *n_events)
{
  return FALSE;
}

static void
gdk_device_winpointer_get_state (GdkDevice       *device,
                                 GdkWindow       *window,
                                 gdouble         *axes,
                                 GdkModifierType *mask)
{
  GdkDeviceWinpointer *device_winpointer = GDK_DEVICE_WINPOINTER (device);

  if (mask)
    {
      *mask = get_keyboard_mask ();
      *mask |= device_winpointer->last_button_mask;
    }

  if (axes)
    {
      gsize size = sizeof (double) * device_winpointer->num_axes;
      memcpy (axes, device_winpointer->last_axis_data, size);
    }
}

static void
gdk_device_winpointer_set_window_cursor (GdkDevice *device,
                                         GdkWindow *window,
                                         GdkCursor *cursor)
{
}

static void
gdk_device_winpointer_warp (GdkDevice *device,
                            GdkScreen *screen,
                            gdouble    x,
                            gdouble    y)
{
}

static void
gdk_device_winpointer_query_state (GdkDevice        *device,
                                   GdkWindow        *window,
                                   GdkWindow       **root_window,
                                   GdkWindow       **child_window,
                                   gdouble          *root_x,
                                   gdouble          *root_y,
                                   gdouble          *win_x,
                                   gdouble          *win_y,
                                   GdkModifierType  *mask)
{
  GdkDeviceWinpointer *device_winpointer = GDK_DEVICE_WINPOINTER (device);
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  GdkScreen *screen = gdk_window_get_screen (window);
  HWND hwnd = GDK_WINDOW_HWND (window);
  HWND hwndc = NULL;
  POINT point;

  _gdk_win32_get_cursor_pos (&point);

  if (root_x)
    *root_x = (point.x + _gdk_offset_x) / impl->window_scale;

  if (root_y)
    *root_y = (point.y + _gdk_offset_y) / impl->window_scale;

  if (window == gdk_screen_get_root_window (screen))
    {
      if (win_x)
        *win_x = (point.x + _gdk_offset_x) / impl->window_scale;

      if (win_y)
        *win_y = (point.y + _gdk_offset_y) / impl->window_scale;
    }
  else
    {
      ScreenToClient (hwnd, &point);

      if (win_x)
        *win_x = point.x / impl->window_scale;

      if (win_y)
        *win_y = point.y / impl->window_scale;
    }

  if (child_window)
    {
      if (window == gdk_screen_get_root_window (screen))
        {
          /* Always use WindowFromPoint when searching from the root window.
          *  Only WindowFromPoint is able to look through transparent
          *  layered windows.
          */
          hwndc = GetAncestor (WindowFromPoint (point), GA_ROOT);
        }
      else
        {
          hwndc = ChildWindowFromPoint (hwnd, point);
        }

      if (hwndc && hwndc != hwnd)
        *child_window = gdk_win32_handle_table_lookup (hwndc);
      else
        *child_window = NULL; /* Direct child unknown to gdk */
    }

  if (root_window)
    *root_window = gdk_screen_get_root_window (screen);

  if (mask)
    {
      *mask = get_keyboard_mask ();
      *mask |= device_winpointer->last_button_mask;
    }
}

static GdkGrabStatus
gdk_device_winpointer_grab (GdkDevice    *device,
                            GdkWindow    *window,
                            gboolean      owner_events,
                            GdkEventMask  event_mask,
                            GdkWindow    *confine_to,
                            GdkCursor    *cursor,
                            guint32       time_)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_winpointer_ungrab (GdkDevice *device,
                              guint32    time_)
{
}

static void
screen_to_client (HWND hwnd, POINT screen_pt, POINT *client_pt)
{
  *client_pt = screen_pt;
  ScreenToClient (hwnd, client_pt);
}

GdkWindow *
_gdk_device_winpointer_window_at_position (GdkDevice       *device,
                                           gdouble         *win_x,
                                           gdouble         *win_y,
                                           GdkModifierType *mask,
                                           gboolean         get_toplevel)
{
  GdkDeviceWinpointer *device_winpointer = GDK_DEVICE_WINPOINTER (device);
  GdkWindow *window = NULL;
  GdkWindowImplWin32 *impl = NULL;
  POINT screen_pt, client_pt;
  HWND hwnd;
  RECT rect;

  if (!_gdk_win32_get_cursor_pos (&screen_pt))
    return NULL;

  hwnd = WindowFromPoint (screen_pt);

  if (get_toplevel)
    {
      /* Use WindowFromPoint instead of ChildWindowFromPoint(Ex).
      *  Only WindowFromPoint is able to look through transparent
      *  layered windows.
      */
      hwnd = GetAncestor (hwnd, GA_ROOT);
    }

  /* Verify that we're really inside the client area of the window */
  GetClientRect (hwnd, &rect);
  screen_to_client (hwnd, screen_pt, &client_pt);
  if (!PtInRect (&rect, client_pt))
    hwnd = NULL;

  if (!get_toplevel && hwnd == NULL)
    {
      /* If we didn't hit any window, return the root window */
      /* note that the root window ain't a toplevel window */
      window = gdk_get_default_root_window ();
      impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      if (win_x)
        *win_x = (screen_pt.x + _gdk_offset_x) / impl->window_scale;
      if (win_y)
        *win_y = (screen_pt.y + _gdk_offset_y) / impl->window_scale;

      return window;
    }

  window = gdk_win32_handle_table_lookup (hwnd);

  if (window && (win_x || win_y))
    {
      impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      if (win_x)
        *win_x = client_pt.x / impl->window_scale;
      if (win_y)
        *win_y = client_pt.y / impl->window_scale;
    }

  if (mask)
    {
      *mask = get_keyboard_mask ();
      *mask |= device_winpointer->last_button_mask;
    }

  return window;
}

static void
gdk_device_winpointer_select_window_events (GdkDevice    *device,
                                            GdkWindow    *window,
                                            GdkEventMask  event_mask)
{
}

static void
gdk_device_winpointer_init (GdkDeviceWinpointer *device_winpointer)
{
  device_winpointer->device_handle = NULL;
  device_winpointer->start_cursor_id = 0;
  device_winpointer->end_cursor_id = 0;

  device_winpointer->origin_x = 0;
  device_winpointer->origin_y = 0;
  device_winpointer->scale_x = 0.0;
  device_winpointer->scale_y = 0.0;

  device_winpointer->last_axis_data = NULL;
  device_winpointer->num_axes = 0;
  device_winpointer->last_button_mask = 0;
}

static void
gdk_device_winpointer_finalize (GObject *object)
{
  GdkDeviceWinpointer *device_winpointer = GDK_DEVICE_WINPOINTER (object);

  g_free (device_winpointer->last_axis_data);

  G_OBJECT_CLASS (gdk_device_winpointer_parent_class)->finalize (object);
}

static void
gdk_device_winpointer_class_init (GdkDeviceWinpointerClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_device_winpointer_finalize;
  device_class->get_history = gdk_device_winpointer_get_history;
  device_class->get_state = gdk_device_winpointer_get_state;
  device_class->set_window_cursor = gdk_device_winpointer_set_window_cursor;
  device_class->warp = gdk_device_winpointer_warp;
  device_class->query_state = gdk_device_winpointer_query_state;
  device_class->grab = gdk_device_winpointer_grab;
  device_class->ungrab = gdk_device_winpointer_ungrab;
  device_class->window_at_position = _gdk_device_winpointer_window_at_position;
  device_class->select_window_events = gdk_device_winpointer_select_window_events;
}
