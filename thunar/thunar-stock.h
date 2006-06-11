/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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

#ifndef __THUNAR_STOCK_H__
#define __THUNAR_STOCK_H__

G_BEGIN_DECLS;

/* stock items for custom actions in Thunar */
#define THUNAR_STOCK_CREATE "thunar-create"
#define THUNAR_STOCK_RENAME "thunar-rename"

/* stock icons for the ThunarLauncher */
#define THUNAR_STOCK_DESKTOP "thunar-desktop"

/* stock icons for the ThunarShortcutsPane */
#define THUNAR_STOCK_SHORTCUTS "thunar-shortcuts"

/* stock icons for the ThunarPermissionsChooser */
#define THUNAR_STOCK_TEMPLATES "thunar-templates"

void thunar_stock_init (void) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_STOCK_H__ */
