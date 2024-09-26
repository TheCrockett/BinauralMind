#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C"
{
#endif
 void on_drawingarea_graph_realize (GtkWidget * widget, gpointer user_data);
 gboolean on_drawingarea_graph_expose_event (GtkWidget * widget,
                                             GdkEventExpose * event,
                                             gpointer user_data);
 gboolean on_drawingarea_graph_configure_event (GtkWidget * widget,
                                                GdkEventConfigure * event,
                                                gpointer user_data);
 gboolean on_drawingarea_graph_delete_event (GtkWidget * widget,
                                             GdkEvent * event,
                                             gpointer user_data);
 gboolean on_drawingarea_graph_motion_notify_event (GtkWidget * widget,
                                                    GdkEventMotion * event,
                                                    gpointer user_data);
 gboolean on_drawingarea_graph_button_press_event (GtkWidget * widget,
                                                   GdkEventButton * event,
                                                   gpointer user_data);
 gboolean on_drawingarea_graph_button_release_event (GtkWidget * widget,
                                                     GdkEventButton * event,
                                                     gpointer user_data);
 gboolean on_drawingarea_graph_key_press_event (GtkWidget * widget,
                                                GdkEventKey * event,
                                                gpointer user_data);

 gboolean on_drawingarea_graph_key_release_event (GtkWidget * widget,
                                                  GdkEventKey * event,
                                                  gpointer user_data);

 gboolean on_drawingarea_graph_enter_notify_event (GtkWidget * widget,
                                                   GdkEventCrossing * event,
                                                   gpointer user_data);

 void on_window_main_destroy (GtkObject * object, gpointer user_data);
 void on_view_frequency1_activate (GtkMenuItem * menuitem,
                                   gpointer user_data);
 void on_view_stereo_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_view_volume_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_view_beat_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_buttonPlay_clicked (GtkButton * button, gpointer user_data);
 void on_buttonRewind_clicked (GtkButton * button, gpointer user_data);
 void on_buttonForward_clicked (GtkButton * button, gpointer user_data);
 void on_buttonStop_clicked (GtkButton * button, gpointer user_data);
 void on_buttonVoicesVisible_clicked (GtkButton * button, gpointer user_data);
 void on_radiobuttonGraphView_BaseFreq_toggled
  (GtkToggleButton * togglebutton, gpointer user_data);
 void on_radiobuttonGraphView_BeatFreq_toggled
  (GtkToggleButton * togglebutton, gpointer user_data);
 void on_radiobuttonGraphView_Volume_toggled (GtkToggleButton * togglebutton,
                                              gpointer user_data);
 void on_radiobuttonGraphView_Balance_toggled
  (GtkToggleButton * togglebutton, gpointer user_data);
 void on_entryLoops_activate (GtkEntry * entry, gpointer user_data);
 void on_checkbuttonSwapStereo_toggled (GtkToggleButton * togglebutton,
                                        gpointer user_data);
 void on_checkbuttonOutputMono_toggled (GtkToggleButton * togglebutton,
                                        gpointer user_data);
 void on_hscaleVolume_value_changed (GtkRange * range, gpointer user_data);
 void on_hscaleBalance_value_changed (GtkRange * range, gpointer user_data);
 void on_vscale_Y_value_changed (GtkRange * range, gpointer user_data);
 gboolean on_vscale_Y_button_release_event (GtkWidget * widget,
                                            GdkEventButton * event,
                                            gpointer user_data);
 gboolean on_vscale_Y_button_press_event (GtkWidget * widget,
                                          GdkEventButton * event,
                                          gpointer user_data);
 ///////////////////////////////
 //Menus:
 void on_menuitem_new_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_open_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_OpenMerge_activate (GtkMenuItem * menuitem,
                                      gpointer user_data);
 void on_menuitem_OpenFromLibrary_activate (GtkMenuItem * menuitem,
                                            gpointer user_data);
 void on_menuitem_save_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_save_as_activate (GtkMenuItem * menuitem,
                                    gpointer user_data);
 void on_menuitem_quit_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_about_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_help_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_revert_activate (GtkMenuItem * menuitem,
                                   gpointer user_data);
 void on_menuitem_ScaleTime_activate (GtkMenuItem * menuitem,
                                      gpointer user_data);
 void on_menuitem_ScaleY_activate (GtkMenuItem * menuitem,
                                   gpointer user_data);
 void on_menuitem_AlignDataPoints_activate (GtkMenuItem * menuitem,
                                            gpointer user_data);
 void on_menuitem_AddRandomToY_activate (GtkMenuItem * menuitem,
                                         gpointer user_data);
 void on_menuitem_AddRandomToX_activate (GtkMenuItem * menuitem,
                                         gpointer user_data);
 void on_menuitem_AddToY_activate (GtkMenuItem * menuitem,
                                   gpointer user_data);
 void on_menuitem_AddToX_activate (GtkMenuItem * menuitem,
                                   gpointer user_data);
 void on_menuitem_SelectInterval_activate (GtkMenuItem * menuitem,
                                           gpointer user_data);
 void on_menuitem_SelectNeighbor_activate (GtkMenuItem * menuitem,
                                           gpointer user_data);
 void on_menuitem_SelectDuration_activate (GtkMenuItem * menuitem,
                                           gpointer user_data);
 void on_menuitem_SelectProximity_all_activate (GtkMenuItem * menuitem,
                                                gpointer user_data);
 void on_menuitem_SelectProximity_singlepoint_activate (GtkMenuItem *
                                                        menuitem,
                                                        gpointer user_data);
 void on_menuitem_preferences_activate (GtkMenuItem * menuitem,
                                        gpointer user_data);
 void on_menuitem_DataPointSize_activate (GtkMenuItem * menuitem,
                                          gpointer user_data);
 void on_menuitem_voices_activate (GtkMenuItem * menuitem,
                                   gpointer user_data);
 void on_menuitem_SelectAll_activate (GtkMenuItem * menuitem,
                                      gpointer user_data);
 void on_export_audio_to_file_activate (GtkMenuItem * menuitem,
                                        gpointer user_data);
 void on_export_mp3_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_undo_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_redo_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_EditDP_cut_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_EditDP_copy_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_EditDP_paste_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_EditDP_delete_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_EditDP_clear_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_EditDP_paste_at_end_activate (GtkMenuItem * menuitem,
                                       gpointer user_data);
 void on_EditDP_properties_activate (GtkMenuItem * menuitem,
                                     gpointer user_data);
 void on_EditVoice_add_activate (GtkMenuItem * menuitem, gpointer user_data);
 void
  on_EditVoice_delete_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_menuitem_apply_activate (GtkMenuItem * menuitem, gpointer user_data);
 void on_EditVoice_properties_activate (GtkMenuItem * menuitem,
                                        gpointer user_data);
 void on_menuitem_SelectAllInVoice_activate (GtkMenuItem * menuitem,
                                             gpointer user_data);
 void on_menuitem_DuplicateSelectedVoice_activate (GtkMenuItem * menuitem,
                                                   gpointer user_data);
 void on_menuitem_DuplicateAllVoices_activate (GtkMenuItem * menuitem,
                                               gpointer user_data);
 void on_menuitem_SelectLast_activate (GtkMenuItem * menuitem,
                                       gpointer user_data);
 void on_menuitem_SelectFirst_activate (GtkMenuItem * menuitem,
                                        gpointer user_data);
 void on_menuitem_TruncateSchedule_activate (GtkMenuItem * menuitem,
                                             gpointer user_data);
 void on_menuitem_ReverseVoice_activate (GtkMenuItem * menuitem,
                                         gpointer user_data);
 void on_menuitem_InvertY_activate (GtkMenuItem * menuitem,
                                    gpointer user_data);
 void on_menuitem_SelectDuration_activate (GtkMenuItem * menuitem,
                                           gpointer user_data);
 void on_hscale_X_value_changed (GtkRange * range, gpointer user_data);
 gboolean on_hscale_X_button_press_event (GtkWidget * widget,
                                          GdkEventButton * event,
                                          gpointer user_data);
 gboolean on_hscale_X_button_release_event (GtkWidget * widget,
                                            GdkEventButton * event,
                                            gpointer user_data);
 void on_checkbutton_Yscale_toggled (GtkToggleButton * togglebutton,
                                     gpointer user_data);
 void on_checkbutton_MagneticPointer_toggled (GtkToggleButton * togglebutton,
                                              gpointer user_data);
 void on_checkbutton_Xscale_toggled (GtkToggleButton * togglebutton,
                                     gpointer user_data);
 void on_menuitem_RoundValues_activate (GtkMenuItem * menuitem,
                                        gpointer user_data);
 void on_menuitem_GnauralNet_StartStop_activate (GtkMenuItem * menuitem,
                                                 gpointer user_data);
 void on_menuitem_GnauralNet_JoinFriend_activate (GtkMenuItem * menuitem,
                                                  gpointer user_data);
 void on_menuitem_GnauralNet_PhaseInfo_activate (GtkMenuItem * menuitem,
                                                 gpointer user_data);
 void on_menuitem_GnauralNet_ShowInfo_activate (GtkMenuItem * menuitem,
                                                gpointer user_data);
 //End Menus
 ///////////////////////////////
#ifdef __cplusplus
}
#endif
