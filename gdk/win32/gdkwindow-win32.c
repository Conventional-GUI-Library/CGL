/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2004 Tor Lillqvist
 * Copyright (C) 2001-2011 Hans Breuer
 * Copyright (C) 2007-2009 Cody Russell
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
#include <stdlib.h>

#include "gdk.h"
#include "gdkwindowimpl.h"
#include "gdkprivate-win32.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager-win32.h"
#include "gdkenumtypes.h"
#include "gdkwin32.h"
#include "gdkdisplayprivate.h"
#include "gdkvisualprivate.h"
#include "gdkwin32window.h"

#include <cairo-win32.h>

static void gdk_window_impl_win32_init       (GdkWindowImplWin32      *window);
static void gdk_window_impl_win32_class_init (GdkWindowImplWin32Class *klass);
static void gdk_window_impl_win32_finalize   (GObject                 *object);

static gpointer parent_class = NULL;
static GSList *modal_window_stack = NULL;

static const cairo_user_data_key_t gdk_win32_cairo_key;
typedef struct _FullscreenInfo FullscreenInfo;

struct _FullscreenInfo
{
  RECT  r;
  guint hint_flags;
  LONG  style;
};

static void     update_style_bits         (GdkWindow         *window);
static gboolean _gdk_window_get_functions (GdkWindow         *window,
                                           GdkWMFunction     *functions);
static HDC     _gdk_win32_impl_acquire_dc (GdkWindowImplWin32 *impl);
static void    _gdk_win32_impl_release_dc (GdkWindowImplWin32 *impl);

#define WINDOW_IS_TOPLEVEL(window)		   \
  (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN && \
   GDK_WINDOW_TYPE (window) != GDK_WINDOW_OFFSCREEN)

GdkScreen *
GDK_WINDOW_SCREEN (GObject *win)
{
  return _gdk_screen;
}

struct _GdkWin32Window {
  GdkWindow parent;
};

struct _GdkWin32WindowClass {
  GdkWindowClass parent_class;
};

G_DEFINE_TYPE (GdkWin32Window, gdk_win32_window, GDK_TYPE_WINDOW)

static void
gdk_win32_window_class_init (GdkWin32WindowClass *window_class)
{
}

static void
gdk_win32_window_init (GdkWin32Window *window)
{
}


G_DEFINE_TYPE (GdkWindowImplWin32, gdk_window_impl_win32, GDK_TYPE_WINDOW_IMPL)

GType
_gdk_window_impl_win32_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkWindowImplWin32Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_window_impl_win32_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkWindowImplWin32),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_window_impl_win32_init,
      };

      object_type = g_type_register_static (GDK_TYPE_WINDOW_IMPL,
                                            "GdkWindowImplWin32",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_window_impl_win32_init (GdkWindowImplWin32 *impl)
{
  impl->toplevel_window_type = -1;
  impl->hcursor = NULL;
  impl->hicon_big = NULL;
  impl->hicon_small = NULL;
  impl->hint_flags = 0;
  impl->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
  impl->transient_owner = NULL;
  impl->transient_children = NULL;
  impl->num_transients = 0;
  impl->changing_state = FALSE;
}

static void
gdk_window_impl_win32_finalize (GObject *object)
{
  GdkWindow *wrapper;
  GdkWindowImplWin32 *window_impl;
  
  g_return_if_fail (GDK_IS_WINDOW_IMPL_WIN32 (object));

  window_impl = GDK_WINDOW_IMPL_WIN32 (object);
  
  wrapper = window_impl->wrapper;

  if (!GDK_WINDOW_DESTROYED (wrapper))
    {
      gdk_win32_handle_table_remove (window_impl->handle);
    }

  if (window_impl->hcursor != NULL)
    {
      if (GetCursor () == window_impl->hcursor)
	SetCursor (NULL);

      GDI_CALL (DestroyCursor, (window_impl->hcursor));
      window_impl->hcursor = NULL;
    }

  if (window_impl->hicon_big != NULL)
    {
      GDI_CALL (DestroyIcon, (window_impl->hicon_big));
      window_impl->hicon_big = NULL;
    }

  if (window_impl->hicon_small != NULL)
    {
      GDI_CALL (DestroyIcon, (window_impl->hicon_small));
      window_impl->hicon_small = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
_gdk_win32_adjust_client_rect (GdkWindow *window,
			       RECT      *rect)
{
  LONG style, exstyle;

  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
  exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);
  API_CALL (AdjustWindowRectEx, (rect, style, FALSE, exstyle));
}

void
_gdk_root_window_size_init (void)
{
  GdkWindow *window;
  GdkRectangle rect;
  int i;

  window = GDK_WINDOW (_gdk_root);
  rect = _gdk_monitors[0].rect;
  for (i = 1; i < _gdk_num_monitors; i++)
    gdk_rectangle_union (&rect, &_gdk_monitors[i].rect, &rect);

  window->width = rect.width;
  window->height = rect.height;
}

void
_gdk_windowing_window_init (GdkScreen *screen)
{
  GdkWindow *window;
  GdkWindowImplWin32 *impl_win32;

  g_assert (_gdk_root == NULL);
  
  _gdk_root = _gdk_display_create_window (_gdk_display);

  window = (GdkWindow *)_gdk_root;
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WIN32, NULL);
  impl_win32 = GDK_WINDOW_IMPL_WIN32 (window->impl);
  impl_win32->wrapper = window;

  window->impl_window = window;
  window->visual = gdk_screen_get_system_visual (screen);

  window->window_type = GDK_WINDOW_ROOT;
  window->depth = window->visual->depth;

  _gdk_root_window_size_init ();

  window->x = 0;
  window->y = 0;
  window->abs_x = 0;
  window->abs_y = 0;
  /* width and height already initialised in _gdk_root_window_size_init() */
  window->viewable = TRUE;

  gdk_win32_handle_table_insert ((HANDLE *) &impl_win32->handle, _gdk_root);

  GDK_NOTE (MISC, g_print ("_gdk_root=%p\n", GDK_WINDOW_HWND (_gdk_root)));
}

static const gchar *
get_default_title (void)
{
  const char *title;
  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();

  return title;
}

/* RegisterGdkClass
 *   is a wrapper function for RegisterWindowClassEx.
 *   It creates at least one unique class for every 
 *   GdkWindowType. If support for single window-specific icons
 *   is ever needed (e.g Dialog specific), every such window should
 *   get its own class
 */
static ATOM
RegisterGdkClass (GdkWindowType wtype, GdkWindowTypeHint wtype_hint)
{
  static ATOM klassTOPLEVEL   = 0;
  static ATOM klassCHILD      = 0;
  static ATOM klassTEMP       = 0;
  static ATOM klassTEMPSHADOW = 0;
  static HICON hAppIcon = NULL;
  static HICON hAppIconSm = NULL;
  static WNDCLASSEXW wcl; 
  ATOM klass = 0;

  wcl.cbSize = sizeof (WNDCLASSEX);
  wcl.style = 0; /* DON'T set CS_<H,V>REDRAW. It causes total redraw
                  * on WM_SIZE and WM_MOVE. Flicker, Performance!
                  */
  wcl.lpfnWndProc = _gdk_win32_window_procedure;
  wcl.cbClsExtra = 0;
  wcl.cbWndExtra = 0;
  wcl.hInstance = _gdk_app_hmodule;
  wcl.hIcon = 0;
  wcl.hIconSm = 0;

  /* initialize once! */
  if (0 == hAppIcon && 0 == hAppIconSm)
    {
      gchar sLoc [MAX_PATH+1];

      if (0 != GetModuleFileName (_gdk_app_hmodule, sLoc, MAX_PATH))
        {
          ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);

          if (0 == hAppIcon && 0 == hAppIconSm)
            {
              if (0 != GetModuleFileName (_gdk_dll_hinstance, sLoc, MAX_PATH))
		{
		  ExtractIconEx (sLoc, 0, &hAppIcon, &hAppIconSm, 1);
		}
            }
        }

      if (0 == hAppIcon && 0 == hAppIconSm)
        {
          hAppIcon = LoadImage (NULL, IDI_APPLICATION, IMAGE_ICON,
                                GetSystemMetrics (SM_CXICON),
                                GetSystemMetrics (SM_CYICON), 0);
          hAppIconSm = LoadImage (NULL, IDI_APPLICATION, IMAGE_ICON,
                                  GetSystemMetrics (SM_CXSMICON),
                                  GetSystemMetrics (SM_CYSMICON), 0);
        }
    }

  if (0 == hAppIcon)
    hAppIcon = hAppIconSm;
  else if (0 == hAppIconSm)
    hAppIconSm = hAppIcon;

  wcl.lpszMenuName = NULL;

  /* initialize once per class */
  /*
   * HB: Setting the background brush leads to flicker, because we
   * don't get asked how to clear the background. This is not what
   * we want, at least not for input_only windows ...
   */
#define ONCE_PER_CLASS() \
  wcl.hIcon = CopyIcon (hAppIcon); \
  wcl.hIconSm = CopyIcon (hAppIconSm); \
  wcl.hbrBackground = NULL; \
  wcl.hCursor = LoadCursor (NULL, IDC_ARROW); 
  
  switch (wtype)
    {
    case GDK_WINDOW_TOPLEVEL:
      if (0 == klassTOPLEVEL)
	{
	  wcl.lpszClassName = L"gdkWindowToplevel";
	  
	  ONCE_PER_CLASS ();
	  klassTOPLEVEL = RegisterClassExW (&wcl);
	}
      klass = klassTOPLEVEL;
      break;
      
    case GDK_WINDOW_CHILD:
      if (0 == klassCHILD)
	{
	  wcl.lpszClassName = L"gdkWindowChild";
	  
	  wcl.style |= CS_PARENTDC; /* MSDN: ... enhances system performance. */
	  ONCE_PER_CLASS ();
	  klassCHILD = RegisterClassExW (&wcl);
	}
      klass = klassCHILD;
      break;
      
    case GDK_WINDOW_TEMP:
      if ((wtype_hint == GDK_WINDOW_TYPE_HINT_MENU) ||
          (wtype_hint == GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU) ||
          (wtype_hint == GDK_WINDOW_TYPE_HINT_POPUP_MENU) ||
          (wtype_hint == GDK_WINDOW_TYPE_HINT_TOOLTIP))
        {
          if (klassTEMPSHADOW == 0)
            {
              wcl.lpszClassName = L"gdkWindowTempShadow";
              wcl.style |= CS_SAVEBITS;
              if (LOBYTE (g_win32_get_windows_version()) > 0x05 ||
		  LOWORD (g_win32_get_windows_version()) == 0x0105)
		{
		  /* Windows XP (5.1) or above */
		  wcl.style |= 0x00020000; /* CS_DROPSHADOW */
		}
              ONCE_PER_CLASS ();
              klassTEMPSHADOW = RegisterClassExW (&wcl);
            }

          klass = klassTEMPSHADOW;
        }
       else
        {
          if (klassTEMP == 0)
            {
              wcl.lpszClassName = L"gdkWindowTemp";
              wcl.style |= CS_SAVEBITS;
              ONCE_PER_CLASS ();
              klassTEMP = RegisterClassExW (&wcl);
            }

          klass = klassTEMP;
        }
      break;
      
    default:
      g_assert_not_reached ();
      break;
    }
  
  if (klass == 0)
    {
      WIN32_API_FAILED ("RegisterClassExW");
      g_error ("That is a fatal error");
    }
  return klass;
}

/*
 * Create native windows.
 *
 * With the default Gdk the created windows are mostly toplevel windows.
 *
 * Placement of the window is derived from the passed in window,
 * except for toplevel window where OS/Window Manager placement
 * is used.
 *
 * The visual parameter, is based on GDK_WA_VISUAL if set already.
 * From attributes the only things used is: colormap, title, 
 * wmclass and type_hint. [1]. We are checking redundant information
 * and complain if that changes, which would break this implementation
 * again.
 *
 * [1] http://mail.gnome.org/archives/gtk-devel-list/2010-August/msg00214.html
 */
