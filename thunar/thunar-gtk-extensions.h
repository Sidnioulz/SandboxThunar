/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_GTK_EXTENSIONS_H__
#define __THUNAR_GTK_EXTENSIONS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

GtkToolItem *thunar_gtk_action_group_create_tool_item     (GtkActionGroup *action_group,
                                                           const gchar    *action_name) G_GNUC_INTERNAL G_GNUC_MALLOC;
void         thunar_gtk_action_group_set_action_sensitive (GtkActionGroup *action_group,
                                                           const gchar    *action_name,
                                                           gboolean        sensitive) G_GNUC_INTERNAL;

void         thunar_gtk_icon_factory_insert_icon          (GtkIconFactory *icon_factory,
                                                           const gchar    *stock_id,
                                                           const gchar    *icon_name) G_GNUC_INTERNAL;

GtkAction   *thunar_gtk_ui_manager_get_action_by_name     (GtkUIManager   *ui_manager,
                                                           const gchar    *action_name) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_GTK_EXTENSIONS_H__ */
