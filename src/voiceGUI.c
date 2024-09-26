/*
   voiceGUI.c
   code for listing and selecting Gnaural voices 
   Depends on:
   glib.h
   gtk/gtk.h
   main.h
   voiceGUI.h
   ScheduleGUI.h

   Theory: give main_UpdateGUI_Voices() a GtkVBox, and this fills it with a table

   Copyright (C) 2011  Bret Logan

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

//TODO: 
// - 20100625: Duplicate Voices not actually updating list (i think it updates list before BB or SG have the new data)
// - 20100624: Make list NOT keep refilling from SG/BB, but literally delete the specific LIST row when voice deleted, etc.
// - 20100624: Verify that SG ID's can be in arbitrary order for everything to still function right
// - 20100623: figure out why main_UpdateGUI_Voices is getting called double times
// - 20100622: make voice list box sizeable
// - 20100623: make windows pop-out (dissociable)
// - 20100624: add "Solo"
// - 20100625: figure out why a voice has to be visible to be deleted

#include <glib.h>
#include <gtk/gtk.h>
#include "main.h"
#include "voiceGUI.h"
#include "ScheduleGUI.h"
#include "BinauralBeat.h"

/////////////////////////////////////////////////////////////
//Globals variables:
GtkWidget *VG_TreeView = NULL;  //GtkWidget so i can add to other widgets, but cast to GtkTreeView when needed 
gboolean VG_RelistFlag = FALSE; //gets set to tell lister to redo list
enum
{
 VG_COLUMN_COUNT,
 VG_COLUMN_VIEW,
 VG_COLUMN_MUTE,
 VG_COLUMN_TYPE,
 VG_COLUMN_DESC,
 VG_COLUMN_COUNTTOTS,
 VG_COLUMN_COUNTSELS,
 VG_COLUMNCOUNT
};

/////////////////////////////////////////////////////
//20070629: returns the SG_Voice that matches the ID or NULL if 
//there is no match. Pray to god that IDs are always unique, 'k?
SG_Voice *VG_GetVoice (int ID)
{
 SG_Voice *curVoice = SG_FirstVoice;

 while (curVoice != NULL)
 {
  if (curVoice->ID == ID)
  {
   return curVoice;
  }
  curVoice = curVoice->NextVoice;
 }
 SG_DBGOUT_INT ("No SG ID", ID);
 VG_RelistFlag = TRUE;
 return NULL;
}

/////////////////////////////////////////////////////
//20070629: returns the equivalent BB voice from a SG voice,
//needed because BB is arrays and SG linked-lists, but BB is always 
//sync'd to SG's list order
//IMPORTANT: ERROR RETURNS -1
int VG_GetVoiceIndex (SG_Voice * refVoice)
{
 SG_Voice *curVoice = SG_FirstVoice;
 int count = 0;

 while (curVoice != NULL)
 {
  if (curVoice == refVoice)
  {
   return count;
  }
  count++;
  curVoice = curVoice->NextVoice;
 }
 SG_DBGOUT ("NO SUCH VOICE!");
 VG_RelistFlag = TRUE;
 return -1;
}

/////////////////////////////////////
//20100624: status: works ok
gint VG_GetVoiceCount ()
{
 return BB_VoiceCount;
}

/////////////////////////////////////////////////////////
//20100623: need to rework
//Purpose: to manually highlight a specific entry in vgui
void VG_List_SetSelectedVoice (GtkTreeModel * model,
                               GtkTreeSelection * selection, gint index)
{
 int i;
 GtkTreeIter iter;
 gtk_tree_model_get_iter_first (model, &iter);
 for (i = 0; i < index; i++)
  gtk_tree_model_iter_next (model, &iter);
 gtk_tree_selection_select_iter (selection, &iter);
}

/////////////////////////////////////
//20100622: status: works ok
static void VG_Checkbox_View (GtkCellRendererToggle * cell,
                              gchar * path_str, gpointer data)
{
 GtkTreeModel *model = (GtkTreeModel *) data;
 GtkTreeIter iter;
 GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
 gboolean CheckboxState;
 gint voiceID;

 // get toggled iter and extract checkbox state and voiceID from it:
 gtk_tree_model_get_iter (model, &iter, path);
 gtk_tree_model_get (model, &iter, VG_COLUMN_VIEW, &CheckboxState,
                     VG_COLUMN_COUNT, &voiceID, -1);

 SG_DBGOUT_INT ("View, SG ID", voiceID);

 //get voice index: 
 SG_Voice *curVoice = VG_GetVoice (voiceID);
 if (NULL != curVoice)
 {
  curVoice->hide = CheckboxState;

  if (TRUE == curVoice->hide)
  {
   SG_VoiceTestLegalSelection ();
   if (curVoice->state == SG_SELECTED)
    curVoice->state = SG_UNSELECTED;
  }
 }

 // toggle the checkbox value FALSE/TRUE:
 CheckboxState ^= 1;

 // set new value 
 gtk_list_store_set (GTK_LIST_STORE (model), &iter, VG_COLUMN_VIEW,
                     CheckboxState, -1);

 // clean up 
 gtk_tree_path_free (path);
 SG_ConvertDataToXY (main_drawing_area);        //20110519
}

/////////////////////////////////////
//20100622
static void VG_Checkbox_Mute (GtkCellRendererToggle * cell,
                              gchar * path_str, gpointer data)
{
 GtkTreeModel *model = (GtkTreeModel *) data;
 GtkTreeIter iter;
 GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
 gboolean CheckboxState;
 gint voiceID;

 // get toggled iter
 gtk_tree_model_get_iter (model, &iter, path);
 gtk_tree_model_get (model, &iter, VG_COLUMN_MUTE, &CheckboxState,
                     VG_COLUMN_COUNT, &voiceID, -1);

 SG_DBGOUT_INT ("Mute, SG ID:", voiceID);

 // do something with the value 
 CheckboxState ^= 1;

 //get voice index: 
 SG_Voice *curVoice = VG_GetVoice (voiceID);
 if (NULL != curVoice)
 {
  curVoice->mute = CheckboxState;
  voiceID = VG_GetVoiceIndex (curVoice);
  if (-1 != voiceID)
  {
   BB_Voice[voiceID].mute = CheckboxState;
   SG_DBGOUT_INT ("Mute, BB ID", voiceID);
  }
 }
 // set new value 
 gtk_list_store_set (GTK_LIST_STORE (model), &iter, VG_COLUMN_MUTE,
                     CheckboxState, -1);
 // clean up 
 gtk_tree_path_free (path);
}

////////////////////////////////
//20100622: status: working, not done
gboolean VG_onSelectionChange (GtkTreeSelection * selection,
                               GtkTreeModel * model,
                               GtkTreePath * path,
                               gboolean path_currently_selected,
                               gpointer userdata)
{
 //    g_print ("New Row Selected (%s line %d)\n",__FILE__, __LINE__);
 GtkTreeIter iter;

 if (gtk_tree_model_get_iter (model, &iter, path))
 {
  int voiceID;
  //20100624: voiceID should never be assumed to be an actual count 
  //index because it can be wildly out of whack when voices are deleted
  gtk_tree_model_get (model, &iter, VG_COLUMN_COUNT, &voiceID, -1);
  if (!path_currently_selected)
  {
   SG_DBGOUT_INT ("Selected SG Voice:", voiceID);
   SG_SelectVoice (VG_GetVoice (voiceID));      //this looks for that ID, not using it a literal count
  }
  else
  {
   SG_DBGOUT_INT ("Unselected SG Voice:", voiceID);
  }
 }
 //SG_DBGOUT_INT ("BB-VG=", VG_VerifyCount(GTK_TREE_VIEW (VG_TreeView)));
 return TRUE;   // allow selection state to change
}

///////////////////////////////////////////////////////////////////
//20100622: Brings up Voices Properties box for selected row
//          status: working, not done
gboolean VG_onRightButton (GtkWidget * treeview,
                           GdkEventButton * event, gpointer userdata)
{
 if (NULL == gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)))
 {
  SG_DBGOUT ("Treeview == NULL");
 }
 //VG_RefillList (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview)));  //20100624 - critical!
 // single click with the right mouse button:
 if (event->type == GDK_BUTTON_PRESS && event->button == 3)
 {
  //g_print ("Single right click on the tree view.\n");
  SG_Voice *curVoice = SG_SelectVoice (NULL);   //return currently selected voice
  main_VoicePropertiesDialog (main_drawing_area, curVoice);
  //VG_DestroyTreeView ();
  //VG_RefillList (gtk_tree_view_get_model (GTK_TREE_VIEW (treeview))); //20100624 - critical!
  //main_UpdateGUI_Voices (main_vboxVoices);

  // optional: select row if no row is selected or only
  // one other row is selected
  if (0)
  {
   GtkTreeSelection *selection;

   selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

   // Note: gtk_tree_selection_count_selected_rows() does not
   // exist in gtk+-2.0, only in gtk+ >= v2.2 !
   if (gtk_tree_selection_count_selected_rows (selection) <= 1)
   {
    GtkTreePath *path;

    // Get tree path for row that was clicked 
    if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
                                       (gint) event->x,
                                       (gint) event->y,
                                       &path, NULL, NULL, NULL))
    {
     gtk_tree_selection_unselect_all (selection);
     gtk_tree_selection_select_path (selection, path);
     gtk_tree_path_free (path);
    }
   }
  }     // end of optional bit 
  //view_popup_menu(treeview, event, userdata);
  return TRUE;  // we handled this
 }
 return FALSE;  // we did not handle this
}

/////////////////////////////////////////////
//20100625 - gives user ability to edit descriptions on-the-fly
void VG_EditDescription_callback (GtkCellRendererText * cell,
                                  gchar * path_string,
                                  gchar * new_text, gpointer user_data)
{
 int voiceID;
 GtkTreeIter iter;
 GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (VG_TreeView));
 gtk_tree_model_get_iter_from_string (model, &iter, path_string);
 gtk_tree_model_get (model, &iter, VG_COLUMN_COUNT, &voiceID, -1);
 SG_DBGOUT (new_text);
 SG_Voice *curVoice = VG_GetVoice (voiceID);
 if (NULL != curVoice)
 {
  SG_StringAllocateAndCopy (&(curVoice->description), (char *) new_text);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, VG_COLUMN_DESC, new_text,
                      -1);
 }
}

/////////////////////////////////////////////
//20100625 - toggles all-on or all off
void VG_Mute_column_clicked (GtkTreeViewColumn * column, gpointer user_data)
{
 SG_Voice *curVoice = SG_FirstVoice;
 int VoiceIndex = 0;
 static gboolean val = TRUE;

 while (curVoice != NULL)
 {
  curVoice->mute = BB_Voice[VoiceIndex].mute = val;
  curVoice = curVoice->NextVoice;
  ++VoiceIndex;
 }
 val ^= 1;
 VG_FillList (GTK_TREE_VIEW (VG_TreeView));
}

/////////////////////////////////////////////
//20100625 - toggles see-all or see-none
void VG_View_column_clicked (GtkTreeViewColumn * column, gpointer user_data)
{
 SG_Voice *curVoice = SG_FirstVoice;
 static gboolean val = TRUE;

 while (curVoice != NULL)
 {
  curVoice->hide = val;
  curVoice = curVoice->NextVoice;
 }
 val ^= 1;
 VG_FillList (GTK_TREE_VIEW (VG_TreeView));
 SG_ConvertDataToXY (main_drawing_area);        //20110519
}

//=====================
/////////////////////////////////////////////
//20100622 - Adds columns to the List store. Called after
//           VG_List_CreateModel ()
static void VG_List_AddColumns (GtkTreeView * treeview)
{
 GtkCellRenderer *renderer;
 GtkTreeViewColumn *column;
 GtkTreeModel *model = gtk_tree_view_get_model (treeview);

 //These are the columns: 
 // VG_COLUMN_COUNT,     int
 // VG_COLUMN_VIEW,      checkbox
 // VG_COLUMN_MUTE,      checkbox
 // VG_COLUMN_TYPE,      string
 // VG_COLUMN_DESC,      string
 // VG_COLUMN_COUNTTOTS, int
 // VG_COLUMN_COUNTSELS, int

 //Now for the actual columns:
 // VG_COLUMN_COUNT, int:
 renderer = gtk_cell_renderer_text_new ();
 g_object_set (G_OBJECT (renderer), "foreground", "red", NULL);
 column = gtk_tree_view_column_new_with_attributes ("ID",
                                                    renderer,
                                                    "text",
                                                    VG_COLUMN_COUNT, NULL);
 gtk_tree_view_column_set_sort_column_id (column, VG_COLUMN_COUNT);
 gtk_tree_view_append_column (treeview, column);

 // VG_COLUMN_VIEW, checkbox:
 renderer = gtk_cell_renderer_toggle_new ();
 g_signal_connect (renderer, "toggled", G_CALLBACK (VG_Checkbox_View), model);
 column =
  gtk_tree_view_column_new_with_attributes ("View", renderer, "active",
                                            VG_COLUMN_VIEW, NULL);
 gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),        // set this column to a fixed sizing (of 50 pixels) 
                                  GTK_TREE_VIEW_COLUMN_FIXED);
 gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column), 50);
 gtk_tree_view_column_set_clickable (column, TRUE);
 gtk_tree_view_append_column (treeview, column);
 g_signal_connect (G_OBJECT (column), "clicked",
                   G_CALLBACK (VG_View_column_clicked), NULL);

 // VG_COLUMN_MUTE, checkbox:
 renderer = gtk_cell_renderer_toggle_new ();
 g_signal_connect (renderer, "toggled", G_CALLBACK (VG_Checkbox_Mute), model);
 column =
  gtk_tree_view_column_new_with_attributes ("Mute", renderer, "active",
                                            VG_COLUMN_MUTE, NULL);
 gtk_tree_view_column_set_sizing (GTK_TREE_VIEW_COLUMN (column),        // set this column to a fixed sizing (of 50 pixels) 
                                  GTK_TREE_VIEW_COLUMN_FIXED);
 gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (column), 50);
 gtk_tree_view_column_set_clickable (column, TRUE);
 gtk_tree_view_append_column (treeview, column);
 g_signal_connect (G_OBJECT (column), "clicked",
                   G_CALLBACK (VG_Mute_column_clicked), NULL);

 // VG_COLUMN_TYPE, string:
 renderer = gtk_cell_renderer_text_new ();
 g_object_set (G_OBJECT (renderer), "foreground", "blue", NULL);
 column = gtk_tree_view_column_new_with_attributes ("Type",
                                                    renderer,
                                                    "text",
                                                    VG_COLUMN_TYPE, NULL);
 gtk_tree_view_column_set_sort_column_id (column, VG_COLUMN_TYPE);
 gtk_tree_view_append_column (treeview, column);

 // VG_COLUMN_DESC, string:
 renderer = gtk_cell_renderer_text_new ();
 g_object_set (renderer, "editable", TRUE, NULL);
 g_signal_connect (renderer, "edited",
                   (GCallback) VG_EditDescription_callback, NULL);
 column =
  gtk_tree_view_column_new_with_attributes ("Description", renderer, "text",
                                            VG_COLUMN_DESC, NULL);
 gtk_tree_view_column_set_sort_column_id (column, VG_COLUMN_DESC);
 gtk_tree_view_append_column (treeview, column);

 // VG_COLUMN_COUNTTOTS, int:
 renderer = gtk_cell_renderer_text_new ();
 column = gtk_tree_view_column_new_with_attributes ("Total",
                                                    renderer,
                                                    "text",
                                                    VG_COLUMN_COUNTTOTS,
                                                    NULL);
 gtk_tree_view_column_set_sort_column_id (column, VG_COLUMN_COUNTTOTS);
 gtk_tree_view_append_column (treeview, column);

 // VG_COLUMN_COUNTSELS, int:
 renderer = gtk_cell_renderer_text_new ();
 column = gtk_tree_view_column_new_with_attributes ("Selected",
                                                    renderer,
                                                    "text",
                                                    VG_COLUMN_COUNTSELS,
                                                    NULL);
 gtk_tree_view_column_set_sort_column_id (column, VG_COLUMN_COUNTSELS);
 gtk_tree_view_append_column (treeview, column);
}

/////////////////////////////////////////////
//20100622 - creates the List store and adds my data to it. Called 
//           first, before VG_List_AddColumns
static GtkTreeModel *VG_List_CreateModel ()
{
 GtkListStore *store;
 GtkTreeIter iter;

 //"Another way to refer to a row in a list or tree is GtkTreeIter. A tree iter is just a structure that contains a couple of pointers that mean something to the model you are using. Tree iters are used internally by models, and they often contain a direct pointer to the internal data of the row in question. You should never look at the content of a tree iter and you must not modify it directly either."

 // create list store                              
 store = gtk_list_store_new (VG_COLUMNCOUNT,    // total count of colums
                             G_TYPE_INT,        // VG_COLUMN_COUNT
                             G_TYPE_BOOLEAN,    // VG_COLUMN_VIEW
                             G_TYPE_BOOLEAN,    // VG_COLUMN_MUTE
                             G_TYPE_STRING,     // VG_COLUMN_TYPE
                             G_TYPE_STRING,     // VG_COLUMN_DESC
                             G_TYPE_INT,        // VG_COLUMN_COUNTTOTS
                             G_TYPE_INT // VG_COLUMN_COUNTSELS
  );

 // add data to the list store 
 int VoiceIndex = 0;
 int dpcount_all = 0;
 int dpcount_selected = 0;
 SG_Voice *curVoice = SG_FirstVoice;

 /* populate the model: */
 while (curVoice != NULL)
 {
  main_sTmp[0] = '\0';
  main_VoiceInfoFormatter_type (curVoice, main_sTmp);
  SG_CountVoiceDPs (curVoice, &dpcount_all, &dpcount_selected);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      VG_COLUMN_COUNT, curVoice->ID,
                      VG_COLUMN_VIEW, TRUE == curVoice->hide ? FALSE : TRUE,
                      VG_COLUMN_MUTE, BB_Voice[VoiceIndex].mute,
                      VG_COLUMN_TYPE, main_sTmp,
                      VG_COLUMN_DESC, curVoice->description,
                      VG_COLUMN_COUNTTOTS, dpcount_all,
                      VG_COLUMN_COUNTSELS, dpcount_selected, -1);

  //main loop update
  ++VoiceIndex;
  curVoice = curVoice->NextVoice;
 }
 ////////////////////////////////

 return GTK_TREE_MODEL (store);
}

