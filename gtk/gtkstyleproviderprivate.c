/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gtkstyleproviderprivate.h"

#include "gtkintl.h"
#include "gtkstyleprovider.h"

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_INTERFACE (GtkStyleProviderPrivate, _gtk_style_provider_private, GTK_TYPE_STYLE_PROVIDER)

static guint signals[LAST_SIGNAL];

static void
_gtk_style_provider_private_default_init (GtkStyleProviderPrivateInterface *iface)
{
  signals[CHANGED] = g_signal_new (I_("-gtk-private-changed"),
                                   G_TYPE_FROM_INTERFACE (iface),
                                   G_SIGNAL_RUN_LAST,
                                   G_STRUCT_OFFSET (GtkStyleProviderPrivateInterface, changed),
                                   NULL, NULL,
                                   g_cclosure_marshal_VOID__VOID,
                                   G_TYPE_NONE, 0);

}

GtkSymbolicColor *
_gtk_style_provider_private_get_color (GtkStyleProviderPrivate *provider,
                                       const char              *name)
{
  GtkStyleProviderPrivateInterface *iface;

  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider), NULL);

  iface = GTK_STYLE_PROVIDER_PRIVATE_GET_INTERFACE (provider);

  if (!iface->get_color)
    return NULL;

  return iface->get_color (provider, name);
}

void
_gtk_style_provider_private_lookup (GtkStyleProviderPrivate *provider,
                                    const GtkCssMatcher     *matcher,
                                    GtkCssLookup            *lookup)
{
  GtkStyleProviderPrivateInterface *iface;

  g_return_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider));
  g_return_if_fail (matcher != NULL);
  g_return_if_fail (lookup != NULL);

  iface = GTK_STYLE_PROVIDER_PRIVATE_GET_INTERFACE (provider);

  if (!iface->lookup)
    return;

  iface->lookup (provider, matcher, lookup);
}

GtkCssChange
_gtk_style_provider_private_get_change (GtkStyleProviderPrivate *provider,
                                        const GtkCssMatcher     *matcher)
{
  GtkStyleProviderPrivateInterface *iface;

  g_return_val_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider), GTK_CSS_CHANGE_ANY);
  g_return_val_if_fail (matcher != NULL, GTK_CSS_CHANGE_ANY);

  iface = GTK_STYLE_PROVIDER_PRIVATE_GET_INTERFACE (provider);

  if (!iface->get_change)
    return GTK_CSS_CHANGE_ANY;

  return iface->get_change (provider, matcher);
}

void
_gtk_style_provider_private_changed (GtkStyleProviderPrivate *provider)
{
  g_return_if_fail (GTK_IS_STYLE_PROVIDER_PRIVATE (provider));

  g_signal_emit (provider, signals[CHANGED], 0);
}

