/*
   gnauralRecentMenu.c

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "main.h"
#include "gnauralXML.h"
#include "gnauralRecentMenu.h"

GtkRecentManager *gnauralRecentMenu_recent_manager = NULL;
GtkWidget *gnauralRecentMenu_recent_menu = NULL;

///////////////////////////////////////////
//20101014: takes an initted GtkBuilder
void gnauralRecentMenu_Init (GtkBuilder * xml)
{
 gnauralRecentMenu_recent_manager = gtk_recent_manager_get_default ();
 gnauralRecentMenu_recent_menu =
  (GtkWidget *) gnauralRecentMenu_recent_create_menu ();
 g_signal_connect (G_OBJECT (gnauralRecentMenu_recent_menu), "item-activated",
                   G_CALLBACK (gnauralRecentMenu_on_open_recent_activate),
                   NULL);
 gtk_menu_item_set_submenu (GTK_MENU_ITEM
                            (gtk_builder_get_object (xml, "menuitem_recent")),
                            gnauralRecentMenu_recent_menu);
}

///////////////////////////////////////////
//added 20101014
void gnauralRecentMenu_on_open_recent_activate (GtkRecentChooser * chooser,
                                                gpointer user_data)
{
 GtkRecentInfo *item;
 gchar *filename;
 gchar *utf8_filename = NULL;
 GtkWidget *dialog;
 const gchar *uri;

 g_return_if_fail (chooser && GTK_IS_RECENT_CHOOSER (chooser));
 item = gtk_recent_chooser_get_current_item (chooser);
 if (!item)
  return;
 uri = gtk_recent_info_get_uri (item);
 filename = g_filename_from_uri (uri, NULL, NULL);
 if (filename != NULL)
 {
  utf8_filename = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
  g_free (filename);
 }

 if (!utf8_filename || (0 != main_TestPathOrFileExistence (utf8_filename)))
 {
  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "'%s' no longer exists", utf8_filename);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
  gtk_recent_manager_remove_item (gnauralRecentMenu_recent_manager, uri,
                                  NULL);
 }
 else
 {
  gxml_XMLReadFile (utf8_filename, main_drawing_area, FALSE);
 }

 gtk_recent_info_unref (item);
 return;
}

////////////////////////////////////////////
//added 20101014 Create a menu of recent files
GtkWidget *gnauralRecentMenu_recent_create_menu (void)
{
 GtkWidget *recent_menu;
 GtkRecentFilter *recent_filter;

 recent_menu =
  gtk_recent_chooser_menu_new_for_manager (gnauralRecentMenu_recent_manager);
 gtk_recent_chooser_menu_set_show_numbers (GTK_RECENT_CHOOSER_MENU
                                           (recent_menu), TRUE);

 //20110527 - set FALSE because of massive crash bug introduce in a recent
 //GTK+ win release, occurring when it couldn't find the proper icon:
 gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER (recent_menu), FALSE);
 gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (recent_menu), 8);
 gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (recent_menu),
                                   GTK_RECENT_SORT_MRU);
 gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (recent_menu), TRUE);

 recent_filter = gtk_recent_filter_new ();
 gtk_recent_filter_add_mime_type (recent_filter, GNAURAL_MIME_TYPE);
 gtk_recent_filter_add_pattern (recent_filter, "*.gnaural");
 gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (recent_menu),
                                recent_filter);
 SG_DBGOUT ("gnauralRecentMenu_recent_create_menu");
 return recent_menu;
}

////////////////////////////////////////////
//added 20101014
void gnauralRecentMenu_add_utf8_filename (const gchar * utf8_filename)
{
 GtkRecentData *recent_data;
 gchar *filename;
 gchar *uri;
 gchar *pwd;
 static gchar *groups[2] = {
  "gnaural",
  NULL
 };

 recent_data = g_slice_new (GtkRecentData);
 recent_data->display_name = NULL;
 recent_data->description = NULL;
 recent_data->mime_type = GNAURAL_MIME_TYPE;
 recent_data->app_name = (gchar *) g_get_application_name ();
 recent_data->app_exec = g_strjoin (" ", g_get_prgname (), "%f", NULL);
 recent_data->groups = groups;
 recent_data->is_private = FALSE;
 filename = g_filename_from_utf8 (utf8_filename, -1, NULL, NULL, NULL);
 if (filename != NULL)
 {
  if (!g_path_is_absolute (filename))
  {
   gchar *absolute_filename;
   pwd = g_get_current_dir ();
   absolute_filename = g_build_filename (pwd, filename, NULL);
   g_free (pwd);
   g_free (filename);
   filename = absolute_filename;
  }
  uri = g_filename_to_uri (filename, NULL, NULL);
  if (uri != NULL)
  {
   gtk_recent_manager_add_full (gnauralRecentMenu_recent_manager, uri,
                                recent_data);
   g_free (uri);
  }
  g_free (filename);
 }
 g_free (recent_data->app_exec);
 g_slice_free (GtkRecentData, recent_data);
}