/////////////////////////////////////////////////////////
//20100624: hopefully fast count of rows
gint VG_CountRows (GtkTreeView * treeview)
{
 int count = 0;
 GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
 GtkTreeIter iter;
 gtk_tree_model_get_iter_first (model, &iter);
 do
 {
  //gtk_tree_model_get (model, &iter, VG_COLUMN_COUNT, &voiceID, -1);
  //gtk_list_store_set (GTK_LIST_STORE (model), &iter, VG_COLUMN_COUNT, i++, -1);
  ++count;
 }
 while (FALSE != gtk_tree_model_iter_next (model, &iter));
 return count;
}

   /////////////////////////////////////////////////////////
   //20100624: Makes sure BB/SG have same number of voices as VG
   //returns TRUE of counts were already sync'd
int VG_VerifyCount (GtkTreeView * treeview)
{
 return (BB_VoiceCount - VG_CountRows (GTK_TREE_VIEW (treeview)));
}

/////////////////////////////////////////////////////////
//20100624: Only deletes it if it's full
void VG_DestroyTreeView ()
{
 if (NULL != VG_TreeView)
 {
  SG_DBGOUT ("Destroying old VG_TreeView");
  gtk_widget_destroy (VG_TreeView);
  VG_TreeView = NULL;
 }
}

