#ifndef _VOICEGUI_H_
#define _VOICEGUI_H_

extern gboolean VG_RelistFlag;  //set TRUE to tell lister to redo list
extern GtkWidget *VG_TreeView;
extern void main_UpdateGUI_Voices (GtkVBox * mainbox);
extern void vgui_Checkbox_Mute (GtkCellRendererToggle * cell,
                                gchar * path_str, gpointer data);
extern void vgui_Checkbox_View (GtkCellRendererToggle * cell,
                                gchar * path_str, gpointer data);
extern void vgui_List_AddColumns (GtkTreeView * treeview);
extern GtkTreeModel *vgui_List_CreateModel (void);
extern SG_Voice *vgui_Voice_GetIndex (int index);
extern gboolean vgui_onRightButton (GtkWidget * treeview,
                                    GdkEventButton * event,
                                    gpointer userdata);
extern gboolean vgui_onSelectionChange (GtkTreeSelection * selection,
                                        GtkTreeModel * model,
                                        GtkTreePath * path,
                                        gboolean path_currently_selected,
                                        gpointer userdata);
extern void VG_FillList (GtkTreeView * treeview);
extern void VG_DestroyTreeView ();      //20100624: used for Open and New
extern int VG_VerifyCount (GtkTreeView * treeview);
extern SG_Voice *VG_GetVoice (int ID);  //returns an SG Voice for an SG ID
extern int VG_GetVoiceIndex (SG_Voice * refVoice);      //returns a BB voice index for an SG Voice
extern gint VG_GetVoiceCount ();        //just BB_VoiceCount at the moment
#endif
