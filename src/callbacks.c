#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>     //needed for keyboard translations
#include "callbacks.h"
#include "main.h"

///////////////////////////////
//Menus:
void on_menuitem_new_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_NewGraph (1);
}

void on_menuitem_SetScheduleInfo_activate (GtkMenuItem * menuitem,
                                           gpointer user_data)
{
 main_SetScheduleInfo (TRUE);
}

void on_menuitem_RestoreDefaultFile_activate (GtkMenuItem * menuitem,
                                              gpointer user_data)
{
 main_WriteDefaultFile ();
}

void on_menuitem_edit_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_on_edit_activate ();
}

void on_menuitem_revert_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_on_revert_activate (menuitem, user_data);
}

void on_export_audio_to_file_activate (GtkMenuItem * menuitem,
                                       gpointer user_data)
{
 main_on_export_audio_to_file_activate ();
}

//NOTE: THIS OPTION WAS DEACTIVATED IN THE GLADE FILE 20071203
void on_export_mp3_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_on_export_mp3_activate ();
}

void on_menuitem_undo_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_z);
}

void on_menuitem_redo_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_y);
}

void on_menuitem_apply_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_p);
}

void on_menuitem_preferences_activate (GtkMenuItem * menuitem,
                                       gpointer user_data)
{
 main_Preferences ();
}

void on_menuitem_DataPointSize_activate (GtkMenuItem * menuitem,
                                         gpointer user_data)
{
 main_DataPointSize ();
}

void on_menuitem_voices_activate (GtkMenuItem * menuitem, gpointer user_data)
{
}

void on_menuitem_open_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_o);
}

void on_menuitem_OpenMerge_activate (GtkMenuItem * menuitem,
                                     gpointer user_data)
{
 main_OpenFile (TRUE, NULL);
}

void on_menuitem_OpenFromLibrary_activate (GtkMenuItem * menuitem,
                                           gpointer user_data)
{
 main_OpenFromLibrary ();
}

void on_menuitem_save_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_s);
}

void on_menuitem_save_as_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 //main_EventToKeypress (GDK_CONTROL_MASK, GDK_s);
 main_OnUserSaveAsFile ();
}

void on_menuitem_quit_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_q);
}

void on_EditDP_cut_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_x);
}

void on_EditDP_copy_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_c);
}

void on_EditDP_paste_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_v);
}

void on_EditDP_delete_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (0, GDK_Delete);
}

void on_EditDP_delete_time_activate (GtkMenuItem * menuitem,
                                     gpointer user_data)
{
 main_EventToKeypress (GDK_SHIFT_MASK, GDK_Delete);
}

void on_EditDP_clear_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_ClearDataPointsInVoices ();
}

void on_EditDP_paste_at_end_activate (GtkMenuItem * menuitem,
                                      gpointer user_data)
{
 main_EventToKeypress (GDK_SHIFT_MASK, GDK_V);
}

void on_EditDP_properties_activate (GtkMenuItem * menuitem,
                                    gpointer user_data)
{
 main_DPPropertiesDialog ();
}

void on_EditVoice_add_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_j);
}

void on_EditVoice_delete_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_d);
}

//=Start Selection Menu:
void
on_menuitem_SelectAll_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_a);
}

void
on_menuitem_DeselectAll_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_SHIFT_MASK, GDK_A);
}

void
on_menuitem_SelectInvert_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_i);
}

void
on_menuitem_DeselectAllInVoice_activate (GtkMenuItem * menuitem,
                                         gpointer user_data)
{
 main_EventToKeypress (GDK_SHIFT_MASK, GDK_E);
}

void
on_menuitem_InvertSelectionInVoice_activate (GtkMenuItem * menuitem,
                                             gpointer user_data)
{
 main_EventToKeypress (GDK_SHIFT_MASK, GDK_I);
}

void on_menuitem_SelectLast_activate (GtkMenuItem * menuitem,
                                      gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_u);
}

void on_menuitem_SelectFirst_activate (GtkMenuItem * menuitem,
                                       gpointer user_data)
{
 main_EventToKeypress (GDK_SHIFT_MASK, GDK_U);
}

void on_menuitem_SelectDuration_activate (GtkMenuItem * menuitem,
                                          gpointer user_data)
{
 main_SelectDuration ();
}

void on_menuitem_SelectProximity_all_activate (GtkMenuItem * menuitem,
                                               gpointer user_data)
{
 main_SelectProximity_All ();
}

void on_menuitem_SelectProximity_singlepoint_activate (GtkMenuItem * menuitem,
                                                       gpointer user_data)
{
 main_SelectProximity_SinglePoint ();
}

//=End Selection Menu

void
on_EditVoice_properties_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_t);
}

void
on_menuitem_SelectAllInVoice_activate (GtkMenuItem * menuitem,
                                       gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_e);
}

void
on_menuitem_AlignDataPoints_activate (GtkMenuItem * menuitem,
                                      gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_l);
}

void
on_menuitem_ScaleTime_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_ScaleDataPoints_Time (main_drawing_area);
}