void
_gdk_win32_display_create_window_impl (GdkDisplay    *display,
				       GdkWindow     *window,
				       GdkWindow     *real_parent,
				       GdkScreen     *screen,
				       GdkEventMask   event_mask,
				       GdkWindowAttr *attributes,
				       gint           attributes_mask)
{
  HWND hwndNew;
  HANDLE hparent;
  ATOM klass = 0;
  DWORD dwStyle = 0, dwExStyle;
  RECT rect;
  GdkWindowImplWin32 *impl;
  const gchar *title;
  wchar_t *wtitle;
  gboolean override_redirect;
  gint window_width, window_height;
  gint offset_x = 0, offset_y = 0;
  gint x, y, real_x = 0, real_y = 0;
  /* check consistency of redundant information */
  guint remaining_mask = attributes_mask;

  GDK_NOTE (MISC,
	    g_print ("_gdk_window_impl_new: %s %s\n",
		     (window->window_type == GDK_WINDOW_TOPLEVEL ? "TOPLEVEL" :
		      (window->window_type == GDK_WINDOW_CHILD ? "CHILD" :
		       (window->window_type == GDK_WINDOW_TEMP ? "TEMP" :
			"???"))),
		     (attributes->wclass == GDK_INPUT_OUTPUT ? "" : "input-only"))
			   );

  /* to ensure to not miss important information some additional check against
   * attributes which may silently work on X11 */
  if ((attributes_mask & GDK_WA_X) != 0)
    {
      g_assert (attributes->x == window->x);
      remaining_mask &= ~GDK_WA_X;
    }
  if ((attributes_mask & GDK_WA_Y) != 0)
    {
      g_assert (attributes->y == window->y);
      remaining_mask &= ~GDK_WA_Y;
    }
  override_redirect = FALSE;
  if ((attributes_mask & GDK_WA_NOREDIR) != 0)
    {
      override_redirect = !!attributes->override_redirect;
      remaining_mask &= ~GDK_WA_NOREDIR;
    }

  if ((remaining_mask & ~(GDK_WA_WMCLASS|GDK_WA_VISUAL|GDK_WA_CURSOR|GDK_WA_TITLE|GDK_WA_TYPE_HINT)) != 0)
    g_warning ("_gdk_window_impl_new: uexpected attribute 0x%X",
               remaining_mask & ~(GDK_WA_WMCLASS|GDK_WA_VISUAL|GDK_WA_CURSOR|GDK_WA_TITLE|GDK_WA_TYPE_HINT));

  hparent = GDK_WINDOW_HWND (real_parent);

  impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WIN32, NULL);
  impl->wrapper = GDK_WINDOW (window);
  window->impl = GDK_WINDOW_IMPL (impl);

  if (attributes_mask & GDK_WA_VISUAL)
    g_assert (gdk_screen_get_system_visual (screen) == attributes->visual);

  impl->override_redirect = override_redirect;

  /* wclass is not any longer set always, but if is ... */
  if ((attributes_mask & GDK_WA_WMCLASS) == GDK_WA_WMCLASS)
    g_assert ((attributes->wclass == GDK_INPUT_OUTPUT) == !window->input_only);

  if (!window->input_only)
    {
      dwExStyle = 0;
    }
  else
    {
      /* I very much doubt using WS_EX_TRANSPARENT actually
       * corresponds to how X11 InputOnly windows work, but it appears
       * to work well enough for the actual use cases in gtk.
       */
      dwExStyle = WS_EX_TRANSPARENT;
      GDK_NOTE (MISC, g_print ("... GDK_INPUT_ONLY\n"));
    }

  switch (window->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
      if (GDK_WINDOW_TYPE (window->parent) != GDK_WINDOW_ROOT)
	{
	  /* The common code warns for this case. */
	  hparent = GetDesktopWindow ();
	}
      /* Children of foreign windows aren't toplevel windows */
      if (GDK_WINDOW_TYPE (real_parent) == GDK_WINDOW_FOREIGN)
	{
	  dwStyle = WS_CHILDWINDOW | WS_CLIPCHILDREN;
	}
      else
	{
	  if (window->window_type == GDK_WINDOW_TOPLEVEL)
	    dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
	  else
	    dwStyle = WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_CLIPCHILDREN;

	  offset_x = _gdk_offset_x;
	  offset_y = _gdk_offset_y;
	}
      break;

    case GDK_WINDOW_CHILD:
      dwStyle = WS_CHILDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      break;

    case GDK_WINDOW_TEMP:
      /* A temp window is not necessarily a top level window */
      dwStyle = (_gdk_root == real_parent ? WS_POPUP : WS_CHILDWINDOW);
      dwStyle |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
      dwExStyle |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
      offset_x = _gdk_offset_x;
      offset_y = _gdk_offset_y;
      break;

    default:
      g_assert_not_reached ();
    }

  if (window->window_type != GDK_WINDOW_CHILD)
    {
      rect.left = window->x;
      rect.top = window->y;
      rect.right = window->width;
      rect.bottom = window->height;

      AdjustWindowRectEx (&rect, dwStyle, FALSE, dwExStyle);

      real_x = window->x - offset_x;
      real_y = window->y - offset_y;

      if (window->window_type == GDK_WINDOW_TOPLEVEL)
	{
	  /* We initially place it at default so that we can get the
	     default window positioning if we want */
	  x = y = CW_USEDEFAULT;
	}
      else
	{
	  /* TEMP, FOREIGN: Put these where requested */
	  x = real_x;
	  y = real_y;
	}

      window_width = rect.right - rect.left;
      window_height = rect.bottom - rect.top;
    }
  else
    {
      /* adjust position relative to real_parent */
      window_width = window->width;
      window_height = window->height;
      /* use given position for initial placement, native coordinates */
      x = window->x + window->parent->abs_x - offset_x;
      y = window->y + window->parent->abs_y - offset_y;
    }

  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = get_default_title ();
  if (!title || !*title)
    title = "";

  impl->native_event_mask = GDK_STRUCTURE_MASK | event_mask;
      
  if (attributes_mask & GDK_WA_TYPE_HINT)
    gdk_window_set_type_hint (window, attributes->type_hint);

  if (impl->type_hint == GDK_WINDOW_TYPE_HINT_UTILITY)
    dwExStyle |= WS_EX_TOOLWINDOW;

  klass = RegisterGdkClass (window->window_type, impl->type_hint);

  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);
  
  hwndNew = CreateWindowExW (dwExStyle,
			     MAKEINTRESOURCEW (klass),
			     wtitle,
			     dwStyle,
			     x,
			     y,
			     window_width, window_height,
			     hparent,
			     NULL,
			     _gdk_app_hmodule,
			     window);
  if (GDK_WINDOW_HWND (window) != hwndNew)
    {
      g_warning ("gdk_window_new: gdk_event_translate::WM_CREATE (%p, %p) HWND mismatch.",
		 GDK_WINDOW_HWND (window),
		 hwndNew);

      /* HB: IHMO due to a race condition the handle was increased by
       * one, which causes much trouble. Because I can't find the 
       * real bug, try to workaround it ...
       * To reproduce: compile with MSVC 5, DEBUG=1
       */
# if 0
      gdk_win32_handle_table_remove (GDK_WINDOW_HWND (window));
      GDK_WINDOW_HWND (window) = hwndNew;
      gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);
# else
      /* the old behaviour, but with warning */
      impl->handle = hwndNew;
# endif

    }

  if (window->window_type != GDK_WINDOW_CHILD)
    {
      GetWindowRect (GDK_WINDOW_HWND (window), &rect);
      impl->initial_x = rect.left;
      impl->initial_y = rect.top;

      /* Now we know the initial position, move to actually specified position */
      if (real_x != x || real_y != y)
	{
	  API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
				   real_x, real_y, 0, 0,
				   SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
	}
    }

  g_object_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);

  GDK_NOTE (MISC, g_print ("... \"%s\" %dx%d@%+d%+d %p = %p\n",
			   title,
			   window_width, window_height,
			   window->x - offset_x,
			   window->y - offset_y, 
			   hparent,
			   GDK_WINDOW_HWND (window)));

  /* Add window handle to title */
  GDK_NOTE (MISC_OR_EVENTS, gdk_window_set_title (window, title));

  g_free (wtitle);

  if (impl->handle == NULL)
    {
      WIN32_API_FAILED ("CreateWindowExW");
      g_object_unref (window);
      return;
    }

//  if (!from_set_skip_taskbar_hint && window->window_type == GDK_WINDOW_TEMP)
//    gdk_window_set_skip_taskbar_hint (window, TRUE);

  if (attributes_mask & GDK_WA_CURSOR)
    gdk_window_set_cursor (window, attributes->cursor);
}

GdkWindow *
gdk_win32_window_foreign_new_for_display (GdkDisplay      *display,
                                          HWND             anid)
{
  GdkWindow *window;
  GdkWindowImplWin32 *impl;

  HANDLE parent;
  RECT rect;
  POINT point;

  g_return_val_if_fail (display == _gdk_display, NULL);

  if ((window = gdk_win32_window_lookup_for_display (display, anid)) != NULL)
    return g_object_ref (window);

  window = _gdk_display_create_window (display);
  window->visual = gdk_screen_get_system_visual (_gdk_screen);
  window->impl = g_object_new (GDK_TYPE_WINDOW_IMPL_WIN32, NULL);
  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  impl->wrapper = window;
  parent = GetParent (anid);
  
  window->parent = gdk_win32_handle_table_lookup (parent);
  if (!window->parent || GDK_WINDOW_TYPE (window->parent) == GDK_WINDOW_FOREIGN)
    window->parent = _gdk_root;
  
  window->parent->children = g_list_prepend (window->parent->children, window);

  GetClientRect ((HWND) anid, &rect);
  point.x = rect.left;
  point.y = rect.right;
  ClientToScreen ((HWND) anid, &point);
  if (parent != GetDesktopWindow ())
    ScreenToClient (parent, &point);
  window->x = point.x;
  window->y = point.y;
  window->width = rect.right - rect.left;
  window->height = rect.bottom - rect.top;
  window->window_type = GDK_WINDOW_FOREIGN;
  window->destroyed = FALSE;
  window->event_mask = GDK_ALL_EVENTS_MASK; /* XXX */
  if (IsWindowVisible ((HWND) anid))
    window->state &= (~GDK_WINDOW_STATE_WITHDRAWN);
  else
    window->state |= GDK_WINDOW_STATE_WITHDRAWN;
  if (GetWindowLong ((HWND)anid, GWL_EXSTYLE) & WS_EX_TOPMOST)
    window->state |= GDK_WINDOW_STATE_ABOVE;
  else
    window->state &= (~GDK_WINDOW_STATE_ABOVE);
  window->state &= (~GDK_WINDOW_STATE_BELOW);
  window->viewable = TRUE;

  window->depth = gdk_visual_get_system ()->depth;

  g_object_ref (window);
  gdk_win32_handle_table_insert (&GDK_WINDOW_HWND (window), window);

  GDK_NOTE (MISC, g_print ("gdk_win32_window_foreign_new_for_display: %p: %s@%+d%+d\n",
			   (HWND) anid,
			   _gdk_win32_window_description (window),
			   window->x, window->y));

  return window;
}

static void
gdk_win32_window_destroy (GdkWindow *window,
			  gboolean   recursing,
			  gboolean   foreign_destroy)
{
  GdkWindowImplWin32 *window_impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  GSList *tmp;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_win32_window_destroy: %p\n",
			   GDK_WINDOW_HWND (window)));

  /* Remove ourself from the modal stack */
  _gdk_remove_modal_window (window);

  /* Remove all our transient children */
  tmp = window_impl->transient_children;
  while (tmp != NULL)
    {
      GdkWindow *child = tmp->data;
      GdkWindowImplWin32 *child_impl = GDK_WINDOW_IMPL_WIN32 (GDK_WINDOW (child)->impl);

      child_impl->transient_owner = NULL;
      tmp = g_slist_next (tmp);
    }
  g_slist_free (window_impl->transient_children);
  window_impl->transient_children = NULL;

  /* Remove ourself from our transient owner */
  if (window_impl->transient_owner != NULL)
    {
      gdk_window_set_transient_for (window, NULL);
    }

  if (!recursing && !foreign_destroy)
    {
      window->destroyed = TRUE;
      DestroyWindow (GDK_WINDOW_HWND (window));
    }
}

static cairo_surface_t *
gdk_win32_window_resize_cairo_surface (GdkWindow       *window,
                                       cairo_surface_t *surface,
                                       gint             width,
                                       gint             height)
{
  /* XXX: Make Cairo surface use DC clip */
  cairo_surface_destroy (surface);

  return NULL;
}

static void
gdk_win32_window_destroy_foreign (GdkWindow *window)
{
  /* It's somebody else's window, but in our hierarchy, so reparent it
   * to the desktop, and then try to destroy it.
   */
  gdk_window_hide (window);
  gdk_window_reparent (window, NULL, 0, 0);
  
  PostMessage (GDK_WINDOW_HWND (window), WM_CLOSE, 0, 0);
}

/* This function is called when the window really gone.
 */
static void
gdk_win32_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (EVENTS,
	    g_print ("gdk_window_destroy_notify: %p%s\n",
		     GDK_WINDOW_HWND (window),
		     (GDK_WINDOW_DESTROYED (window) ? " (destroyed)" : "")));

  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	g_warning ("window %p unexpectedly destroyed",
		   GDK_WINDOW_HWND (window));

      _gdk_window_destroy (window, TRUE);
    }
  
  gdk_win32_handle_table_remove (GDK_WINDOW_HWND (window));
  g_object_unref (window);
}

static void
get_outer_rect (GdkWindow *window,
		gint       width,
		gint       height,
		RECT      *rect)
{
  rect->left = rect->top = 0;
  rect->right = width;
  rect->bottom = height;
      
  _gdk_win32_adjust_client_rect (window, rect);
}

