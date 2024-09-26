/*
   gnaural.h
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
#ifndef _GNAURAL_MAIN_H_
#define _GNAURAL_MAIN_H_
#include "ScheduleGUI.h"

extern GtkWidget *main_window;
extern GtkWidget *main_drawing_area;
extern GtkToggleButton *main_togglebuttonViewFreq;
extern GtkToggleButton *main_togglebuttonViewBeat;
extern GtkToggleButton *main_togglebuttonViewVol;
extern GtkToggleButton *main_togglebuttonViewBal;
extern GThread *main_thread_WriteEngineWAV;
extern GtkVBox *main_vboxVoices;

extern char main_sTmp[];

//general string that can be written to at any time by anything
//so long as "anything" isn't in a different thread
extern char gnaural_tmpstr[];
extern char main_GnauralIcon[4648];
extern char main_GnauralHelpShort[];
extern char main_GnauralHelp[];
extern gchar main_DefaultSchedule[];
extern char *main_Info_Description;
extern char *main_Info_Author;
extern char *main_Info_Title;

extern int main_gnaural_guiflag;        //will be set internall to TRUE if there is a GUI
extern gchar main_sCurrentGnauralFilenameAndPath[];     // ~/.gnaural/schedule.gnaural

//NOTE: in main.c also has static int mainPA_MyCallback ();

extern gboolean main_XY_scaleflag;
extern void main_on_checkbutton_MagneticPointer_toggled (GtkToggleButton *
                                                         togglebutton);

//START Globals taken from ScheduleGUI:
extern void main_LoadBinauralBeatSoundEngine ();        // Takes SG data and connects it to BB data
extern int main_TestPathOrFileExistence (char *PathName);
extern void main_quit ();
extern void main_EventToKeypress (guint state, guint keyval);
extern void main_AboutDialogBox (void);
extern void main_activate_url (GtkAboutDialog * about,
                               const gchar * url, gpointer data);
extern void main_UpdateGUI_Progressbar (void);
extern gboolean main_UpdateGUI (gpointer data);
extern void main_SetGraphType (GtkToggleButton * togglebutton);
extern void main_SetGraphToggle (GtkWidget * widget);
extern void main_OnButton_VoicesVisible (GtkButton * button);
extern void main_OnButton_Play (GtkButton * button);
extern void main_OnButton_ForwardRewind (float amount);
extern void main_OnButton_Stop (GtkButton * button);
extern void main_UpdateGUI_Status (gchar * msg);
extern void main_UpdateGUI_ProjectedRuntime ();
extern void main_UpdateGUI_Labels ();
extern void main_UpdateGUI_Statusbar (const char *msg1, const char *msg2);
extern void main_UpdateGUI_entryLoops ();
extern void main_UpdateGUI_UserDataInfo ();
extern void main_on_hscaleVolume (float range);
extern void main_on_hscaleBalance (float range);
extern void main_on_export_mp3_activate ();
extern void main_on_export_audio_to_file_activate ();
extern void main_AudioWriteFile_start ();
extern void main_AudioWriteFile (void *ptr);
extern void main_on_edit_activate ();
extern int main_MessageDialogBox (char *msg,
                                  GtkMessageType messagetype,
                                  GtkButtonsType buttonformation);
extern void main_Cleanup ();
extern void main_on_entryLoops_activate (GtkEntry * entry,
                                         gpointer user_data);
extern void main_SetLoops (unsigned int loops);
extern void main_SetIcon ();
extern void main_SetupPathsAndFiles (int bDefaultFlag); //TRUE sets up $HOME/gnaural.schedule, FALSE works with whatever is in main_sCurrentGnauralFilenameAndPath
extern void main_InterceptCtrl_C (int sig);
extern void main_ParseCmdLine (int argc, char *argv[]);
extern void main_RunCmdLineMode ();
extern void main_UpdateTerminalGUI (FILE * gstream);
extern void main_key_arrowkeyhandler (GtkWidget * widget,
                                      int vertical, int horizontal);
extern gboolean main_key_press_event (GtkWidget * widget,
                                      GdkEventKey * event);
extern gboolean main_key_release_event (GtkWidget * widget,
                                        GdkEventKey * event);
extern gboolean main_realize (GtkWidget * widget, GdkEventConfigure * event);
extern gboolean main_expose_event (GtkWidget * widget,
                                   GdkEventExpose * event);
extern gboolean main_delete_event (GtkWidget * window,
                                   GdkEvent * e, gpointer data);
extern gboolean main_button_release_event (GtkWidget * widget,
                                           GdkEventButton * event);
extern gboolean main_button_press_event (GtkWidget * widget,
                                         GdkEventButton * event);
extern gboolean main_configure_event (GtkWidget * widget,
                                      GdkEventConfigure * event);
extern gboolean main_motion_notify_event (GtkWidget * widget,
                                          GdkEventMotion * event);
extern int main_GNAURAL1FILE_SchedFilenameToSchedule (char *filename);  //handles original (obsolete) Gnaural file format
extern void main_GNAURAL1FILE_ParseCmd (FILE * stream); //handles original (obsolete) Gnaural file format
extern void main_GNAURAL1FILE_SchedBuffToSchedule (char *str);  //handles original (obsolete) Gnaural file format
extern gchar *main_OpenFileDialog (gchar * strUserfilter, char *path);  //returns NULL or valid string that must be freed with g_free()
//extern gchar *main_SaveFileDialog (const char *setname);//returns NULL or valid string that must be freed with g_free()
extern void main_WriteDefaultFile ();
extern void main_OnUserSaveAsFile ();   //from Gnaural1 20070304. Saves file; checks for File existence before save
extern int main_AskForSaveAsFilename (char *userfilename);      //from Gnaural1 20070304, replaced main_SaveFileDialog()
extern gboolean main_VoicePropertiesDialog (GtkWidget * widget,
                                            SG_Voice * userVoice);
extern int main_InitGlade ();   //returns 0 on success
extern void main_on_revert_activate (GtkMenuItem * menuitem,
                                     gpointer user_data);
extern void main_NewGraph (int voices);
extern void main_UpdateGUI_FileInfo (char *filename);
extern void main_ClearDataPointsInVoices ();
extern void main_UpdateGUI_PlaceInGraph ();
extern double main_AskUserForNumberDialog (char *title,
                                           char *question,
                                           double *startingval);
extern gboolean main_SetScheduleInfo (gboolean FillEntriesFlag);
extern void main_DuplicateSelectedVoice ();
extern void main_ScaleDataPoints_Time (GtkWidget * widget);
extern void main_DeleteSelectedVoice (GtkWidget * widget);
extern void main_ScaleDatPoints_Y (GtkWidget * widget);
extern void main_AddRandomToDataPoints_Y (GtkWidget * widget);
extern void main_AddRandomToDataPoints_time (GtkWidget * widget);
extern void main_AddToDataPoints_Y (GtkWidget * widget);
extern void main_AddToDataPoints_time (GtkWidget * widget);
extern void main_SelectInterval ();
extern void main_SelectNeighbor ();
extern void main_OpenFile (gboolean OpenMerge, char *path);
extern void main_ReverseVoice ();
extern void main_SelectLastDPs ();
extern void main_SelectFirstDPs ();
extern void main_TruncateSchedule ();
extern void main_PasteAtEnd ();
extern void main_InvertY ();
extern void main_SelectDuration_All ();
extern void main_SelectDuration_SinglePoint ();
extern void main_Preferences ();
extern void main_SelectDuration ();
extern void main_SelectProximity_All ();
extern void main_SelectProximity_SinglePoint ();
extern void main_ResetScheduleInfo ();
extern void main_Sleep (int microseconds);      //BB wants to call this as a function pointer
extern void main_DuplicateAllVoices ();
extern int main_LoadSoundFile (char *filename,
                               int **buffer, unsigned int *size);
extern void main_ProcessAudioFile (char *filename,
                                   int **buffer, unsigned int *size);
extern void main_CleanupAudioFileData ();
extern void main_ProcessVolBal ();
extern void main_vscale_Y_button_event (GtkWidget * widget, gboolean pressed);
extern void main_vscale_Y_value_change (GtkRange * range);
extern void main_hscale_X_button_event (GtkWidget * widget, gboolean pressed);
extern void main_hscale_X_value_change (GtkRange * range);
extern void main_slider_XY_handler (float vertical, float horizontal);
extern void main_FormatProgressString ();
extern void main_DialogAddFileFilters (GtkWidget * dialog,
                                       gchar * strFilterString);
extern void main_MakeAudioFormatsFileFilterString (gchar * strUserfilter,
                                                   int size);
extern void main_OnDragDataReceived (GtkWidget * wgt,
                                     GdkDragContext * context, int x, int y,
                                     GtkSelectionData * seldata, guint info,
                                     guint time, gpointer userdata);
extern void main_RoundValues ();
extern void main_GnauralNet_StopStart (GtkMenuItem * menuitem);
extern void main_GnauralNet_JoinFriend (GtkMenuItem * menuitem);
extern void main_GnauralNet_PhaseInfo (GtkMenuItem * menuitem);
extern void main_GnauralNet_ShowInfo (GtkMenuItem * menuitem);
extern void main_AskUserForIP_Port (char *title, char *question, char *IP,
                                    unsigned short Port, int type);
extern int main_CheckSndfileVersion (void);
extern int main_CheckSndfileFormat (int format);
extern void main_progressbar_button_press_event (GtkWidget * widget,
                                                 GdkEventButton * event);
extern int main_playAudio_SoundInit ();
extern void main_SelectFont ();
extern void main_DataPointSize ();
extern void main_OpenFromLibrary ();
extern void main_VoiceInfoFormatter_type (SG_Voice * curVoice, char *tmstr);
extern void main_VoiceInfoFormatter (SG_Voice * curVoice);
extern double Stopwatch ();     //20101109
extern void main_PhaseInfoToggle (unsigned int ip, unsigned short port);
extern void main_DPPropertiesDialog ();
#endif