void on_menuitem_ScaleY_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_g);
}

void on_menuitem_AddRandomToY_activate (GtkMenuItem * menuitem,
                                        gpointer user_data)
{
 main_AddRandomToDataPoints_Y (main_drawing_area);
}

void on_menuitem_AddRandomToX_activate (GtkMenuItem * menuitem,
                                        gpointer user_data)
{
 main_AddRandomToDataPoints_time (main_drawing_area);
}

void on_menuitem_AddToY_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_AddToDataPoints_Y (main_drawing_area);
}

void on_menuitem_AddToX_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_AddToDataPoints_time (main_drawing_area);
}

void on_menuitem_DuplicateSelectedVoice_activate (GtkMenuItem * menuitem,
                                                  gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_b);
}

void on_menuitem_DuplicateAllVoices_activate (GtkMenuItem * menuitem,
                                              gpointer user_data)
{
 main_DuplicateAllVoices ();
}

void on_menuitem_SelectInterval_activate (GtkMenuItem * menuitem,
                                          gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_k);
}

void on_menuitem_SelectNeighbor_activate (GtkMenuItem * menuitem,
                                          gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_m);
}

void on_menuitem_about_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_AboutDialogBox ();
}

void on_menuitem_help_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_MessageDialogBox (main_GnauralHelp, GTK_MESSAGE_INFO, GTK_BUTTONS_OK);
}

void on_menuitem_ReverseVoice_activate (GtkMenuItem * menuitem,
                                        gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_r);
}

void on_menuitem_TruncateSchedule_activate (GtkMenuItem * menuitem,
                                            gpointer user_data)
{
 main_TruncateSchedule ();
}

void on_menuitem_InvertY_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 main_InvertY ();
}

void on_menuitem_RoundValues_activate (GtkMenuItem * menuitem,
                                       gpointer user_data)
{
 main_RoundValues ();
}

void on_menuitem_GnauralNet_StartStop_activate (GtkMenuItem * menuitem,
                                                gpointer user_data)
{
 main_GnauralNet_StopStart (menuitem);
}

void on_menuitem_GnauralNet_JoinFriend_activate (GtkMenuItem * menuitem,
                                                 gpointer user_data)
{
 main_GnauralNet_JoinFriend (menuitem);
}

void on_menuitem_GnauralNet_PhaseInfo_activate (GtkMenuItem * menuitem,
                                                gpointer user_data)
{
 main_GnauralNet_PhaseInfo (menuitem);
}

void on_menuitem_GnauralNet_ShowInfo_activate (GtkMenuItem * menuitem,
                                               gpointer user_data)
{
 main_GnauralNet_ShowInfo (menuitem);
}

//End Menus
///////////////////////////////

void on_drawingarea_graph_realize (GtkWidget * widget, gpointer user_data)
{
 main_realize (widget, user_data);
}

gboolean on_drawingarea_graph_expose_event (GtkWidget * widget,
                                            GdkEventExpose * event,
                                            gpointer user_data)
{
 main_expose_event (widget, event);
 return FALSE;
}

gboolean on_drawingarea_graph_configure_event (GtkWidget * widget,
                                               GdkEventConfigure * event,
                                               gpointer user_data)
{
 main_configure_event (widget, event);
 return FALSE;
}

gboolean on_drawingarea_graph_delete_event (GtkWidget * widget,
                                            GdkEvent * event,
                                            gpointer user_data)
{
 return FALSE;
}

gboolean on_drawingarea_graph_motion_notify_event (GtkWidget * widget,
                                                   GdkEventMotion * event,
                                                   gpointer user_data)
{
 main_motion_notify_event (widget, event);
 return FALSE;
}

gboolean on_drawingarea_graph_button_press_event (GtkWidget * widget,
                                                  GdkEventButton * event,
                                                  gpointer user_data)
{
 main_button_press_event (widget, event);
 return FALSE;
}

gboolean on_drawingarea_graph_button_release_event (GtkWidget * widget,
                                                    GdkEventButton * event,
                                                    gpointer user_data)
{
 main_button_release_event (widget, event);
 return FALSE;
}

//e20070620:
gboolean on_drawingarea_graph_key_press_event (GtkWidget * widget,
                                               GdkEventKey * event,
                                               gpointer user_data)
{
 main_key_press_event (widget, event);
 // fprintf(stderr,"Did a keypress in Drawingarea\n");
 // return FALSE;
 return TRUE;   //this tells GTK that I don't want it processing this any further
}

//e20070620:
gboolean on_drawingarea_graph_key_release_event (GtkWidget * widget,
                                                 GdkEventKey * event,
                                                 gpointer user_data)
{
 //fprintf(stderr,"Did a keyrelease in Drawingarea\n");
 main_key_release_event (widget, event);
 return FALSE;
}

//e20070620
gboolean on_drawingarea_graph_enter_notify_event (GtkWidget * widget,
                                                  GdkEventCrossing * event,
                                                  gpointer user_data)
{
 gtk_widget_grab_focus (main_drawing_area);
 //This works too:
 // gtk_window_set_focus(main_window, main_drawing_area);
 return FALSE;
}