static void
adjust_for_gravity_hints (GdkWindow *window,
			  RECT      *outer_rect,
			  gint		*x,
			  gint		*y)
{
  GdkWindowImplWin32 *impl;

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  if (impl->hint_flags & GDK_HINT_WIN_GRAVITY)
    {
#ifdef G_ENABLE_DEBUG
      gint orig_x = *x, orig_y = *y;
#endif

      switch (impl->hints.win_gravity)
	{
	case GDK_GRAVITY_NORTH:
	case GDK_GRAVITY_CENTER:
	case GDK_GRAVITY_SOUTH:
	  *x -= (outer_rect->right - outer_rect->left) / 2;
	  *x += window->width / 2;
	  break;
	      
	case GDK_GRAVITY_SOUTH_EAST:
	case GDK_GRAVITY_EAST:
	case GDK_GRAVITY_NORTH_EAST:
	  *x -= outer_rect->right - outer_rect->left;
	  *x += window->width;
	  break;

	case GDK_GRAVITY_STATIC:
	  *x += outer_rect->left;
	  break;

	default:
	  break;
	}

      switch (impl->hints.win_gravity)
	{
	case GDK_GRAVITY_WEST:
	case GDK_GRAVITY_CENTER:
	case GDK_GRAVITY_EAST:
	  *y -= (outer_rect->bottom - outer_rect->top) / 2;
	  *y += window->height / 2;
	  break;

	case GDK_GRAVITY_SOUTH_WEST:
	case GDK_GRAVITY_SOUTH:
	case GDK_GRAVITY_SOUTH_EAST:
	  *y -= outer_rect->bottom - outer_rect->top;
	  *y += window->height;
	  break;

	case GDK_GRAVITY_STATIC:
	  *y += outer_rect->top;
	  break;

	default:
	  break;
	}
      GDK_NOTE (MISC,
		(orig_x != *x || orig_y != *y) ?
		g_print ("adjust_for_gravity_hints: x: %d->%d, y: %d->%d\n",
			 orig_x, *x, orig_y, *y)
		  : (void) 0);
    }
}

static void
show_window_internal (GdkWindow *window,
                      gboolean   already_mapped,
		      gboolean   deiconify)
{
  GdkWindowImplWin32 *window_impl;
  gboolean focus_on_map = FALSE;
  DWORD exstyle;

  if (window->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("show_window_internal: %p: %s%s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (window->state),
			   (deiconify ? " deiconify" : "")));
  
  /* If asked to show (not deiconify) an withdrawn and iconified
   * window, do that.
   */
  if (!deiconify &&
      !already_mapped &&
      (window->state & GDK_WINDOW_STATE_ICONIFIED))
    {	
      ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWMINNOACTIVE);
      return;
    }
  
  /* If asked to just show an iconified window, do nothing. */
  if (!deiconify && (window->state & GDK_WINDOW_STATE_ICONIFIED))
    return;
  
  /* If asked to deiconify an already noniconified window, do
   * nothing. (Especially, don't cause the window to rise and
   * activate. There are different calls for that.)
   */
  if (deiconify && !(window->state & GDK_WINDOW_STATE_ICONIFIED))
    return;
  
  /* If asked to show (but not raise) a window that is already
   * visible, do nothing.
   */
  if (!deiconify && !already_mapped && IsWindowVisible (GDK_WINDOW_HWND (window)))
    return;

  /* Other cases */
  
  if (!already_mapped)
    focus_on_map = window->focus_on_map;

  exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);

  /* Use SetWindowPos to show transparent windows so automatic redraws
   * in other windows can be suppressed.
   */
  if (exstyle & WS_EX_TRANSPARENT)
    {
      UINT flags = SWP_SHOWWINDOW | SWP_NOREDRAW | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER;

      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP || !focus_on_map)
	flags |= SWP_NOACTIVATE;

      SetWindowPos (GDK_WINDOW_HWND (window), HWND_TOP, 0, 0, 0, 0, flags);

      return;
    }

  /* For initial map of "normal" windows we want to emulate WM window
   * positioning behaviour, which means: 
   * + Use user specified position if GDK_HINT_POS or GDK_HINT_USER_POS
   * otherwise:
   * + default to the initial CW_USEDEFAULT placement,
   *   no matter if the user moved the window before showing it.
   * + Certain window types and hints have more elaborate positioning
   *   schemes.
   */
  window_impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  if (!already_mapped &&
      GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL &&
      (window_impl->hint_flags & (GDK_HINT_POS | GDK_HINT_USER_POS)) == 0 &&
      !window_impl->override_redirect)
    {
      gboolean center = FALSE;
      RECT window_rect, center_on_rect;
      int x, y;

      x = window_impl->initial_x;
      y = window_impl->initial_y;

      if (window_impl->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN)
	{
	  HMONITOR monitor;
	  MONITORINFO mi;

	  monitor = MonitorFromWindow (GDK_WINDOW_HWND (window), MONITOR_DEFAULTTONEAREST);
	  mi.cbSize = sizeof (mi);
	  if (monitor && GetMonitorInfo (monitor, &mi))
	    center_on_rect = mi.rcMonitor;
	  else
	    {
	      center_on_rect.left = 0;
	      center_on_rect.right = 0;
	      center_on_rect.right = GetSystemMetrics (SM_CXSCREEN);
	      center_on_rect.bottom = GetSystemMetrics (SM_CYSCREEN);
	    }
	  center = TRUE;
	}
      else if (window_impl->transient_owner != NULL &&
	       GDK_WINDOW_IS_MAPPED (window_impl->transient_owner))
	{
	  GdkWindow *owner = window_impl->transient_owner;
	  /* Center on transient parent */
	  center_on_rect.left = owner->x;
	  center_on_rect.top = owner->y;
	  center_on_rect.right = center_on_rect.left + owner->width;
	  center_on_rect.bottom = center_on_rect.top + owner->height;
	  _gdk_win32_adjust_client_rect (GDK_WINDOW (owner), &center_on_rect);
	  center = TRUE;
	}

      if (center)
	{
	  window_rect.left = 0;
	  window_rect.top = 0;
	  window_rect.right = window->width;
	  window_rect.bottom = window->height;
	  _gdk_win32_adjust_client_rect (window, &window_rect);

	  x = center_on_rect.left + ((center_on_rect.right - center_on_rect.left) - (window_rect.right - window_rect.left)) / 2;
	  y = center_on_rect.top + ((center_on_rect.bottom - center_on_rect.top) - (window_rect.bottom - window_rect.top)) / 2;
	}

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
			       x, y, 0, 0,
			       SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
    }

  if (!already_mapped &&
      GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL &&
      !window_impl->override_redirect)
    {
      /* Ensure new windows are fully onscreen */
      RECT window_rect;
      HMONITOR monitor;
      MONITORINFO mi;
      int x, y;

      GetWindowRect (GDK_WINDOW_HWND (window), &window_rect);

      monitor = MonitorFromWindow (GDK_WINDOW_HWND (window), MONITOR_DEFAULTTONEAREST);
      mi.cbSize = sizeof (mi);
      if (monitor && GetMonitorInfo (monitor, &mi))
	{
	  x = window_rect.left;
	  y = window_rect.top;

	  if (window_rect.right > mi.rcWork.right)
	    {
	      window_rect.left -= (window_rect.right - mi.rcWork.right);
	      window_rect.right -= (window_rect.right - mi.rcWork.right);
	    }

	  if (window_rect.bottom > mi.rcWork.bottom)
	    {
	      window_rect.top -= (window_rect.bottom - mi.rcWork.bottom);
	      window_rect.bottom -= (window_rect.bottom - mi.rcWork.bottom);
	    }

	  if (window_rect.left < mi.rcWork.left)
	    {
	      window_rect.right += (mi.rcWork.left - window_rect.left);
	      window_rect.left += (mi.rcWork.left - window_rect.left);
	    }

	  if (window_rect.top < mi.rcWork.top)
	    {
	      window_rect.bottom += (mi.rcWork.top - window_rect.top);
	      window_rect.top += (mi.rcWork.top - window_rect.top);
	    }

	  if (x != window_rect.left || y != window_rect.top)
	    API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
				     window_rect.left, window_rect.top, 0, 0,
				     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
	}
    }


  if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
    {
      gdk_window_fullscreen (window);
    }
  else if (window->state & GDK_WINDOW_STATE_MAXIMIZED)
    {
      ShowWindow (GDK_WINDOW_HWND (window), SW_MAXIMIZE);
    }
  else if (window->state & GDK_WINDOW_STATE_ICONIFIED)
    {
      if (focus_on_map)
	ShowWindow (GDK_WINDOW_HWND (window), SW_RESTORE);
      else
	ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNOACTIVATE);
    }
  else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP || !focus_on_map)
    {
      ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNOACTIVATE);
    }
  else
    {
      ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNORMAL);
    }

  /* Sync STATE_ABOVE to TOPMOST */
  if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_TEMP &&
      (((window->state & GDK_WINDOW_STATE_ABOVE) &&
	!(exstyle & WS_EX_TOPMOST)) ||
       (!(window->state & GDK_WINDOW_STATE_ABOVE) &&
	(exstyle & WS_EX_TOPMOST))))
    {
      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window),
			       (window->state & GDK_WINDOW_STATE_ABOVE)?HWND_TOPMOST:HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE));
    }
}

static void
gdk_win32_window_show (GdkWindow *window, 
		       gboolean already_mapped)
{
  show_window_internal (window, FALSE, FALSE);
}

static void
gdk_win32_window_hide (GdkWindow *window)
{
  if (window->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_window_hide: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (window->state)));
  
  if (GDK_WINDOW_IS_MAPPED (window))
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_WITHDRAWN);
  
  _gdk_window_clear_update_area (window);
  
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL)
    ShowOwnedPopups (GDK_WINDOW_HWND (window), FALSE);
  
  if (GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
    {
      SetWindowPos (GDK_WINDOW_HWND (window), HWND_BOTTOM,
		    0, 0, 0, 0,
		    SWP_HIDEWINDOW | SWP_NOREDRAW | SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE);
    }
  else
    {
      ShowWindow (GDK_WINDOW_HWND (window), SW_HIDE);
    }
}

static void
gdk_win32_window_withdraw (GdkWindow *window)
{
  if (window->destroyed)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_window_withdraw: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (window->state)));

  gdk_window_hide (window);	/* ??? */
}

static void
gdk_win32_window_move (GdkWindow *window,
		       gint x, gint y)
{
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_window_move: %p: %+d%+d\n",
                           GDK_WINDOW_HWND (window), x, y));

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  /* Don't check GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD.
   * Foreign windows (another app's windows) might be children of our
   * windows! Especially in the case of gtkplug/socket.
   */
  if (GetAncestor (GDK_WINDOW_HWND (window), GA_PARENT) != GetDesktopWindow ())
    {
      _gdk_window_move_resize_child (window, x, y, window->width, window->height);
    }
  else
    {
      RECT outer_rect;

      get_outer_rect (window, window->width, window->height, &outer_rect);

      adjust_for_gravity_hints (window, &outer_rect, &x, &y);

      GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,0,0,"
                               "NOACTIVATE|NOSIZE|NOZORDER)\n",
                               GDK_WINDOW_HWND (window),
                               x - _gdk_offset_x, y - _gdk_offset_y));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
                               x - _gdk_offset_x, y - _gdk_offset_y, 0, 0,
                               SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER));
    }
}

static void
gdk_win32_window_resize (GdkWindow *window,
			 gint width, gint height)
{
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  GDK_NOTE (MISC, g_print ("gdk_win32_window_resize: %p: %dx%d\n",
                           GDK_WINDOW_HWND (window), width, height));

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  if (GetAncestor (GDK_WINDOW_HWND (window), GA_PARENT) != GetDesktopWindow ())
    {
      _gdk_window_move_resize_child (window, window->x, window->y, width, height);
    }
  else
    {
      RECT outer_rect;

      get_outer_rect (window, width, height, &outer_rect);

      GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,0,0,%ld,%ld,"
                               "NOACTIVATE|NOMOVE|NOZORDER)\n",
                               GDK_WINDOW_HWND (window),
                               outer_rect.right - outer_rect.left,
                               outer_rect.bottom - outer_rect.top));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
                               0, 0,
                               outer_rect.right - outer_rect.left,
                               outer_rect.bottom - outer_rect.top,
                               SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER));
      window->resize_count += 1;
    }
}

static void
gdk_win32_window_move_resize_internal (GdkWindow *window,
				       gint       x,
				       gint       y,
				       gint       width,
				       gint       height)
{
  GdkWindowImplWin32 *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  GDK_NOTE (MISC, g_print ("gdk_win32_window_move_resize: %p: %dx%d@%+d%+d\n",
                           GDK_WINDOW_HWND (window),
                           width, height, x, y));

  if (GetAncestor (GDK_WINDOW_HWND (window), GA_PARENT) != GetDesktopWindow ())
    {
      _gdk_window_move_resize_child (window, x, y, width, height);
    }
  else
    {
      RECT outer_rect;

      get_outer_rect (window, width, height, &outer_rect);

      adjust_for_gravity_hints (window, &outer_rect, &x, &y);

      GDK_NOTE (MISC, g_print ("... SetWindowPos(%p,NULL,%d,%d,%ld,%ld,"
                               "NOACTIVATE|NOZORDER)\n",
                               GDK_WINDOW_HWND (window),
                               x - _gdk_offset_x, y - _gdk_offset_y,
                               outer_rect.right - outer_rect.left,
                               outer_rect.bottom - outer_rect.top));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), NULL,
                               x - _gdk_offset_x, y - _gdk_offset_y,
                               outer_rect.right - outer_rect.left,
                               outer_rect.bottom - outer_rect.top,
                               SWP_NOACTIVATE | SWP_NOZORDER));
    }
}

