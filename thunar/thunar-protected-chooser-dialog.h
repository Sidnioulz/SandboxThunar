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

#ifndef __THUNAR_PROTECTED_CHOOSER_DIALOG_H__
#define __THUNAR_PROTECTED_CHOOSER_DIALOG_H__

#include <thunar/thunar-file.h>
#include <thunar/thunar-protected-chooser-model.h>

G_BEGIN_DECLS;

typedef struct _ThunarProtectedChooserDialogClass ThunarProtectedChooserDialogClass;
typedef struct _ThunarProtectedChooserDialog      ThunarProtectedChooserDialog;

#define THUNAR_TYPE_PROTECTED_CHOOSER_DIALOG            (thunar_protected_chooser_dialog_get_type ())
#define THUNAR_PROTECTED_CHOOSER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_DIALOG, ThunarProtectedChooserDialog))
#define THUNAR_PROTECTED_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PROTECTED_CHOOSER_DIALOG, ThunarProtectedChooserDialogClass))
#define THUNAR_IS_PROTECTED_CHOOSER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_DIALOG))
#define THUNAR_IS_PROTECTED_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PROTECTED_CHOOSER_DIALOG))
#define THUNAR_PROTECTED_CHOOSER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_DIALOG, ThunarProtectedChooserDialogClass))

GType       thunar_protected_chooser_dialog_get_type  (void) G_GNUC_CONST;

void        thunar_show_protected_chooser_dialog      (gpointer             parent);
void        thunar_protected_chooser_dialog_set_model (ThunarProtectedChooserDialog *dialog,
                                                       ThunarProtectedChooserModel  *model);
GAppInfo*   thunar_protected_chooser_dialog_get_current_info (ThunarProtectedChooserDialog *dialog);

G_END_DECLS;

#endif /* !__THUNAR_PROTECTED_CHOOSER_DIALOG_H__ */