/////////////////////////////////////////////
//20100624
void VG_FillList (GtkTreeView * treeview)
{
 SG_Voice *curVoice;
 int voiceID;
 GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
 GtkTreeIter iter;
 gtk_tree_model_get_iter_first (model, &iter);

 // add data to the list store 
 int dpcount_all = 0;
 int dpcount_selected = 0;

 // populate the model:
 do
 {
  //first get the voiceID:
  gtk_tree_model_get (model, &iter, VG_COLUMN_COUNT, &voiceID, -1);
  //now match it up to the correct SG_Voice:
  curVoice = VG_GetVoice (voiceID);
  if (curVoice != NULL)
  {
   main_sTmp[0] = '\0';
   main_VoiceInfoFormatter_type (curVoice, main_sTmp);
   SG_CountVoiceDPs (curVoice, &dpcount_all, &dpcount_selected);
   gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                       VG_COLUMN_COUNT, curVoice->ID,
                       VG_COLUMN_VIEW, TRUE == curVoice->hide ? FALSE : TRUE,
                       VG_COLUMN_MUTE, curVoice->mute,
                       VG_COLUMN_TYPE, main_sTmp,
                       VG_COLUMN_DESC, curVoice->description,
                       VG_COLUMN_COUNTTOTS, dpcount_all,
                       VG_COLUMN_COUNTSELS, dpcount_selected, -1);
  }
  else
  {
   //bad news if we got here - probably means SG has uncaringly 
   //reordered IDs, gotta rebuild. THAT'S A BUG
   VG_DestroyTreeView ();
   return;
  }

  //  gtk_list_store_set (GTK_LIST_STORE (model), &iter, VG_COLUMN_COUNT, i++, -1);
 }
 while (FALSE != gtk_tree_model_iter_next (model, &iter));
}

