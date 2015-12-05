/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_PROTECTED_MANAGER_H__
#define __THUNAR_PROTECTED_MANAGER_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

#define THUNAR_FILE_EMBLEM_PROTECTED        "emblem-protected"

typedef struct _ThunarProtectedManagerClass ThunarProtectedManagerClass;
typedef struct _ThunarProtectedManager      ThunarProtectedManager;

#define THUNAR_TYPE_PROTECTED_MANAGER             (thunar_protected_manager_get_type ())
#define THUNAR_PROTECTED_MANAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PROTECTED_MANAGER, ThunarProtectedManager))
#define THUNAR_PROTECTED_MANAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PROTECTED_MANAGER, ThunarProtectedManagerClass))
#define THUNAR_IS_PROTECTED_MANAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PROTECTED_MANAGER))
#define THUNAR_IS_PROTECTED_MANAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PROTECTED_MANAGER))
#define THUNAR_PROTECTED_MANAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PROTECTED_MANAGER, ThunarProtectedManager))

GType                   thunar_protected_manager_get_type                   (void) G_GNUC_CONST;
gboolean                thunar_protected_manager_is_file_protected_directly (ThunarFile *);
gboolean                thunar_protected_manager_is_file_protected          (ThunarFile *);
gboolean                thunar_protected_show_protection_dialog             (GtkWidget *,
                                                                             GList *);
gboolean                thunar_protected_add_protected_file                 (ThunarFile *,
                                                                             gchar *);
gboolean                thunar_protected_remove_protected_file              (ThunarFile *);
gboolean                thunar_protected_manager_flush                      (void);
ThunarProtectedManager* thunar_protected_manager_new                        (void);
ThunarProtectedManager* thunar_protected_manager_get                        (void);

G_END_DECLS;

#endif /* !__THUNAR_PROTECTED_MANAGER_H__ */
