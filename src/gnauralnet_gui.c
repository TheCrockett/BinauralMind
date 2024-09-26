/*
   gnauralnet_gui.c
   Code that allows user to access info and features of GnauralNet with a GTK+ GUI.
   Depends on:
   gnauralnet.h
   gnauralnet_clocksync.c
   gnauralnet_lists.c
   gnauralnet_main.c
   gnauralnet_socket.c

   Copyright (C) 2008  Bret Logan

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

#include <gtk/gtk.h>
#include "gnauralnet.h"
#include "main.h"       //needed ONLY for main_MessageDialogBox()

//Global variables:
GtkWidget *GN_gui_CreateGtkList (void);

///////////////////////////////////////////////////////////
//This creates a generic dialog box and puts a GtkTreeView
//containing all the GN data in to it.
void GN_ShowInfo ()
{
 GtkWidget *dialog,
 *label;
 //char sTmp[256];

 if (GNAURALNET_RUNNING != GN_My.State)
 {
  main_MessageDialogBox
   ("You're not running GnauralNet", GTK_MESSAGE_INFO, GTK_BUTTONS_OK);
  return;
 }

 dialog = gtk_dialog_new_with_buttons ("Message",
                                       (GtkWindow *) NULL,
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_STOCK_OK, GTK_RESPONSE_NONE, NULL);
 g_signal_connect_swapped (dialog,
                           "response",
                           G_CALLBACK (gtk_widget_destroy), dialog);

 //GN_Friend *curFriend = GN_My.FirstFriend;
 //struct in_addr tmp_in_addr;

 if (NULL == GN_My.FirstFriend)
 {
  label = gtk_label_new ("No contacts to report");
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label);
 }
 else
 {
  label = gtk_label_new ("Current Friends: (IP, Port, Schedule):");
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label);

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
                     GN_gui_CreateGtkList ());
 }
 gtk_widget_show_all (dialog);
}

enum
{
 GNGUI_IP_COLUMN,
 GNGUI_PORT_COLUMN,
 GNGUI_SCHED_COLUMN,
 GNGUI_N_COLUMNS
};

///////////////////////////////////////////////
//This creates and returns a GtkTreeView, to be added
//like any ordinary widget in to a vbox, etc. It does
//so by creating a GtkTreeStorefills, filling it with 
//GN data, then using that to create the GtkTreeView
//(it then deletes the GtkTreeStore).
//see:
// http://library.gnome.org/devel/gtk/stable/TreeWidget.html
// http://scentric.net/tutorial/sec-treeview-col-example.html
GtkWidget *GN_gui_CreateGtkList (void)
{
 GtkTreeStore *store;
 GtkWidget *tree = NULL;
 //GtkTreeViewColumn *column;
 GtkCellRenderer *renderer;
 GtkTreeIter iter;              //only need one unless doing a real tree view

 GN_Friend *curFriend = GN_My.FirstFriend;
 struct in_addr tmp_in_addr;

 /* Create a model.  We are using the store model for now, though we
  * could use any other GtkTreeModel */
 store = gtk_tree_store_new (GNGUI_N_COLUMNS,
                             G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN);

 /* populate the model: */
 while (curFriend != NULL)
 {
  tmp_in_addr.s_addr = curFriend->IP;
  gtk_tree_store_append (store, &iter, NULL);   // Acquire an iterator for tree
  gtk_tree_store_set (store, &iter,
                      GNGUI_IP_COLUMN, inet_ntoa (tmp_in_addr),
                      GNGUI_PORT_COLUMN, ntohs (curFriend->Port),
                      GNGUI_SCHED_COLUMN,
                      (curFriend->RunningID ==
                       GN_My.RunningID) ? TRUE : FALSE, -1);

  //all list passes need this:
  curFriend = curFriend->NextFriend;
 }

 /* Create a view */
 tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));

 /* The view now holds a reference.  We can get rid of our own
  * reference */
 g_object_unref (G_OBJECT (store));

 //Create a cell render:
 renderer = gtk_cell_renderer_text_new ();
 // g_object_set (G_OBJECT (renderer), "foreground", "red", NULL);

 /* First column */
 /* Create a column, associating the "text" attribute of the
  * cell_renderer to the first column of the model */
 gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree),
                                              -1,
                                              "IP",
                                              renderer,
                                              "text", GNGUI_IP_COLUMN, NULL);

 /* Second column */
 renderer = gtk_cell_renderer_text_new ();
 gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree),
                                              -1,
                                              "Port",
                                              renderer,
                                              "text", GNGUI_PORT_COLUMN,
                                              NULL);

 /* Last column */
 renderer = gtk_cell_renderer_toggle_new ();
 gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree),
                                              -1,
                                              "Schedule",
                                              renderer,
                                              "active", GNGUI_SCHED_COLUMN,
                                              NULL);

 /* Now we can manipulate the view just like any other GTK widget */
 return tree;
}
