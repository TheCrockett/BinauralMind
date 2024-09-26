/*
   gnauralRecentMenu.h - declarations for gnauralRecentMenu.c

   Copyright (C) 20101014  Bret Logan

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _GNAURALRECENTMENU_H_
#define _GNAURALRECENTMENU_H_
#define GNAURAL_MIME_TYPE "application/x-gnaural"
extern GtkRecentManager *gnauralRecentMenu_recent_manager;
extern GtkWidget *gnauralRecentMenu_recent_menu;
extern void gnauralRecentMenu_Init (GtkBuilder * xml);
extern void gnauralRecentMenu_on_open_recent_activate (GtkRecentChooser *
                                                       chooser,
                                                       gpointer user_data);
extern GtkWidget *gnauralRecentMenu_recent_create_menu (void);
extern void gnauralRecentMenu_add_utf8_filename (const gchar * utf8_filename);
#endif
