/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __THUNAR_PROTECTED_CHOOSER_BUTTON_H__
#define __THUNAR_PROTECTED_CHOOSER_BUTTON_H__

#include <thunar/thunar-file.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarProtectedChooserButtonClass ThunarProtectedChooserButtonClass;
typedef struct _ThunarProtectedChooserButton      ThunarProtectedChooserButton;

enum
{
  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_NAME,
  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_ICON,
  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_APPLICATION,
  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_SENSITIVE,
  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_COLUMN_STYLE,
  THUNAR_PROTECTED_CHOOSER_BUTTON_STORE_N_COLUMNS
};

#define THUNAR_TYPE_PROTECTED_CHOOSER_BUTTON            (thunar_protected_chooser_button_get_type ())
#define THUNAR_PROTECTED_CHOOSER_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_BUTTON, ThunarProtectedChooserButton))
#define THUNAR_PROTECTED_CHOOSER_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PROTECTED_CHOOSER_BUTTON, ThunarProtectedChooserButtonClass))
#define THUNAR_IS_PROTECTED_CHOOSER_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_BUTTON))
#define THUNAR_IS_PROTECTED_CHOOSER_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PROTECTED_CHOOSER_BUTTON))
#define THUNAR_PROTECTED_CHOOSER_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_BUTTON, ThunarProtectedChooserButtonClass))

GType       thunar_protected_chooser_button_get_type (void) G_GNUC_CONST;

GtkWidget  *thunar_protected_chooser_button_new      (void) G_GNUC_MALLOC;

GAppInfo   *thunar_protected_chooser_button_get_current_info (ThunarProtectedChooserButton *chooser_button) G_GNUC_CONST;

void        thunar_protected_chooser_button_update_list (ThunarProtectedChooserButton *chooser_button,
                                                         GList                        *apps,
                                                         ThunarFile                   *file);

G_END_DECLS;

#endif /* !__THUNAR_PROTECTED_CHOOSER_BUTTON_H__ */
