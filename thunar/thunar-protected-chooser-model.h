/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_PROTECTED_CHOOSER_MODEL_H__
#define __THUNAR_PROTECTED_CHOOSER_MODEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarProtectedChooserModelClass ThunarProtectedChooserModelClass;
typedef struct _ThunarProtectedChooserModel      ThunarProtectedChooserModel;

#define THUNAR_TYPE_PROTECTED_CHOOSER_MODEL            (thunar_protected_chooser_model_get_type ())
#define THUNAR_PROTECTED_CHOOSER_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_MODEL, ThunarProtectedChooserModel))
#define THUNAR_PROTECTED_CHOOSER_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PROTECTED_CHOOSER_MODEL, ThunarProtectedChooserModelClass))
#define THUNAR_IS_PROTECTED_CHOOSER_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_MODEL))
#define THUNAR_IS_PROTECTED_CHOOSER_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PROTECTED_CHOOSER_MODEL))
#define THUNAR_PROTECTED_CHOOSER_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PROTECTED_CHOOSER_MODEL, ThunarProtectedChooserModelClass))

/**
 * ThunarProtectedChooserModelColumn:
 * @THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_NAME        : the name of the application.
 * @THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_ICON        : the name or absolute path of the application's icon.
 * @THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_APPLICATION : the #GAppInfo object.
 * @THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_STYLE       : custom font style.
 * @THUNAR_PROTECTED_CHOOSER_MODEL_N_COLUMNS          : the number of columns in #ThunarProtectedChooserModel.
 *
 * The identifiers for the columns provided by the #ThunarProtectedChooserModel.
 **/
typedef enum
{
  THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_NAME,
  THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_ICON,
  THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_APPLICATION,
  THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_STYLE,
  THUNAR_PROTECTED_CHOOSER_MODEL_COLUMN_WEIGHT,
  THUNAR_PROTECTED_CHOOSER_MODEL_N_COLUMNS,
} ThunarProtectedChooserModelColumn;

GType               thunar_protected_chooser_model_get_type         (void) G_GNUC_CONST;

ThunarProtectedChooserModel *thunar_protected_chooser_model_new              (const gchar        *content_type) G_GNUC_MALLOC;
const gchar        *thunar_protected_chooser_model_get_content_type (ThunarProtectedChooserModel *model);
gboolean            thunar_protected_chooser_model_remove           (ThunarProtectedChooserModel *model,
                                                           GtkTreeIter        *iter,
                                                           GError            **error);

G_END_DECLS;

#endif /* !__THUNAR_PROTECTED_CHOOSER_MODEL_H__ */