static void
gdk_win32_window_move_resize (GdkWindow *window,
			      gboolean   with_move,
			      gint       x,
			      gint       y,
			      gint       width,
			      gint       height)
{
  GdkWindowImplWin32 *window_impl;

  window_impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  window_impl->inhibit_configure = TRUE;

  /* We ignore changes to the window being moved or resized by the 
     user, as we don't want to fight the user */
  if (GDK_WINDOW_HWND (window) == _modal_move_resize_window)
    goto out;

  if (with_move && (width < 0 && height < 0))
    {
      gdk_win32_window_move (window, x, y);
    }
  else
    {
      if (with_move)
	{
	  gdk_win32_window_move_resize_internal (window, x, y, width, height);
	}
      else
	{
	  gdk_win32_window_resize (window, width, height);
	}
    }

 out:
  window_impl->inhibit_configure = FALSE;

  if (WINDOW_IS_TOPLEVEL (window))
    _gdk_win32_emit_configure_event (window);
}

static gboolean
gdk_win32_window_reparent (GdkWindow *window,
			   GdkWindow *new_parent,
			   gint       x,
			   gint       y)
{
  GdkWindow *parent;
  GdkWindow *old_parent;
  GdkWindowImplWin32 *impl;
  gboolean was_toplevel;
  LONG style;

  if (!new_parent)
    new_parent = _gdk_root;

  old_parent = window->parent;
  parent = new_parent;
  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  GDK_NOTE (MISC, g_print ("gdk_win32_window_reparent: %p: %p\n",
			   GDK_WINDOW_HWND (window),
			   GDK_WINDOW_HWND (new_parent)));

  style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);

  was_toplevel = GetAncestor (GDK_WINDOW_HWND (window), GA_PARENT) == GetDesktopWindow ();
  if (was_toplevel && new_parent != _gdk_root)
    {
      /* Reparenting from top-level (child of desktop). Clear out
       * decorations.
       */
      style &= ~(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
      style |= WS_CHILD;
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);
    }
  else if (new_parent == _gdk_root)
    {
      /* Reparenting to top-level. Add decorations. */
      style &= ~(WS_CHILD);
      style |= WS_OVERLAPPEDWINDOW;
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, style);
    }

  API_CALL (SetParent, (GDK_WINDOW_HWND (window),
			GDK_WINDOW_HWND (new_parent)));
  
  API_CALL (MoveWindow, (GDK_WINDOW_HWND (window),
			 x, y, window->width, window->height, TRUE));

  /* From here on, we treat parents of type GDK_WINDOW_FOREIGN like
   * the root window
   */
  if (GDK_WINDOW_TYPE (new_parent) == GDK_WINDOW_FOREIGN)
    new_parent = _gdk_root;
  
  window->parent = new_parent;

  /* Switch the window type as appropriate */

  switch (GDK_WINDOW_TYPE (new_parent))
    {
    case GDK_WINDOW_ROOT:
      if (impl->toplevel_window_type != -1)
	GDK_WINDOW_TYPE (window) = impl->toplevel_window_type;
      else if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	GDK_WINDOW_TYPE (window) = GDK_WINDOW_TOPLEVEL;
      break;

    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_TEMP:
      if (WINDOW_IS_TOPLEVEL (window))
	{
	  /* Save the original window type so we can restore it if the
	   * window is reparented back to be a toplevel.
	   */
	  impl->toplevel_window_type = GDK_WINDOW_TYPE (window);
	  GDK_WINDOW_TYPE (window) = GDK_WINDOW_CHILD;
	}
    }

  if (old_parent)
    old_parent->children =
      g_list_remove (old_parent->children, window);

  parent->children = g_list_prepend (parent->children, window);

  return FALSE;
}

static void
gdk_win32_window_raise (GdkWindow *window)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_window_raise: %p\n",
			       GDK_WINDOW_HWND (window)));

      if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TEMP)
        API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_TOPMOST,
	                         0, 0, 0, 0,
				 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
      else if (window->accept_focus)
        /* Do not wrap this in an API_CALL macro as SetForegroundWindow might
         * fail when for example dragging a window belonging to a different
         * application at the time of a gtk_window_present() call due to focus
         * stealing prevention. */
        SetForegroundWindow (GDK_WINDOW_HWND (window));
      else
        API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_TOP,
  			         0, 0, 0, 0,
			         SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
    }
}

static void
gdk_win32_window_lower (GdkWindow *window)
{
  if (!GDK_WINDOW_DESTROYED (window))
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_window_lower: %p\n"
			       "... SetWindowPos(%p,HWND_BOTTOM,0,0,0,0,"
			       "NOACTIVATE|NOMOVE|NOSIZE)\n",
			       GDK_WINDOW_HWND (window),
			       GDK_WINDOW_HWND (window)));

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_BOTTOM,
			       0, 0, 0, 0,
			       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE));
    }
}

static void
gdk_win32_window_set_urgency_hint (GdkWindow *window,
			     gboolean   urgent)
{
  FLASHWINFO flashwinfo;
  typedef BOOL (WINAPI *PFN_FlashWindowEx) (FLASHWINFO*);
  PFN_FlashWindowEx flashWindowEx = NULL;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  flashWindowEx = (PFN_FlashWindowEx) GetProcAddress (GetModuleHandle ("user32.dll"), "FlashWindowEx");

  if (flashWindowEx)
    {
      flashwinfo.cbSize = sizeof (flashwinfo);
      flashwinfo.hwnd = GDK_WINDOW_HWND (window);
      if (urgent)
	flashwinfo.dwFlags = FLASHW_ALL | FLASHW_TIMER;
      else
	flashwinfo.dwFlags = FLASHW_STOP;
      flashwinfo.uCount = 0;
      flashwinfo.dwTimeout = 0;
      
      flashWindowEx (&flashwinfo);
    }
  else
    {
      FlashWindow (GDK_WINDOW_HWND (window), urgent);
    }
}

static gboolean
get_effective_window_decorations (GdkWindow       *window,
                                  GdkWMDecoration *decoration)
{
  GdkWindowImplWin32 *impl;

  impl = (GdkWindowImplWin32 *)window->impl;

  if (gdk_window_get_decorations (window, decoration))
    return TRUE;
    
  if (window->window_type != GDK_WINDOW_TOPLEVEL) 
    {
      return FALSE;
    }

  if ((impl->hint_flags & GDK_HINT_MIN_SIZE) &&
      (impl->hint_flags & GDK_HINT_MAX_SIZE) &&
      impl->hints.min_width == impl->hints.max_width &&
      impl->hints.min_height == impl->hints.max_height)
    {
      *decoration = GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MAXIMIZE;

      if (impl->type_hint == GDK_WINDOW_TYPE_HINT_DIALOG ||
	  impl->type_hint == GDK_WINDOW_TYPE_HINT_MENU ||
	  impl->type_hint == GDK_WINDOW_TYPE_HINT_TOOLBAR)
	{
	  *decoration |= GDK_DECOR_MINIMIZE;
	}
      else if (impl->type_hint == GDK_WINDOW_TYPE_HINT_SPLASHSCREEN)
	{
	  *decoration |= GDK_DECOR_MENU | GDK_DECOR_MINIMIZE;
	}

      return TRUE;
    }
  else if (impl->hint_flags & GDK_HINT_MAX_SIZE)
    {
      *decoration = GDK_DECOR_ALL | GDK_DECOR_MAXIMIZE;
      if (impl->type_hint == GDK_WINDOW_TYPE_HINT_DIALOG ||
	  impl->type_hint == GDK_WINDOW_TYPE_HINT_MENU ||
	  impl->type_hint == GDK_WINDOW_TYPE_HINT_TOOLBAR)
	{
	  *decoration |= GDK_DECOR_MINIMIZE;
	}

      return TRUE;
    }
  else
    {
      switch (impl->type_hint)
	{
	case GDK_WINDOW_TYPE_HINT_DIALOG:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_WINDOW_TYPE_HINT_MENU:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_WINDOW_TYPE_HINT_TOOLBAR:
	case GDK_WINDOW_TYPE_HINT_UTILITY:
	  gdk_window_set_skip_taskbar_hint (window, TRUE);
	  gdk_window_set_skip_pager_hint (window, TRUE);
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;

	case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
	  *decoration = (GDK_DECOR_ALL | GDK_DECOR_RESIZEH | GDK_DECOR_MENU |
			 GDK_DECOR_MINIMIZE | GDK_DECOR_MAXIMIZE);
	  return TRUE;
	  
	case GDK_WINDOW_TYPE_HINT_DOCK:
	  return FALSE;
	  
	case GDK_WINDOW_TYPE_HINT_DESKTOP:
	  return FALSE;

	default:
	  /* Fall thru */
	case GDK_WINDOW_TYPE_HINT_NORMAL:
	  *decoration = GDK_DECOR_ALL;
	  return TRUE;
	}
    }
    
  return FALSE;
}

static void
gdk_win32_window_set_geometry_hints (GdkWindow         *window,
			       const GdkGeometry *geometry,
			       GdkWindowHints     geom_mask)
{
  GdkWindowImplWin32 *impl;
  FullscreenInfo *fi;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_set_geometry_hints: %p\n",
			   GDK_WINDOW_HWND (window)));

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  fi = g_object_get_data (G_OBJECT (window), "fullscreen-info");
  if (fi)
    fi->hint_flags = geom_mask;
  else
    impl->hint_flags = geom_mask;
  impl->hints = *geometry;

  if (geom_mask & GDK_HINT_POS)
    ; /* even the X11 mplementation doesn't care */

  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      GDK_NOTE (MISC, g_print ("... MIN_SIZE: %dx%d\n",
			       geometry->min_width, geometry->min_height));
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      GDK_NOTE (MISC, g_print ("... MAX_SIZE: %dx%d\n",
			       geometry->max_width, geometry->max_height));
    }

  if (geom_mask & GDK_HINT_BASE_SIZE)
    {
      GDK_NOTE (MISC, g_print ("... BASE_SIZE: %dx%d\n",
			       geometry->base_width, geometry->base_height));
    }
  
  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      GDK_NOTE (MISC, g_print ("... RESIZE_INC: (%d,%d)\n",
			       geometry->width_inc, geometry->height_inc));
    }
  
  if (geom_mask & GDK_HINT_ASPECT)
    {
      GDK_NOTE (MISC, g_print ("... ASPECT: %g--%g\n",
			       geometry->min_aspect, geometry->max_aspect));
    }

  if (geom_mask & GDK_HINT_WIN_GRAVITY)
    {
      GDK_NOTE (MISC, g_print ("... GRAVITY: %d\n", geometry->win_gravity));
    }

  update_style_bits (window);
}

static void
gdk_win32_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  wchar_t *wtitle;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (title != NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Empty window titles not allowed, so set it to just a period. */
  if (!title[0])
    title = ".";
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_title: %p: %s\n",
			   GDK_WINDOW_HWND (window), title));
  
  GDK_NOTE (MISC_OR_EVENTS, title = g_strdup_printf ("%p %s", GDK_WINDOW_HWND (window), title));

  wtitle = g_utf8_to_utf16 (title, -1, NULL, NULL, NULL);
  API_CALL (SetWindowTextW, (GDK_WINDOW_HWND (window), wtitle));
  g_free (wtitle);

  GDK_NOTE (MISC_OR_EVENTS, g_free ((char *) title));
}

static void
gdk_win32_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_role: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   (role ? role : "NULL")));
  /* XXX */
}

static void
gdk_win32_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  HWND window_id, parent_id;
  GdkWindowImplWin32 *window_impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  GdkWindowImplWin32 *parent_impl = NULL;
  GSList *item;

  g_return_if_fail (GDK_IS_WINDOW (window));

  window_id = GDK_WINDOW_HWND (window);
  parent_id = parent != NULL ? GDK_WINDOW_HWND (parent) : NULL;

  GDK_NOTE (MISC, g_print ("gdk_window_set_transient_for: %p: %p\n", window_id, parent_id));

  if (GDK_WINDOW_DESTROYED (window) || (parent && GDK_WINDOW_DESTROYED (parent)))
    {
      if (GDK_WINDOW_DESTROYED (window))
	GDK_NOTE (MISC, g_print ("... destroyed!\n"));
      else
	GDK_NOTE (MISC, g_print ("... owner destroyed!\n"));

      return;
    }

  if (window->window_type == GDK_WINDOW_CHILD)
    {
      GDK_NOTE (MISC, g_print ("... a child window!\n"));
      return;
    }

  if (parent == NULL)
    {
      GdkWindowImplWin32 *trans_impl = GDK_WINDOW_IMPL_WIN32 (window_impl->transient_owner->impl);
      if (trans_impl->transient_children != NULL)
        {
          item = g_slist_find (trans_impl->transient_children, window);
          item->data = NULL;
          trans_impl->transient_children = g_slist_delete_link (trans_impl->transient_children, item);
          trans_impl->num_transients--;

          if (!trans_impl->num_transients)
            {
              trans_impl->transient_children = NULL;
            }
        }
      g_object_unref (G_OBJECT (window_impl->transient_owner));
      g_object_unref (G_OBJECT (window));

      window_impl->transient_owner = NULL;
    }
  else
    {
      parent_impl = GDK_WINDOW_IMPL_WIN32 (parent->impl);

      parent_impl->transient_children = g_slist_append (parent_impl->transient_children, window);
      g_object_ref (G_OBJECT (window));
      parent_impl->num_transients++;
      window_impl->transient_owner = parent;
      g_object_ref (G_OBJECT (parent));
    }

  /* This changes the *owner* of the window, despite the misleading
   * name. (Owner and parent are unrelated concepts.) At least that's
   * what people who seem to know what they talk about say on
   * USENET. Search on Google.
   */
  SetLastError (0);
  if (SetWindowLongPtr (window_id, GWLP_HWNDPARENT, (LONG_PTR) parent_id) == 0 &&
      GetLastError () != 0)
    WIN32_API_FAILED ("SetWindowLongPtr");
}