/////////////////////////////////////////////////////////
//20100622: the big one.
//Purpose: 
// - (once for init only) create a scrollable window to put inside GtkVBox user gives me
// - (once for init and then time a voice is added or subtracted) create a list to store SG/BB data
// - (every call) Be sure BB_VoiceCount and number of rows is identical (adding or subtracting as needed)
// - refill list with things that can change (more complicated than it seems, as list order is arbitrary)
void main_UpdateGUI_Voices (GtkVBox * mainbox)
{
 static GtkWidget *sw = NULL;   //create so I can delete something inside box user passed me
 static int CellHeight = 24;    //arbitrary, gets set for-real later
 //############
 //First do the stuff that only happens once:
 //Create the scrollwindow to put in user's box if it doesn't exist yet:
 if (NULL == sw)
 {
  //  gtk_widget_destroy (sw);
  //Make a scrollable window to put in user's thang: 
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (mainbox), sw, TRUE, TRUE, 0);
 }

 //Create the list if it doesn't exist yet:
 if (TRUE == VG_RelistFlag || NULL == VG_TreeView || 0 != VG_VerifyCount (GTK_TREE_VIEW (VG_TreeView))) //this part is a hack because i am finding weirdness in Voice IDs
 {
  VG_RelistFlag = FALSE;
  SG_DBGOUT ("Creating new VG_TreeView");
  // create tree model by loading my data
  GtkTreeModel *treemodel = VG_List_CreateModel ();

  VG_DestroyTreeView ();        //only does it if != NULL

  // create tree view 
  VG_TreeView = gtk_tree_view_new_with_model (treemodel);
  //gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (treeview), TRUE) ;
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (VG_TreeView), TRUE);
  gtk_tree_view_set_search_column (GTK_TREE_VIEW (VG_TreeView),
                                   VG_COLUMN_DESC);
  GtkTreeSelection *selection =
   gtk_tree_view_get_selection (GTK_TREE_VIEW (VG_TreeView));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  //start 20110519
  int i = 0;
  SG_Voice *selVoice = SG_SelectVoice (NULL);
  if (NULL != selVoice)
  {
   i = VG_GetVoiceIndex (selVoice);
   if (-1 == i)
    i = 0;
  }
  VG_List_SetSelectedVoice (treemodel, selection, i);   //20100624: need to rework
  //end 20110519

  //two different approaches to doing this, using the more dedicated one:
  // g_signal_connect(selection, "changed",  G_CALLBACK(VG_onSelectionChange), NULL);
  gtk_tree_selection_set_select_function (selection, VG_onSelectionChange,
                                          NULL, NULL);

  //to determine when right mouse button is pressed:
  g_signal_connect (VG_TreeView, "button-press-event",
                    (GCallback) VG_onRightButton, NULL);

  g_object_unref (treemodel);   //20100624: WHAT? Apparently OK, i use gtk_tree_view_get_model

  gtk_container_add (GTK_CONTAINER (sw), VG_TreeView);

  // add columns to the tree view 
  VG_List_AddColumns (GTK_TREE_VIEW (VG_TreeView));

  //get height here so i don't have to do it over and over: 
  gtk_tree_view_column_cell_get_size (gtk_tree_view_get_column
                                      (GTK_TREE_VIEW (VG_TreeView), 0), NULL,
                                      NULL, NULL, NULL, &CellHeight);
  //SG_DBGOUT_INT ("row height:", CellHeight);
  CellHeight += 2;
  gtk_widget_show_all (sw);
 }
 //End of stuff only done once
 //############

 // 20100623: try to keep a sensible amount of list showing:
 if (6 > VG_GetVoiceCount ())
 {
  gtk_widget_set_usize (sw, 600, CellHeight * (1 + VG_GetVoiceCount ()));
 }
 else
 {
  gtk_widget_set_usize (sw, 600, CellHeight * 6);
 }

 VG_FillList (GTK_TREE_VIEW (VG_TreeView));
}
