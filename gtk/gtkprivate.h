/* GTK - The GIMP Toolkit
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

#ifndef __GTK_PRIVATE_H__
#define __GTK_PRIVATE_H__

#include <glib.h>

#include "gtksettings.h"

#include "gtkcsstypesprivate.h"

#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS


#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

/* Many keyboard shortcuts for Mac are the same as for X
 * except they use Command key instead of Control (e.g. Cut,
 * Copy, Paste). This symbol is for those simple cases. */
#ifndef GDK_WINDOWING_QUARTZ
#define GTK_DEFAULT_ACCEL_MOD_MASK GDK_CONTROL_MASK
#define GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL GDK_CONTROL_MASK
#else
#define GTK_DEFAULT_ACCEL_MOD_MASK GDK_MOD2_MASK
#define GTK_DEFAULT_ACCEL_MOD_MASK_VIRTUAL GDK_META_MASK
#endif

#ifdef G_DISABLE_CAST_CHECKS
/* This is true for debug no and minimum */
#define gtk_internal_return_if_fail(__expr) G_STMT_START{ (void)0; }G_STMT_END
#define gtk_internal_return_val_if_fail(__expr, __val) G_STMT_START{ (void)0; }G_STMT_END
#else
/* This is true for debug yes */
#define gtk_internal_return_if_fail(__expr) g_return_if_fail(__expr)
#define gtk_internal_return_val_if_fail(__expr, __val) g_return_val_if_fail(__expr, __val)
#endif

const gchar * _gtk_get_datadir            (void);
const gchar * _gtk_get_libdir             (void);
const gchar * _gtk_get_sysconfdir         (void);
const gchar * _gtk_get_localedir          (void);
const gchar * _gtk_get_data_prefix        (void);

gboolean      _gtk_fnmatch                (const char *pattern,
                                           const char *string,
                                           gboolean    no_leading_period);
 
gchar   *_gtk_get_lc_ctype (void);

void          _gtk_ensure_resources       (void);

gboolean _gtk_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                           GValue                *return_accu,
                                           const GValue          *handler_return,
                                           gpointer               dummy);
 
gboolean _gtk_single_string_accumulator   (GSignalInvocationHint *ihint,
                                           GValue                *return_accu,
                                           const GValue          *handler_return,
                                           gpointer               dummy);

#ifndef GDK_WINDOWING_QUARTZ
#define GTK_TOGGLE_GROUP_MOD_MASK 0
#else
#define GTK_TOGGLE_GROUP_MOD_MASK GDK_MOD1_MASK
#endif

gboolean _gtk_button_event_triggers_context_menu (GdkEventButton *event);

gboolean _gtk_translate_keyboard_accel_state     (GdkKeymap       *keymap,
                                                  guint            hardware_keycode,
                                                  GdkModifierType  state,
                                                  GdkModifierType  accel_mask,
                                                  gint             group,
                                                  guint           *keyval,
                                                  gint            *effective_group,
                                                  gint            *level,
                                                  GdkModifierType *consumed_modifiers);


gboolean _gtk_translate_keyboard_accel_state   (GdkKeymap       *keymap,
                                                guint            hardware_keycode,
                                                GdkModifierType  state,
                                                GdkModifierType  accel_mask,
                                                gint             group,
                                                guint           *keyval,
                                                gint            *effective_group,
                                                gint            *level,
                                                GdkModifierType *consumed_modifiers);

G_END_DECLS

#endif /* __GTK_PRIVATE_H__ */