void
_gdk_push_modal_window (GdkWindow *window)
{
  modal_window_stack = g_slist_prepend (modal_window_stack,
                                        window);
}

void
_gdk_remove_modal_window (GdkWindow *window)
{
  GSList *tmp;

  g_return_if_fail (window != NULL);

  /* It's possible to be NULL here if someone sets the modal hint of the window
   * to FALSE before a modal window stack has ever been created. */
  if (modal_window_stack == NULL)
    return;

  /* Find the requested window in the stack and remove it.  Yeah, I realize this
   * means we're not a 'real stack', strictly speaking.  Sue me. :) */
  tmp = g_slist_find (modal_window_stack, window);
  if (tmp != NULL)
    {
      modal_window_stack = g_slist_delete_link (modal_window_stack, tmp);
    }
}

gboolean
_gdk_modal_blocked (GdkWindow *window)
{
  GSList *l;
  gboolean found_any = FALSE;

  for (l = modal_window_stack; l != NULL; l = l->next)
    {
      GdkWindow *modal = l->data;

      if (modal == window)
	return FALSE;

      if (GDK_WINDOW_IS_MAPPED (modal))
	found_any = TRUE;
    }

  return found_any;
}

GdkWindow *
_gdk_modal_current (void)
{
  GSList *l;

  for (l = modal_window_stack; l != NULL; l = l->next)
    {
      GdkWindow *modal = l->data;

      if (GDK_WINDOW_IS_MAPPED (modal))
	return modal;
    }

  return NULL;
}

static void
gdk_win32_window_set_background (GdkWindow       *window,
				 cairo_pattern_t *pattern)
{
}

static void
gdk_win32_window_set_device_cursor (GdkWindow *window,
                                    GdkDevice *device,
                                    GdkCursor *cursor)
{
  GdkWindowImplWin32 *impl;
  GdkWin32Cursor *cursor_private;
  HCURSOR hcursor;
  HCURSOR hprevcursor;
  
  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  cursor_private = (GdkWin32Cursor*) cursor;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!cursor)
    hcursor = NULL;
  else
    hcursor = cursor_private->hcursor;
  
  GDK_NOTE (MISC, g_print ("gdk_win32_window_set_cursor: %p: %p\n",
			   GDK_WINDOW_HWND (window),
			   hcursor));

  /* First get the old cursor, if any (we wait to free the old one
   * since it may be the current cursor set in the Win32 API right
   * now).
   */
  hprevcursor = impl->hcursor;

  GDK_DEVICE_GET_CLASS (device)->set_window_cursor (device, window, cursor);

  if (hcursor == NULL)
    impl->hcursor = NULL;
  else
    {
      /* We must copy the cursor as it is OK to destroy the GdkCursor
       * while still in use for some window. See for instance
       * gimp_change_win_cursor() which calls gdk_window_set_cursor
       * (win, cursor), and immediately afterwards gdk_cursor_destroy
       * (cursor).
       */
      if ((impl->hcursor = CopyCursor (hcursor)) == NULL)
	WIN32_API_FAILED ("CopyCursor");
      GDK_NOTE (MISC, g_print ("... CopyCursor (%p) = %p\n",
			       hcursor, impl->hcursor));
    }

  /* Destroy the previous cursor */
  if (hprevcursor != NULL)
    {
      GDK_NOTE (MISC, g_print ("... DestroyCursor (%p)\n", hprevcursor));
      API_CALL (DestroyCursor, (hprevcursor));
    }
}

static void
gdk_win32_window_get_geometry (GdkWindow *window,
			       gint      *x,
			       gint      *y,
			       gint      *width,
			       gint      *height)
{
  if (!window)
    window = _gdk_root;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      RECT rect;

      API_CALL (GetClientRect, (GDK_WINDOW_HWND (window), &rect));

      if (window != _gdk_root)
	{
	  POINT pt;
	  GdkWindow *parent = gdk_window_get_parent (window);

	  pt.x = rect.left;
	  pt.y = rect.top;
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  ScreenToClient (GDK_WINDOW_HWND (parent), &pt);
	  rect.left = pt.x;
	  rect.top = pt.y;

	  pt.x = rect.right;
	  pt.y = rect.bottom;
	  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
	  ScreenToClient (GDK_WINDOW_HWND (parent), &pt);
	  rect.right = pt.x;
	  rect.bottom = pt.y;

	  if (parent == _gdk_root)
	    {
	      rect.left += _gdk_offset_x;
	      rect.top += _gdk_offset_y;
	      rect.right += _gdk_offset_x;
	      rect.bottom += _gdk_offset_y;
	    }
	}

      if (x)
	*x = rect.left;
      if (y)
	*y = rect.top;
      if (width)
	*width = rect.right - rect.left;
      if (height)
	*height = rect.bottom - rect.top;

      GDK_NOTE (MISC, g_print ("gdk_win32_window_get_geometry: %p: %ldx%ldx%d@%+ld%+ld\n",
			       GDK_WINDOW_HWND (window),
			       rect.right - rect.left, rect.bottom - rect.top,
			       gdk_window_get_visual (window)->depth,
			       rect.left, rect.top));
    }
}

static gint
gdk_win32_window_get_root_coords (GdkWindow *window,
				  gint       x,
				  gint       y,
				  gint      *root_x,
				  gint      *root_y)
{
  gint tx;
  gint ty;
  POINT pt;

  pt.x = x;
  pt.y = y;
  ClientToScreen (GDK_WINDOW_HWND (window), &pt);
  tx = pt.x;
  ty = pt.y;
  
  if (root_x)
    *root_x = tx + _gdk_offset_x;
  if (root_y)
    *root_y = ty + _gdk_offset_y;

  GDK_NOTE (MISC, g_print ("gdk_win32_window_get_root_coords: %p: %+d%+d %+d%+d\n",
			   GDK_WINDOW_HWND (window),
			   x, y,
			   tx + _gdk_offset_x, ty + _gdk_offset_y));
  return 1;
}

static void
gdk_win32_window_restack_under (GdkWindow *window,
				GList *native_siblings)
{
	// ### TODO
}

static void
gdk_win32_window_restack_toplevel (GdkWindow *window,
				   GdkWindow *sibling,
				   gboolean   above)
{
	// ### TODO
}

static void
gdk_win32_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  GdkRectangle rect;

  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_get_frame_extents (window, &rect);

  if (x)
    *x = rect.x;

  if (y)
    *y = rect.y;

  GDK_NOTE (MISC, g_print ("gdk_window_get_root_origin: %p: %+d%+d\n",
			   GDK_WINDOW_HWND (window), rect.x, rect.y));
}

static void
gdk_win32_window_get_frame_extents (GdkWindow    *window,
                              GdkRectangle *rect)
{
  HWND hwnd;
  RECT r;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (rect != NULL);

  rect->x = 0;
  rect->y = 0;
  rect->width = 1;
  rect->height = 1;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* FIXME: window is documented to be a toplevel GdkWindow, so is it really
   * necessary to walk its parent chain?
   */
  while (window->parent && window->parent->parent)
    window = window->parent;

  hwnd = GDK_WINDOW_HWND (window);
  API_CALL (GetWindowRect, (hwnd, &r));

  rect->x = r.left + _gdk_offset_x;
  rect->y = r.top + _gdk_offset_y;
  rect->width = r.right - r.left;
  rect->height = r.bottom - r.top;

  GDK_NOTE (MISC, g_print ("gdk_window_get_frame_extents: %p: %ldx%ld@%+ld%+ld\n",
			   GDK_WINDOW_HWND (window),
			   r.right - r.left, r.bottom - r.top,
			   r.left, r.top));
}

static gboolean
gdk_window_win32_get_device_state (GdkWindow       *window,
                                   GdkDevice       *device,
                                   gdouble         *x,
                                   gdouble         *y,
                                   GdkModifierType *mask)
{
  GdkWindow *child;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), FALSE);

  GDK_DEVICE_GET_CLASS (device)->query_state (device, window,
                                              NULL, &child,
                                              NULL, NULL,
                                              x, y, mask);
  return (child != NULL);
}

void
_gdk_windowing_get_device_state (GdkDisplay       *display,
                                 GdkDevice        *device,
                                 GdkScreen       **screen,
                                 gint             *x,
                                 gint             *y,
                                 GdkModifierType  *mask)
{
  g_return_if_fail (display == _gdk_display);

  if (screen)
    *screen = _gdk_screen;

  GDK_DEVICE_GET_CLASS (device)->query_state (device,
                                              gdk_screen_get_root_window (_gdk_screen),
                                              NULL, NULL,
                                              x, y,
                                              NULL, NULL,
                                              mask);
}

void
gdk_display_warp_device (GdkDisplay *display,
                         GdkDevice  *device,
                         GdkScreen  *screen,
                         gint        x,
                         gint        y)
{
  g_return_if_fail (display == _gdk_display);
  g_return_if_fail (screen == _gdk_screen);
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (display == gdk_device_get_display (device));

  GDK_DEVICE_GET_CLASS (device)->warp (device, screen, x, y);
}

GdkWindow*
_gdk_windowing_window_at_device_position (GdkDisplay      *display,
                                          GdkDevice       *device,
                                          gint            *win_x,
                                          gint            *win_y,
                                          GdkModifierType *mask,
                                          gboolean         get_toplevel)
{
  return GDK_DEVICE_GET_CLASS (device)->window_at_position (device, win_x, win_y, mask, get_toplevel);
}

static GdkEventMask  
gdk_win32_window_get_events (GdkWindow *window)
{
  GdkWindowImplWin32 *impl;

  if (GDK_WINDOW_DESTROYED (window))
    return 0;

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  return impl->native_event_mask;
}

static void          
gdk_win32_window_set_events (GdkWindow   *window,
			     GdkEventMask event_mask)
{
  GdkWindowImplWin32 *impl;

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  /* gdk_window_new() always sets the GDK_STRUCTURE_MASK, so better
   * set it here, too. Not that I know or remember why it is
   * necessary, will have to test some day.
   */
  impl->native_event_mask = GDK_STRUCTURE_MASK | event_mask;
}

static void
do_shape_combine_region (GdkWindow *window,
			 HRGN	    hrgn,
			 gint       x, gint y)
{
  RECT rect;

  GetClientRect (GDK_WINDOW_HWND (window), &rect);
  _gdk_win32_adjust_client_rect (window, &rect);

  OffsetRgn (hrgn, -rect.left, -rect.top);
  OffsetRgn (hrgn, x, y);

  /* If this is a top-level window, add the title bar to the region */
  if (GDK_WINDOW_TYPE (window) == GDK_WINDOW_TOPLEVEL)
    {
      HRGN tmp = CreateRectRgn (0, 0, rect.right - rect.left, -rect.top);
      CombineRgn (hrgn, hrgn, tmp, RGN_OR);
      DeleteObject (tmp);
    }
  
  SetWindowRgn (GDK_WINDOW_HWND (window), hrgn, TRUE);
}

static void
gdk_win32_window_set_override_redirect (GdkWindow *window,
				  gboolean   override_redirect)
{
  GdkWindowImplWin32 *window_impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  window_impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  window_impl->override_redirect = !!override_redirect;
}

static void
gdk_win32_window_set_accept_focus (GdkWindow *window,
			     gboolean accept_focus)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  accept_focus = accept_focus != FALSE;

  if (window->accept_focus != accept_focus)
    window->accept_focus = accept_focus;
}

static void
gdk_win32_window_set_focus_on_map (GdkWindow *window,
			     gboolean focus_on_map)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  focus_on_map = focus_on_map != FALSE;

  if (window->focus_on_map != focus_on_map)
    window->focus_on_map = focus_on_map;
}

