/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "gtkcssinheritvalueprivate.h"

#include "gtkcssstylepropertyprivate.h"
#include "gtkstylecontextprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
};

static void
gtk_css_value_inherit_free (GtkCssValue *value)
{
  /* Can only happen if the unique value gets unreffed too often */
  g_assert_not_reached ();
}

static GtkCssValue *
gtk_css_value_inherit_compute (GtkCssValue        *value,
                               guint               property_id,
                               GtkStyleContext    *context,
                               GtkCssDependencies *dependencies)
{
  GtkStyleContext *parent = gtk_style_context_get_parent (context);

  if (parent)
    {
      *dependencies = GTK_CSS_EQUALS_PARENT;
      return _gtk_css_value_ref (_gtk_style_context_peek_property (parent, property_id));
    }
  else
    {
      return _gtk_css_value_compute (_gtk_css_style_property_get_initial_value (_gtk_css_style_property_lookup_by_id (property_id)),
                                     property_id,
                                     context,
                                     dependencies);
    }
}

static gboolean
gtk_css_value_inherit_equal (const GtkCssValue *value1,
                             const GtkCssValue *value2)
{
  return TRUE;
}

static GtkCssValue *
gtk_css_value_inherit_transition (GtkCssValue *start,
                                  GtkCssValue *end,
                                  guint        property_id,
                                  double       progress)
{
  return NULL;
}

static void
gtk_css_value_inherit_print (const GtkCssValue *value,
                             GString           *string)
{
  g_string_append (string, "inherit");
}

static const GtkCssValueClass GTK_CSS_VALUE_INHERIT = {
  gtk_css_value_inherit_free,
  gtk_css_value_inherit_compute,
  gtk_css_value_inherit_equal,
  gtk_css_value_inherit_transition,
  gtk_css_value_inherit_print
};

static GtkCssValue inherit = { &GTK_CSS_VALUE_INHERIT, 1 };

GtkCssValue *
_gtk_css_inherit_value_new (void)
{
  return _gtk_css_value_ref (&inherit);
}