void on_window_main_destroy (GtkObject * object, gpointer user_data)
{
 main_EventToKeypress (GDK_CONTROL_MASK, GDK_q);
}

void on_view_frequency_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 gtk_toggle_button_set_active (main_togglebuttonViewFreq, TRUE);
}

void on_view_stereo_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 gtk_toggle_button_set_active (main_togglebuttonViewBal, TRUE);
}

void on_view_volume_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 gtk_toggle_button_set_active (main_togglebuttonViewVol, TRUE);
}

void on_view_beat_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 gtk_toggle_button_set_active (main_togglebuttonViewBeat, TRUE);
}

void on_buttonPlay_clicked (GtkButton * button, gpointer user_data)
{
 main_OnButton_Play (button);
}

void on_buttonRewind_clicked (GtkButton * button, gpointer user_data)
{
 main_OnButton_ForwardRewind (-.05);
}

void on_buttonForward_clicked (GtkButton * button, gpointer user_data)
{
 main_OnButton_ForwardRewind (.051);
}

void on_buttonStop_clicked (GtkButton * button, gpointer user_data)
{
 main_OnButton_Stop (button);
}

void on_hscaleVolume_value_changed (GtkRange * range, gpointer user_data)
{
 main_on_hscaleVolume (gtk_range_get_value (range));
}

void on_hscaleBalance_value_changed (GtkRange * range, gpointer user_data)
{
 main_on_hscaleBalance (gtk_range_get_value (range));
}

void on_vscale_Y_value_changed (GtkRange * range, gpointer user_data)
{
 main_vscale_Y_value_change (range);
}

gboolean on_vscale_Y_button_press_event (GtkWidget * widget,
                                         GdkEventButton * event,
                                         gpointer user_data)
{
 main_vscale_Y_button_event (widget, TRUE);
 return FALSE;
}

gboolean on_vscale_Y_button_release_event (GtkWidget * widget,
                                           GdkEventButton * event,
                                           gpointer user_data)
{
 main_vscale_Y_button_event (widget, FALSE);
 return FALSE;
}

void on_hscale_X_value_changed (GtkRange * range, gpointer user_data)
{
 main_hscale_X_value_change (range);
}

gboolean on_hscale_X_button_press_event (GtkWidget * widget,
                                         GdkEventButton * event,
                                         gpointer user_data)
{
 main_hscale_X_button_event (widget, TRUE);
 return FALSE;
}

gboolean on_hscale_X_button_release_event (GtkWidget * widget,
                                           GdkEventButton * event,
                                           gpointer user_data)
{
 main_hscale_X_button_event (widget, FALSE);
 return FALSE;
}

void on_checkbutton_XYscale_toggled (GtkToggleButton * togglebutton,
                                     gpointer user_data)
{
 main_XY_scaleflag = gtk_toggle_button_get_active (togglebutton);
}

void on_checkbutton_MagneticPointer_toggled (GtkToggleButton * togglebutton,
                                             gpointer user_data)
{
 main_on_checkbutton_MagneticPointer_toggled (togglebutton);
}

void on_radiobuttonGraphView_BaseFreq_activate
 (GtkButton * button, gpointer user_data)
{
}

void on_radiobuttonGraphView_BaseFreq_toggled
 (GtkToggleButton * togglebutton, gpointer user_data)
{
 if (TRUE == gtk_toggle_button_get_active (togglebutton))
  main_SetGraphType (togglebutton);
}

void on_radiobuttonGraphView_BeatFreq_toggled
 (GtkToggleButton * togglebutton, gpointer user_data)
{
 if (TRUE == gtk_toggle_button_get_active (togglebutton))
  main_SetGraphType (togglebutton);
}

void on_radiobuttonGraphView_Volume_toggled (GtkToggleButton * togglebutton,
                                             gpointer user_data)
{
 if (TRUE == gtk_toggle_button_get_active (togglebutton))
  main_SetGraphType (togglebutton);
}

void on_radiobuttonGraphView_Balance_toggled
 (GtkToggleButton * togglebutton, gpointer user_data)
{
 if (TRUE == gtk_toggle_button_get_active (togglebutton))
  main_SetGraphType (togglebutton);
}

void on_entryLoops_activate (GtkEntry * entry, gpointer user_data)
{
 main_on_entryLoops_activate (entry, user_data);
}

void on_checkbuttonSwapStereo_toggled (GtkToggleButton * togglebutton,
                                       gpointer user_data)
{
 BB_StereoSwap = gtk_toggle_button_get_active (togglebutton);
}

void on_checkbuttonOutputMono_toggled (GtkToggleButton * togglebutton,
                                       gpointer user_data)
{
 BB_Mono = gtk_toggle_button_get_active (togglebutton);
}

gboolean on_progressbar_main_button_press_event (GtkWidget * widget,
                                                 GdkEventButton * event,
                                                 gpointer user_data)
{
 main_progressbar_button_press_event (widget, event);
 return FALSE;
}