static void
gdk_win32_window_set_icon_list (GdkWindow *window,
			  GList     *pixbufs)
{
  GdkPixbuf *pixbuf, *big_pixbuf, *small_pixbuf;
  gint big_diff, small_diff;
  gint big_w, big_h, small_w, small_h;
  gint w, h;
  gint dw, dh, diff;
  HICON small_hicon, big_hicon;
  GdkWindowImplWin32 *impl;
  gint i, big_i, small_i;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  /* ideal sizes for small and large icons */
  big_w = GetSystemMetrics (SM_CXICON);
  big_h = GetSystemMetrics (SM_CYICON);
  small_w = GetSystemMetrics (SM_CXSMICON);
  small_h = GetSystemMetrics (SM_CYSMICON);

  /* find closest sized icons in the list */
  big_pixbuf = NULL;
  small_pixbuf = NULL;
  big_diff = 0;
  small_diff = 0;
  i = 0;
  while (pixbufs)
    {
      pixbuf = (GdkPixbuf*) pixbufs->data;
      w = gdk_pixbuf_get_width (pixbuf);
      h = gdk_pixbuf_get_height (pixbuf);

      dw = ABS (w - big_w);
      dh = ABS (h - big_h);
      diff = dw*dw + dh*dh;
      if (big_pixbuf == NULL || diff < big_diff)
	{
	  big_pixbuf = pixbuf;
	  big_diff = diff;
	  big_i = i;
	}

      dw = ABS (w - small_w);
      dh = ABS (h - small_h);
      diff = dw*dw + dh*dh;
      if (small_pixbuf == NULL || diff < small_diff)
	{
	  small_pixbuf = pixbuf;
	  small_diff = diff;
	  small_i = i;
	}

      pixbufs = g_list_next (pixbufs);
      i++;
    }

  /* Create the icons */
  big_hicon = _gdk_win32_pixbuf_to_hicon (big_pixbuf);
  small_hicon = _gdk_win32_pixbuf_to_hicon (small_pixbuf);

  /* Set the icons */
  SendMessageW (GDK_WINDOW_HWND (window), WM_SETICON, ICON_BIG,
		(LPARAM)big_hicon);
  SendMessageW (GDK_WINDOW_HWND (window), WM_SETICON, ICON_SMALL,
		(LPARAM)small_hicon);

  /* Store the icons, destroying any previous icons */
  if (impl->hicon_big)
    GDI_CALL (DestroyIcon, (impl->hicon_big));
  impl->hicon_big = big_hicon;
  if (impl->hicon_small)
    GDI_CALL (DestroyIcon, (impl->hicon_small));
  impl->hicon_small = small_hicon;
}

static void
gdk_win32_window_set_icon_name (GdkWindow   *window, 
			  const gchar *name)
{
  /* In case I manage to confuse this again (or somebody else does):
   * Please note that "icon name" here really *does* mean the name or
   * title of an window minimized as an icon on the desktop, or in the
   * taskbar. It has nothing to do with the freedesktop.org icon
   * naming stuff.
   */

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
#if 0
  /* This is not the correct thing to do. We should keep both the
   * "normal" window title, and the icon name. When the window is
   * minimized, call SetWindowText() with the icon name, and when the
   * window is restored, with the normal window title. Also, the name
   * is in UTF-8, so we should do the normal conversion to either wide
   * chars or system codepage, and use either the W or A version of
   * SetWindowText(), depending on Windows version.
   */
  API_CALL (SetWindowText, (GDK_WINDOW_HWND (window), name));
#endif
}

static GdkWindow *
gdk_win32_window_get_group (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD, NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;
  
  g_warning ("gdk_window_get_group not yet implemented");

  return NULL;
}

static void
gdk_win32_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD);
  g_return_if_fail (leader == NULL || GDK_IS_WINDOW (leader));

  if (GDK_WINDOW_DESTROYED (window) || GDK_WINDOW_DESTROYED (leader))
    return;
  
  g_warning ("gdk_window_set_group not implemented");
}

static void
update_single_bit (LONG    *style,
                   gboolean all,
		   int      gdk_bit,
		   int      style_bit)
{
  /* all controls the interpretation of gdk_bit -- if all is TRUE,
   * gdk_bit indicates whether style_bit is off; if all is FALSE, gdk
   * bit indicate whether style_bit is on
   */
  if ((!all && gdk_bit) || (all && !gdk_bit))
    *style |= style_bit;
  else
    *style &= ~style_bit;
}

static void
update_style_bits (GdkWindow *window)
{
  GdkWindowImplWin32 *impl = (GdkWindowImplWin32 *)window->impl;
  GdkWMDecoration decorations;
  LONG old_style, new_style, old_exstyle, new_exstyle;
  gboolean all;
  RECT rect, before, after;

  if (window->state & GDK_WINDOW_STATE_FULLSCREEN)
    return;

  old_style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);
  old_exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);

  GetClientRect (GDK_WINDOW_HWND (window), &before);
  after = before;
  AdjustWindowRectEx (&before, old_style, FALSE, old_exstyle);

  new_style = old_style;
  new_exstyle = old_exstyle;

  if (window->window_type == GDK_WINDOW_TEMP)
    new_exstyle |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
  else if (impl->type_hint == GDK_WINDOW_TYPE_HINT_UTILITY)
    new_exstyle |= WS_EX_TOOLWINDOW ;
  else
    new_exstyle &= ~WS_EX_TOOLWINDOW;

  if (get_effective_window_decorations (window, &decorations))
    {
      all = (decorations & GDK_DECOR_ALL);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_BORDER, WS_BORDER);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_RESIZEH, WS_THICKFRAME);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_TITLE, WS_CAPTION);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MENU, WS_SYSMENU);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MINIMIZE, WS_MINIMIZEBOX);
      update_single_bit (&new_style, all, decorations & GDK_DECOR_MAXIMIZE, WS_MAXIMIZEBOX);
    }

  if (old_style == new_style && old_exstyle == new_exstyle )
    {
      GDK_NOTE (MISC, g_print ("update_style_bits: %p: no change\n",
			       GDK_WINDOW_HWND (window)));
      return;
    }

  if (old_style != new_style)
    {
      GDK_NOTE (MISC, g_print ("update_style_bits: %p: STYLE: %s => %s\n",
			       GDK_WINDOW_HWND (window),
			       _gdk_win32_window_style_to_string (old_style),
			       _gdk_win32_window_style_to_string (new_style)));
      
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, new_style);
    }

  if (old_exstyle != new_exstyle)
    {
      GDK_NOTE (MISC, g_print ("update_style_bits: %p: EXSTYLE: %s => %s\n",
			       GDK_WINDOW_HWND (window),
			       _gdk_win32_window_exstyle_to_string (old_exstyle),
			       _gdk_win32_window_exstyle_to_string (new_exstyle)));
      
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE, new_exstyle);
    }

  AdjustWindowRectEx (&after, new_style, FALSE, new_exstyle);

  GetWindowRect (GDK_WINDOW_HWND (window), &rect);
  rect.left += after.left - before.left;
  rect.top += after.top - before.top;
  rect.right += after.right - before.right;
  rect.bottom += after.bottom - before.bottom;

  SetWindowPos (GDK_WINDOW_HWND (window), NULL,
		rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		SWP_FRAMECHANGED | SWP_NOACTIVATE | 
		SWP_NOREPOSITION | SWP_NOZORDER);

}

static void
update_single_system_menu_entry (HMENU    hmenu,
				 gboolean all,
				 int      gdk_bit,
				 int      menu_entry)
{
  /* all controls the interpretation of gdk_bit -- if all is TRUE,
   * gdk_bit indicates whether menu entry is disabled; if all is
   * FALSE, gdk bit indicate whether menu entry is enabled
   */
  if ((!all && gdk_bit) || (all && !gdk_bit))
    EnableMenuItem (hmenu, menu_entry, MF_BYCOMMAND | MF_ENABLED);
  else
    EnableMenuItem (hmenu, menu_entry, MF_BYCOMMAND | MF_GRAYED);
}

static void
update_system_menu (GdkWindow *window)
{
  GdkWMFunction functions;
  BOOL all;

  if (_gdk_window_get_functions (window, &functions))
    {
      HMENU hmenu = GetSystemMenu (GDK_WINDOW_HWND (window), FALSE);

      all = (functions & GDK_FUNC_ALL);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_RESIZE, SC_SIZE);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_MOVE, SC_MOVE);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_MINIMIZE, SC_MINIMIZE);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_MAXIMIZE, SC_MAXIMIZE);
      update_single_system_menu_entry (hmenu, all, functions & GDK_FUNC_CLOSE, SC_CLOSE);
    }
}

static GQuark
get_decorations_quark ()
{
  static GQuark quark = 0;
  
  if (!quark)
    quark = g_quark_from_static_string ("gdk-window-decorations");
  
  return quark;
}

static void
gdk_win32_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  GdkWMDecoration* decorations_copy;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_decorations: %p: %s %s%s%s%s%s%s\n",
			   GDK_WINDOW_HWND (window),
			   (decorations & GDK_DECOR_ALL ? "clearing" : "setting"),
			   (decorations & GDK_DECOR_BORDER ? "BORDER " : ""),
			   (decorations & GDK_DECOR_RESIZEH ? "RESIZEH " : ""),
			   (decorations & GDK_DECOR_TITLE ? "TITLE " : ""),
			   (decorations & GDK_DECOR_MENU ? "MENU " : ""),
			   (decorations & GDK_DECOR_MINIMIZE ? "MINIMIZE " : ""),
			   (decorations & GDK_DECOR_MAXIMIZE ? "MAXIMIZE " : "")));

  decorations_copy = g_malloc (sizeof (GdkWMDecoration));
  *decorations_copy = decorations;
  g_object_set_qdata_full (G_OBJECT (window), get_decorations_quark (), decorations_copy, g_free);

  update_style_bits (window);
}

static gboolean
gdk_win32_window_get_decorations (GdkWindow       *window,
			    GdkWMDecoration *decorations)
{
  GdkWMDecoration* decorations_set;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  decorations_set = g_object_get_qdata (G_OBJECT (window), get_decorations_quark ());
  if (decorations_set)
    *decorations = *decorations_set;

  return (decorations_set != NULL);
}

static GQuark
get_functions_quark ()
{
  static GQuark quark = 0;
  
  if (!quark)
    quark = g_quark_from_static_string ("gdk-window-functions");
  
  return quark;
}

static void
gdk_win32_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  GdkWMFunction* functions_copy;

  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_NOTE (MISC, g_print ("gdk_window_set_functions: %p: %s %s%s%s%s%s\n",
			   GDK_WINDOW_HWND (window),
			   (functions & GDK_FUNC_ALL ? "clearing" : "setting"),
			   (functions & GDK_FUNC_RESIZE ? "RESIZE " : ""),
			   (functions & GDK_FUNC_MOVE ? "MOVE " : ""),
			   (functions & GDK_FUNC_MINIMIZE ? "MINIMIZE " : ""),
			   (functions & GDK_FUNC_MAXIMIZE ? "MAXIMIZE " : ""),
			   (functions & GDK_FUNC_CLOSE ? "CLOSE " : "")));

  functions_copy = g_malloc (sizeof (GdkWMFunction));
  *functions_copy = functions;
  g_object_set_qdata_full (G_OBJECT (window), get_functions_quark (), functions_copy, g_free);

  update_system_menu (window);
}

gboolean
_gdk_window_get_functions (GdkWindow     *window,
		           GdkWMFunction *functions)
{
  GdkWMFunction* functions_set;
  
  functions_set = g_object_get_qdata (G_OBJECT (window), get_functions_quark ());
  if (functions_set)
    *functions = *functions_set;

  return (functions_set != NULL);
}

static gboolean 
gdk_win32_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  return !use_static;
}

static void
gdk_win32_window_begin_resize_drag (GdkWindow     *window,
                              GdkWindowEdge  edge,
                              GdkDevice     *device,
                              gint           button,
                              gint           root_x,
                              gint           root_y,
                              guint32        timestamp)
{
  WPARAM winedge;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Tell Windows to start interactively resizing the window by pretending that
   * the left pointer button was clicked in the suitable edge or corner. This
   * will only work if the button is down when this function is called, and
   * will only work with button 1 (left), since Windows only allows window
   * dragging using the left mouse button.
   */
  if (button != 1)
    return;
  
  /* Must break the automatic grab that occured when the button was
   * pressed, otherwise it won't work.
   */
  gdk_display_pointer_ungrab (_gdk_display, 0);

  switch (edge)
    {
    case GDK_WINDOW_EDGE_NORTH_WEST:
      winedge = HTTOPLEFT;
      break;

    case GDK_WINDOW_EDGE_NORTH:
      winedge = HTTOP;
      break;

    case GDK_WINDOW_EDGE_NORTH_EAST:
      winedge = HTTOPRIGHT;
      break;

    case GDK_WINDOW_EDGE_WEST:
      winedge = HTLEFT;
      break;

    case GDK_WINDOW_EDGE_EAST:
      winedge = HTRIGHT;
      break;

    case GDK_WINDOW_EDGE_SOUTH_WEST:
      winedge = HTBOTTOMLEFT;
      break;

    case GDK_WINDOW_EDGE_SOUTH:
      winedge = HTBOTTOM;
      break;

    case GDK_WINDOW_EDGE_SOUTH_EAST:
    default:
      winedge = HTBOTTOMRIGHT;
      break;
    }

  DefWindowProcW (GDK_WINDOW_HWND (window), WM_NCLBUTTONDOWN, winedge,
		  MAKELPARAM (root_x - _gdk_offset_x, root_y - _gdk_offset_y));
}

static void
gdk_win32_window_begin_move_drag (GdkWindow *window,
                            GdkDevice *device,
                            gint       button,
                            gint       root_x,
                            gint       root_y,
                            guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* Tell Windows to start interactively moving the window by pretending that
   * the left pointer button was clicked in the titlebar. This will only work
   * if the button is down when this function is called, and will only work
   * with button 1 (left), since Windows only allows window dragging using the
   * left mouse button.
   */
  if (button != 1)
    return;
  
  /* Must break the automatic grab that occured when the button was pressed,
   * otherwise it won't work.
   */
  gdk_display_pointer_ungrab (_gdk_display, 0);

  DefWindowProcW (GDK_WINDOW_HWND (window), WM_NCLBUTTONDOWN, HTCAPTION,
		  MAKELPARAM (root_x - _gdk_offset_x, root_y - _gdk_offset_y));
}


/*
 * Setting window states
 */
static void
gdk_win32_window_iconify (GdkWindow *window)
{
  HWND old_active_window;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_iconify: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (window->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      old_active_window = GetActiveWindow ();
      ShowWindow (GDK_WINDOW_HWND (window), SW_MINIMIZE);
      if (old_active_window != GDK_WINDOW_HWND (window))
	SetActiveWindow (old_active_window);
    }
  else
    {
      gdk_synthesize_window_state (window,
                                   0,
                                   GDK_WINDOW_STATE_ICONIFIED);
    }
}

static void
gdk_win32_window_deiconify (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_deiconify: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (window->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    {  
      show_window_internal (window, GDK_WINDOW_IS_MAPPED (window), TRUE);
    }
  else
    {
      gdk_synthesize_window_state (window,
                                   GDK_WINDOW_STATE_ICONIFIED,
                                   0);
    }
}

static void
gdk_win32_window_stick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* FIXME: Do something? */
}

static void
gdk_win32_window_unstick (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  /* FIXME: Do something? */
}

static void
gdk_win32_window_maximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_maximize: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (window->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    ShowWindow (GDK_WINDOW_HWND (window), SW_MAXIMIZE);
  else
    gdk_synthesize_window_state (window,
				 0,
				 GDK_WINDOW_STATE_MAXIMIZED);
}

static void
gdk_win32_window_unmaximize (GdkWindow *window)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_unmaximize: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (window->state)));

  if (GDK_WINDOW_IS_MAPPED (window))
    ShowWindow (GDK_WINDOW_HWND (window), SW_RESTORE);
  else
    gdk_synthesize_window_state (window,
				 GDK_WINDOW_STATE_MAXIMIZED,
				 0);
}

static void
gdk_win32_window_fullscreen (GdkWindow *window)
{
  gint x, y, width, height;
  FullscreenInfo *fi;
  HMONITOR monitor;
  MONITORINFO mi;

  g_return_if_fail (GDK_IS_WINDOW (window));

  fi = g_new (FullscreenInfo, 1);

  if (!GetWindowRect (GDK_WINDOW_HWND (window), &(fi->r)))
    g_free (fi);
  else
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      monitor = MonitorFromWindow (GDK_WINDOW_HWND (window), MONITOR_DEFAULTTONEAREST);
      mi.cbSize = sizeof (mi);
      if (monitor && GetMonitorInfo (monitor, &mi))
	{
	  x = mi.rcMonitor.left;
	  y = mi.rcMonitor.top;
	  width = mi.rcMonitor.right - x;
	  height = mi.rcMonitor.bottom - y;
	}
      else
	{
	  x = y = 0;
	  width = GetSystemMetrics (SM_CXSCREEN);
	  height = GetSystemMetrics (SM_CYSCREEN);
	}

      /* remember for restoring */
      fi->hint_flags = impl->hint_flags;
      impl->hint_flags &= ~GDK_HINT_MAX_SIZE;
      g_object_set_data (G_OBJECT (window), "fullscreen-info", fi);
      fi->style = GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE);

      /* Send state change before configure event */
      gdk_synthesize_window_state (window, 0, GDK_WINDOW_STATE_FULLSCREEN);

      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, 
                     (fi->style & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);

      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_TOP,
			       x, y, width, height,
			       SWP_NOCOPYBITS | SWP_SHOWWINDOW));
    }
}

static void
gdk_win32_window_unfullscreen (GdkWindow *window)
{
  FullscreenInfo *fi;

  g_return_if_fail (GDK_IS_WINDOW (window));

  fi = g_object_get_data (G_OBJECT (window), "fullscreen-info");
  if (fi)
    {
      GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

      gdk_synthesize_window_state (window, GDK_WINDOW_STATE_FULLSCREEN, 0);

      impl->hint_flags = fi->hint_flags;
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE, fi->style);
      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), HWND_NOTOPMOST,
			       fi->r.left, fi->r.top,
			       fi->r.right - fi->r.left, fi->r.bottom - fi->r.top,
			       SWP_NOCOPYBITS | SWP_SHOWWINDOW));
      
      g_object_set_data (G_OBJECT (window), "fullscreen-info", NULL);
      g_free (fi);
      update_style_bits (window);
    }
}

static void
gdk_win32_window_set_keep_above (GdkWindow *window,
			   gboolean   setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_set_keep_above: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   setting ? "YES" : "NO"));

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window),
			       setting ? HWND_TOPMOST : HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE));
    }

  gdk_synthesize_window_state (window,
			       setting ? GDK_WINDOW_STATE_BELOW : GDK_WINDOW_STATE_ABOVE,
			       setting ? GDK_WINDOW_STATE_ABOVE : 0);
}

static void
gdk_win32_window_set_keep_below (GdkWindow *window,
			   gboolean   setting)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_set_keep_below: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   setting ? "YES" : "NO"));

  if (GDK_WINDOW_IS_MAPPED (window))
    {
      API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window),
			       setting ? HWND_BOTTOM : HWND_NOTOPMOST,
			       0, 0, 0, 0,
			       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE));
    }

  gdk_synthesize_window_state (window,
			       setting ? GDK_WINDOW_STATE_ABOVE : GDK_WINDOW_STATE_BELOW,
			       setting ? GDK_WINDOW_STATE_BELOW : 0);
}

static void
gdk_win32_window_focus (GdkWindow *window,
			guint32    timestamp)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  GDK_NOTE (MISC, g_print ("gdk_window_focus: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   _gdk_win32_window_state_to_string (window->state)));

  if (window->state & GDK_WINDOW_STATE_MAXIMIZED)
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWMAXIMIZED);
  else
    ShowWindow (GDK_WINDOW_HWND (window), SW_SHOWNORMAL);
  SetFocus (GDK_WINDOW_HWND (window));
}

static void
gdk_win32_window_set_modal_hint (GdkWindow *window,
			   gboolean   modal)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC, g_print ("gdk_window_set_modal_hint: %p: %s\n",
			   GDK_WINDOW_HWND (window),
			   modal ? "YES" : "NO"));

  if (modal == window->modal_hint)
    return;

  window->modal_hint = modal;

#if 0
  /* Not sure about this one.. -- Cody */
  if (GDK_WINDOW_IS_MAPPED (window))
    API_CALL (SetWindowPos, (GDK_WINDOW_HWND (window), 
			     modal ? HWND_TOPMOST : HWND_NOTOPMOST,
			     0, 0, 0, 0,
			     SWP_NOMOVE | SWP_NOSIZE));
#else

  if (modal)
    {
      _gdk_push_modal_window (window);
      gdk_window_raise (window);
    }
  else
    {
      _gdk_remove_modal_window (window);
    }

#endif
}

static void
gdk_win32_window_set_skip_taskbar_hint (GdkWindow *window,
				  gboolean   skips_taskbar)
{
  static GdkWindow *owner = NULL;
  //GdkWindowAttr wa;

  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (MISC, g_print ("gdk_window_set_skip_taskbar_hint: %p: %s, doing nothing\n",
			   GDK_WINDOW_HWND (window),
			   skips_taskbar ? "YES" : "NO"));

  // ### TODO: Need to figure out what to do here.
  return;

  if (skips_taskbar)
    {
#if 0
      if (owner == NULL)
		{
		  wa.window_type = GDK_WINDOW_TEMP;
		  wa.wclass = GDK_INPUT_OUTPUT;
		  wa.width = wa.height = 1;
		  wa.event_mask = 0;
		  owner = gdk_window_new_internal (NULL, &wa, 0, TRUE);
		}
#endif

      SetWindowLongPtr (GDK_WINDOW_HWND (window), GWLP_HWNDPARENT, (LONG_PTR) GDK_WINDOW_HWND (owner));

#if 0 /* Should we also turn off the minimize and maximize buttons? */
      SetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE,
		     GetWindowLong (GDK_WINDOW_HWND (window), GWL_STYLE) & ~(WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU));
     
      SetWindowPos (GDK_WINDOW_HWND (window), NULL,
		    0, 0, 0, 0,
		    SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOMOVE |
		    SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
#endif
    }
  else
    {
      SetWindowLongPtr (GDK_WINDOW_HWND (window), GWLP_HWNDPARENT, 0);
    }
}

static void
gdk_win32_window_set_skip_pager_hint (GdkWindow *window,
				gboolean   skips_pager)
{
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (MISC, g_print ("gdk_window_set_skip_pager_hint: %p: %s, doing nothing\n",
			   GDK_WINDOW_HWND (window),
			   skips_pager ? "YES" : "NO"));
}

static void
gdk_win32_window_set_type_hint (GdkWindow        *window,
			  GdkWindowTypeHint hint)
{
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (MISC,
	    G_STMT_START{
	      static GEnumClass *class = NULL;
	      if (!class)
		class = g_type_class_ref (GDK_TYPE_WINDOW_TYPE_HINT);
	      g_print ("gdk_window_set_type_hint: %p: %s\n",
		       GDK_WINDOW_HWND (window),
		       g_enum_get_value (class, hint)->value_name);
	    }G_STMT_END);

  ((GdkWindowImplWin32 *)window->impl)->type_hint = hint;

  update_style_bits (window);
}

static GdkWindowTypeHint
gdk_win32_window_get_type_hint (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);
  
  if (GDK_WINDOW_DESTROYED (window))
    return GDK_WINDOW_TYPE_HINT_NORMAL;

  return GDK_WINDOW_IMPL_WIN32 (window->impl)->type_hint;
}

static HRGN
cairo_region_to_hrgn (const cairo_region_t *region,
		      gint                  x_origin,
		      gint                  y_origin)
{
  HRGN hrgn;
  RGNDATA *rgndata;
  RECT *rect;
  cairo_rectangle_int_t r;
  const int nrects = cairo_region_num_rectangles (region);
  guint nbytes =
    sizeof (RGNDATAHEADER) + (sizeof (RECT) * nrects);
  int i;

  rgndata = g_malloc (nbytes);
  rgndata->rdh.dwSize = sizeof (RGNDATAHEADER);
  rgndata->rdh.iType = RDH_RECTANGLES;
  rgndata->rdh.nCount = rgndata->rdh.nRgnSize = 0;
  SetRect (&rgndata->rdh.rcBound,
	   G_MAXLONG, G_MAXLONG, G_MINLONG, G_MINLONG);

  for (i = 0; i < nrects; i++)
    {
      rect = ((RECT *) rgndata->Buffer) + rgndata->rdh.nCount++;
      
      cairo_region_get_rectangle (region, i, &r);
      rect->left = r.x + x_origin;
      rect->right = rect->left + r.width;
      rect->top = r.y + y_origin;
      rect->bottom = rect->top + r.height;

      if (rect->left < rgndata->rdh.rcBound.left)
	rgndata->rdh.rcBound.left = rect->left;
      if (rect->right > rgndata->rdh.rcBound.right)
	rgndata->rdh.rcBound.right = rect->right;
      if (rect->top < rgndata->rdh.rcBound.top)
	rgndata->rdh.rcBound.top = rect->top;
      if (rect->bottom > rgndata->rdh.rcBound.bottom)
	rgndata->rdh.rcBound.bottom = rect->bottom;
    }
  if ((hrgn = ExtCreateRegion (NULL, nbytes, rgndata)) == NULL)
    WIN32_API_FAILED ("ExtCreateRegion");

  g_free (rgndata);

  return (hrgn);
}

static void
gdk_win32_window_shape_combine_region (GdkWindow       *window,
				       const cairo_region_t *shape_region,
				       gint             offset_x,
				       gint             offset_y)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (!shape_region)
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_window_shape_combine_region: %p: none\n",
			       GDK_WINDOW_HWND (window)));
      SetWindowRgn (GDK_WINDOW_HWND (window), NULL, TRUE);
    }
  else
    {
      HRGN hrgn;

      hrgn = cairo_region_to_hrgn (shape_region, 0, 0);
      
      GDK_NOTE (MISC, g_print ("gdk_win32_window_shape_combine_region: %p: %p\n",
			       GDK_WINDOW_HWND (window),
			       hrgn));

      do_shape_combine_region (window, hrgn, offset_x, offset_y);
    }
}

GdkWindow *
gdk_win32_window_lookup_for_display (GdkDisplay *display,
                                     HWND        anid)
{
  g_return_val_if_fail (display == _gdk_display, NULL);

  return (GdkWindow*) gdk_win32_handle_table_lookup (anid);
}

static void
gdk_win32_window_set_opacity (GdkWindow *window,
			gdouble    opacity)
{
  LONG exstyle;
  typedef BOOL (WINAPI *PFN_SetLayeredWindowAttributes) (HWND, COLORREF, BYTE, DWORD);
  PFN_SetLayeredWindowAttributes setLayeredWindowAttributes = NULL;

  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (WINDOW_IS_TOPLEVEL (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  exstyle = GetWindowLong (GDK_WINDOW_HWND (window), GWL_EXSTYLE);

  if (!(exstyle & WS_EX_LAYERED))
    SetWindowLong (GDK_WINDOW_HWND (window),
		    GWL_EXSTYLE,
		    exstyle | WS_EX_LAYERED);

  setLayeredWindowAttributes = 
    (PFN_SetLayeredWindowAttributes)GetProcAddress (GetModuleHandle ("user32.dll"), "SetLayeredWindowAttributes");

  if (setLayeredWindowAttributes)
    {
      API_CALL (setLayeredWindowAttributes, (GDK_WINDOW_HWND (window),
					     0,
					     opacity * 0xff,
					     LWA_ALPHA));
    }
}

static cairo_region_t *
gdk_win32_window_get_shape (GdkWindow *window)
{
  HRGN hrgn = CreateRectRgn (0, 0, 0, 0);
  int  type = GetWindowRgn (GDK_WINDOW_HWND (window), hrgn);

  if (type == SIMPLEREGION || type == COMPLEXREGION)
    {
      cairo_region_t *region = _gdk_win32_hrgn_to_region (hrgn);

      DeleteObject (hrgn);
      return region;
    }

  return NULL;
}

static gboolean
_gdk_win32_window_queue_antiexpose (GdkWindow *window,
				    cairo_region_t *area)
{
  HRGN hrgn = cairo_region_to_hrgn (area, 0, 0);

  GDK_NOTE (EVENTS, g_print ("_gdk_windowing_window_queue_antiexpose: ValidateRgn %p %s\n",
			     GDK_WINDOW_HWND (window),
			     _gdk_win32_cairo_region_to_string (area)));

  ValidateRgn (GDK_WINDOW_HWND (window), hrgn);

  DeleteObject (hrgn);

  return FALSE;
}

/* Gets called from gdwindow.c(do_move_region_bits_on_impl)
 * and got tested with testgtk::big_window. Given the previous,
 * untested implementation this one looks much too simple ;)
 */
static void
_gdk_win32_window_translate (GdkWindow *window,
                             cairo_region_t *area, /* In impl window coords */
                             gint       dx,
                             gint       dy)
{
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);
  HRGN hrgn, area_hrgn;
  cairo_region_t *update_region;
  HDC hdc;
  int ret;

  /* Note: This is the destination area, not the source, and
     it has been moved by dx, dy from the source area */
  area_hrgn = cairo_region_to_hrgn (area, 0, 0);

  /* First we copy any outstanding invalid areas in the 
     source area to the new position in the destination area */
  hrgn = CreateRectRgn (0, 0, 0, 0);
  ret = GetUpdateRgn (GDK_WINDOW_HWND (window), hrgn, FALSE);
  if (ret == ERROR)
    WIN32_API_FAILED ("GetUpdateRgn");
  else if (ret != NULLREGION)
    {
      /* Convert the source invalid region as it would be copied */
      OffsetRgn (hrgn, dx, dy);
      /* Keep what intersects the copy destination area */
      ret = CombineRgn (hrgn, hrgn, area_hrgn, RGN_AND);
      /* And invalidate it */
      if (ret == ERROR)
        WIN32_API_FAILED ("CombineRgn");
      else if (ret != NULLREGION)
	API_CALL (InvalidateRgn, (GDK_WINDOW_HWND (window), hrgn, TRUE));
    }

  /* Then we copy the bits, invalidating whatever is copied from
     otherwise invisible areas */

  hdc = _gdk_win32_impl_acquire_dc (impl);

  /* Clip hdc to target region */
  API_CALL (SelectClipRgn, (hdc, area_hrgn));

  SetRectRgn (hrgn, 0, 0, 0, 0);

  if (!ScrollDC (hdc, dx, dy, NULL, NULL, hrgn, NULL))
    WIN32_GDI_FAILED ("ScrollDC");
  else
    {
      update_region = _gdk_win32_hrgn_to_region (hrgn);
      if (!cairo_region_is_empty (update_region))
	_gdk_window_invalidate_for_expose (window, update_region);
      cairo_region_destroy (update_region);
    }

  /* Unset hdc clip region */
  API_CALL (SelectClipRgn, (hdc, NULL));

  _gdk_win32_impl_release_dc (impl);

  if (!DeleteObject (hrgn))
    WIN32_GDI_FAILED ("DeleteObject");

  if (!DeleteObject (area_hrgn))
    WIN32_GDI_FAILED ("DeleteObject");
}

static void
gdk_win32_input_shape_combine_region (GdkWindow *window,
				      const cairo_region_t *shape_region,
				      gint offset_x,
				      gint offset_y)
{
  if (GDK_WINDOW_DESTROYED (window))
    return;
  /* CHECK: are these really supposed to be the same? */
  gdk_win32_window_shape_combine_region (window, shape_region, offset_x, offset_y);
}

static void
gdk_win32_window_process_updates_recurse (GdkWindow *window,
					       cairo_region_t *region)
{
  _gdk_window_process_updates_recurse (window, region);
}

gboolean
gdk_win32_window_is_win32 (GdkWindow *window)
{
  return GDK_WINDOW_IS_WIN32 (window);
}

/**
 * _gdk_win32_acquire_dc
 * @impl: a Win32 #GdkWindowImplWin32 implementation
 * 
 * Gets a DC with the given drawable selected into it.
 *
 * Return value: The DC, on success. Otherwise
 *  %NULL. If this function succeeded
 *  _gdk_win32_impl_release_dc()  must be called
 *  release the DC when you are done using it.
 **/
static HDC 
_gdk_win32_impl_acquire_dc (GdkWindowImplWin32 *impl)
{
  if (GDK_IS_WINDOW_IMPL_WIN32 (impl) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->hdc)
    {
      impl->hdc = GetDC (impl->handle);
      if (!impl->hdc)
	WIN32_GDI_FAILED ("GetDC");
    }

  if (impl->hdc)
    {
      impl->hdc_count++;
      return impl->hdc;
    }
  else
    {
      return NULL;
    }
}

/**
 * _gdk_win32_impl_release_dc
 * @impl: a Win32 #GdkWindowImplWin32 implementation
 * 
 * Releases the reference count for the DC
 * from _gdk_win32_impl_acquire_dc()
 **/
static void
_gdk_win32_impl_release_dc (GdkWindowImplWin32 *impl)
{
  g_return_if_fail (impl->hdc_count > 0);

  impl->hdc_count--;
  if (impl->hdc_count == 0)
    {
      if (impl->saved_dc_bitmap)
	{
	  GDI_CALL (SelectObject, (impl->hdc, impl->saved_dc_bitmap));
	  impl->saved_dc_bitmap = NULL;
	}
      
      if (impl->hdc)
	{
	  GDI_CALL (ReleaseDC, (impl->handle, impl->hdc));
	  impl->hdc = NULL;
	}
    }
}

HWND
gdk_win32_window_get_impl_hwnd (GdkWindow *window)
{
  if (GDK_WINDOW_IS_WIN32 (window))
    return GDK_WINDOW_HWND (window);
  return NULL;
}

static void
gdk_win32_cairo_surface_destroy (void *data)
{
  GdkWindowImplWin32 *impl = data;

  _gdk_win32_impl_release_dc (impl);
  impl->cairo_surface = NULL;
}

static cairo_surface_t *
gdk_win32_ref_cairo_surface (GdkWindow *window)
{
  GdkWindowImplWin32 *impl = GDK_WINDOW_IMPL_WIN32 (window->impl);

  if (GDK_IS_WINDOW_IMPL_WIN32 (impl) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      HDC hdc = _gdk_win32_impl_acquire_dc (impl);
      if (!hdc)
	return NULL;

      impl->cairo_surface = cairo_win32_surface_create (hdc);

      cairo_surface_set_user_data (impl->cairo_surface, &gdk_win32_cairo_key,
				   impl, gdk_win32_cairo_surface_destroy);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

static void
gdk_window_impl_win32_class_init (GdkWindowImplWin32Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_win32_finalize;
  
  impl_class->ref_cairo_surface = gdk_win32_ref_cairo_surface;

  impl_class->show = gdk_win32_window_show;
  impl_class->hide = gdk_win32_window_hide;
  impl_class->withdraw = gdk_win32_window_withdraw;
  impl_class->set_events = gdk_win32_window_set_events;
  impl_class->get_events = gdk_win32_window_get_events;
  impl_class->raise = gdk_win32_window_raise;
  impl_class->lower = gdk_win32_window_lower;
  impl_class->restack_under = gdk_win32_window_restack_under;
  impl_class->restack_toplevel = gdk_win32_window_restack_toplevel;
  impl_class->move_resize = gdk_win32_window_move_resize;
  impl_class->set_background = gdk_win32_window_set_background;
  impl_class->reparent = gdk_win32_window_reparent;
  impl_class->set_device_cursor = gdk_win32_window_set_device_cursor;
  impl_class->get_geometry = gdk_win32_window_get_geometry;
  impl_class->get_device_state = gdk_window_win32_get_device_state;
  impl_class->get_root_coords = gdk_win32_window_get_root_coords;

  impl_class->shape_combine_region = gdk_win32_window_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_win32_input_shape_combine_region;
  impl_class->set_static_gravities = gdk_win32_window_set_static_gravities;
  impl_class->queue_antiexpose = _gdk_win32_window_queue_antiexpose;
  impl_class->translate = _gdk_win32_window_translate;
  impl_class->destroy = gdk_win32_window_destroy;
  impl_class->destroy_foreign = gdk_win32_window_destroy_foreign;
  impl_class->resize_cairo_surface = gdk_win32_window_resize_cairo_surface;
  impl_class->get_shape = gdk_win32_window_get_shape;
  //FIXME?: impl_class->get_input_shape = gdk_win32_window_get_input_shape;

  //impl_class->beep = gdk_x11_window_beep;

  impl_class->focus = gdk_win32_window_focus;
  impl_class->set_type_hint = gdk_win32_window_set_type_hint;
  impl_class->get_type_hint = gdk_win32_window_get_type_hint;
  impl_class->set_modal_hint = gdk_win32_window_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_win32_window_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_win32_window_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_win32_window_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_win32_window_set_geometry_hints;
  impl_class->set_title = gdk_win32_window_set_title;
  impl_class->set_role = gdk_win32_window_set_role;
  //impl_class->set_startup_id = gdk_x11_window_set_startup_id;
  impl_class->set_transient_for = gdk_win32_window_set_transient_for;
  impl_class->get_root_origin = gdk_win32_window_get_root_origin;
  impl_class->get_frame_extents = gdk_win32_window_get_frame_extents;
  impl_class->set_override_redirect = gdk_win32_window_set_override_redirect;
  impl_class->set_accept_focus = gdk_win32_window_set_accept_focus;
  impl_class->set_focus_on_map = gdk_win32_window_set_focus_on_map;
  impl_class->set_icon_list = gdk_win32_window_set_icon_list;
  impl_class->set_icon_name = gdk_win32_window_set_icon_name;
  impl_class->iconify = gdk_win32_window_iconify;
  impl_class->deiconify = gdk_win32_window_deiconify;
  impl_class->stick = gdk_win32_window_stick;
  impl_class->unstick = gdk_win32_window_unstick;
  impl_class->maximize = gdk_win32_window_maximize;
  impl_class->unmaximize = gdk_win32_window_unmaximize;
  impl_class->fullscreen = gdk_win32_window_fullscreen;
  impl_class->unfullscreen = gdk_win32_window_unfullscreen;
  impl_class->set_keep_above = gdk_win32_window_set_keep_above;
  impl_class->set_keep_below = gdk_win32_window_set_keep_below;
  impl_class->get_group = gdk_win32_window_get_group;
  impl_class->set_group = gdk_win32_window_set_group;
  impl_class->set_decorations = gdk_win32_window_set_decorations;
  impl_class->get_decorations = gdk_win32_window_get_decorations;
  impl_class->set_functions = gdk_win32_window_set_functions;

  impl_class->begin_resize_drag = gdk_win32_window_begin_resize_drag;
  impl_class->begin_move_drag = gdk_win32_window_begin_move_drag;
  impl_class->set_opacity = gdk_win32_window_set_opacity;
  //impl_class->set_composited = gdk_win32_window_set_composited;
  impl_class->destroy_notify = gdk_win32_window_destroy_notify;
  impl_class->get_drag_protocol = _gdk_win32_window_get_drag_protocol;
  impl_class->register_dnd = _gdk_win32_window_register_dnd;
  impl_class->drag_begin = _gdk_win32_window_drag_begin;
  impl_class->process_updates_recurse = gdk_win32_window_process_updates_recurse;
  //? impl_class->sync_rendering = _gdk_win32_window_sync_rendering;
  impl_class->simulate_key = _gdk_win32_window_simulate_key;
  impl_class->simulate_button = _gdk_win32_window_simulate_button;
  impl_class->get_property = _gdk_win32_window_get_property;
  impl_class->change_property = _gdk_win32_window_change_property;
  impl_class->delete_property = _gdk_win32_window_delete_property;
}

HGDIOBJ
gdk_win32_window_get_handle (GdkWindow *window)
{
  /* Try to ensure the window has a native window */
  if (!_gdk_window_has_impl (window))
    gdk_window_ensure_native (window);

  if (!GDK_WINDOW_IS_WIN32 (window))
    {
      g_warning (G_STRLOC " window is not a native Win32 window");
      return NULL;
    }

  return GDK_WINDOW_HWND (window);
}