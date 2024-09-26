/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

////////////////////////////////////////////////////////////
// main.c, by Bret Logan (c) 2010
// Comprises Gnaural by uniting all the peripheral files. This is 
// where any code should go that ties together the different modules 
// (ScheduleGUI, ScheduleXML, BinauralBeat, Gnauralnet, etc.) and the 
// GUI; they should not require any reference to each other directly.
////////////////////////////////////////////////////////////

//TODO:
// - perhaps conglomerate glade and audio file searches to on function in order to prioritize relative paths
// - add "Truncated from start" sort of command in addition to the current truncate-from-end
// - implement MP3 functionality for files opened at command line; can't currently find file so opens default
// - BUG: when writing WAV to stdout (buggy CMDLINE generally), I pump out so much DBG info that the stream is polluted!
// - Make properties box check for all properties whether all DPs have them or not
// - add balance control to Properties box (no backdoor way to do it now via properties volumes; in fact, they ruin any preexisting balance relationships)
// - Make GUI "Schedule Info" text NOT center justified, and allow description to be in a scrollable box so it can be long
//BUGS:
// - [FIXED?] Add New Voice sometime checks the Mute box (but isn't even muted) [20100614: may have fixed this, see BB_LoadDefaultVoice()]

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>     //needed for memcpy
#include <unistd.h>     //needed for path/file setup: getcwd, getopt
#include <errno.h>      //needed for path/file setup
#include <signal.h>     //needed for SIGINT
#include <sys/stat.h>   //needed for path/file setup: main_testfileforexistance
#include <stdlib.h>
#include <math.h>       //for fabs()
#include <ctype.h>      //for toupper
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>     //needed for keyboard translations
#include <sndfile.h>
#include <portaudio.h>
#include <locale.h>
#include "gettext.h"

#include "callbacks.h"
#include "ScheduleGUI.h"
#include "ScheduleXML.h"
#include "gnauralXML.h"
#include "BinauralBeat.h"
#include "main.h"
#include "main_ArrayConstants.h"
#include "playAudio.h"
#include "exportAudio.h"
#include "gnauralnet.h"
#include "voiceGUI.h"
#include "gnauralRecentMenu.h"
#include "gnauralVU.h"

#ifdef GNAURAL_WIN32
#include <windows.h>    //for win32
#include <mmsystem.h>   //for sound on win32
#include <io.h> //for binary-mode stdout on Win32
#include <fcntl.h>      //for binary-mode stdout on Win32
#include <process.h>    //for _spawn on Win32
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#endif

//critical assignment for installation stage:
#define GLADE_FILE_NAME "gnaural.glade"
#define GLADE_FILE PACKAGE_DATA_DIR"/"PACKAGE"/"GLADE_FILE_NAME

//20100528: on where linux distros will install presets:
#define GNAURAL_PRESET_DIR PACKAGE_DATA_DIR"/"PACKAGE"/presets"
#define GNAURAL_FILEFILTERSTRING "~Gnaural Files~*.gnaural,~Text Files~*.txt,~All Files~,*"
#define GNAURAL_GUI_UPDATEINTERVAL 128  //64

//drag'n'drop stuff
//very basic, treats anything dropped like a string
enum
{
 DND_TARGET_STRING,
};

static GtkTargetEntry targetentries[] = {
 {"text/plain", 0, DND_TARGET_STRING},  //allows text to be dropped
 {"text/uri-list", 0, DND_TARGET_STRING},       //allows a file to be dropped
};

//end drag'n'drop stuff

//START Need two globals for AudioFileData.
//All Audio files get stored here, and are indexed by their filename
typedef struct main_AFData_type
{
 int *PCM_data;
 int PCM_data_size;             //holds the number of elements in PCM_data;
 char *filename;
} main_AFData;
static main_AFData *main_AudioFileData = NULL;
static int main_AudioFileData_count = 0;        //always holds number of main_AudioFileData elements

//START Globals taken from ScheduleGUI:
GdkPixmap *main_pixmap_Graph;   //main drawing area; must be created by external code (i.e., a main.cpp), then set in SG_Init()
char *main_Info_Description = NULL;
char *main_Info_Author = NULL;
char *main_Info_Title = NULL;
GtkWidget *main_window = NULL;
GtkWidget *main_vpanedListGraph = NULL;
GtkWidget *main_frameVoices = NULL;
GtkVBox *main_vboxVoices = NULL;
GtkWidget *main_drawing_area = NULL;
GtkWidget *main_ProgressBar = NULL;
GtkButton *main_buttonPlayPause = NULL;
guint GUIUpdateHandle = 0;      //handle to the timer that updates the GUI every GNAURAL_GUI_UPDATEINTERVAL ms.
char *main_AudioWriteFile_name = NULL;  //this gets alloted a default name in main();, NEVER equals NULL
GThread *main_AudioWriteFile_thread = NULL;     //20071201 this will equal NULL if a file is NOT being written to
int main_AudioWriteFile_format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;      //this is merely a default
int gnaural_pauseflag = (~0);   //pauseflag controls all movement through schedule, also stops and starts sound
GtkLabel *main_labelFileName = NULL;
GtkLabel *main_labelFileDescription = NULL;
GtkLabel *main_labelFileTitle = NULL;
GtkLabel *main_labelFileAuthor = NULL;
GtkLabel *main_labelStatus = NULL;
GtkLabel *main_labelTotalRuntime = NULL;
GtkLabel *main_labelCurrentRuntime = NULL;
GtkEntry *main_entryLoops = NULL;
GtkToggleButton *main_togglebuttonViewFreq = NULL;
GtkToggleButton *main_togglebuttonViewBeat = NULL;
GtkToggleButton *main_togglebuttonViewVol = NULL;
GtkToggleButton *main_togglebuttonViewBal = NULL;
GtkStatusbar *main_Statusbar = NULL;
GtkHScale *main_hscaleVolume = NULL;
GtkHScale *main_hscaleBalance = NULL;
GtkToggleButton *main_checkbuttonSwapStereo = NULL;
GtkToggleButton *main_checkbuttonOutputMono = NULL;     //20100405
gboolean main_KeyDown = FALSE;  //a way to keep track of start of arrow event in order to backup only once per mass event
char main_sTmp[PATH_MAX + PATH_MAX + 1];        //just a place to store tmp strings
const char main_sSec[] = "sec ";
const char main_sMin[] = "min ";
const char main_sInf[] = "inf";

const char main_sBinauralBeat[] = "Binaural Beat";
const char main_sPinkNoise[] = "Pink Noise";
const char main_sAudioFile[] = "Audio File";
const char main_sIsochronicPulses[] = "Isochronic Pulses";
const char main_sIsochronicPulses_alt[] = "Alt Isochronic Pulses";
const char main_sWaterDrops[] = "Water Drops";
const char main_sRain[] = "Rain";

//the only purpose of this is to help load old-style Gnaural files:
struct
{
 float FreqBase;
 float Volume_Tone;
 float Volume_Noiz;
 int loops;
 int loopcount;
 int ScheduleEntriesCount;
}
OldGnauralVars;

//path and filename variables:
char main_sPathCurrentWorking[PATH_MAX + 1];    //whatever directory user called gnaural from
char main_sPathHome[PATH_MAX + 1];      // ~/                            -- not meaningfull in Win32
char main_sPathGnaural[PATH_MAX + 1];   // ~/.gnaural/                   -- not meaningfull in Win32
char main_sPathTemp[PATH_MAX + PATH_MAX + 1];   //used to copy the other paths and strcat filenames-to
gchar *main_sPathExecutable = NULL;     //so program can call itself to make MP3
gchar main_sCurrentGnauralFilenameAndPath[PATH_MAX + 1];        // ~/.gnaural/schedule.gnaural

//my getopt() vars/flags:
#define GNAURAL_CMDLINEOPTS     ":w:a:sodpih"
int cmdline_w = 0;              //Output .WAV file directly to file
int cmdline_a = 0;              //tell Gnaural which sound HW to access
int cmdline_s = 0;              //Create fresh Schedule file flag
int cmdline_o = 0;              //Output .WAV directly to stdout
int cmdline_d = 0;              //Show debugging information
int cmdline_p = 0;              //Output to sound system
int cmdline_i = 0;              //Show detailed console information
//  int cmdline_h = 0; //show help [not actually needed, since it is handled during parsing]

int main_gnaural_guiflag = FALSE;       //will be set internall to TRUE if there is a GUI
float main_OverallVolume = 1.f; //this solely mirrors the OverallVolume slider; don't set
float main_OverallBalance = 0.f;        //this solely mirrors the OverallBalance slider; don't set
gboolean main_vscale_Y_mousebuttondown = FALSE;
float main_vscale_Y = 0.0f;
gboolean main_hscale_X_mousebuttondown = FALSE;
float main_hscale_X = 0.0f;
gboolean main_XY_scaleflag = FALSE;
gboolean main_MagneticPointerflag = FALSE;      //added 20101006

/////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
 BB_SeedRand (3676, 2676862);   //anything but 0 on either or twins seems OK
 BB_UserSleep = main_Sleep;
#ifdef ENABLE_NLS
 setlocale (LC_ALL, "");
 bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
 bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
 textdomain (GETTEXT_PACKAGE);
 //setlocale (LC_ALL, "");
 //bindtextdomain (PACKAGE, LOCALEDIR);
 //textdomain (PACKAGE);
#endif

#ifdef GNAURAL_WIN32
 //this ensures that any potential Windows stdout activity is in binary mode:
 _setmode (_fileno (stdout), O_BINARY);
 //  _setmode (_fileno (stdin), O_BINARY);
#endif

 //setup a few globals:
 main_sPathExecutable = argv[0];        //so program can call itself to make MP3, among other things
 main_sCurrentGnauralFilenameAndPath[0] = '\0'; //this tells main_SetupPathsAndFiles to use default filename
 SG_StringAllocateAndCopy (&main_AudioWriteFile_name, "Gnaural");

 main_ResetScheduleInfo ();

 //do command line parsing:
 main_ParseCmdLine (argc, argv);

 //Take care of stuff joint to command-line and GUI first:
 //trap Ctrl-C:
 if (signal (SIGINT, SIG_IGN) != SIG_IGN)
 {
  signal (SIGINT, main_InterceptCtrl_C);
 }

 //next need to set up paths, since bb (below) will need to know
 //(among other things) where to access/create gnaural_schedule.txt file:
 main_SetupPathsAndFiles ((main_sCurrentGnauralFilenameAndPath[0] ==
                           '\0') ? TRUE : FALSE);

 //see if file exists; if not, write it:
 if (0 != main_TestPathOrFileExistence (main_sCurrentGnauralFilenameAndPath))
 {
  main_WriteDefaultFile ();
 }

 //Init GDK Threads, needed for file writing:
 //added 20051126 to support GThread cross-compatibility. Yes, it is supposed to be called before gtk_init()
 // call  echo `pkg-config --libs gthread-2.0` to see the libs to link to; I was segfaulting endlessly because I was linking
 //with old libs! This is the proper:
 //g++  -g -O2  -o gnaural  main.o BinauralBeat.o support.o interface.o callbacks.o pa_lib.o pa_unix.o pa_unix_oss.o -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lm -lpangoxft-1.0 -lpangox-1.0 -lpangoft2-1.0 -lpango-1.0 -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0 -pthread -lgthread-2.0
 //if (!g_thread_supported ()) g_thread_init (NULL);
 g_thread_init (NULL);
 gdk_threads_init ();
 //next line was moved below on 20071126 as per GTK+ FAQ example
 //gdk_threads_enter ();        //if I don't do this, the Win32 version blocks! The documentation is amazingly vague.

 //init GTK:
 gtk_set_locale ();
 gtk_init (&argc, &argv);

 main_SetIcon ();       //added 20051202; bruteforce method to avoid nasty GTK path/function inconsistencies

 //NOTE: even for command line, this needs to be here:
 //this returns non-0 if it doesn't find the file gnaural.glade:
 if (0 != main_InitGlade ())
 {
  main_Cleanup ();
  return 0;
 }

 //open user file and put it in BB:
 gxml_XMLReadFile (main_sCurrentGnauralFilenameAndPath, main_drawing_area,
                   FALSE);

 //sort the args:
 //The main question: Do I need a GUI? Basically boils down to these two categories:
 //THINGS AFFECTING BOTH GUI AND TERM VERSION:
 //- cmdline_a: Tell Gnaural which sound HW to use
 //- cmdline_d: Show debugging information
 //THINGS AFFECTING SOLEY COMMANDLINE VERSION:
 //- cmdline_h: Print "Help" [already did this by this point] DOES NOT REQUIRE SOUND SYSTEM
 //- cmdline_o: dump a .WAV file to sdtout DOES NOT REQUIRE SOUND SYSTEM
 //- cmdline_w: dump a .WAV file to a file DOES NOT REQUIRE SOUND SYSTEM
 //- cmdline_s: create a fresh gnaural_schedule.txt file DOES NOT REQUIRE SOUND SYSTEM
 //- cmdline_i: show terminal-style GUI info
 //- cmdline_p: run the schedule directly through the soundsystem
 if ((cmdline_i + cmdline_p + cmdline_o + cmdline_w + cmdline_s) > 0)
 {      //do the command line version then exit:
  SG_DBGOUT ("Entering console mode");
  main_gnaural_guiflag = FALSE;
  main_RunCmdLineMode ();
  main_Cleanup ();
  exit (EXIT_SUCCESS);
 }

 //got here, so must be using GUI:
 main_gnaural_guiflag = TRUE;

 //init the sound:
 main_playAudio_SoundInit ();

 //start stopped:
 main_OnButton_Stop (NULL);

 //Now we can update some GUI stuff:
 //set graph view:
 main_SetGraphToggle (main_drawing_area);
 main_UpdateGUI_FileInfo (main_sCurrentGnauralFilenameAndPath);
 main_UpdateGUI_Voices (main_vboxVoices);

 //setup the main GUI timer:
 GUIUpdateHandle =
  g_timeout_add (GNAURAL_GUI_UPDATEINTERVAL, main_UpdateGUI, NULL);

 //this is a hack, but does silence an unsightly gdk_draw error:
 main_configure_event (main_window, NULL);

 //do drag'n'drop stuff:
 gtk_drag_dest_set (main_window, GTK_DEST_DEFAULT_ALL, targetentries,
                    G_N_ELEMENTS (targetentries), GDK_ACTION_COPY);

 g_signal_connect (main_window, "drag_data_received",
                   G_CALLBACK (main_OnDragDataReceived), NULL);
 //end drag'n'drop

 //enters this until gtk_main_quit() is called
 gdk_threads_enter ();  //essential to do in a threaded GTK+ program
 gtk_main ();

 //flasher_Cleanup (main_FD);//disconnected 20071022
 main_Cleanup ();
 return 0;
}

/////////////////////////////////////////////////////
void main_Cleanup ()
{
 //turn off gnauralnet thread:
 GN_stop ();

 //cleanup any write threads if active:
 if (NULL != main_AudioWriteFile_thread)
 {
  BB_DBGOUT ("Aborting audio write file thread");
  BB_WriteStopFlag = TRUE;
  while (NULL != main_AudioWriteFile_thread)
  {
   main_Sleep (G_USEC_PER_SEC);
  }
 }
 //clean up sound resources:
 playAudio_SoundCleanup ();

 //cleanup BB:
 BB_CleanupVoices ();

 //cleanup SG. NOTE: this should have been called once already at main_quit(), but to be sure:
 SG_Cleanup ();

 if (main_AudioWriteFile_name != NULL)
 {
  free (main_AudioWriteFile_name);
  main_AudioWriteFile_name = NULL;
 }

 //cleansup the globals:
 if (main_Info_Title != NULL)
 {
  free (main_Info_Title);
  main_Info_Title = NULL;
 }
 if (main_Info_Description != NULL)
 {
  free (main_Info_Description);
  main_Info_Description = NULL;
 }
 if (main_Info_Author != NULL)
 {
  free (main_Info_Author);
  main_Info_Author = NULL;
 }

 if (NULL != main_AudioFileData)
 {
  main_CleanupAudioFileData ();
 }

 //don't forget to do this!:
 BB_DBGOUT ("Calling gdk_threads_leave");
 gdk_threads_leave ();
}

/////////////////////////////////////////
//a20070812
void main_CleanupAudioFileData ()
{
 //SG_DBGOUT_INT("Cleaning up BB:", BB_VoiceCount);
 SG_DBGOUT_INT ("NULL'ing all PCM data in BB, voices:", BB_VoiceCount);
 BB_NullAllPCMData ();  //have to do this, unfortunately
 SG_DBGOUT_INT ("Cleaning up Audio Data, filecount:",
                main_AudioFileData_count);
 int i;

 for (i = 0; i < main_AudioFileData_count; i++)
 {
  main_AudioFileData[i].PCM_data_size = 0;
  if (NULL != main_AudioFileData[i].PCM_data)
  {
   SG_DBGOUT_INT ("Cleaning up Audio Data, array", i);
   free (main_AudioFileData[i].PCM_data);
   main_AudioFileData[i].PCM_data = NULL;
  }
  if (NULL != main_AudioFileData[i].filename)
  {
   free (main_AudioFileData[i].filename);
   main_AudioFileData[i].filename = NULL;
  }
 }
 main_AudioFileData_count = 0;
 SG_DBGOUT ("Cleaning up last bit of Audio Data");
 if (NULL != main_AudioFileData)
 {
  free (main_AudioFileData);
  main_AudioFileData = NULL;
 }
}

/////////////////////////////////////////
//a20070702: to cleanup initialization
//returns 0 on success
int main_InitGlade ()
{
 GError *error = NULL;
 GtkBuilder *xml = gtk_builder_new ();
 gchar *errstr = "Couldn't load GtkBuilder file: ";

 //First task: find the glade file. Search Priority:
 // 1) check cwd (since user may want to use a custom one)
 // 2) try "dist installation location", as per PACKAGE_DATA_DIR
 // 3) try the executable's dir
 SG_DBGOUT ("Start search for Glade file");
 SG_DBGOUT_STR ("Looking in cwd for:", GLADE_FILE_NAME);
 SG_DBGOUT_STR ("getcwd():", getcwd (main_sTmp, sizeof main_sTmp));
 SG_DBGOUT_STR ("realpath(getcwd):", realpath (main_sTmp, main_sPathTemp));

 if (!g_file_test (GLADE_FILE_NAME, G_FILE_TEST_EXISTS) ||
     0 == (gtk_builder_add_from_file (xml, GLADE_FILE_NAME, &error)))
 {
  //failed so trying again:
  if (NULL != error)
  {
   g_warning ("%s%s", errstr, error->message);
   g_error_free (error);
   error = NULL;
  }
  SG_DBGOUT_STR
   ("No valid GtkBuilder file there, trying configured install path:",
    GLADE_FILE);
  if (!g_file_test (GLADE_FILE, G_FILE_TEST_EXISTS)
      || 0 == (gtk_builder_add_from_file (xml, GLADE_FILE, &error)))
  {
   //failed so trying again:
   if (NULL != error)
   {
    g_warning ("%s%s", errstr, error->message);
    g_error_free (error);
    error = NULL;
   }
   //failed so trying again:
   if (NULL == realpath (main_sPathExecutable, main_sTmp))
   {
    SG_DBGOUT_STR ("realpath returned NULL for ", main_sPathExecutable);
    g_strlcpy (main_sTmp, main_sPathExecutable,
               strlen (main_sPathExecutable));
   }
   //SG_DBGOUT_STR ("argv[0] is ", main_sPathExecutable);   
   //SG_DBGOUT_STR ("argv[0] realpath is ", main_sTmp);   
   gchar *tmpstr1 = g_path_get_dirname (main_sTmp);
   gchar *tmpstr2 =
    g_strconcat (tmpstr1, G_DIR_SEPARATOR_S, GLADE_FILE_NAME, NULL);
   g_free (tmpstr1);
   SG_DBGOUT_STR ("Not there, trying app's dir:", tmpstr2);
   if (!g_file_test (tmpstr2, G_FILE_TEST_EXISTS) ||
       0 == (gtk_builder_add_from_file (xml, tmpstr2, &error)))
   {
    //failed so trying again:
    if (NULL != error)
    {
     g_warning ("%s%s", errstr, error->message);
     g_error_free (error);
     error = NULL;
    }
    SG_ERROUT ("Couldn't find glade file locally, ");
    return -1;
   }
   g_free (tmpstr2);
  }
 }
 SG_DBGOUT ("Found glade file");

 // connect signal handlers:
 gtk_builder_connect_signals (xml, NULL);

 //connect the GTK main objects:
 //Get the main window:
 main_window = GTK_WIDGET (gtk_builder_get_object (xml, "window_main"));
 //gtk_widget_set_name (main_window, "Gnaural Binaural Beat Generator");
 if (main_window == NULL)
 {
  SG_DBGOUT ("Didn't fine main_window in glade file");
 }

 //Get the progress bar:
 main_ProgressBar =
  GTK_WIDGET (gtk_builder_get_object (xml, "progressbar_main"));
 if (main_ProgressBar == NULL)
 {
  SG_DBGOUT ("Didn't fine progressbar_main in glade file");
 }

 // create the drawing area (now create 20100626):
 main_drawing_area = gtk_drawing_area_new ();   //20100626
 //determine which events it should receive:
 gtk_widget_add_events (GTK_WIDGET (main_drawing_area),
                        GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK |
                        GDK_POINTER_MOTION_HINT_MASK | GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK | GDK_KEY_PRESS_MASK |
                        GDK_KEY_RELEASE_MASK | GDK_ENTER_NOTIFY_MASK |
                        GDK_LEAVE_NOTIFY_MASK);

 GTK_WIDGET_SET_FLAGS (GTK_WIDGET (main_drawing_area), GTK_CAN_FOCUS);

 //now setup all the signals:
 g_signal_connect (main_drawing_area, "realize",
                   G_CALLBACK (on_drawingarea_graph_realize), NULL);
 g_signal_connect (main_drawing_area, "configure_event",
                   G_CALLBACK (on_drawingarea_graph_configure_event), NULL);
 g_signal_connect (main_drawing_area, "delete_event",
                   G_CALLBACK (on_drawingarea_graph_delete_event), NULL);
 g_signal_connect (main_drawing_area, "expose_event",
                   G_CALLBACK (on_drawingarea_graph_expose_event), NULL);
 g_signal_connect (main_drawing_area, "button_press_event",
                   G_CALLBACK (on_drawingarea_graph_button_press_event),
                   NULL);
 g_signal_connect (main_drawing_area, "button_release_event",
                   G_CALLBACK (on_drawingarea_graph_button_release_event),
                   NULL);
 g_signal_connect (main_drawing_area, "key_press_event",
                   G_CALLBACK (on_drawingarea_graph_key_press_event), NULL);
 g_signal_connect (main_drawing_area, "key_release_event",
                   G_CALLBACK (on_drawingarea_graph_key_release_event), NULL);
 g_signal_connect (main_drawing_area, "motion_notify_event",
                   G_CALLBACK (on_drawingarea_graph_motion_notify_event),
                   NULL);
 g_signal_connect (main_drawing_area, "enter_notify_event",
                   G_CALLBACK (on_drawingarea_graph_enter_notify_event),
                   NULL);
 //Set size to something rational then show it:
 gtk_widget_set_size_request (GTK_WIDGET (main_drawing_area), 800, 128);
 gtk_widget_show (main_drawing_area);

 //create the Voices frame:
 main_frameVoices = gtk_frame_new ("Voices");

 //create the Voices vbox:
 main_vboxVoices = (GtkVBox *) gtk_vbox_new (FALSE, 0);

 //add the vbox to the frame:
 gtk_container_add (GTK_CONTAINER (main_frameVoices),
                    GTK_WIDGET (main_vboxVoices));

 main_vpanedListGraph =
  GTK_WIDGET (gtk_builder_get_object (xml, "vpanedListGraph"));
 gtk_paned_add1 (GTK_PANED (main_vpanedListGraph), main_frameVoices);
 gtk_paned_add2 (GTK_PANED (main_vpanedListGraph), main_drawing_area);
 gtk_widget_show_all (main_vpanedListGraph);

 //get the statusbar:
 main_Statusbar =
  (GtkStatusbar *)
  GTK_WIDGET (gtk_builder_get_object (xml, "statusbar_main"));

 //get the Play/Pause button:
 main_buttonPlayPause =
  (GtkButton *) GTK_WIDGET (gtk_builder_get_object (xml, "buttonPlay"));

 //get menuitems that will need updates:
 main_togglebuttonViewFreq =
  (GtkToggleButton *) GTK_WIDGET (gtk_builder_get_object (xml,
                                                          "radiobuttonGraphView_BaseFreq"));
 main_togglebuttonViewBeat =
  (GtkToggleButton *) GTK_WIDGET (gtk_builder_get_object (xml,
                                                          "radiobuttonGraphView_BeatFreq"));
 main_togglebuttonViewVol =
  (GtkToggleButton *) GTK_WIDGET (gtk_builder_get_object (xml,
                                                          "radiobuttonGraphView_Volume"));
 main_togglebuttonViewBal =
  (GtkToggleButton *) GTK_WIDGET (gtk_builder_get_object (xml,
                                                          "radiobuttonGraphView_Balance"));

 //20100528: see if there's a /usr/share/gnaural/preset style library
 //and set menu item accordingly:
 GtkWidget *w =
  GTK_WIDGET (gtk_builder_get_object (xml, "menuitem_OpenFromLibrary"));
 if (0 != main_TestPathOrFileExistence (GNAURAL_PRESET_DIR))
 {
  gtk_widget_set_sensitive (w, FALSE);
 }

 //get all labels:
 main_labelFileName =
  (GtkLabel *) GTK_WIDGET (gtk_builder_get_object (xml, "labelFileName"));
 main_labelFileAuthor =
  (GtkLabel *) GTK_WIDGET (gtk_builder_get_object (xml, "labelFileAuthor"));
 main_labelFileDescription =
  (GtkLabel *)
  GTK_WIDGET (gtk_builder_get_object (xml, "labelFileDescription"));
 main_labelFileTitle =
  (GtkLabel *) GTK_WIDGET (gtk_builder_get_object (xml, "labelFileTitle"));
 main_labelStatus =
  (GtkLabel *) GTK_WIDGET (gtk_builder_get_object (xml, "labelStatus"));
 main_labelTotalRuntime =
  (GtkLabel *) GTK_WIDGET (gtk_builder_get_object (xml, "labelTotalRuntime"));
 main_labelCurrentRuntime =
  (GtkLabel *)
  GTK_WIDGET (gtk_builder_get_object (xml, "labelCurrentRuntime"));

 //get Entry:
 main_entryLoops =
  (GtkEntry *) GTK_WIDGET (gtk_builder_get_object (xml, "entryLoops"));

 //get the Volume hscale:
 main_hscaleVolume =
  (GtkHScale *) GTK_WIDGET (gtk_builder_get_object (xml, "hscaleVolume"));

 //get the Volume hscale:
 main_hscaleBalance =
  (GtkHScale *) GTK_WIDGET (gtk_builder_get_object (xml, "hscaleBalance"));

 //get the checkbuttonSwapStereo:
 main_checkbuttonSwapStereo =
  (GtkToggleButton *)
  GTK_WIDGET (gtk_builder_get_object (xml, "checkbuttonSwapStereo"));

 //get the checkbuttonSwapStereo:
 main_checkbuttonOutputMono =
  (GtkToggleButton *)
  GTK_WIDGET (gtk_builder_get_object (xml, "checkbuttonOutputMono"));

 //20101014: do the recent file stuff:
 gnauralRecentMenu_Init (xml);

 //20101014: setup volume meters
 gnauralVU_init (xml);

 return 0;
}

/////////////////////////////////////////////////////
//20101012: DND is poorly documented and so poorly implemented 
//that i have to leave this basically a complete hack, 
//all it really does is allow user to drag a file fron nautilus
//Main problem in a nutshell is that DND isn't differentiating between
//URIs and Strings for some reason. The best info seems to be here:
//http://live.gnome.org/GnomeLove/DragNDropTutorial
/////////////////////////////////////////////////////
void main_OnDragDataReceived (GtkWidget * widget,
                              GdkDragContext * context,
                              int x,
                              int y,
                              GtkSelectionData * selection_data,
                              guint target_type, guint time,
                              gpointer userdata)
{
 gboolean dnd_success = FALSE;
 gboolean delete_selection_data = FALSE;

 if ((selection_data != NULL) && (selection_data->length >= 0))
 {
  if (context->action == GDK_ACTION_MOVE)
   delete_selection_data = TRUE;
  if (DND_TARGET_STRING == target_type)
  {
   dnd_success = TRUE;

   //for some reason, g_filename_from_uri returns lots of garbage,
   //so just hunt for any valid filename out of whatever came in the
   //ugliest manner possible:
   gchar *filename =
    g_filename_from_uri ((char *) selection_data->data, NULL, NULL);

   if (NULL == filename)
   {
    SG_ERROUT ("DND: invalid filename");
    return;     //hack
   }

   SG_DBGOUT_INT ("DND format:", selection_data->format);
   SG_DBGOUT_INT ("DND length:", selection_data->length);
   SG_DBGOUT_STR ("DND data:", selection_data->data);

   //int i = selection_data->length;//OOPS! i'm an idiot!
   int i = strlen (filename);
   while (i > 0)
   {
    if (0 == main_TestPathOrFileExistence (filename))
     //if (TRUE == g_file_test (filename, G_FILE_TEST_EXISTS))
    {
     break;
    }
    --i;
    filename[i] = '\0';
    SG_DBGOUT_STR ("DND: invalid file:", filename);
    SG_DBGOUT_INT ("i:", i);
   }

   if (i > 0)
   {
    SG_DBGOUT_STR ("DND: valid file:", filename);
    gxml_XMLReadFile (filename, main_drawing_area, FALSE);
   }
   else
   {
    SG_ERROUT ("DND: invalid filename");
   }
   SG_DBGOUT_STR ("freeing filename", filename);
   g_free (filename);   //this causes segfaults sometimes for no apparent reason
  }
  else
  {
   SG_DBGOUT ("DND failed");
  }
 }
 //this basically avoids ghost icon dragging across screen a min. later
 gtk_drag_finish (context, dnd_success, delete_selection_data, time);
}

/////////////////////////////////////////////////////
void main_ResetScheduleInfo ()
{
 SG_StringAllocateAndCopy (&main_Info_Author, NULL);
 SG_StringAllocateAndCopy (&main_Info_Title, NULL);
 SG_StringAllocateAndCopy (&main_Info_Description, NULL);
}

/////////////////////////////////////////////////////
gboolean main_button_release_event (GtkWidget * widget,
                                    GdkEventButton * event)
{
 SG_button_release_event (widget, event);
 // if (SG_GraphHasChanged == TRUE)
 if (SG_DataHasChanged == TRUE)
 {
  main_LoadBinauralBeatSoundEngine ();
 }
 return TRUE;
}

/////////////////////////////////////////////////////
gboolean main_motion_notify_event (GtkWidget * widget, GdkEventMotion * event)
{
 SG_motion_notify_event (widget, event);
 //SG_GraphHasChanged = TRUE;
 return FALSE;
}

/////////////////////////////////////////////////////
gboolean main_button_press_event (GtkWidget * widget, GdkEventButton * event)
{
 SG_button_press_event (widget, event);
 if (SG_DataHasChanged == TRUE)
 {
  main_LoadBinauralBeatSoundEngine ();
 }
 return TRUE;
}

/////////////////////////////////////////////////////
gboolean main_key_release_event (GtkWidget * widget, GdkEventKey * event)
{
 main_KeyDown = FALSE;
 return FALSE;
}

/////////////////////////////////////////////////////
gboolean main_key_press_event (GtkWidget * widget, GdkEventKey * event)
{
 //NOTE: MOST GNAURAL KEYBOARD INPUT IS HANDLED BY GTK ACCELLERATORS,
 //therefore almost everything here gets served from callbacks.c:

 // widget = main_drawing_area;
 //====deal with keys pressed with Ctrl:
 if (event->state & GDK_CONTROL_MASK)
 {
  switch (event->keyval)
  {
  case GDK_a:  //select all:
   SG_SelectDataPoints (-8, -8, widget->allocation.width + 8,
                        widget->allocation.height + 8, TRUE);
   SG_ConvertDataToXY (widget);
   SG_DrawGraph (widget);
   break;

  case GDK_b:
   {    //Scale Time:
    main_DuplicateSelectedVoice ();
   }
   break;

  case GDK_c:  //copy:
   SG_CopySelectedDataPoints (widget);
   main_UpdateGUI_Progressbar ();
   break;

  case GDK_d:  //Delete currently selected voice:
   main_DeleteSelectedVoice (widget);
   break;

  case GDK_e:  //Select All Points in currently selected voice:
   SG_SelectDataPoints_Voice (SG_SelectVoice (NULL), TRUE);
   SG_DrawGraph (widget);
   break;

  case GDK_g:
   main_ScaleDatPoints_Y (widget);
   break;

   //GDK_h is used by Help

  case GDK_i:  //Inverts selections in all visible voices:
   SG_SelectInvertDataPoints_All ();
   SG_DrawGraph (widget);
   break;

  case GDK_j:  //add new voice:
   main_VoicePropertiesDialog (widget, NULL);
   break;

  case GDK_k:  //Select Interval:
   main_SelectInterval ();
   break;

  case GDK_l:  //line up points:
   SG_BackupDataPoints (main_drawing_area);
   SG_AlignDataPoints (widget);
   break;

  case GDK_m:
   main_SelectNeighbor ();
   break;

   //GDK_n is already used

  case GDK_o:
   main_OpenFile (FALSE, NULL);
   break;

  case GDK_p:  //Apply Graph to sound engine:
   main_LoadBinauralBeatSoundEngine ();
   break;

  case GDK_q:  //quit:
   main_quit ();
   break;

  case GDK_r:  //reverse voice:
   main_ReverseVoice ();
   break;

  case GDK_s:  //save to file:
   gxml_XMLWriteFile (main_sCurrentGnauralFilenameAndPath);
   break;

  case GDK_t:
   {    //Edit Voice Properties:
    SG_Voice *tmpVoice = SG_SelectVoice (NULL);

    if (tmpVoice != NULL)
    {
     main_VoicePropertiesDialog (widget, tmpVoice);
    }
   }
   break;

  case GDK_u:  //Select last DPs in visible voices:
   main_SelectLastDPs ();
   break;

  case GDK_v:  //paste:
   SG_BackupDataPoints (main_drawing_area);
   SG_PasteSelectedDataPoints (widget, TRUE);
   break;

  case GDK_w:  //Change Graph View:
   switch (SG_GraphType)
   {
   case SG_GRAPHTYPE_BEATFREQ:
    main_SetGraphType (main_togglebuttonViewVol);
    break;

   case SG_GRAPHTYPE_BASEFREQ:
    main_SetGraphType (main_togglebuttonViewBeat);
    break;

   case SG_GRAPHTYPE_VOLUME:
    main_SetGraphType (main_togglebuttonViewBal);
    break;

   case SG_GRAPHTYPE_VOLUME_BALANCE:
    main_SetGraphType (main_togglebuttonViewFreq);
    break;

   default:
    main_SetGraphType (main_togglebuttonViewFreq);
    break;
   }
   SG_ConvertDataToXY (widget);
   SG_DrawGraph (widget);
   break;

  case GDK_x:  //cut:
   SG_BackupDataPoints (widget);
   SG_CopySelectedDataPoints (widget);
   SG_DeleteDataPoints (widget, FALSE, TRUE);
   break;

  case GDK_y:  //undo-redo:
  case GDK_z:
   //NOTE: undo/redo are dealt with by accellerators because their workings are closely tied with menu states:
   BB_ResetAllVoices ();        //a20070730
   SG_RestoreDataPoints (widget, FALSE);
   break;
  }     //end keyval
  return FALSE;
 }      // end deal with keys pressed with Ctrl

 //====deal with keys pressed with Shift:
 if (event->state & GDK_SHIFT_MASK)
 {
  switch (event->keyval)
  {
  case GDK_A:  //deselect all:
   SG_DeselectDataPoints ();
   SG_DrawGraph (widget);
   break;

  case GDK_E:  //Deselect All Points in currently selected voice:
   SG_SelectDataPoints_Voice (SG_SelectVoice (NULL), FALSE);
   SG_DrawGraph (widget);
   break;

  case GDK_I:  //Inverts selections in all visible voices:
   SG_SelectInvertDataPoints_Voice (SG_SelectVoice (NULL));
   SG_DrawGraph (widget);
   break;

  case GDK_U:  //Select first DPs in visible voices:
   main_SelectFirstDPs ();
   break;

  case GDK_V:  //Pastes whatever's in buffer to very end of schedule
   main_PasteAtEnd ();
   break;

  case GDK_X:  //cut (deleting time also):
   SG_BackupDataPoints (widget);
   SG_CopySelectedDataPoints (widget);
   SG_DeleteDataPoints (widget, TRUE, TRUE);
   break;

  case GDK_Delete:
   //delete points and delete points and time (Shift):
   SG_BackupDataPoints (widget);
   SG_DeleteDataPoints (widget, (event->state & GDK_SHIFT_MASK), TRUE);
   break;

   //a20070620 :
  case GDK_Up:
  case GDK_Down:
   main_key_arrowkeyhandler (widget, (event->keyval == GDK_Up) ? -10 : 10, 0);
   break;

   //a20070620 :
  case GDK_Left:
  case GDK_Right:
   main_key_arrowkeyhandler (widget, 0,
                             (event->keyval == GDK_Left) ? -10 : 10);
   break;
  }     //end keyval
  return FALSE;
 }      // end deal with keys pressed with Shift

 //====Finally, deal with pure keys:
 switch (event->keyval)
 {
 case GDK_Delete:
  SG_BackupDataPoints (widget);
  SG_DeleteDataPoints (widget, (event->state & GDK_SHIFT_MASK), TRUE);
  break;

  //a20070620 :
 case GDK_Up:
 case GDK_Down:
  main_key_arrowkeyhandler (widget, (event->keyval == GDK_Up) ? -1 : 1, 0);
  break;

  //a20070620 :
 case GDK_Left:
 case GDK_Right:
  main_key_arrowkeyhandler (widget, 0, (event->keyval == GDK_Left) ? -1 : 1);
  break;
 }      //end keyval
 return FALSE;
}       //end all keys

/////////////////////////////////////////////////////
//a20070620:
void main_key_arrowkeyhandler (GtkWidget * widget,
                               int vertical, int horizontal)
{
 if (vertical == 0 && horizontal == 0)
 {
  return;
 }
 //20070620 now deal with backup:
 if (main_KeyDown == FALSE)
 {
  SG_BackupDataPoints (widget); //a20070620
 }
 main_KeyDown = TRUE;
 if (vertical != 0)
 {
  SG_MoveSelectedDataPoints (widget, 0, vertical);
  SG_ConvertYToData_SelectedPoints (widget);
 }

 //a20070620 :
 if (horizontal != 0)
 {
  SG_MoveSelectedDataPoints (widget, horizontal, 0);
  SG_ConvertXToDuration_AllPoints (widget);
 }

 SG_ConvertDataToXY (widget);   // NOTE: I need to call this both to set limits and to bring XY's outside of graph back in
 SG_DrawGraph (widget);
 SG_DataHasChanged = SG_GraphHasChanged = TRUE; //20070105 tricky way to do main_LoadBinauralBeatSoundEngine in a bulk way
}

/////////////////////////////////////////////////////
// Initializing or resizing, so create a new backing pixmap of the appropriate size
gboolean main_configure_event (GtkWidget * widget, GdkEventConfigure * event)
{
 if (main_pixmap_Graph != NULL)
 {
  SG_DBGOUT ("Destroying old pixmap");
  g_object_unref (main_pixmap_Graph);
 }

 SG_DBGOUT ("Creating new pixmap");
 main_pixmap_Graph =
  gdk_pixmap_new (widget->window, widget->allocation.width,
                  widget->allocation.height, -1);
 // init the graph:
 SG_DBGOUT ("Initing SG_Graph");
 SG_Init (main_pixmap_Graph);

 //try commenting these out:
 SG_ConvertDataToXY (widget);
 SG_DrawGraph (widget);
 return TRUE;
}

/////////////////////////////////////////////////////
// This is the repaint signal, so redraw the screen from the backing pixmap
gboolean main_expose_event (GtkWidget * widget, GdkEventExpose * event)
{
 /*
    The GtkDrawingArea widget is used for creating custom user interface elements.
    It's essentially a blank widget; you can draw on widget->window. After creating a
    drawing area, the application may want to connect to:

    -      Mouse and button press signals to respond to input from the user.
    (Use gtk_widget_add_events() to enable events you wish to receive.)

    -     The "realize" signal to take any necessary actions when the widget is instantiated
    on a particular display. (Create GDK resources in response to this signal.)

    -     The "SG_configure_event" signal to take any necessary actions when the widget changes size.

    -    The "expose_event" signal to handle redrawing the contents of the widget.

    GDK automatically clears the exposed area to the background color before sending the
    expose event, and that drawing is implicitly clipped to the exposed area.
  */
 gdk_draw_drawable (widget->window,
                    widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
                    main_pixmap_Graph, event->area.x, event->area.y,
                    event->area.x, event->area.y, event->area.width,
                    event->area.height);

 return FALSE;
}

/////////////////////////////////////////////////////
//NOTE: there is also a main_Cleanup(); not sure if they are in all cases cooperative.
void main_quit ()
{
 //  exit (0);
 g_source_remove (GUIUpdateHandle);
 GUIUpdateHandle = 0;

 SG_Cleanup ();
 gtk_main_quit ();
}

/////////////////////////////////////////////////////
//20070702: This apparently never actually gets called:
//"problem is the common one where libglade has already
//shown the widget before the signal handlers are connected, so you never
//get the signal. The solution is to set the window's 'Visible' property
//to FALSE in glade, and call gtk_widget_show() on it after calling
//glade_xml_signal_autoconnect()."
gboolean main_realize (GtkWidget * widget, GdkEventConfigure * event)
{
 return FALSE;
}

/////////////////////////////////////////////////////
void main_UpdateGUI_entryLoops ()
{
 sprintf (main_sTmp, "%d", BB_Loops);
 gtk_entry_set_text (GTK_ENTRY (main_entryLoops), main_sTmp);
}

/////////////////////////////////////////////////////
void main_SetLoops (unsigned int loops)
{
 int diff = BB_Loops - BB_LoopCount;

 if (loops != 0)
 {      //i.e., we're not in inf. loop mode:
  if (diff >= loops)
   BB_LoopCount = 1;    //this keeps it from running forever
  else
   BB_LoopCount = loops - diff;
 }
 else
 {      // inf. loop mode:
  BB_LoopCount = 0;
 }
 BB_Loops = loops;
 main_UpdateGUI_entryLoops ();
 main_UpdateGUI_ProjectedRuntime ();
 //  main_LoadBinauralBeatSoundEngine();
}

/////////////////////////////////////////////////////
void main_on_entryLoops_activate (GtkEntry * entry, gpointer user_data)
{
 //  const gchar *entry_text;
 //  entry_text = gtk_entry_get_text (GTK_ENTRY (entry));
 //  printf ("Entry contents: %s\n", entry_text);
 //  const gchar *entry_text = gtk_entry_get_text (GTK_ENTRY (entry));
 main_SetLoops (((unsigned
                  int) (g_ascii_strtod (gtk_entry_get_text
                                        (GTK_ENTRY (entry)), NULL))));
}

/////////////////////////////////////////////////////
//Takes a pointers to hold the Audio Data and a filename, and checks
//if it is valid data, loads it in to a local list so that BB can
//just grab pointers to already loaded audio data here instead of having 
//to reload entire audio files every reload.
void main_ProcessAudioFile (char *filename, int **buffer, unsigned int *size)
{
 int i;

 for (i = 0; i < main_AudioFileData_count; i++)
 {
  if (0 == strcmp (filename, main_AudioFileData[i].filename))
  {     //file was already opened:
   (*buffer) = main_AudioFileData[i].PCM_data;
   (*size) = main_AudioFileData[i].PCM_data_size;
   SG_DBGOUT_STR ("Audio Data already loaded, not loading", filename);
   return;
  }
 }
 SG_DBGOUT_STR ("Audio Data not already loaded, loading", filename);

 //i could try this, but why add an extra step?:
 //int main_TestPathOrFileExistence (char *PathName)

 //If i got here, i didn't already load the audio file, so first need 
 //to try to open the explicit filename:
 if (0 != main_LoadSoundFile (filename, &(*buffer), &(*size)))
 {      //that failed, so now assume filename was using a relative path:
  SG_DBGOUT_STR ("No valid audio file:", filename);
  SG_DBGOUT ("Trying schedule file's directory...");
  //20100628: adding code to look for unpathed filenames in the current
  //schedule-file's directory:
  gchar *tmpfile = g_path_get_dirname (main_sCurrentGnauralFilenameAndPath);
  gchar tmpstr[PATH_MAX];
  sprintf (tmpstr, "%s/%s", tmpfile, filename);
  g_free (tmpfile);
  if (0 != main_LoadSoundFile (tmpstr, &(*buffer), &(*size)))
  {     //totally failed, giving up (could try ~/.gnaural, but am not):
   SG_ERROUT ("No valid audio file:");
   SG_ERROUT (tmpstr);
   return;
  }
 }
 //successful load of PCM data, now need to add it to a list. 
 //first make a copy of the list, one-larger than original:
 main_AFData *tmp_AudioFileData =
  (main_AFData *) malloc (sizeof (main_AFData) *
                          (main_AudioFileData_count + 1));
 if (NULL != main_AudioFileData)
 {
  memcpy (tmp_AudioFileData, main_AudioFileData,
          sizeof (main_AFData) * main_AudioFileData_count);
 }
 SG_DBGOUT_INT ("Number of audio files open:", main_AudioFileData_count + 1);

 //now add new last element's data:
 tmp_AudioFileData[main_AudioFileData_count].PCM_data = (*buffer);
 tmp_AudioFileData[main_AudioFileData_count].PCM_data_size = (*size);
 tmp_AudioFileData[main_AudioFileData_count].filename = NULL;   //IMPORTANT!
 SG_StringAllocateAndCopy (&
                           (tmp_AudioFileData
                            [main_AudioFileData_count].filename), filename);
 if (NULL != main_AudioFileData)
 {
  free (main_AudioFileData);
 }
 main_AudioFileData = tmp_AudioFileData;
 ++main_AudioFileData_count;
}

/////////////////////////////////////////////////////
//main_LoadBinauralBeatSoundEngine() Loads BB with
//whatever data is currently in SG data.
//Call this whenever ScheduleGUI and BinauralBeat aren't
//data sync'd
void main_LoadBinauralBeatSoundEngine ()
{
 GN_Time_ResetSeniority ();
 SG_DBGOUT ("Copying Graph to BB Engine");
 SG_Voice *curVoice;

 SG_DataPoint *curDP;

 int count_voice;

 int count_dp;

 // first count the number of voices:
 count_voice = 0;
 curVoice = SG_FirstVoice;
 while (curVoice != NULL)
 {
  ++count_voice;
  curVoice = curVoice->NextVoice;
 }

 SG_DBGOUT_INT ("Total number of Voices:", count_voice);

 //1) prepare the sound engine for that many voices:
 //NOTE: as of 20070731, this also cleans old BB_Voice data:
 BB_InitVoices (count_voice);

 SG_DBGOUT_INT ("Preparing sound engine, number of voices:", count_voice);
 //now go through each voice, count the datapoints,
 //put the datapoints in a raw array, then feed it to
 //the sound engine's data stuctures:
 count_voice = 0;
 curVoice = SG_FirstVoice;

 //CRUCIAL to zero BB_TotalDuration, or else old value might persist (see BB Notes section):
 BB_TotalDuration = 0;
 while (curVoice != NULL)
 {
  //first count how many DPs this voice has:
  count_dp = 0;
  curDP = curVoice->FirstDataPoint;
  do
  {
   ++count_dp;
   //all list loops need this:
   curDP = curDP->NextDataPoint;
  }
  while (curDP != NULL);
  /*
     SG_DBGOUT_INT ("Voice:", count_voice);
     SG_DBGOUT_INT ("\tType:", curVoice->type);
     SG_DBGOUT_INT ("\tEvent Count:", count_dp);
   */
  //2) Allot Entry memory for each voice by running BB_SetupVoice() on each:
  BB_SetupVoice (count_voice, curVoice->type, curVoice->mute, curVoice->mono,
                 count_dp);

  //3) Load memory for each entry alotted in step 2:
  count_dp = 0;
  curDP = curVoice->FirstDataPoint;
  do
  {
   BB_Voice[count_voice].Entry[count_dp].beatfreq_start_HALF =
    curDP->beatfreq * 0.5;
   /*
      BB_Voice[count_voice].Entry[count_dp].beatfreqL_start =
      curDP->beatfreq * 0.5;
      BB_Voice[count_voice].Entry[count_dp].beatfreqR_start =
      curDP->beatfreq * -0.5;
    */
   BB_Voice[count_voice].Entry[count_dp].duration = curDP->duration;
   BB_Voice[count_voice].Entry[count_dp].basefreq_start = curDP->basefreq;
   BB_Voice[count_voice].Entry[count_dp].volL_start = curDP->volume_left;
   BB_Voice[count_voice].Entry[count_dp].volR_start = curDP->volume_right;
   ++count_dp;
   //all list loops need this:
   curDP = curDP->NextDataPoint;
  }
  while (curDP != NULL);
  /*
     SG_DBGOUT_INT("Voice Type:", BB_Voice[count_voice].type);
     SG_DBGOUT_FLT("Voice Entry HZ:", BB_Voice[count_voice].Entry[2].beatfreqL_start);
     SG_DBGOUT_FLT("Voice Entry Duration:", BB_Voice[count_voice].Entry[2].duration);
     SG_DBGOUT_FLT("Voice Entry basefreq:", BB_Voice[count_voice].Entry[2].basefreq_start);
     SG_DBGOUT_FLT("Voice Entry volume:", BB_Voice[count_voice].Entry[2].volL_start);
   */

  //4) Run BB_CalibrateVoice() on the voice:
  BB_CalibrateVoice (count_voice);

  //5) see if this was a PCM voice:
  if (BB_VOICETYPE_PCM == curVoice->type)
  {
   /*
      //this works well, but I don't want to load directly in to BB anymore (too inefficient)
      main_LoadSoundFile (curVoice->description,
      (int **) &(BB_Voice[count_voice].PCM_samples),
      &(BB_Voice[count_voice].PCM_samples_samples_size));
    */
   //This loads locally, allowing me to check if voice is already and then simply assign pointers to BB:
   main_ProcessAudioFile (curVoice->description,
                          (int **) &(BB_Voice[count_voice].PCM_samples),
                          &(BB_Voice[count_voice].PCM_samples_size));
  }

  //advance to next voice:
  curVoice = curVoice->NextVoice;
  ++count_voice;
 }

 BB_PauseFlag = FALSE;  //CRITICAL, added 20070803
 //next two lines added 20070129 to deal with weird BB_TotalDuration inconsistencies
 //BB_DetermineTotalDuration();
 int fixes = BB_FixVoiceDurations ();

 SG_DBGOUT_INT ("\tVoices fixed:", fixes);

 main_UpdateGUI_ProjectedRuntime ();
}

///////////////////////////////////////////////////////////////
//Parse user's string for patern/name pairs. Titles are bounded
//by "~" symbols, and subsequent filters trailed by commas, like:
//"~All Files~*,~Audio Files~*.wav,*.aiff,*.au,*.flac,~Image Files~*.jpg,*.gif,*.bmp"
void main_DialogAddFileFilters (GtkWidget * dialog, gchar * strFilterString)
{
 GtkFileFilter *filter;

 char *strFilterString_copy = NULL;

 //NOTE: Need to work with our own copy of strUserfilter:
 SG_StringAllocateAndCopy (&strFilterString_copy, strFilterString);
 //Need strFilterString_copy for free'ing later:
 char *sFilterString = strFilterString_copy;

 char *substr;

 BB_DBGOUT_STR ("Filter string: ", sFilterString);
 while (NULL != sFilterString && '\0' != *sFilterString)
 {
  BB_DBGOUT ("Looking for a new title");
  //get a title:
  while ('~' == *sFilterString && '\0' != *sFilterString)
  {
   BB_DBGOUT ("Found start of a new title");
   substr = sFilterString + 1;
   //look for next "~":
   while ('~' != *(++sFilterString));
   //cap it at temporary "end" for substr:
   *sFilterString = '\0';
   BB_DBGOUT_STR (" Filter title  :", substr);
   filter = gtk_file_filter_new ();
   gtk_file_filter_set_name (filter, substr);
   //now start adding filters for that title:
   while ('\0' != *(++sFilterString) && '~' != *sFilterString)
   {
    substr = sFilterString;
    //run to next comma:
    while (',' != *sFilterString && '\0' != *sFilterString)
     ++sFilterString;
    if ('\0' == *sFilterString)
    {
     BB_DBGOUT_STR ("[last one] Filter pattern:", substr);
     gtk_file_filter_add_pattern (filter, substr);
     break;
    }
    //cap for substring
    *sFilterString = '\0';
    BB_DBGOUT_STR (" Filter pattern:", substr);
    gtk_file_filter_add_pattern (filter, substr);
   }
   BB_DBGOUT ("Done collecting patterns for that title");
   gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  }
 }
 BB_DBGOUT ("Done parsing user string");
 free (strFilterString_copy);
}

//////////////////////////////////////////////////////////
//NOTE: I could be nice and actually have this allocate a string 
//that is ensured big enough to hold all sndfile formats,
//but at this juncture this is good enough.
//This function checks sndfile for all usable formats and 
//concatenates them in to a formatted string listable by GTK 
//dialog comboboxes:
void main_MakeAudioFormatsFileFilterString (gchar * strUserfilter, int size)
{
 //start filter string here:
 g_strlcpy (strUserfilter, "~Gnaural Audio Files~", size);

 //START standard sndfile list code vars=====================
 SF_FORMAT_INFO info;           //carries format, samplerate, channels, etc.
 SF_INFO sfinfo;                //carries format, name, and extension
 char buffer[128];
 int format,
  major_count,
  subtype_count,
  m,
  s;

 sf_command (NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof (int));
 sf_command (NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count,
             sizeof (int));
 //END standard sndfile list code vars=====================

 //START standard sndfile list code=====================
 //clean everything up before filling it
 memset (&sfinfo, 0, sizeof (sfinfo));
 buffer[0] = 0;
 sfinfo.channels = 2;

 for (m = 0; m < major_count; m++)
 {
  info.format = m;
  sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
  format = info.format;
  for (s = 0; s < subtype_count; s++)
  {
   info.format = s;
   sf_command (NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof (info));

   format = (format & SF_FORMAT_TYPEMASK) | info.format;

   sfinfo.format = format;
   //--------put my stuff here:

   //now check all formats:
   //SG_DBGOUT_STR ("Found format:", info.name);
   //we're only interested in 16-bit PCM or OGG:
   if (main_CheckSndfileFormat (sfinfo.format))
   {
    info.format = m;
    sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
    g_strlcat (strUserfilter, "*.", size);
    g_strlcat (strUserfilter, info.extension, size);
    g_strlcat (strUserfilter, ",", size);
    BB_DBGOUT_STR ("Adding extension to filter string:", strUserfilter);
   }

   //--------end my stuff
  };
 };
 //END standard sndfile list code=====================

 //end filter string here:
 g_strlcat (strUserfilter, "~All Files~*", size);
}

/////////////////////////////////////////////////////
//returns NULL if failed; otherwise, be sure to free
//what this returns with g_free();
//strUserfilter should be formatted like: 
//"~Audio Files~*.wav,*.aiff,*.au,*.flac,~Image Files~*.jpg,*.gif,*.bmp"
gchar *main_OpenFileDialog (gchar * strUserfilter, char *path)
{
 GtkWidget *dialog;

 char *filename = NULL;

 dialog =
  gtk_file_chooser_dialog_new ("Open File", NULL,
                               GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
                               GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
                               GTK_RESPONSE_ACCEPT, NULL);

 //this isn't very user friendly; it is just easy for me:
 //gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
 //                                     main_sPathGnaural);

 //20100528:
 gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), path);

 //these are alternatives:
 // gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), main_sCurrentGnauralFilenameAndPath);
 // gchar * tmpname = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER (dialog));
 // gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), tmpname);
 // g_free (tmpname);

 main_DialogAddFileFilters (dialog, strUserfilter);

 if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
 {
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
 }
 gtk_widget_destroy (dialog);
 return filename;
}

//////////////////////
//Opens a Save As File dialog, fills a user alotted char * array (big enough
//to handle anything) with the filename IF something valid was chosen;
//but be sure to check return value before using it: ==0 means success
//(i.e., use the string), !=0 failure.
int main_AskForSaveAsFilename (char *userfilename)
{
 GtkWidget *dialog;

 dialog =
  gtk_file_chooser_dialog_new ("Save Gnaural Schedule as...", NULL,
                               GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL,
                               GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE,
                               GTK_RESPONSE_ACCEPT, NULL);
 //20070301 This required GTK+2.0 > 2.8, so dropping it for *ugly* stat() hacks.
 //NOTE: next call wasn't even available in Debian unstable a year ago; seems there now
 // gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
 gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog),
                                main_sCurrentGnauralFilenameAndPath);

 main_DialogAddFileFilters (dialog, GNAURAL_FILEFILTERSTRING);

 if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
 {
  char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

  if (strlen (filename) > 0)
  {
   strcpy (userfilename, filename);
  }
  strcpy (userfilename, filename);
  g_free (filename);
  gtk_widget_destroy (dialog);
  return 0;     //user picked a filename
 }
 gtk_widget_destroy (dialog);
 return 1;      //failed to select a file
}

//======================================
//calls a dialog to get a filename, if file exists, asks if
//user is sure about overwriting; if answer is no, repeats,
//otherwise just overwrites.
void main_OnUserSaveAsFile ()
{
 int repeat;

 do
 {
  repeat = 0;
  //first ask for filename:
  if (0 != main_AskForSaveAsFilename (main_sPathTemp))
  {
   return;      //do nothing
  }
  if (0 == main_TestPathOrFileExistence (main_sPathTemp))
  {
   if (0 ==
       main_MessageDialogBox
       ("That file exists. Are you sure you want to overwrite it?",
        GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO))
   {
    ++repeat;
   }
  }
 }
 while (0 != repeat);

 if (strlen (main_sPathTemp) > 0)
 {
  gxml_XMLWriteFile (main_sPathTemp);
  main_UpdateGUI_FileInfo (main_sPathTemp);
 }
}

//========================================
void main_on_edit_activate ()
{       //this is a hack -- no time to write editor or figure out how to access default editor, so let a typical platform editors do it:
 //###############FOR LINUX################
#ifndef GNAURAL_WIN32
 //NOTE: I think it is trying to launch this twice, maybe a mouse-click artifact? Might want to implement only one instance code
 //BUG 20060116: if user leaves Gedit open after exiting, it is as if the sound system is still being owned by Gnaual until Gedit is closed
 sprintf (main_sPathTemp, "%s%s%s", "gedit ",
          main_sCurrentGnauralFilenameAndPath, " &");
 //[20100405: added check to avoid compiler complaints]:
 if (0 == system (main_sPathTemp))
 {
 }
#endif

 //###############FOR WIN32################
#ifdef GNAURAL_WIN32
 ShellExecute (0, "open",       // Operation to perform
               //"c:\\windows\\notepad.exe", // Application name
               "notepad.exe",   // Application name
               main_sCurrentGnauralFilenameAndPath,     // Additional parameters
               0,       // Default directory
               SW_SHOW);
#endif
}

/////////////////////////////////////////////////////
void main_EventToKeypress (guint state, guint keyval)
{
 GdkEventKey event;

 //in main_key_press_event, only state and keyval are required:
 event.state = state;
 event.keyval = keyval;
 main_key_press_event (main_drawing_area, &event);
}

/////////////////////////////////////////////////////
//this function only exists to support main_AboutDialogBox below
//#include <gnome.h>
void main_activate_url (GtkAboutDialog * about,
                        const gchar * uri, gpointer data)
{
#ifdef GNAURAL_WIN32
 //##########FOR WIN32##########
 ShellExecute (NULL, "open", uri, NULL, NULL, SW_SHOWNORMAL);
#else
 const gchar *argv[3];
 argv[0] = "xdg-open";
 argv[1] = uri;
 argv[2] = NULL;
 if (!g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH,
                     NULL, NULL, NULL, NULL))
 {
  argv[0] = "firefox";
  if (!g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL, NULL))
  {
   argv[0] = "mozilla";
   if (!g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH, NULL,
                       NULL, NULL, NULL))
   {
    argv[0] = "safari";
    if (!g_spawn_async
        (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL,
         NULL))
    {
     argv[0] = "opera";
     if (!g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH,
                         NULL, NULL, NULL, NULL))
     {
      argv[0] = "konqueror";
      if (!g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH,
                          NULL, NULL, NULL, NULL))
      {
       argv[0] = "netscape";
       g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, NULL, NULL);
      }
     }
    }
   }
  }
 }
#endif

}

/////////////////////////////////////////////////////
void main_AboutDialogBox ()
{
 static GtkWidget *about = NULL;

 const gchar *authors[] = {
  "Bret Logan <gnaural@users.sourceforge.net>",
  NULL
 };
 const gchar *documenters[] = {
  "Bret Logan <gnaural@users.sourceforge.net>",
  NULL
 };
 const gchar *copyright = "Copyright \xc2\xa9 2003-2011 Bret Logan\n";

 const gchar *comments =
  "A programmable audio generator intended as an aural aid to meditation and relaxation, implementing the binaural beat principle as described in Gerald Oster's Oct. 1973 Scientific American article \"Auditory Beats in the Brain.\"";
 const gchar *license =
  "This program is free software; you can redistribute it and/or modify\n\
    it under the terms of the GNU General Public License as published by\n\
    the Free Software Foundation; either version 2 of the License, or\n\
    (at your option) any later version.\n\n\
    This program is distributed in the hope that it will be useful,\n\
    but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  For more\n\
    details on the GNU General Public License, contact:\n\n\
    \tFree Software Foundation, Inc.\n\
    \t59 Temple Place - Suite 330\n\
    \tBoston, MA 02111-1307, USA.";

 const gchar *website = "http://gnaural.sourceforge.net/";

 //~ Translators: This is a special message that shouldn't be translated
 //~ literally. It is used in the about box to give credits to
 //~ the translators.
 //~ Thus, you should translate it to your name and email address.
 //~ You can also include other translators who have contributed to
 //~ this translation; in that case, please write them on separate
 //~ lines seperated by newlines (\n).
 const gchar *translator_credits = "translator-credits";

 if (about != NULL)
 {
  gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (main_window));
  gtk_window_present (GTK_WINDOW (about));
  return;
 }
 gtk_about_dialog_set_url_hook (main_activate_url, NULL, NULL);
 about = GTK_WIDGET (g_object_new (GTK_TYPE_ABOUT_DIALOG,       //
                                   "program-name", "Gnaural",   //
                                   "version", VERSION,  //
                                   "copyright", copyright,      // 
                                   "comments", comments,        // 
                                   "website", website,  // 
                                   "authors", authors,  // 
                                   "documenters", documenters,  // 
                                   "translator-credits", translator_credits,    // 
                                   "logo", NULL,        //this makes it use the default icon set up in main()
                                   "license", license,  //
                                   //"wrap-license", FALSE,       //
                                   NULL));
 gtk_window_set_destroy_with_parent (GTK_WINDOW (about), TRUE);
 g_signal_connect (about, "response", G_CALLBACK (gtk_widget_destroy), NULL);
 g_signal_connect (about, "destroy", G_CALLBACK (gtk_widget_destroyed),
                   &about);
 gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (main_window));
 gtk_window_present (GTK_WINDOW (about));
}

/////////////////////////////////////////////////////
//this gets called every GNAURAL_GUI_UPDATEINTERVAL milliseconds
//20070710: changed to only update one GUI feature per call
gboolean main_UpdateGUI (gpointer data)
{
 static int curcount = 0;

 if (++curcount > 3)
 {
  curcount = 0;
 }

 //added 20071126:
 gdk_threads_enter ();

 //deal with Y slider:
 if (TRUE == main_vscale_Y_mousebuttondown)
 {
  main_slider_XY_handler (main_vscale_Y, 0);
 }
 else if (TRUE == main_hscale_X_mousebuttondown)
 {
  main_slider_XY_handler (0, main_hscale_X);
 }

 switch (curcount)
 {
 case 0:
  //added 20070105 to deal with so many inputs not being updated:
  if (TRUE == SG_DataHasChanged)
  {
   main_LoadBinauralBeatSoundEngine ();
   SG_DataHasChanged = FALSE;
   VG_RelistFlag = TRUE;
  }
  break;

 case 1:
  main_UpdateGUI_PlaceInGraph ();
  break;

 case 2:
  main_UpdateGUI_Progressbar ();
  if (TRUE == SG_GraphHasChanged)
  {
   main_UpdateGUI_Voices (main_vboxVoices);
   SG_GraphHasChanged = FALSE;
  }
  break;

 case 3:
  main_UpdateGUI_Labels ();
  main_UpdateVU ();
  break;
 }

 //added 20071126:
 gdk_threads_leave ();

 return TRUE;
}

/////////////////////////////////////////////////////
void main_UpdateGUI_PlaceInGraph ()
{
 SG_DrawGraph (main_drawing_area);
}

/////////////////////////////////////////////////////
void main_UpdateGUI_Progressbar ()
{
 long total =
  (BB_CurrentSampleCountLooped +
   BB_CurrentSampleCount) / (int) BB_AUDIOSAMPLERATE;
 float i = 0;

 if (BB_TotalDuration > 0)
 {
  if (BB_Loops > 0)
  {
   i = total / (float) (BB_TotalDuration * BB_Loops);
  }
  if (i > 1)
  {
   i = 1;
  }
  else if (i < 0)
  {
   i = 0;
  }
  gtk_progress_bar_set_fraction ((GtkProgressBar *) main_ProgressBar, i);
 }
 else
 {
  gtk_progress_bar_set_fraction ((GtkProgressBar *) main_ProgressBar, 0);
 }
}

//======================================
void main_OnButton_Play (GtkButton * button)
{
 GN_Time_ResetSeniority ();
 if (main_AudioWriteFile_thread != NULL)
 {
  return;       //disable Play button if we're writing an audio file.
 }

 if (playAudio_SoundStream == NULL)
 {
  playAudio_SoundCleanup ();
  main_playAudio_SoundInit ();  //try to (re)start sound if it couldn't be initted before
  if (playAudio_SoundStream == NULL)
  {
   return;      //still didn't init, so give up
  }
  gnaural_pauseflag = ~gnaural_pauseflag;       //yes, a little silly, but necessary
 }

 gnaural_pauseflag = ~gnaural_pauseflag;
 if (gnaural_pauseflag == 0)
 {      // i.e., it was already paused:
  //continue playing sound:
  if (playAudio_SoundStream != NULL)
  {
   Pa_StartStream (playAudio_SoundStream);
  }

  //Following is so I don't erase good stuff on screen until full UpdateLoopInforestart:
  if (((BB_InfoFlag) & BB_COMPLETED) != 0)
  {
   BB_InfoFlag &= ~BB_COMPLETED;
   BB_CurrentSampleCountLooped = 0;
  }
  gtk_button_set_label (button, "_Pause");
  GtkWidget *image =
   gtk_image_new_from_stock (GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  main_UpdateGUI_Status ("Playing");
 }
 else
 {      //e.g., it was not paused
  //stop playing sound:
  if (playAudio_SoundStream != NULL)
  {
   Pa_StopStream (playAudio_SoundStream);
  }

  gtk_button_set_label (button, "_Play");
  GtkWidget *image =
   gtk_image_new_from_stock (GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  main_UpdateGUI_Status ("Paused");
 }
 // bbl_UpdateGUI_Info ();
}

/////////////////////////////////////////////////////
//added 20100620, in order to break up main_VoiceInfoFormatter ()
//for new list code (this loads main_sTmp)
//NEED TO BE SURE tmstr IS TERMINATED!!!
void main_VoiceInfoFormatter_type (SG_Voice * curVoice, char *tmstr)
{
 //      main_sTmp[0]='\0';
 //  sprintf (main_sTmp, "%d [", curVoice->ID);
 //  sprintf (main_sTmp, "[", curVoice->ID);
 switch (curVoice->type)
 {
 case BB_VOICETYPE_BINAURALBEAT:
  strcat (tmstr, main_sBinauralBeat);
  break;

 case BB_VOICETYPE_PINKNOISE:
  strcat (tmstr, main_sPinkNoise);
  break;

 case BB_VOICETYPE_PCM:
  strcat (tmstr, main_sAudioFile);
  break;

 case BB_VOICETYPE_ISOPULSE:
  strcat (tmstr, main_sIsochronicPulses);
  break;

 case BB_VOICETYPE_ISOPULSE_ALT:
  strcat (tmstr, main_sIsochronicPulses_alt);
  break;

 case BB_VOICETYPE_WATERDROPS:
  strcat (tmstr, main_sWaterDrops);
  break;

 case BB_VOICETYPE_RAIN:
  strcat (tmstr, main_sRain);
  break;
 }
}

/////////////////////////////////////////////////////
//added 20070625, loads main_sTmp
void main_VoiceInfoFormatter (SG_Voice * curVoice)
{
 static char num[64];

 if (curVoice != NULL)
 {
  sprintf (main_sTmp, "%d [", curVoice->ID);
  main_VoiceInfoFormatter_type (curVoice, main_sTmp);
  strcat (main_sTmp, "] ");
  strcat (main_sTmp, curVoice->description);

  //add count to end:
  int dpcount_all = 0,
   dpcount_selected = 0;

  SG_CountVoiceDPs (curVoice, &dpcount_all, &dpcount_selected);
  sprintf (num, " (%d,%d)", dpcount_all, dpcount_selected);
  strcat (main_sTmp, num);
 }
 else
 {
  main_sTmp[0] = '\0';
 }
}

/////////////////////////////////////////////////////
//negative amount rewinds, positive fast-forwards
//"amount" should be between -1.0 and 1.0
void main_OnButton_ForwardRewind (float amount)
{
 GN_Time_ResetSeniority ();
 if (amount < 0)
 {
  BB_ResetAllVoices ();
 }
 int i =
  BB_CurrentSampleCount + (BB_TotalDuration * amount * BB_AUDIOSAMPLERATE);
 if (i < 0)
  i = 0;
 else if (i > (BB_TotalDuration * BB_AUDIOSAMPLERATE))
  i = BB_TotalDuration * BB_AUDIOSAMPLERATE;
 BB_CurrentSampleCount = i;
}

/////////////////////////////////////////////////////
void main_OnButton_Stop (GtkButton * button)
{
 GN_Time_ResetSeniority ();
 BB_WriteStopFlag = 1;  //try to kill a write if it is what's happening
 //first step is to pause:
 gnaural_pauseflag = 0; //to be sure bbl_OnButton_Play() thinks it is was running
 main_OnButton_Play (main_buttonPlayPause);     // Simulate a user button push to enter paused state

 //now make schedule and everything else go back to begining
 BB_Reset ();
 main_UpdateGUI_Status ("Stopped");
 //bbl_UpdateGUI_Info ();
 //gtk_label_set_text (LabelProgramStatus, "Program Stopped");
}

/////////////////////////////////////////////////////
void main_UpdateGUI_Status (char *msg)
{
 sprintf (main_sTmp, "Status: %s", msg);
 gtk_label_set_text (main_labelStatus, main_sTmp);
}

/////////////////////////////////////////////////////
//20070105 updated to measure looping
void main_UpdateGUI_ProjectedRuntime ()
{
 const char sProjected[] = "Projected Runtime: ";

 if (BB_Loops != 0)
 {
  int total = (int) (BB_TotalDuration * BB_Loops);

  if (1 == BB_Loops)
  {
   sprintf (main_sTmp, "%s%d%s%d%s", sProjected, total / 60, main_sMin,
            total % 60, main_sSec);
  }
  else
  {
   sprintf (main_sTmp, "%s%d%s%d%s(%d%s%d%sx %d)", sProjected, total / 60,
            main_sMin, total % 60, main_sSec, ((int) BB_TotalDuration) / 60,
            main_sMin, ((int) BB_TotalDuration) % 60, main_sSec, BB_Loops);
  }
  gtk_label_set_text (main_labelTotalRuntime, main_sTmp);
 }
 else
 {
  sprintf (main_sTmp, "%sForever (%d%s%d%sx %s)", sProjected,
           ((int) BB_TotalDuration) / 60, main_sMin,
           ((int) BB_TotalDuration) % 60, main_sSec, main_sInf);
  gtk_label_set_text (main_labelTotalRuntime, main_sTmp);
 }
}

/////////////////////////////////////////////////////
void main_FormatProgressString ()
{
 const char sProgress[] = "Progress: ";

 const char sLoop[] = "(Loop: ";

 int total =
  (BB_CurrentSampleCountLooped + BB_CurrentSampleCount) / BB_AUDIOSAMPLERATE;
 int remaining = (int) ((BB_TotalDuration * BB_Loops) - total);

 if (BB_Loops != 0)
 {
  sprintf (main_sTmp, "%s%d%s%d%s Remaining: %d%s%d%s%s%d/%d)", sProgress,
           total / 60, main_sMin, total % 60, main_sSec, remaining / 60,
           main_sMin, remaining % 60, main_sSec, sLoop,
           BB_Loops - BB_LoopCount + 1, BB_Loops);
 }
 else
 {
  sprintf (main_sTmp, "%s%d%s%d%s%s%d/%s)", sProgress, (total / 60),
           main_sMin, total % 60, main_sSec, sLoop, (-BB_LoopCount) + 1,
           main_sInf);
 }
}

//======================================
//QUESTIONABLE BUG FIXES:
//-BUG: figure out why resetting bb->InfoFlag's caused update labels timer to quit.
//            Wow, this one was a bit as if the garage door opened every time I pulled
//            my hair. Bizarre. One of the wierdest bugs I've ever seen.
// SOLUTION: made BinauralBeat's InfoFlag an unsigned int (from int), made
//            explicit that the timer func returned TRUE, and rephrased bit setting line
//             back to what it had been when the bug first appeared (?!).
//            Hard to believe this really fixed it, but it no longer does the bad behavior. (20051020)
void main_UpdateGUI_Labels ()
{

 //-----------------------------------------------------------
 //START stuff that gets updated EVERY second:
 //
 // current time point within schedule:
 main_FormatProgressString ();
 gtk_label_set_text (main_labelCurrentRuntime, main_sTmp);

 //a20070620 Voice info:
 // main_VoiceInfoFormatter (SG_SelectVoice (NULL));
 // gtk_label_set_text (main_labelVoiceInfo, main_sTmp);
 //
 //END stuff that gets updated EVERY second:
 //-----------------------------------------------------------------------

 //-----------------------------------------------------------------------
 //START check bb->InfoFlag
 //now update things that BinauralBeat says are changed:
 if (BB_InfoFlag != 0)
 {
  //.........................
  //if schedule is done:
  if (((BB_InfoFlag) & BB_COMPLETED) != 0)
  {
   //  bbl_OnButton_Stop ();
   //        gtk_label_set_text (LabelProgramStatus, "Schedule Completed");
   gnaural_pauseflag = 0;       //so bbl_OnButton_Play() thinks I was running
   //main_OnButton_Play (); // I'm simulating user button push to pause
   main_OnButton_Stop (NULL);
   main_UpdateGUI_Status ("Schedule Completed");
   //   bb->loopcount = bb->loops;
   //   gtk_progress_bar_set_fraction (ProgressBar_Overall, 1.0);

  }

  //.........................
  //if starting a new loop:
  if (((BB_InfoFlag) & BB_NEWLOOP) != 0)
  {
   //reset the "new loop" bit of InfoFlag:
   BB_InfoFlag &= ~BB_NEWLOOP;
   //bbl_UpdateGUI_LoopInfo ();
  }
 }      //END check bb->InfoFlag
 //-------------------------------------------------------------
}

/////////////////////////////////////////////////////
void main_UpdateGUI_Statusbar (const char *msg1, const char *msg2)
{
 // g_print ("Trying to update statusbar\n");
 // sprintf(main_sTmp, "Trying to update statusbar\n");
 gint context_id = gtk_statusbar_get_context_id (main_Statusbar, "SC");

 gtk_statusbar_pop (main_Statusbar, context_id);
 sprintf (main_sTmp, "%s %s", msg1, msg2);
 gtk_statusbar_push (main_Statusbar, context_id, main_sTmp);
}

//======================================
//following tries to be sure I don't end with a \ or / (which happens
//only when getcwd returns a root like / or C:\, D:\   :
void main_FixPathTerminator (char *pth)
{
 //  #ifdef GNAURAL_WIN32
 if (pth[(strlen (main_sPathCurrentWorking) - 1)] == '\\')
 {
  pth[(strlen (main_sPathCurrentWorking) - 1)] = '\0';
 }
 // #endif
 else
  // #ifndef GNAURAL_WIN32
 if (main_sPathCurrentWorking[(strlen (main_sPathCurrentWorking) - 1)] == '/')
 {
  main_sPathCurrentWorking[(strlen (main_sPathCurrentWorking) - 1)] = '\0';
 }
 // #endif
}

//======================================
//returns zero if file or path DOES exist
int main_TestPathOrFileExistence (char *PathName)
{
 struct stat buf;

 return stat (PathName, &buf);
}

//======================================
//The most important thing this does is set main_sCurrentGnauralFilenameAndPath
//e20070705: If bDefaultFlag == TRUE sets up $HOME/gnaural.schedule,
//if FALSE, works with whatever is in main_sCurrentGnauralFilenameAndPath
void main_SetupPathsAndFiles (int bDefaultFlag)
{
 //Current Working Directory: [20100405: added check to avoid compiler complaints]
 if (NULL == getcwd (main_sPathCurrentWorking, PATH_MAX))
 {
  fprintf (stderr, "Couldn't get current working directory, errno: %i\n",
           errno);
 }
 main_FixPathTerminator (main_sPathCurrentWorking);

 //DBGOUT(szDir_CurrentWorking);

 //copy CWD to the other vars, just in case I forget to put anything in them:
 strcpy (main_sPathHome, main_sPathCurrentWorking);
 strcpy (main_sPathGnaural, main_sPathCurrentWorking);

 SG_DBGOUT ("This was the command to launch Gnaural:");
 SG_DBGOUT (main_sPathExecutable);

 SG_DBGOUT ("This is the current working directory:");
 SG_DBGOUT (main_sPathCurrentWorking);

#ifdef GNAURAL_WIN32
 //###########START WIN32-ONLY CODE#############
 //In Win32, I try to work from wherever the executable is located:

 //-----
 //START of determine Gnaural's home directory:
 //HKEY_LOCAL_MACHINE\\Software\Microsoft\\Windows\CurrentVersion\\App Paths\\gnaural.exe
 //C:\Program Files\Gnaural;c:\Program Files\Common Files\GTK\2.0\bin
 HKEY hkey;

 if (ERROR_SUCCESS ==
     RegOpenKeyEx (HKEY_LOCAL_MACHINE,
                   "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\gnaural.exe",
                   0, KEY_QUERY_VALUE, &hkey))
 {
  SG_DBGOUT ("Found gnaural.exe registry entry");
  DWORD type,
   count = sizeof (main_sPathTemp);

  if (ERROR_SUCCESS ==
      RegQueryValueExA (hkey, "Path", 0, &type, (LPBYTE) main_sPathTemp,
                        &count))
  {
   //Looking for the first path in the list:
   char *tmpch;

   tmpch = strchr (main_sPathTemp, ';');        //search for first ';'
   if (tmpch != NULL)
   {
    (*tmpch) = '\0';
   }
   strcpy (main_sPathGnaural, main_sPathTemp);
   main_FixPathTerminator (main_sPathGnaural);
  }
  RegCloseKey (hkey);
 }
 else
 {
  //try a bruteforce approach:
  //20070919: I THINK THIS HAS main_TestPathOrFileExistence BACKWARD (not changing until I am sure)
  //20101201: finally changed it, i do think this was wrong and finally caused a problem on a user restricted machine:
  gchar winpathstr[] = "C:\\Program Files\\Gnaural";
  SG_DBGOUT_STR ("No gnaural.exe registry entry, checking manually for",
                 winpathstr);
  if (0 == main_TestPathOrFileExistence (winpathstr))
  {
   SG_DBGOUT_STR (winpathstr, "did exist");
   strcpy (main_sPathGnaural, winpathstr);
  }
  else
  {
   SG_DBGOUT_STR (winpathstr, "did NOT exist");
  }
 }

 /*
    //NOTE: Original intent (below commented code) was to have all file creation in executable's home directory, but
    //it failed because of "/" and "\" inconsistencies between WINE and Win32, rendering
    //mixed paths, etc. Since getcwd() seems to be mostly consistent on each
    //platform, I eventually stuck with that -- but in the end, a WINE bug causing gcwd to return
    //(in my case) H:\ for both root and my home directory made me give-up and just
    //use the (goddamned) registry.

    strcpy (main_sPathTemp, szFile_ExecutablePath);

    //Now make all slashes consistent with DOS:
    int i=strlen(main_sPathTemp);
    while ((--i)>-1) if (main_sPathTemp[i]=='/') main_sPathTemp[i]='\\';

    //Now throw away executable name if there is any:
    char *tmpch;
    tmpch=strrchr(main_sPathTemp,'\\'); //search for first slash from end
    if (tmpch!=NULL) {
    (*tmpch)='\0';

    GNAURAL_DBGOUT("This is the directory Gnaural is being run from:");
    GNAURAL_DBGOUT(main_sPathTemp);

    //now return all slashes to DOS style:
    i=strlen(main_sPathTemp);
    while ((--i)>-1) if (main_sPathTemp[i]=='/') main_sPathTemp[i]='\\';

    //now change in to that directory [still necessary?!]
    chdir(main_sPathTemp);

    //now ADD starting slash if it isn't there:
    if (main_sPathTemp[0]!='\\')
    {
    for (i=strlen(main_sPathTemp);i>-1;i--)  main_sPathTemp[i+1]=main_sPathTemp[i];
    main_sPathTemp[0]='\\';
    }
    }
    //If I got here, user is already working from Gnaural's directory:
    else main_sPathTemp[0]='\0';

    //now attach this string to the end of CWD:
    strcpy(szDir_Gnaural,szDir_CurrentWorking);
    strcat(szDir_Gnaural,main_sPathTemp);
  */

 //END of determine Gnaural's home directory
 //-----

 //Finally, setup file and path to gnaural_schedule.txt file:
 if (bDefaultFlag == TRUE)
 {
  sprintf (main_sCurrentGnauralFilenameAndPath, "%s\\schedule.gnaural",
           main_sPathGnaural);
 }
 SG_DBGOUT ("Constructing default schedule path:");
 SG_DBGOUT (main_sCurrentGnauralFilenameAndPath);
 return;
 //###########END WIN32-ONLY CODE#############
#endif

#ifndef GNAURAL_WIN32
 //###########START 'NUX-ONLY CODE#############
 //In Windows, Gnaural just does everything in it's formal installation directory.
 //In 'nix, it works from ~/.gnaural

 //figure out the paths/directories needed for all file access:
 if (NULL != getenv ("HOME"))
 {
  //Home directory:
  strcpy (main_sPathHome, getenv ("HOME"));
  //~/.gnaural directory:
  sprintf (main_sPathGnaural, "%s/.gnaural", main_sPathHome);
 }
 else
 {      //failed, so harmlessly put current directory in there...
  SG_ERROUT ("Couldn't determine home directory");
  strcat (main_sPathHome, main_sPathCurrentWorking);
  strcat (main_sPathGnaural, main_sPathCurrentWorking);
 }

 //create the .gnaural directory if it doesn't exist yet:szDir_CurrentWorking
 if (0 != main_TestPathOrFileExistence (main_sPathGnaural))
 {
  SG_DBGOUT ("Creating ~/.gnaural directory");

  if (0 != mkdir (main_sPathGnaural, 0777))
  {
   fprintf (stderr, "Couldn't make ~/.gnaural directory %s: errno=%i\n",
            main_sPathGnaural, errno);
  }
 }
 else
 {
  SG_DBGOUT ("Found ~/.gnaural directory; will use.");
 }
 //setup file and path to gnaural_schedule.txt file:
 if (bDefaultFlag == TRUE)
 {
  sprintf (main_sCurrentGnauralFilenameAndPath, "%s/schedule.gnaural",
           main_sPathGnaural);
 }
 SG_DBGOUT ("Accessing schedule file: ");
 SG_DBGOUT (main_sCurrentGnauralFilenameAndPath);
 return;
#endif
 //###########END 'NUX-ONLY CODE#############
}

/////////////////////////////////////////////////////
//20070824 Highly improved Vol-Bal routine taken from Gnaural
void main_ProcessVolBal ()
{
 BB_VolumeOverall_right = BB_VolumeOverall_left = main_OverallVolume;

 if (main_OverallBalance > 0)
 {      //that means attenuate Left:
  BB_VolumeOverall_left *= (1.f - fabs (main_OverallBalance));
 }
 else
 {      //that means attenuate Right:
  BB_VolumeOverall_right *= (1.f - fabs (main_OverallBalance));
 }
}

/////////////////////////////////////////////////////
void main_on_hscaleBalance (float range)
{
 main_OverallBalance = range *= .01;
 main_ProcessVolBal ();
}

/////////////////////////////////////////////////////
void main_on_hscaleVolume (float range)
{
 main_OverallVolume = range *= .01;
 main_ProcessVolBal ();
}

/////////////////////////////////////////////////////
//Main graphtype func. Sets global SG_GraphType to the appropriate 
//define from the given togglebutton and redraws graph to new view. 
//Used to determine graph type from user selection
void main_SetGraphType (GtkToggleButton * togglebutton)
{
 gtk_toggle_button_set_active (togglebutton, TRUE);
 if (togglebutton == main_togglebuttonViewFreq)
 {
  SG_GraphType = SG_GRAPHTYPE_BASEFREQ;
 }
 else if (togglebutton == main_togglebuttonViewBeat)
 {
  SG_GraphType = SG_GRAPHTYPE_BEATFREQ;
 }
 else if (togglebutton == main_togglebuttonViewVol)
 {
  SG_GraphType = SG_GRAPHTYPE_VOLUME;
 }
 else if (togglebutton == main_togglebuttonViewBal)
 {
  SG_GraphType = SG_GRAPHTYPE_VOLUME_BALANCE;
 };
 SG_ConvertDataToXY (main_drawing_area);
 SG_DrawGraph (main_drawing_area);
}

/////////////////////////////////////////////////////
//Sets graph view toggle according to SG_GraphType
void main_SetGraphToggle (GtkWidget * widget)
{
 switch (SG_GraphType)
 {
 case SG_GRAPHTYPE_BASEFREQ:
  main_SetGraphType (main_togglebuttonViewFreq);
  break;

 case SG_GRAPHTYPE_VOLUME:
  main_SetGraphType (main_togglebuttonViewVol);
  break;

 case SG_GRAPHTYPE_VOLUME_BALANCE:
  main_SetGraphType (main_togglebuttonViewBal);
  break;

 case SG_GRAPHTYPE_BEATFREQ:
  main_SetGraphType (main_togglebuttonViewBeat);
  break;

 default:
  main_SetGraphType (main_togglebuttonViewFreq);
  break;
 }
 SG_ConvertDataToXY (widget);
 SG_DrawGraph (widget);
}

/////////////////////////////////////////////////////
//Basically, a way to get a quick "yes or no" from user, or just present some info.
//Returns 1 for "yes", 0 for "no"
int main_MessageDialogBox (char *msg,
                           GtkMessageType messagetype,
                           GtkButtonsType buttonformation)
{
 GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
                                             GTK_DIALOG_DESTROY_WITH_PARENT,
                                             messagetype,
                                             buttonformation,
                                             "%s", msg);

 //  "Error loading file '%s': %s",  main_sTmp, g_strerror (errno));
 gint res = gtk_dialog_run (GTK_DIALOG (dialog));

 gtk_widget_destroy (dialog);

 if (GTK_RESPONSE_OK == res || GTK_RESPONSE_YES == res ||
     GTK_RESPONSE_APPLY == res || GTK_RESPONSE_ACCEPT == res)
 {
  return 1;
 }
 else
  return 0;
}

//========================================
//NOTE: THIS OPTION WAS DEACTIVATED IN THE GLADE FILE 20071203
void main_on_export_mp3_activate ()
{
 //This is experimental, requires LAME, and runs a second instance of Gnaural in the background;
 //Sorry, Windows users - I don't have a way to hack this for you yet :-(

 main_MessageDialogBox
  ("Sorry, Gnaural can't make MP3's yet, but you can make a WAV file then covert it to MP3 with LAME\nhttp://lame.sourceforge.net",
   GTK_MESSAGE_INFO, GTK_BUTTONS_OK);
 return;

#ifdef GNAURAL_WIN32
 main_MessageDialogBox
  ("Currently, the Windows version can only do this at the command line. Be sure LAME is installed\n(see http://lame.sourceforge.net/),\nthen type:\n\n\tgnaural -o | lame - MyMeditation.mp3",
   GTK_MESSAGE_INFO, GTK_BUTTONS_OK);
 return;
#endif

 if (BB_Loops < 1)
 {
  if (0 ==
      main_MessageDialogBox
      ("You are in endless-loop mode. Do you really want to fill all the space on your hard drive?",
       GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO))
  {
   return;      //don't start write an infinitely large file
  }
 }

 //TODO: check automatically for LAME
 if (0 ==
     main_MessageDialogBox
     ("Do you have LAME installed? If not, this step will fail.\nSee http://lame.sourceforge.net/",
      GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO))
 {
  return;
 }
 //TODO check disk space for user, update GUI status window to tell that writing is going on

 //Ask for a name for the MP3 file:
 GtkWidget *dialog;

 dialog =
  gtk_file_chooser_dialog_new ("Save MP3 Audio File",
                               GTK_WINDOW (main_window),
                               GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL,
                               GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE,
                               GTK_RESPONSE_ACCEPT, NULL);
 //gtk_entry_set_text (GTK_ENTRY (entry_Input), "Gnaural.wav");
 gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                      main_sPathCurrentWorking);
 gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "Gnaural.mp3");

 //gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);//doesn't exist yet in Debian unstable

 if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
 {
  char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

  if (strlen (filename) > 0)
  {
   //this works great, just doesn't give user any way to know when it is done, and (worse) doesn't always reliably find it's own executable.
   //###############FOR LINUX################
#ifndef GNAURAL_WIN32
   sprintf (main_sPathTemp, "%s -o | lame - %s &", main_sPathExecutable,
            filename);
   //[20100405: added check to avoid compiler complaints]:
   if (0 == system (main_sPathTemp))
   {
   }
#endif
   //###############FOR WIN32################
#ifdef GNAURAL_WIN32
   sprintf (main_sPathTemp, "%s -o | lame - %s", main_sPathExecutable,
            filename);
   system (main_sPathTemp);
   //  _spawn(main_sPathTemp);
   //_spawnlp (_P_DETACH, szFile_ExecutablePath, "-o", "|", "lame","-",  filename, NULL);
   //_execlp (szFile_ExecutablePath, "-o", "|", "lame","-",  filename, NULL);
   /*
      ShellExecute(0,
      "open", // Operation to perform
      main_sPathTemp, // Application name
      NULL, // Additional parameters
      0, // Default directory
      SW_SHOW);
    */
#endif

   main_MessageDialogBox
    ("A second copy of Gnaural has be spawned, and will run in the background until finished. In case of a problem, you can kill the processes with:\nkillall gnaural",
     GTK_MESSAGE_INFO, GTK_BUTTONS_OK);
  }
  g_free (filename);
 }
 gtk_widget_destroy (dialog);
}

//START writethreadstuff======================================
/////////////////////////////////////////////////////
void main_on_export_audio_to_file_activate ()
{
 char *filename = NULL;

 if (0 != main_CheckSndfileVersion ())
 {
  return;
 }

 if (BB_Loops < 1)
 {
  if (0 ==
      main_MessageDialogBox
      ("You are in endless-loop mode. Do you really want to fill all the space on your hard drive?",
       GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO))
  {
   return;      //can't write an infinitely large file! (COULD USE A DIALOG)
  }
 }
 if (main_AudioWriteFile_thread != NULL)
 {
  main_MessageDialogBox ("You are already writing a file.",
                         GTK_MESSAGE_ERROR, GTK_BUTTONS_OK);
  return;       //this means someone is already writing (COULD USE A DIALOG)
 }
 //TODO check disk space for user, update GUI status window to tell that writing is going on
 GtkWidget *dialog;

 dialog =
  gtk_file_chooser_dialog_new ("Save Audio File",
                               GTK_WINDOW (main_window),
                               GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL,
                               GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE,
                               GTK_RESPONSE_ACCEPT, NULL);

 //create a Box to put some extra stuff in:
 GtkWidget *box = gtk_vbox_new (FALSE, 0);

 gtk_widget_show (box);
 gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (dialog), box);

 //add some information for user
 GtkWidget *label_Info =
  gtk_label_new
  ("Filename extension will be appended automatically according to format chosen below:");
 gtk_widget_show (label_Info);
 gtk_box_pack_start (GTK_BOX (box), label_Info, FALSE, FALSE, 0);

 GtkWidget *comboboxentry1 = gtk_combo_box_entry_new_text ();

 gtk_widget_show (comboboxentry1);

 int ComboboxEntryCount = 0;
 static int ComboboxFiletypeSelection = -1;
 char *strFileExtension = 0;

 //START standard sndfile list code vars=====================
 SF_FORMAT_INFO info;
 SF_INFO sfinfo;
 char buffer[128];
 int format,
  major_count,
  subtype_count,
  m,
  s;

 sf_command (NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof (int));
 sf_command (NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count,
             sizeof (int));
 //END standard sndfile list code vars=====================

 //START standard sndfile list code=====================
 //clean everything up before filling it
 memset (&sfinfo, 0, sizeof (sfinfo));
 buffer[0] = 0;
 sfinfo.channels = 2;

 for (m = 0; m < major_count; m++)
 {
  info.format = m;
  sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
  format = info.format;
  for (s = 0; s < subtype_count; s++)
  {
   info.format = s;
   sf_command (NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof (info));

   //put the whole number together and let sfinfo hold it:
   format = (format & SF_FORMAT_TYPEMASK) | info.format;

   sfinfo.format = format;
   //--------put my stuff here:
   //fill comboboxes with all compatible formats:
   if (main_CheckSndfileFormat (sfinfo.format))
   {
    //now do this over again in order to get the name:
    info.format = m;
    sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));

    SG_DBGOUT_STR ("Adding format:", info.name);
    gtk_combo_box_append_text (GTK_COMBO_BOX (comboboxentry1), info.name);
    //if this is first time user is doing this, set default to whatever my default was:
    if (-1 == ComboboxFiletypeSelection
        && (main_AudioWriteFile_format & SF_FORMAT_TYPEMASK) == info.format)
    {
     ComboboxFiletypeSelection = ComboboxEntryCount;
     SG_DBGOUT ("[Defaulting to above format]");
    }
    ++ComboboxEntryCount;
   }
   else
   {
    SG_DBGOUT_STR ("NOT adding format:", info.name);
   }
   //--------end my stuff
  };
 };
 //END standard sndfile list code=====================

 //add the usual filters for sndfile's formats:
 //old way:
 //main_DialogAddFileFilters (dialog,"~Gnaural Audio Files~*.wav,*.aiff,*.au,*.flac,*.ogg,~All Files~*");
 //new way:
 main_MakeAudioFormatsFileFilterString (main_sTmp, sizeof (main_sTmp));
 main_DialogAddFileFilters (dialog, main_sTmp);

 //make ComboboxFiletypeSelection hightlight last chosen comboboxentry for format:
 gtk_combo_box_set_active ((GtkComboBox *) comboboxentry1,
                           ComboboxFiletypeSelection);
 gtk_box_pack_end (GTK_BOX (box), comboboxentry1, FALSE, FALSE, 0);

 //extract base from filename for convenience:
 filename = g_path_get_basename (main_AudioWriteFile_name);
 gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), filename);
 g_free (filename);

 //add some filters for user's convenience:
 //old way:
 // main_DialogAddFileFilters (dialog,
 //    "~Gnaural Audio Files~*.wav,*.aiff,*.au,*.flac,*.ogg,~All Files~*");

 if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
 {
  filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
  if (strlen (filename) > 0)
  {
   ComboboxFiletypeSelection =
    gtk_combo_box_get_active ((GtkComboBox *) comboboxentry1);

   //now get ComboboxFiletypeSelection extension and sndfile type to write:
   main_sTmp[0] = '\0';
   ComboboxEntryCount = 0;

   //START standard sndfile list code=====================
   //clean vars before filling them:
   memset (&sfinfo, 0, sizeof (sfinfo));
   buffer[0] = 0;
   sfinfo.channels = 2;

   for (m = 0; m < major_count; m++)
   {
    info.format = m;
    sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
    format = info.format;
    for (s = 0; s < subtype_count; s++)
    {
     info.format = s;
     sf_command (NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof (info));

     format = (format & SF_FORMAT_TYPEMASK) | info.format;

     sfinfo.format = format;
     //--------put my stuff here:
     if (main_CheckSndfileFormat (sfinfo.format))
     {
      info.format = m;
      sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
      if (ComboboxEntryCount == ComboboxFiletypeSelection)
      {
       main_AudioWriteFile_format = sfinfo.format;
       sprintf (main_sTmp, ".%s", info.extension);
       strFileExtension = main_sTmp;
       m = major_count;
       break;   //critical so that info.* data above doesn't change
      }
      ++ComboboxEntryCount;
     }
     //--------end my stuff
    };
   };
   //END standard sndfile list code=====================

   //now, check for extension, if there is one, see if it is same
   //as ComboboxFiletypeSelection; if so do nothing, otherwise add correct one here:
   SG_StringAllocateAndCopy (&main_AudioWriteFile_name, filename);
   //asign address of last character of string to ext:
   char *ext = &(main_AudioWriteFile_name[strlen (main_AudioWriteFile_name)]);

   //count backward through string until either first character is found OR a "." is found:
   for (; main_AudioWriteFile_name != ext && ext[0] != '.'; ext--);
   //if no "." was found, leave it alone:
   if (ext == main_AudioWriteFile_name)
   {
    BB_DBGOUT ("Filename has no extension");
   }
   else
    //if a "." was found:
   {
    //make sure really removing a Gnaural-understood extension:
    BB_DBGOUT_STR ("Previous file extension:", ext);
    for (m = 0; m < major_count; m++)
    {
     info.format = m;
     sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info));
     if (0 == strncasecmp (&ext[1], info.extension, strlen (info.extension)))
     {
      //TODO: it would be nice if I alerted user IF their extension
      //is different from the SF_AUDIOFORMAT they "chose", because
      //I just replace it anyway:
      BB_DBGOUT ("Extension known - removing");
      *ext = '\0';      //"remove" extension
      break;
     }
    }

    if (m >= major_count)
    {
     BB_DBGOUT ("Extension unknown - not removing");
    }
   }

   //put an extension on:
   strcpy (main_sPathTemp, main_AudioWriteFile_name);
   strcat (main_sPathTemp, strFileExtension);

   //see if file already exists:
   if (0 == main_TestPathOrFileExistence (main_sPathTemp) &&
       0 == main_MessageDialogBox (main_sPathTemp,
                                   GTK_MESSAGE_INFO, GTK_BUTTONS_YES_NO))
   {
    BB_DBGOUT ("File already exists, user aborted Write");
   }
   else
   {
    SG_StringAllocateAndCopy (&main_AudioWriteFile_name, main_sPathTemp);
    BB_DBGOUT_STR ("New Filename:", main_AudioWriteFile_name);
    main_OnButton_Stop (NULL);
    main_AudioWriteFile_start ();
   }
  }
  g_free (filename);
 }
 gtk_widget_destroy (dialog);
}

/////////////////////////////////////////////////////
//Only interested in SubTypes SF_FORMAT_PCM_16 and SF_FORMAT_VORBIS:
int main_CheckSndfileFormat (int format)
{
 SF_INFO sfinfo;

 memset (&sfinfo, 0, sizeof (sfinfo));
 sfinfo.channels = 2;
 sfinfo.format = format;

 // sfinfo.format = (formatMajor & SF_FORMAT_TYPEMASK) | format;
 //Now check for either SF_FORMAT_PCM_16 or SF_FORMAT_VORBIS
 if (SF_FORMAT_PCM_16 != (format & SF_FORMAT_SUBMASK)
     && SF_FORMAT_VORBIS != (format & SF_FORMAT_SUBMASK))
  return FALSE;
 return sf_format_check (&sfinfo);
}

//========================================
void main_AudioWriteFile (void *ptr)
{
 BB_DBGOUT ("Starting Audio Write File thread");
 BB_DBGOUT_INT ("Audio SF_FORMAT type:", main_AudioWriteFile_format);
 //BB_WriteWAVFile (main_AudioWriteFile_name); //OLD WAY, replaced 20071130
 exportAudioToFile (main_AudioWriteFile_name, main_AudioWriteFile_format);
 main_AudioWriteFile_thread = NULL;     //so rest of program knows writing is done
 BB_DBGOUT ("Audio Write File thread exiting");
 g_thread_exit (0);     //this should be updated to g_thread_join(GThread) when I get more time
}

//======================================
void main_AudioWriteFile_start ()
{
 if (main_AudioWriteFile_thread != NULL)
 {
  main_MessageDialogBox ("You are already writing one", GTK_MESSAGE_ERROR,
                         GTK_BUTTONS_OK);
  return;       //this means someone is already writing
 }
 //Note: you must be in paused state to do writing!
 //bbl_UpdateGUI_Info ();

 //user can set this to non-0 at any time to stop write:
 BB_WriteStopFlag = 0;
 //new gthread code (new 20051126):
 GError *thread_err = NULL;

 if (NULL ==
     (main_AudioWriteFile_thread
      =
      g_thread_create ((GThreadFunc) main_AudioWriteFile, (void *) NULL, TRUE,
                       &thread_err)))
 {
  fprintf (stderr, "g_thread_create failed: %s!!\n", thread_err->message);
  g_error_free (thread_err);
 }
}

//END writethreadstuff======================================

//====START OF IMPORT OLD GNAURAL FILE SECTION====
//======================================
//this just reads the values found between brackets []
void main_GNAURAL1FILE_ParseCmd (FILE * stream)
{
 char strNum[128];

 char strCmd[128];

 char tmpchar;

 int inum = 0,
  icmd = 0;

 strNum[0] = strCmd[0] = '\0';
 while ((tmpchar = fgetc (stream)) != ']' && feof (stream) == 0)
 {
  //eat up any extra characters
  //while (tmpchar=='=' || ' ') tmpchar=fgetc( stream );
  //if it is a number, put it in the number string:
  if ((tmpchar >= '0' && tmpchar <= '9') || tmpchar == '.' || tmpchar == '-')
  {
   strNum[inum] = tmpchar;
   inum++;
  }

  //if it is a string, put it in Cmd string:
  else
   if ((tmpchar >= 'A'
        && tmpchar <= 'Z') || (tmpchar >= 'a' && tmpchar <= 'z'))
  {
   strCmd[icmd] = toupper (tmpchar);
   icmd++;
  }
  else if (tmpchar == '#')
  {     //user put a comment after a command!
   while (feof (stream) == 0 && tmpchar != '\n')
    tmpchar = fgetc (stream);
   return;
  }
 }      //end of parsing line:

 strNum[inum] = '\0';
 strCmd[icmd] = '\0';
 if (!strcmp (strCmd, "BASEFREQ"))
 {
  OldGnauralVars.FreqBase = (float) g_ascii_strtod (strNum, NULL);
  if (OldGnauralVars.FreqBase < 40)
   OldGnauralVars.FreqBase = 40;
  if (OldGnauralVars.FreqBase > 1000)
   OldGnauralVars.FreqBase = 1000;
 }

 if (!strcmp (strCmd, "TONEVOL"))
 {
  OldGnauralVars.Volume_Tone = (float) g_ascii_strtod (strNum, NULL);
  if (OldGnauralVars.Volume_Tone < 0)
   OldGnauralVars.Volume_Tone = 0;
  if (OldGnauralVars.Volume_Tone > 100)
   OldGnauralVars.Volume_Tone = 100;
  //    OldGnauralVars.Volume_Tone = (int) (163.84 * ftemp + 0.5);
  OldGnauralVars.Volume_Tone = (float) (OldGnauralVars.Volume_Tone * 0.01);
 }

 if (!strcmp (strCmd, "NOISEVOL"))
 {
  OldGnauralVars.Volume_Noiz = (float) g_ascii_strtod (strNum, NULL);
  if (OldGnauralVars.Volume_Noiz < 0)
   OldGnauralVars.Volume_Noiz = 0;
  if (OldGnauralVars.Volume_Noiz > 100)
   OldGnauralVars.Volume_Noiz = 100;
  OldGnauralVars.Volume_Noiz = (float) (OldGnauralVars.Volume_Noiz * 0.01);
 }

 if (!strcmp (strCmd, "LOOPS"))
 {
  OldGnauralVars.loops = atoi (strNum);
  if (OldGnauralVars.loops < 0)
   OldGnauralVars.loops = 0;
  OldGnauralVars.loopcount = OldGnauralVars.loops;
 }
}

//======================================
void main_GNAURAL1FILE_SchedBuffToSchedule (char *str)
{
 //the philosophy here: take a string with commas separating the
 //floats, hunt for commas. When one is found, start a new string
 //beginning with next character. Take every following character up
 //to (but not including) the next comma. Then store that float, dispose
 //of the tempstring, and hunt some more until length of string is
 //covered.
 char tmpstr[256];

 float *tmpfloat = NULL;

 char substr[256];

 //first count how many floats are in the string:
 int floatcount = 1;

 unsigned long i;

 for (i = 0; i < strlen (str); i++)
 {
  if (str[i] == ',')
  {
   floatcount++;
  }     // end if comma
 }      //end for i
 //do it all again, now that I know how many floats:
 tmpfloat = (float *) malloc (floatcount * sizeof (float));
 floatcount = 0;
 int j = 0;

 for (i = 0; i < strlen (str); i++)
 {
  //first extract a number to a string:
  if ((str[i] >= '0' && str[i] <= '9') || str[i] == '.' || str[i] == '-')
   tmpstr[j++] = str[i];
  //if I found a comma, end the string:
  else if (str[i] == ',')
  {
   tmpstr[j] = '\0';
   strcpy (substr, tmpstr);
   tmpfloat[floatcount] = (float) g_ascii_strtod (substr, NULL);
   j = 0;
   floatcount++;
  }     // end if comma
 }      //end for i
 if (j != 0)
 {      //there should be one more float to get:
  tmpstr[j] = '\0';
  strcpy (substr, tmpstr);
  tmpfloat[floatcount] = (float) g_ascii_strtod (substr, NULL);
  ++floatcount;
 }      // end if j!=0
 //==START LOADING THE FLOATS IN TO THE GRAPH:
 OldGnauralVars.ScheduleEntriesCount = floatcount / 3;
 //Now init SG so that it can start to receive the schedule.
 //Since this is an old Gnaural file, there must be only two voices, first being
 //the explicitly directed binaural beat, the second simple unmodulated noise. The
 //philosophy of Gnaural file opening is to let the Graph (SG) always lead the BB
 //engine, so load the graph by creating 2 voices with SG_SetupDefaultDataPoints(),
 //then delete all DPs and start loading adding real ones to the end (the noise one only
 //has one DP, with the volume set).
 SG_SetupDefaultDataPoints (2); //first voice is BB data, second will be noise
 //important: cleanup all datapoints (since new data will be instated), and NULL first DPs:
 SG_CleanupDataPoints (SG_FirstVoice->FirstDataPoint);
 SG_FirstVoice->FirstDataPoint = NULL;
 SG_CleanupDataPoints (SG_FirstVoice->NextVoice->FirstDataPoint);
 SG_FirstVoice->NextVoice->FirstDataPoint = NULL;
 //for reference:
 /*
    struct  {
    float FreqBase;
    float Volume_Tone;
    float Volume_Noiz;
    int loops;
    int loopcount;
    int StereoNoiz;
    int ScheduleEntriesCount;
    } OldGnauralVars;
  */
 //==START load BinauralBeat voice:
 int buffcount = 0;

 float FreqLStart,
  FreqRStart,
  Duration,
  Duration_total = 0;

 int entry;

 SG_DataPoint *curDP = NULL;

 for (entry = 0; entry < OldGnauralVars.ScheduleEntriesCount; entry++)
 {
  //get the buffer values, put them in temporary storage:
  FreqLStart = (float) tmpfloat[buffcount++];
  FreqRStart = (float) tmpfloat[buffcount++];
  Duration = (float) tmpfloat[buffcount++];
  Duration_total += Duration;
  //apply temporary storage to a real: DP
  curDP = SG_AddNewDataPointToEnd (SG_FirstVoice->FirstDataPoint, SG_FirstVoice, Duration,      //duration
                                   fabs (FreqLStart - FreqRStart),      //beatfreq
                                   OldGnauralVars.Volume_Tone,  //volume_left
                                   OldGnauralVars.Volume_Tone,  //volume_right
                                   OldGnauralVars.FreqBase,     //basefreq
                                   SG_UNSELECTED);      //state
  if (SG_FirstVoice->FirstDataPoint == NULL)
   SG_FirstVoice->FirstDataPoint = curDP;
 }      //end for
 //==END load BinauralBeat voice

 //==START load Noise voice:
 curDP = SG_AddNewDataPointToEnd (SG_FirstVoice->NextVoice->FirstDataPoint, SG_FirstVoice->NextVoice, Duration_total,   //duration
                                  0,    //beatfreq
                                  OldGnauralVars.Volume_Noiz,   //volume_left
                                  OldGnauralVars.Volume_Noiz,   //volume_right
                                  0,    //basefreq
                                  SG_UNSELECTED);       //state
 SG_FirstVoice->NextVoice->FirstDataPoint = curDP;
 //==END load Noise voice:
 //==END LOADING THE FLOATS IN TO THE GRAPH:
 main_SetLoops (OldGnauralVars.loops);
 //  SG_DBGOUT("Got to line 1934");
 free (tmpfloat);
}       //end of extractfloats()

//======================================
//THIS IS INCLUDED SOLEY TO OPEN OLD STYLE GNAURAL SCHEDULES
//opens whatever is passed as filename, dumps it in to str,
//then uses SchedBuffToSchedule to fill Schedule. has to go like this
//because it has to segregate all "command strings" [] from schedule entries
int main_GNAURAL1FILE_SchedFilenameToSchedule (char *filename)
{
 FILE *stream;

 char *str;

 if ((stream = fopen (filename, "r")) == NULL)
 {      //didn't find a schedule file, so return (could try to create one at this point with WriteScheduleToFile()
  return 0;
 }
 //FileFlag = true;

 //START count file elements to know how big to make the char buffer:
 char tmpchar;

 unsigned int i = 0;

 while (feof (stream) == 0)
 {
  tmpchar = fgetc (stream);
  //comments: eat entire lines starting with #:
  if (tmpchar == '#')
  {
   while (feof (stream) == 0 && tmpchar != '\n')
    tmpchar = fgetc (stream);
  }

  //  if (tmpchar>='0' && tmpchar <='9') i++;
  //  else if (tmpchar==',' || tmpchar=='.') i++;

  if ((tmpchar >= '0'
       && tmpchar <= '9')
      || tmpchar == ',' || tmpchar == '.' || tmpchar == '-')
  {
   i++;
  }
 }      //end while
 //END count file elements

 /*
    //20070116 DECIDED NOT TO USE THIS METHOD:
    //Now init SG so that it can start to receive the schedule.
    //Since this is an old Gnaural file, there must be only two voices, first being
    //the assumed noise, the second being the explicitly directed binaural beat. The
    //philosophy of Gnaural file opening is to let the Graph (SG) always lead the BB
    //engine; and in loading the graph, it is customary to load the SG_UndoRedo buffer
    //with the file data, because when using SG_RestoreDataPoints() you only need:
    // 1) SG_UndoRedo.count field filled (total count of ALL datapoints)
    // 2) SG_UndoRedo.VoiceType allocated (one for each voice) and filled
    // 3) SG_UndoRedo.DPdata allotted and these DPdata struct fields filled:
    //  -  duration
    //  -  beatfreq
    //  -  basefreq
    //  -  volume_left
    //  -  volume_right
    //  -  state
    //  -  parent
  */

 //preparing the holding vars for command line parsing:
 OldGnauralVars.FreqBase = 220;
 OldGnauralVars.Volume_Tone = .5;
 OldGnauralVars.Volume_Noiz = .5;
 OldGnauralVars.loops = 1;
 OldGnauralVars.loopcount = 1;
 //START allocate char buffer, rewind file, and do it again this time putting elements in buffer:
 str = (char *) malloc ((++i));
 rewind (stream);
 i = 0;
 while (feof (stream) == 0)
 {
  tmpchar = fgetc (stream);
  //deal with comments:
  if (tmpchar == '#')
  {     //throw whole line away:
   while (feof (stream) == 0 && tmpchar != '\n')
    tmpchar = fgetc (stream);
  }

  //deal with command strings:
  else if (tmpchar == '[')
  {
   main_GNAURAL1FILE_ParseCmd (stream);
  }

  //if it is a number, add it to the monster string:
  else
   if ((tmpchar >= '0'
        && tmpchar <= '9')
       || tmpchar == ',' || tmpchar == '.' || tmpchar == '-')
  {
   str[i] = tmpchar;
   i++;
  }
 }
 //END allocate char buffer, rewind file, and do it again this time putting elements in buffer:

 fclose (stream);
 //put end on the string!!
 str[i] = '\0';
 main_GNAURAL1FILE_SchedBuffToSchedule (str);
 free (str);
 return 1;
}

//====END OF IMPORT OLD GNAURAL FILE SECTION====

//======================================
//I bet I could do this without actually writing to disk, but
//have wrestled with so many bizarrely non-functional GTK
//themed/builtin/window/named icon functions that
//after a full day of failing, I must simply do something
//I understand (see icon_problems.txt):
void main_SetIcon ()
{
#ifdef GNAURAL_WIN32
 // Get the temp path.
 if (0 != GetTempPath (sizeof (main_sPathTemp), main_sPathTemp))        //NOTE: returned string ends in backslash
 {      //success
  strcat (main_sPathTemp, "gnaural-icon.png");
 }
 else
 {      //fail
  sprintf (main_sPathTemp, "gnaural-icon.png");
 }
#endif

#ifndef GNAURAL_WIN32
 sprintf (main_sPathTemp, "%s/%s", main_sPathGnaural, "gnaural-icon.png");
#endif
 FILE *stream;

 if ((stream = fopen (main_sPathTemp, "wb")) == NULL)
 {
  return;
 }
 fwrite (&main_GnauralIcon, sizeof (main_GnauralIcon), 1, stream);
 fclose (stream);
 gtk_window_set_default_icon_from_file (main_sPathTemp, NULL);
}

//======================================
void main_InterceptCtrl_C (int sig)
{
 SG_DBGOUT ("Caught Ctrl-C");
 /*
    if (gnaural_guiflag == GNAURAL_GUIMODE) {
    //20060328: I used to just call gtk_main_quit() here, but it
    //is smarter to let the callback functions associated with window1
    //do their own cleanup:
    gtk_widget_destroy( window1 );
    //   ScheduleGUI_delete_event
    DBGOUT ("Quit GUI mode");
    }
  */
 main_Cleanup ();
 exit (EXIT_SUCCESS);
}

//======================================
void main_ParseCmdLine (int argc, char *argv[])
{
 opterr = 0;    //this means I want to check the ? character myself for errors
 int c;

 //NOTE: options with ":" after them require arguments
 while ((c = getopt (argc, argv, GNAURAL_CMDLINEOPTS)) != -1)
  switch (c)
  {
  case 'h':    //print help
   fprintf (stdout, "Gnaural (ver. %s) - ", VERSION);
   fputs (main_GnauralHelp, stdout);
   exit (EXIT_SUCCESS); //completely quit if the user asked for help.
   break;
  case 'i':    //show terminal-style gui info
   ++cmdline_i;
   break;
  case 'p':    //output directly to sound system
   ++cmdline_p;
   break;
  case 'd':    //output debugging info to stderr
   ++cmdline_d;
   SG_DebugFlag = TRUE;
   BB_DebugFlag = TRUE;
   GN_DebugFlag = TRUE;
   //may go back in to GUI mode, can't know from this
   break;
  case 'o':    //output a .WAV stream to stdout
   ++cmdline_o;
   break;
  case 's':    //create default gnaural_schedule.txt file
   ++cmdline_s;
   if (0 < cmdline_w)
   {
    SG_ERROUT ("Ignoring -w; -s has priority");
    cmdline_w = 0;      //just in case I missed something...
   }
   break;
  case 'a':
   ++cmdline_a;
   playAudio_SoundDevice = atoi (optarg);
   //may go back in to GUI mode, can't know from this
   break;
  case 'w':    //output a .WAV stream to a user specified .wav file
   if (cmdline_s > 0)
   {    //CHANGE 051110: you don't want -s and -w in the same line
    SG_ERROUT ("Ignoring -w; -s has priority");
    cmdline_w = 0;      //just in case I missed something...
    break;
   }
   ++cmdline_w;
   //20071201 Following could probably be done better now:
   SG_StringAllocateAndCopy (&main_AudioWriteFile_name, optarg);
   break;
  case ':':    //some option was missing its argument
   fprintf (stderr, "Missing argument for `-%c'.\n", optopt);
   fputs (main_GnauralHelpShort, stderr);
   exit (EXIT_FAILURE); //completely quit on error
  case '?':
   fprintf (stderr, "Unknown option `-%c'.\n", optopt);
   fputs (main_GnauralHelpShort, stderr);
   exit (EXIT_FAILURE); //completely quit on error
  default:
   fputs (main_GnauralHelpShort, stderr);
   exit (EXIT_FAILURE); //completely quit on error
   break;
  }

 //now see if there was any command-line stuff user inputted that didn't
 //get caught above.
 //a20070705: The last thing to get here is considered a filename
 int index;

 for (index = optind; index < argc; index++)
 {
  //[20100405: added check to avoid compiler complaints]:
  sprintf (main_sCurrentGnauralFilenameAndPath, "%s", argv[index]);
  //  fprintf (stderr, "Error: non-option argument \"%s\"\n", argv[index]);
 }
 //END command line parsing
 return;
}

//======================================
//this is basically just the command line version dropped in as a function-call in
//to the GUI version.
void main_RunCmdLineMode ()
{
 //don't need GUI, so turn pause off:
 gnaural_pauseflag = 0;
 //first do the things that don't require sound at all:
 //- cmdline_s: create a fresh gnaural_schedule.txt file then exits DOES NOT REQUIRE SOUND SYSTEM
 //- cmdline_o: dump a .WAV file to sdtout DOES NOT REQUIRE SOUND SYSTEM
 //- cmdline_w: dump a .WAV file to a file DOES NOT REQUIRE SOUND SYSTEM
 // user asked to create a new default Schedule file;  do that then exit:
 if (cmdline_s > 0)
 {
  //  bb->WriteDefaultScheduleToFile ();
  SG_SetupDefaultDataPoints (2);
  gxml_XMLWriteFile ("schedule.gnaural");
  SG_DBGOUT ("Wrote default gnaural_schedule.txt file");
  return;
 }

 ////////////////
 //NOTE:
 //from this point forward, I work on the premise that user may be piping
 //output to stdout to another program expecting a WAV file
 ////////////////

 //these next three are mutually exclusive, and only cmdline_p needs sound system:
 if (cmdline_w != 0)
 {
  //old direct way of doing this:
  //      fprintf (stderr, "Writing schedule %s to file %s... ",
  //        bb->SchedFilename, w_str);
  //      bb->WriteWAVFile (w_str);
  //      fprintf (stderr, "Done.\n");
  //      return (EXIT_SUCCESS);

  //threaded way of doing the above:
  if (cmdline_i != 0)
  {
   fputs ("\033[2J", stderr);   //clear entire screen
   fputs ("\033[14;1H", stderr);        //place cursor
  }

  fprintf (stderr,
           "Writing WAV data to file %s... \n", main_AudioWriteFile_name);
  //new gthread code (new 20051126):
  GError *thread_err = NULL;

  if ((main_AudioWriteFile_thread =
       g_thread_create ((GThreadFunc) main_AudioWriteFile, (void *) NULL,
                        TRUE, &thread_err)) == NULL)
  {
   fprintf (stderr, "g_thread_create failed: %s!!\n", thread_err->message);
   g_error_free (thread_err);
  }

  while
   (main_AudioWriteFile_thread
    != NULL && (((BB_InfoFlag) & BB_COMPLETED) == 0))
  {
   if (cmdline_i != 0)
   {
    main_UpdateTerminalGUI (stdout);
    fflush (stdout);    //must flush often when using stdout
   }
   main_Sleep (G_USEC_PER_SEC);
  }
  if (cmdline_i != 0)
   fputs ("\033[14;1H", stderr);        //place cursor
  return;
 }

 //----------

 else if (cmdline_o != 0)
 {
  BB_WriteWAVToStream (stdout);
  return;
 }

 //----------

 else if (cmdline_p != 0)
 {      //write to sound system:
  //First, set up the sound system, or exit if not possible:
  //first set-up sound, since program may not return from parsing command line
  if (EXIT_FAILURE == main_playAudio_SoundInit ())
  {
   SG_DBGOUT ("Couldn't intialize the sound system, exiting");
   Pa_Terminate ();
   playAudio_SoundStream = NULL;        //this is how the rest of the program knows if we're using sound or not.
   return;
  }

  SG_DBGOUT ("Writing to sound system");
  if (cmdline_i != 0)
  {
   fputs ("\033[2J", stderr);   //clear entire screen
   fputs ("\033[14;1H", stderr);        //place cursor
  }

  playAudio_SoundStart ();
  while (playAudio_SoundStream != NULL
         && (((BB_InfoFlag) & BB_COMPLETED) == 0))
  {
   if (cmdline_i != 0)
   {
    main_UpdateTerminalGUI (stdout);
    fflush (stdout);    //must flush often when using stdout
   }
   Pa_Sleep (1000);
  };
  if (cmdline_i != 0)
  {
   fputs ("\033[14;1H", stderr);        //place cursor
  }
  return;
 }
}

//======================================
//updates a terminal window via either stdout or stderr streams
void main_UpdateTerminalGUI (FILE * gstream)
{
 //NOTE THIS IS CURRENTLY MOSTLY UNIMPLEMENTED; a lot of decisions
 //will need to be made how to present multi voiced data without a GUI
 // current time point within schedule:

 main_FormatProgressString ();
 fputs ("\033[2;1H", gstream);  //place cursor
 fprintf (gstream, "%s", main_sTmp);
 fputs ("\033[K", gstream);     //clear to end of line
 /*
    //-----------------------------------------------------------
    //START stuff that gets updated EVERY second:
    //
    // current time point within schedule:
    fputs("\033[2;1H", gstream); //place cursor
    fprintf (gstream, "Current Progress: \t%d min. \t%d sec.",
    (BB_CurrentSampleCount + BB_CurrentSampleCountLooped) / 6000,
    ((BB_CurrentSampleCount + BB_CurrentSampleCountLooped) / 100) % 60);
    fputs("\033[K", gstream); //clear to end of line

    //remaining time in entry:
    fputs("\033[5;1H", gstream); //place cursor
    fprintf (gstream, "Time left: \t\t%d sec.",
    ((bb->Schedule[bb->ScheduleCount].AbsoluteEndTime_100 - BB_CurrentSampleCount) / 100));
    fputs("\033[K", gstream); //clear to end of line

    //Left frequency:
    fputs("\033[9;1H", gstream); //place cursor
    fprintf (gstream, "Current Freq. Left: \t%g\n",
    bb->FreqL);
    fputs("\033[K", gstream); //clear to end of line

    //Right frequency:
    // fputs("\033[15;1H",gstream);//place cursor
    fprintf (gstream, "Current Freq. Right: \t%g\n",
    bb->FreqR);
    fputs("\033[K", gstream); //clear to end of line

    //Beat frequency:
    // fputs("\033[16;1H",gstream);//place cursor
    fprintf (gstream, "Current Beat Freq.: \t%g\n",
    bb->FreqL - bb->FreqR);
    fputs("\033[K", gstream); //clear to end of line

    //
    //END stuff that gets updated EVERY second:
    //-----------------------------------------------------------------------

    //-----------------------------------------------------------------------
    //START check bb->InfoFlag
    //now update things that BinauralBeat says are changed:
    if (bb->InfoFlag != 0) {

    //.........................
    //if schedule is done:
    if (((bb->InfoFlag) & BB_COMPLETED) != 0) {
    fputs("\033[7;1H", gstream); //place cursor
    fprintf (gstream, "Schedule Completed");
    fputs("\033[K", gstream); //clear to end of line
    }

    //.........................
    //if starting a new loop:
    if (((bb->InfoFlag) & BB_NEWLOOP) != 0) {
    //reset the "new loop" bit of InfoFlag:
    bb->InfoFlag &= ~BB_NEWLOOP;
    //bbl_UpdateGUI_LoopInfo ();
    }

    //.........................
    //START things that get updated every new entry:
    //if new entry
    if (((bb->InfoFlag) & BB_NEWENTRY) != 0) {
    //reset the "new entry" bit of InfoFlag:
    (bb->InfoFlag) &= (~BB_NEWENTRY);

    //current entry number:
    fputs("\033[3;1H", gstream); //place cursor
    fprintf (gstream, "Current Entry: \t\t%d of %d",
    bb->ScheduleCount + 1, bb->ScheduleEntriesCount);
    fputs("\033[K", gstream); //clear to end of line

    //total time in entry:
    fputs("\033[4;1H", gstream); //place cursor
    fprintf (gstream, "Duration: \t\t%g sec.",
    bb->Schedule[bb->ScheduleCount].Duration);
    fputs("\033[K", gstream); //clear to end of line

    //fprintf(gstream,"%d",bb->Schedule[bb->ScheduleCount].AbsoluteEndTime_100/100);
    //SetDlgItemText(lEndCount, gstream);

    fputs("\033[6;1H", gstream); //place cursor
    fprintf (gstream, "Beat Range: \t\t%g to %g Hz\n",
    bb->Schedule[bb->ScheduleCount].FreqLStart -
    bb->Schedule[bb->ScheduleCount].FreqRStart,
    bb->Schedule[bb->ScheduleCount].FreqLEnd -
    bb->Schedule[bb->ScheduleCount].FreqREnd);
    fputs("\033[K", gstream); //clear to end of line

    //left
    // fputs("\033[7;1H",gstream);//place cursor
    fprintf (gstream, "Left ear: \tStart:\t%g",
    bb->Schedule[bb->ScheduleCount].FreqLStart);
    fputs("\033[K", gstream); //clear to end of line
    fprintf (gstream, "\tEnd:\t%g\n",
    bb->Schedule[bb->ScheduleCount].FreqLEnd);
    fputs("\033[K", gstream); //clear to end of line

    //right
    // fputs("\033[8;1H",gstream);//place cursor
    fprintf (gstream, "Right ear: \tStart:\t%g",
    bb->Schedule[bb->ScheduleCount].FreqRStart);
    fputs("\033[K", gstream); //clear to end of line
    fprintf (gstream, "\tEnd:\t%g",
    bb->Schedule[bb->ScheduleCount].FreqREnd);
    fputs("\033[K", gstream); //clear to end of line

    //this is overdoing it, but need it updated sometime:

    if (bb->loops > 0) {
    fputs("\033[1;1H", gstream); //place cursor
    fprintf (gstream, "Projected Runtime: \t%d min. \t%d sec.",
    (int) (bb->loops * bb->ScheduleTime) / 60,
    (int) (bb->loops * bb->ScheduleTime) % 60);
    fputs("\033[K", gstream); //clear to end of line
    } else {
    fputs("\033[1;1H", gstream); //place cursor
    fprintf (gstream, "Projected Runtime:  FOREVER (Inf. Loop Mode)");
    fputs("\033[K", gstream); //clear to end of line
    }

    }
    //END things that get updated every new entry
    }
  */
}

//======================================
void main_WriteDefaultFile ()
{
 //20070705: Used to write to whatever was in main_sCurrentOpenGnauralFile,
 //but now sets-up default names and paths first:
 main_sCurrentGnauralFilenameAndPath[0] = '\0'; //not necessary, I guess
 main_SetupPathsAndFiles (TRUE);
 //prepare file access:
 FILE *stream;

 if ((stream = fopen (main_sCurrentGnauralFilenameAndPath, "w")) == NULL)
 {
  SG_ERROUT ("Failed to open file for writing!");
  return;
 }
 fprintf (stream, "%s", main_DefaultSchedule);
 fclose (stream);
}

/////////////////////////////////////////////////////
//20100610
void main_VoiceProperties_checkbuttonMono (GtkWidget * widget, gpointer Voice)
{
 SG_Voice *curVoice = (SG_Voice *) Voice;
 if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
 {
  curVoice->mono = TRUE;        //user wants voice mono
 }
 else
 {
  curVoice->mono = FALSE;       //user wants voice mono
 }
}

/////////////////////////////////////////////////////
//20070627: This edits properties of an existing
//voices, OR creates a new voice if curVoice = NULL
gboolean main_VoicePropertiesDialog (GtkWidget * widget, SG_Voice * curVoice)
{
 GtkWidget *dialog = gtk_dialog_new_with_buttons ("Voice Properties", NULL,
                                                  (GtkDialogFlags)
                                                  (GTK_DIALOG_MODAL |
                                                   GTK_DIALOG_DESTROY_WITH_PARENT),
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_OK,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_CANCEL,
                                                  "Choose Audio File",
                                                  GTK_RESPONSE_APPLY,
                                                  "Delete Voice",
                                                  GTK_RESPONSE_NO, NULL);

 //add some stuff:
 GtkWidget *label_VoiceType = gtk_label_new ("Voice Type:");
 GtkWidget *comboboxentry1 = gtk_combo_box_entry_new_text ();
 GtkWidget *checkbuttonMono = gtk_check_button_new_with_label ("Monophonic");
 gtk_widget_show (comboboxentry1);
 //e20070621: List order: don't change; depends on #define values in BinauralBeat.h:
 gtk_combo_box_append_text (GTK_COMBO_BOX (comboboxentry1),
                            main_sBinauralBeat);
 gtk_combo_box_append_text (GTK_COMBO_BOX (comboboxentry1), main_sPinkNoise);
 gtk_combo_box_append_text (GTK_COMBO_BOX (comboboxentry1), main_sAudioFile);
 gtk_combo_box_append_text (GTK_COMBO_BOX (comboboxentry1),
                            main_sIsochronicPulses);
 gtk_combo_box_append_text (GTK_COMBO_BOX (comboboxentry1),
                            main_sIsochronicPulses_alt);
 gtk_combo_box_append_text (GTK_COMBO_BOX (comboboxentry1), main_sWaterDrops);
 gtk_combo_box_append_text (GTK_COMBO_BOX (comboboxentry1), main_sRain);

 GtkWidget *label_VoiceDescrip =
  gtk_label_new ("Voice Description [or Filename if Audio File]:");
 GtkWidget *entry_Input_VoiceDesrip = gtk_entry_new ();

 //put stuff in here relevant to a pre-existing voice's state (as 
 //opposed to a request for new voice):
 if (curVoice != NULL)
 {
  gtk_entry_set_text ((GtkEntry
                       *) entry_Input_VoiceDesrip, curVoice->description);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbuttonMono),
                                curVoice->mono);
  g_signal_connect (checkbuttonMono, "clicked",
                    G_CALLBACK (main_VoiceProperties_checkbuttonMono),
                    (gpointer) curVoice);
 }

 gtk_combo_box_set_active ((GtkComboBox *) comboboxentry1,
                           (curVoice != NULL) ? curVoice->type : 0);
 // Add the label, and show everything we've added to the dialog.
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label_VoiceType);
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), comboboxentry1);
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label_VoiceDescrip);
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), entry_Input_VoiceDesrip);
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), checkbuttonMono);
 gtk_widget_show_all (dialog);
 gint res = FALSE;

 do
 {
  //block until I get a response:
  res = gtk_dialog_run (GTK_DIALOG (dialog));
  switch (res)
  {
  case GTK_RESPONSE_OK:
   {
    if (curVoice != NULL)
    {
     curVoice->type =
      gtk_combo_box_get_active ((GtkComboBox *) comboboxentry1);
    }
    else
    {   //it is a brand new voice:
     curVoice =
      SG_AddNewVoiceToEnd (gtk_combo_box_get_active
                           ((GtkComboBox *) comboboxentry1), 0);

     //start 110520
     //purpose: to give sensible defaults for new voices because
     //always must manually add a first data point to the end of 
     //a new voice:
     switch (curVoice->type)
     {

     case BB_VOICETYPE_BINAURALBEAT:
     case BB_VOICETYPE_ISOPULSE:
     case BB_VOICETYPE_ISOPULSE_ALT:
      SG_AddNewDataPointToEnd (curVoice->FirstDataPoint, curVoice, SG_TotalScheduleDuration,    //duration
                               4,       //beatfreq
                               .5,      //volume
                               .5,      //volume
                               180,     //basefreq
                               SG_SELECTED);
      break;

     case BB_VOICETYPE_PINKNOISE:
      SG_AddNewDataPointToEnd (curVoice->FirstDataPoint, curVoice, SG_TotalScheduleDuration,    //duration
                               0,       //beatfreq
                               .1,      //volume
                               .1,      //volume
                               0,       //basefreq
                               SG_SELECTED);
      break;

     case BB_VOICETYPE_PCM:
      SG_AddNewDataPointToEnd (curVoice->FirstDataPoint, curVoice, SG_TotalScheduleDuration,    //duration
                               0,       //beatfreq
                               .5,      //volume
                               .5,      //volume
                               0,       //basefreq
                               SG_SELECTED);
      break;

     case BB_VOICETYPE_WATERDROPS:
      SG_AddNewDataPointToEnd (curVoice->FirstDataPoint, curVoice, SG_TotalScheduleDuration,    //duration
                               2,       //beatfreq (number of drops)
                               .5,      //volume
                               .5,      //volume
                               0.000352858,     //basefreq (probability)
                               SG_SELECTED);
      break;

     case BB_VOICETYPE_RAIN:
      SG_AddNewDataPointToEnd (curVoice->FirstDataPoint, curVoice, SG_TotalScheduleDuration,    //duration
                               8,       //beatfreq
                               .5,      //volume
                               .5,      //volume
                               .1,      //basefreq
                               SG_SELECTED);
      break;

     default:
      SG_AddNewDataPointToEnd (curVoice->FirstDataPoint, curVoice, SG_TotalScheduleDuration,    //duration
                               0,       //beatfreq
                               .5,      //volume
                               .5,      //volume
                               0,       //basefreq
                               SG_SELECTED);
      break;
     }

     //end 110520
    }

    SG_StringAllocateAndCopy (&
                              (curVoice->description),
                              (char
                               *)
                              gtk_entry_get_text
                              (GTK_ENTRY (entry_Input_VoiceDesrip)));
    SG_SelectVoice (curVoice);
    //   SG_DeselectDataPoints ();
    //   SG_SelectDataPoints_Voice (curVoice, TRUE);
    //   SG_ConvertDataToXY (widget);
    SG_DrawGraph (widget);
    //  VG_DestroyTreeView ();//20100625
    main_UpdateGUI_Voices (main_vboxVoices);
    SG_GraphHasChanged = SG_DataHasChanged = TRUE;      //datahaschanged because voicetype may have changed locally
   }
   break;
   //pick audio file:
  case GTK_RESPONSE_APPLY:
   {
    //old way:
    //gchar *tmpfilename = 
    //main_OpenFileDialog ("~Gnaural Audio Files~*.wav,*.aiff,*.au,*.flac,~All Files~*");
    //new way:
    main_MakeAudioFormatsFileFilterString (main_sTmp, sizeof (main_sTmp));
    gchar *tmpfilename = main_OpenFileDialog (main_sTmp, main_sPathGnaural);

    if (NULL != tmpfilename)
    {
     gtk_entry_set_text ((GtkEntry *) entry_Input_VoiceDesrip, tmpfilename);
    }
    g_free (tmpfilename);
   }
   break;
   //delete voice:
  case GTK_RESPONSE_NO:
   main_DeleteSelectedVoice (widget);
   break;
  default:
   res = FALSE;
   break;
  }
 }
 while (res == GTK_RESPONSE_APPLY);
 gtk_widget_destroy (dialog);
 return res;
}

/////////////////////////////////////////////////////
void main_on_revert_activate (GtkMenuItem * menuitem, gpointer user_data)
{
 if (0 != main_TestPathOrFileExistence (main_sCurrentGnauralFilenameAndPath))
 {
  SG_ERROUT ("Can't revert to a file that doesn't exist.");
  return;
 }
 gxml_XMLReadFile
  (main_sCurrentGnauralFilenameAndPath, main_drawing_area, FALSE);
}

/////////////////////////////////////////////////////
//erases all voices and leaves user with one voice with one DP
void main_NewGraph (int voices)
{
 if (FALSE == main_SetScheduleInfo (FALSE))
 {
  return;
 }
 SG_SetupDefaultDataPoints (voices);
 main_SetLoops (1);
 //SG_DrawGraph (main_drawing_area);
 SG_GraphHasChanged = SG_DataHasChanged = TRUE; //20070105 tricky way to do main_LoadBinauralBeatSoundEngine in a bulk way
 sprintf (main_sTmp, "%s/untitled.gnaural", main_sPathGnaural);
 main_UpdateGUI_FileInfo (main_sTmp);
 VG_DestroyTreeView (); //20100624
 main_UpdateGUI_Voices (main_vboxVoices);
 //this is really just a way for user to free up big chunks of memory if they had lots of AFs open:
 main_CleanupAudioFileData ();
}

/////////////////////////////////////////////////////
void main_UpdateGUI_FileInfo (char *filename)
{
 strcpy (main_sCurrentGnauralFilenameAndPath, filename);
 sprintf (main_sTmp, "File: %s", main_sCurrentGnauralFilenameAndPath);
 gtk_label_set_text (main_labelFileName, main_sTmp);
 sprintf (main_sTmp, "Author: %s", main_Info_Author);
 gtk_label_set_text (main_labelFileAuthor, main_sTmp);
 sprintf (main_sTmp, "Description: %s", main_Info_Description);
 gtk_label_set_text (main_labelFileDescription, main_sTmp);
 sprintf (main_sTmp, "Title: %s", main_Info_Title);
 gtk_label_set_text (main_labelFileTitle, main_sTmp);
}

/////////////////////////////////////////////////////
//clears all but first DP:
void main_ClearDataPointsInVoice (SG_Voice * curVoice)
{
 if (curVoice == NULL)
 {
  return;
 }
 SG_DeselectDataPoints ();
 SG_SelectDataPoints_Voice (curVoice, TRUE);
 curVoice->FirstDataPoint->state = SG_UNSELECTED;
 SG_DeleteDataPoints (main_drawing_area, TRUE, TRUE);
}

/////////////////////////////////////////////////////
//clears all but first DP in ALL voices:
void main_ClearDataPointsInVoices ()
{
 SG_BackupDataPoints (main_drawing_area);
 SG_Voice *curVoice = SG_FirstVoice;

 while (curVoice != NULL)
 {
  main_ClearDataPointsInVoice (curVoice);
  curVoice = curVoice->NextVoice;
 }
}

///////////////////////////////////////////
//returns TRUE if user clicked OK, filling val with result
double main_AskUserForNumberDialog
 (char *title, char *question, double *startingval)
{
 GtkWidget *dialog = gtk_dialog_new_with_buttons (title,
                                                  (GtkWindow *) main_window,
                                                  (GtkDialogFlags)
                                                  (GTK_DIALOG_MODAL |
                                                   GTK_DIALOG_DESTROY_WITH_PARENT),
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT, NULL);

 gboolean res = FALSE;

 //add some stuff:
 GtkWidget *label = gtk_label_new (question);

 GtkWidget *entry_Input = gtk_entry_new ();

 sprintf (main_sTmp, "%g", *startingval);
 gtk_entry_set_text (GTK_ENTRY (entry_Input), main_sTmp);
 // Add the label, and show everything we've added to the dialog.
 gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label);
 gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), entry_Input);
 gtk_widget_show_all (dialog);
 //block until I get a response:
 gint result = gtk_dialog_run (GTK_DIALOG (dialog));

 switch (result)
 {
 case GTK_RESPONSE_ACCEPT:
  {
   *startingval = atof (gtk_entry_get_text (GTK_ENTRY (entry_Input)));
   res = TRUE;
  }
  break;
 default:
  break;
 }
 gtk_widget_destroy (dialog);
 return res;
}

///////////////////////////////////////////
gboolean main_SetScheduleInfo (gboolean FillEntriesFlag)
{
 gboolean ok = FALSE;

 GtkWidget *dialog = gtk_dialog_new_with_buttons ("Edit Schedule Info",
                                                  (GtkWindow *) main_window,
                                                  (GtkDialogFlags)
                                                  (GTK_DIALOG_MODAL |
                                                   GTK_DIALOG_DESTROY_WITH_PARENT),
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT, NULL);

 //add some stuff:
 GtkWidget *label_title = gtk_label_new ("Title:");

 GtkWidget *entry_Input_title = gtk_entry_new ();

 if (TRUE == FillEntriesFlag)
 {
  gtk_entry_set_text (GTK_ENTRY (entry_Input_title), main_Info_Title);
 }
 GtkWidget *label_description = gtk_label_new ("Description");

 GtkWidget *entry_Input_description = gtk_entry_new ();

 if (TRUE == FillEntriesFlag)
 {
  gtk_entry_set_text (GTK_ENTRY
                      (entry_Input_description), main_Info_Description);
 }
 GtkWidget *label_author = gtk_label_new ("Author:");

 GtkWidget *entry_Input_author = gtk_entry_new ();

 if (TRUE == FillEntriesFlag)
 {
  gtk_entry_set_text (GTK_ENTRY (entry_Input_author), main_Info_Author);
 }
 // Add the label, and show everything we've added to the dialog.
 gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label_title);
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), entry_Input_title);
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label_description);
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), entry_Input_description);
 gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label_author);
 gtk_container_add
  (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), entry_Input_author);
 gtk_widget_show_all (dialog);
 //block until I get a response:
 gint result = gtk_dialog_run (GTK_DIALOG (dialog));

 switch (result)
 {
 case GTK_RESPONSE_ACCEPT:
  {
   ok = TRUE;
   SG_StringAllocateAndCopy
    (&main_Info_Title, gtk_entry_get_text (GTK_ENTRY (entry_Input_title)));
   SG_StringAllocateAndCopy
    (&main_Info_Description,
     gtk_entry_get_text (GTK_ENTRY (entry_Input_description)));
   SG_StringAllocateAndCopy
    (&main_Info_Author, gtk_entry_get_text (GTK_ENTRY (entry_Input_author)));
   main_UpdateGUI_FileInfo (main_sCurrentGnauralFilenameAndPath);
  }
  break;
 default:
  break;
 }
 gtk_widget_destroy (dialog);
 return ok;
}

///////////////////////////////////
void main_ReverseVoice ()
{
 SG_BackupDataPoints (main_drawing_area);
 SG_ReverseVoice (main_drawing_area);
}

///////////////////////////////////
void main_DuplicateSelectedVoice ()
{
 SG_BackupDataPoints (main_drawing_area);
 SG_DuplicateSelectedVoice ();
 //VG_DestroyTreeView ();
 main_UpdateGUI_Voices (main_vboxVoices);
}

///////////////////////////////////
void main_ScaleDataPoints_Time (GtkWidget * widget)
{
 static double scalar = 1.0;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Time Scale",
      "Number to scale duration of selected points by:", &scalar))
 {
  SG_BackupDataPoints (widget);
  SG_ScaleDataPoints_Time (widget, scalar);
 }
}

///////////////////////////////////
void main_DeleteSelectedVoice (GtkWidget * widget)
{
 //first check if there was a legally selected voice:  
 SG_Voice *curVoice = SG_SelectVoice (NULL);

 if (curVoice != NULL)
 {
  SG_BackupDataPoints (widget);
  SG_DeleteVoice (curVoice);    //NOTE: Might want to base deletion on SG_Voice ID instead...
  SG_SelectVoice (SG_FirstVoice);
  SG_DrawGraph (widget);
  SG_GraphHasChanged = TRUE;
  //VG_DestroyTreeView ();
  main_UpdateGUI_Voices (main_vboxVoices);
 }
}

///////////////////////////////////
void main_ScaleDatPoints_Y (GtkWidget * widget)
{
 static double scalar = 1.0;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Scale Y", "Number to scale Y values of selected points by:", &scalar))
 {
  SG_BackupDataPoints (widget);
  SG_ScaleDataPoints_Y (widget, scalar);
 }
}

///////////////////////////////////
void main_AddRandomToDataPoints_Y (GtkWidget * widget)
{
 static double val = 0;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Add Randomness to Y",
      "+/- range of random number to add to selected points:", &val))
 {
  SG_BackupDataPoints (widget);
  SG_AddToDataPoints (widget, val, FALSE, TRUE, TRUE);
 }
}

///////////////////////////////////
void main_AddRandomToDataPoints_time (GtkWidget * widget)
{
 static double val = 0;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Add Randomness to Durations",
      "+/- range of seconds to add to selected points:", &val))
 {
  SG_BackupDataPoints (widget);
  SG_AddToDataPoints (widget, val, TRUE, FALSE, TRUE);
 }
}

///////////////////////////////////
void main_AddToDataPoints_Y (GtkWidget * widget)
{
 static double val = 0;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Add to Y",
      "Number of pixels to add to Y axis of selected points:", &val))
 {
  SG_BackupDataPoints (widget);
  SG_AddToDataPoints (widget, val, FALSE, TRUE, FALSE);
 }
}

///////////////////////////////////
void main_AddToDataPoints_time (GtkWidget * widget)
{
 static double val = 0;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Add to Durations",
      "Value in seconds to add to selected points:", &val))
 {
  SG_BackupDataPoints (widget);
  SG_AddToDataPoints (widget, val, TRUE, FALSE, FALSE);
 }
}

///////////////////////////////////
void main_SelectInterval ()
{
 static double val = 2;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Select Interval", "Select at this interval:", &val))
 {
  SG_BackupDataPoints (main_drawing_area);
  SG_SelectIntervalDataPoints_All ((int) val, TRUE, FALSE);
 }
}

///////////////////////////////////
void main_SelectNeighbor ()
{       //Select neighbor to right:
 static double val = 0;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Select Next Datapoints",
      "Deselect currently selected? (1=yes, 0=no):", &val))
 {
  SG_BackupDataPoints (main_drawing_area);
  SG_SelectNeighboringDataPoints (TRUE, ((val == 1) ? TRUE : FALSE));
 }
}

///////////////////////////////////
void main_OpenFile (gboolean OpenMerge, char *path)
{       //Open an XML data file:
 if (NULL == path)
 {
  path = main_sPathGnaural;
 }
 gchar *tmpfilename = main_OpenFileDialog (GNAURAL_FILEFILTERSTRING, path);

 if (tmpfilename == NULL)
 {
  SG_DBGOUT ("Not opening a file.");
  return;
 }
 gxml_XMLReadFile (tmpfilename, main_drawing_area, OpenMerge);
 g_free (tmpfilename);
}

///////////////////////////////////
void main_OpenFromLibrary ()
{
 main_OpenFile (FALSE, GNAURAL_PRESET_DIR);
}

///////////////////////////////////
void main_SelectLastDPs ()
{
 SG_BackupDataPoints (main_drawing_area);
 SG_SelectLastDP_All ();
}

///////////////////////////////////
void main_SelectFirstDPs ()
{
 SG_BackupDataPoints (main_drawing_area);
 SG_SelectFirstDP_All ();
}

///////////////////////////////////
void main_TruncateSchedule ()
{
 static double val = 60;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Truncate (or Grow) Schedule",
      "Schedule End Time (sec.):", &val) && val >= 0)
 {
  SG_BackupDataPoints (main_drawing_area);
  BB_ResetAllVoices ();
  SG_TruncateSchedule (main_drawing_area, val);
 }
}

///////////////////////////////////
void main_PasteAtEnd ()
{
 SG_BackupDataPoints (main_drawing_area);
 SG_PasteDataPointsAtEnd (main_drawing_area);
}

///////////////////////////////////
void main_InvertY ()
{
 SG_BackupDataPoints (main_drawing_area);
 SG_InvertY (main_drawing_area);
}

///////////////////////////////////
void main_SelectDuration ()
{
 static double val = 1;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Select by Duration (sec.)",
      "Selection Threshold (positive for above, negative for below):", &val))
 {
  SG_BackupDataPoints (main_drawing_area);
  SG_SelectDuration (main_drawing_area, val);
 }
}

///////////////////////////////////
void main_SelectProximity_All ()
{
 static double val = 1;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Select by X,Y Proximity (pixels)",
      "Proximity Threshold:", &val) && val >= 0)
 {
  SG_BackupDataPoints (main_drawing_area);
  SG_SelectProximity_All (val);
  //  SG_SelectProximity_SingleDP(SG_FirstVoice->FirstDataPoint, val);
 }
}

///////////////////////////////////
void main_SelectProximity_SinglePoint ()
{
 static double val = 1;

 if (TRUE ==
     main_AskUserForNumberDialog
     ("Select by X,Y Proximity (pixels)",
      "Proximity Threshold:", &val) && val >= 0)
 {
  SG_DataPoint *curDP = NULL;

  SG_Voice *curVoice = SG_FirstVoice;

  while (curVoice != NULL)
  {
   if (curVoice->hide == FALSE)
   {
    curDP = curVoice->FirstDataPoint;
    do
    {
     if (SG_SELECTED == curDP->state)
     {
      break;
     }
     curDP = curDP->NextDataPoint;
    }
    while (curDP != NULL);
   }
   if (curDP != NULL)
   {
    break;
   }
   curVoice = curVoice->NextVoice;
  }

  if (curDP != NULL && SG_SELECTED == curDP->state)
  {
   SG_BackupDataPoints (main_drawing_area);
   SG_SelectProximity_SingleDP (curDP, val);
  }
 }
}

///////////////////////////////////
//just a short multiplatform nap; but Sleep is a wonderfully convluted
//occupation. Look what you might do: clock, delay, ftime, gettimeofday, 
//msleep, nap, napms, nanosleep, setitimer, sleep, Sleep, times, usleep
//Note the handy G_USEC_PER_SEC
void main_Sleep (int microseconds)
{
 g_usleep (microseconds);
 /*
    #ifdef GNAURAL_WIN32
    SG_DBGOUT_INT ("[Win32] Sleep(ms):", ms);
    Sleep (ms);
    #endif

    #ifndef GNAURAL_WIN32
    SG_DBGOUT_INT ("[POSIX] usleep(ms):", ms);
    //Obsolete, but using nanosleep feels like using a bazooka for a fly:
    usleep (ms);
    #endif
  */
}

///////////////////////////////////
void main_DuplicateAllVoices ()
{
 SG_BackupDataPoints (main_drawing_area);
 SG_RestoreDataPoints (main_drawing_area, TRUE);
 VG_RelistFlag = TRUE;
}

///////////////////////////////////
//pass this an empty buffer, and it fills it with sound from filename
//and fills size with number of elements in the array of shorts.
//returns 0 for success.
int main_LoadSoundFile_shorts
 (char *filename, short **buffer, unsigned int *size)
{
 SNDFILE *sndfile;

 SF_INFO sfinfo;

 if (0 != main_CheckSndfileVersion ())
 {
  return 1;
 }

 (*buffer) = NULL;
 memset (&sfinfo, 0, sizeof (sfinfo));
 SG_DBGOUT_STR ("Loading PCM File:", filename);
 if (!(sndfile = sf_open (filename, SFM_READ, &sfinfo)))
 {
  //puts (sf_strerror (NULL)); //removed 20100629 since now we search multiple places for file
  return 1;
 };
 if (sfinfo.channels < 1 || sfinfo.channels > 2)
 {
  SG_DBGOUT_INT ("Error : channels", sfinfo.channels);
  return 1;
 };
 //http://www.mega-nerd.com/libsndfile/FAQ.html
 //Q12 : I'm looking at sf_read*. What are items? What are frames?
 //For a sound file with only one channel, a frame is the same as a item.
 //For multi channel sound files, a single frame contains a single item for each channel.
 (*buffer) = (short *) malloc (sfinfo.frames * sizeof (short) * 2);
 (*size) = sfinfo.frames * 2;
 SG_DBGOUT_INT ("Channels:", sfinfo.channels);
 if (NULL == (*buffer))
 {
  return 1;
 }
 unsigned int readcount = 0;

 if (2 == sfinfo.channels)
 {
  SG_DBGOUT ("Reading stereo data");
  readcount = sf_readf_short (sndfile, (*buffer), (sfinfo.frames));
 }
 else
 {
  SG_DBGOUT ("Reading mono data; converting to stereo");
  while (0 != sf_readf_short (sndfile, &((*buffer)[readcount]), 1))
  {
   ++readcount;
   (*buffer)[readcount] = (*buffer)[(readcount - 1)];
   ++readcount;
  }
 }
 sf_close (sndfile);
 SG_DBGOUT_INT ("readcount: ", readcount);
 SG_DBGOUT_INT ("framecount: ", (int) sfinfo.frames);
 SG_DBGOUT_INT ("array sizeof (bytes):",
                ((int) sfinfo.frames) * sizeof (short) * 2);
 SG_DBGOUT_INT ("array elements: (shorts)", (*size));
 return 0;      //success
 //  Gnaural.wav
}

///////////////////////////////////
//pass this an empty buffer, and it fills it with sound from filename
//and fills size with number of elements in the array of shorts.
//returns 0 for success, filling (*buffer) and (*size) with data
//return non-zero on failure, with  (*buffer) = NULL (*size) = 0
int main_LoadSoundFile (char *filename, int **buffer, unsigned int *size)
{
 SNDFILE *sndfile;

 SF_INFO sfinfo;

 if (0 != main_CheckSndfileVersion ())
 {
  return 1;
 }

 (*buffer) = NULL;
 (*size) = 0;
 memset (&sfinfo, 0, sizeof (sfinfo));
 SG_DBGOUT_STR ("Loading PCM File:", filename);
 if (!(sndfile = sf_open (filename, SFM_READ, &sfinfo)))
 {
  //puts (sf_strerror (NULL)); //removed 20100629 since now we search multiple places for file
  return 1;
 };
 if (sfinfo.channels < 1 || sfinfo.channels > 2)
 {
  SG_DBGOUT_INT ("Error : channels", sfinfo.channels);
  return 1;
 };
 //http://www.mega-nerd.com/libsndfile/FAQ.html
 //Q12 : I'm looking at sf_read*. What are items? What are frames?
 //For a sound file with only one channel, a frame is the same as a item.
 //For multi channel sound files, a single frame contains a single item for each channel.
 (*buffer) = (int *) malloc (sfinfo.frames * sizeof (int));
 (*size) = sfinfo.frames;
 SG_DBGOUT_INT ("Channels:", sfinfo.channels);
 if (NULL == (*buffer))
 {
  return 1;
 }
 unsigned int readcount = 0;

 if (2 == sfinfo.channels)
 {
  SG_DBGOUT ("Reading stereo data");
  readcount = sf_readf_short (sndfile, (short *) (*buffer), (sfinfo.frames));
 }
 else
 {
  SG_DBGOUT ("Reading mono data; converting to stereo");
  while (0 != sf_readf_short (sndfile, (short *) &((*buffer)[readcount]), 1))
  {
   (*buffer)[readcount] &= 0x0000FFFF;
   (*buffer)[readcount] |= (((*buffer)[readcount]) << 16);
   ++readcount;
  }
 }
 sf_close (sndfile);
 SG_DBGOUT_INT ("readcount: ", readcount);
 SG_DBGOUT_INT ("framecount: ", (int) sfinfo.frames);
 SG_DBGOUT_INT ("array sizeof (bytes):",
                ((int) sfinfo.frames) * sizeof (int));
 SG_DBGOUT_INT ("array elements: (shorts)", (*size));
 return 0;      //success
}

///////////////////////////////////////////////
//called whenever opening/re-initing a user file for data
void main_UpdateGUI_UserDataInfo ()
{
 //start checkboxes init
 gtk_toggle_button_set_active
  (main_checkbuttonSwapStereo, (BB_StereoSwap == 0) ? FALSE : TRUE);

 gtk_toggle_button_set_active
  (main_checkbuttonOutputMono, (BB_Mono == 0) ? FALSE : TRUE);
 //end checkboxes init
 //start sliders init
 //now for the tricky overall vol and bal:
 if (BB_VolumeOverall_left > BB_VolumeOverall_right)
 {
  main_OverallVolume = BB_VolumeOverall_left;
  main_OverallBalance =
   -1.f + (BB_VolumeOverall_right / BB_VolumeOverall_left);
 }
 else if (BB_VolumeOverall_left < BB_VolumeOverall_right)
 {
  main_OverallVolume = BB_VolumeOverall_right;
  main_OverallBalance =
   1.f - (BB_VolumeOverall_left / BB_VolumeOverall_right);
 }
 else if (BB_VolumeOverall_left == BB_VolumeOverall_right)
 {
  main_OverallVolume = BB_VolumeOverall_right;
  main_OverallBalance = 0;
 }
 gtk_range_set_value ((GtkRange *) main_hscaleVolume,
                      100 * main_OverallVolume);
 gtk_range_set_value ((GtkRange *) main_hscaleBalance,
                      100 * main_OverallBalance);
 //end sliders init
}

///////////////////////////////////////////////
void main_vscale_Y_button_event (GtkWidget * widget, gboolean pressed)
{
 main_vscale_Y_mousebuttondown = pressed;
 if (FALSE == main_vscale_Y_mousebuttondown)
 {
  gtk_range_set_value ((GtkRange *) widget, 0);
 }
 else
 {
  SG_BackupDataPoints (main_drawing_area);      //a20070620
 }
}

///////////////////////////////////////////////
void main_vscale_Y_value_change (GtkRange * range)
{
 if (TRUE == main_vscale_Y_mousebuttondown)
 {
  main_vscale_Y = -gtk_range_get_value (range);
 }
 else
 {
  main_vscale_Y = 0.0f;
  gtk_range_set_value ((GtkRange *) range, 0);
 }
}

///////////////////////////////////////////////
void main_hscale_X_button_event (GtkWidget * widget, gboolean pressed)
{
 main_hscale_X_mousebuttondown = pressed;
 if (FALSE == main_hscale_X_mousebuttondown)
 {
  gtk_range_set_value ((GtkRange *) widget, 0);
 }
 else
 {
  SG_BackupDataPoints (main_drawing_area);      //a20070620
 }
}

///////////////////////////////////////////////
void main_hscale_X_value_change (GtkRange * range)
{
 if (TRUE == main_hscale_X_mousebuttondown)
 {
  main_hscale_X = gtk_range_get_value (range);
 }
 else
 {
  main_hscale_X = 0.0f;
  gtk_range_set_value ((GtkRange *) range, 0);
 }
}

/////////////////////////////////////////////////////
//a20070620:
void main_slider_XY_handler (float vertical, float horizontal)
{
 if (0.0f == vertical && 0.0f == horizontal)
 {
  return;
 }

 if (vertical != 0.0f)
 {
  if (FALSE == main_XY_scaleflag)
  {
   SG_MoveSelectedDataPoints (main_drawing_area, 0, vertical);
  }
  else
  {
   SG_ScaleDataPoints_Y (main_drawing_area, 1 + (-.1 * vertical));
  }
  SG_ConvertYToData_SelectedPoints (main_drawing_area);
 }

 //a20070620 :
 if (horizontal != 0.0f)
 {
  if (FALSE == main_XY_scaleflag)
  {
   SG_MoveSelectedDataPoints (main_drawing_area, horizontal, 0);
  }
  else
  {
   SG_ScaleDataPoints_Time (main_drawing_area, 1 + (.1 * horizontal));
  }
  SG_ConvertXToDuration_AllPoints (main_drawing_area);
 }

 SG_ConvertDataToXY (main_drawing_area);        // NOTE: I need to call this both to set limits and to bring XY's outside of graph back in
 SG_DrawGraph (main_drawing_area);
 SG_DataHasChanged = SG_GraphHasChanged = TRUE; //20070105 tricky way to do main_LoadBinauralBeatSoundEngine in a bulk way
}

///////////////////////////////////
void main_RoundValues ()
{
 static double val = 100;

 static double param = 0;

 char namestr[] = "Round Selected DPs";

 if (TRUE ==
     main_AskUserForNumberDialog
     (namestr,
      "Round which parameter?\n0: duration\n1: volume_left\n2: volume_right\n3: basefreq\n4: beatfreq",
      &param)
     && TRUE == main_AskUserForNumberDialog (namestr,
                                             "Rounding value:\n1=1th\n10=10th\n100=100th",
                                             &val))
 {
  SG_BackupDataPoints (main_drawing_area);
  SG_RoundValues_All (main_drawing_area, val, param);
 }
}

///////////////////////////////////
//this launches or ends the thread that runs GnauralNet:
void main_GnauralNet_StopStart (GtkMenuItem * menuitem)
{
 GtkWidget *menu_label = gtk_bin_get_child (GTK_BIN (menuitem));

 if (GNAURALNET_STOPPED == GN_My.State)
 {
  if (GNAURALNET_SUCCESS == GN_start (NULL, NULL))      //NULL,NULL uses both default internal funcs in GN for loop and processing
  {
   gtk_label_set_text (GTK_LABEL (menu_label), "Stop Gnauralnet Server");
   return;
  }
 }
 GN_stop ();
 gtk_label_set_text (GTK_LABEL (menu_label), "Start Gnauralnet Server");
}

///////////////////////////////////
void main_GnauralNet_JoinFriend (GtkMenuItem * menuitem)
{
 if (GNAURALNET_RUNNING != GN_My.State)
 {
  main_MessageDialogBox
   ("First start GnauralNet", GTK_MESSAGE_INFO, GTK_BUTTONS_OK);
  return;
 }
 main_AskUserForIP_Port
  ("Connect to another Gnaural", "Enter Port and IP", "127.0.0.1", 7141,
   GN_MSGTYPE_TIMEINFO);
}

///////////////////////////////////
void main_GnauralNet_PhaseInfo (GtkMenuItem * menuitem)
{
 if (GNAURALNET_RUNNING != GN_My.State)
 {
  main_MessageDialogBox
   ("First start GnauralNet", GTK_MESSAGE_INFO, GTK_BUTTONS_OK);
  return;
 }

 //  if (BB_UserFunc == NULL)
 main_AskUserForIP_Port
  ("Send Phase Info To Remote App", "Enter Port and IP", "127.0.0.1", 7141,
   GN_MSGTYPE_PHASEINFO);
}

///////////////////////////////////
void main_GnauralNet_ShowInfo (GtkMenuItem * menuitem)
{
 GN_ShowInfo ();
}

///////////////////////////////////////////
//returns TRUE if user clicked OK, filling val with result
void main_AskUserForIP_Port (char *title, char *question, char *IP,
                             unsigned short Port, int type)
{
 GtkWidget *dialog = gtk_dialog_new_with_buttons (title,
                                                  (GtkWindow *) main_window,
                                                  (GtkDialogFlags)
                                                  (GTK_DIALOG_MODAL |
                                                   GTK_DIALOG_DESTROY_WITH_PARENT),
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_ACCEPT,
                                                  GTK_STOCK_CANCEL,
                                                  GTK_RESPONSE_REJECT, NULL);

 //add some stuff:
 GtkWidget *label = gtk_label_new (question);

 GtkWidget *entry_InputIP = gtk_entry_new ();

 GtkWidget *entry_InputPort = gtk_entry_new ();

 sprintf (main_sTmp, "%s", IP);
 gtk_entry_set_text (GTK_ENTRY (entry_InputIP), main_sTmp);
 sprintf (main_sTmp, "%d", Port);
 gtk_entry_set_text (GTK_ENTRY (entry_InputPort), main_sTmp);
 // Add the label, and show everything we've added to the dialog.
 gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label);
 gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), entry_InputIP);
 gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
                    entry_InputPort);
 gtk_widget_show_all (dialog);
 //block until I get a response:
 gint result = gtk_dialog_run (GTK_DIALOG (dialog));

 switch (result)
 {
 case GTK_RESPONSE_ACCEPT:
  {
   unsigned short port =
    (unsigned short) atoi (gtk_entry_get_text (GTK_ENTRY (entry_InputPort)));
   unsigned int ip =
    inet_addr (gtk_entry_get_text (GTK_ENTRY (entry_InputIP)));
   switch (type)
   {
   case GN_MSGTYPE_TIMEINFO:
    GN_FriendList_Invite (ip, htons (port));
    break;

   case GN_MSGTYPE_PHASEINFO:
    main_PhaseInfoToggle (ip, htons (port));
    break;

   default:
    //SG_DBGOUT ("Shouldn't be here");
    break;
   }

  }
  break;
 default:
  break;
 }
 gtk_widget_destroy (dialog);
}

////////////////////////////////////
//checks if there actually is a sndfile library available
//returns 0 on success
int main_CheckSndfileVersion (void)
{
 main_sTmp[0] = 0;
 sf_command (NULL, SFC_GET_LIB_VERSION, main_sTmp, sizeof (main_sTmp));
 if (strlen (main_sTmp) < 1)
 {
  SG_ERROUT ("Couldn't get sndfile version; did you install it?");
  main_MessageDialogBox ("You need to install the sndfile library",
                         GTK_MESSAGE_ERROR, GTK_BUTTONS_OK);
  return -1;
 };
 SG_DBGOUT_STR ("Your sndfile version:", main_sTmp);
 return 0;
}

////////////////////////////////////
//a20080307
void main_progressbar_button_press_event (GtkWidget * widget,
                                          GdkEventButton * event)
{
 while (TRUE == BB_InCriticalLoopFlag)
 {
  SG_DBGOUT ("In critical loop, waiting");
  main_Sleep (100);
 }

 float x = event->x / (float) widget->allocation.width;

 double samplecount_oneloop = BB_TotalDuration * BB_AUDIOSAMPLERATE;

 if (1 < BB_Loops)
 {
  SG_DBGOUT_INT ("factoring progressbar loops:", BB_Loops);
  double samplecount_total = samplecount_oneloop * BB_Loops;

  //first get the total number of samples user clicked at:
  BB_CurrentSampleCountLooped = (unsigned long) (x * samplecount_total);
  //now translate that to BB_LoopCount:
  BB_LoopCount =
   BB_Loops -
   (BB_CurrentSampleCountLooped / (unsigned long) samplecount_oneloop);
  //now figured out how far in a single schedule pass we were:
  BB_CurrentSampleCount =
   (BB_CurrentSampleCountLooped % (unsigned long) samplecount_oneloop);
  //now subtract that many samples from BB_CurrentSampleCountLooped:
  BB_CurrentSampleCountLooped -= BB_CurrentSampleCount;
  GN_DBGOUT_UINT ("New BB_LoopCount:", BB_LoopCount);
  main_UpdateGUI_entryLoops ();
  main_UpdateGUI_ProjectedRuntime ();
 }
 else
 {
  SG_DBGOUT ("factoring simple progressbar");
  //easy way:
  BB_CurrentSampleCount = (unsigned long) (samplecount_oneloop * x);
 }

 //this ensures that no sync'd friends follow this one now:
 GN_Time_ResetSeniority ();
}

/////////////////////////////////
int main_playAudio_SoundInit ()
{
 int res = playAudio_SoundInit (main_sTmp);

 if (main_gnaural_guiflag == TRUE)
 {
  main_UpdateGUI_Statusbar (main_sTmp, "");
 }
 else
 {
  BB_DBGOUT (main_sTmp);
 }
 return res;
}

///////////////////////////////////
void main_Preferences ()
{
 main_SelectFont ();
}

/////////////////////////////////
//20100408
//will eventually want to set the current font name in dialog
void main_SelectFont ()
{
 GtkResponseType result;
 GtkWidget *dialog = gtk_font_selection_dialog_new ("Select Font");
 result = gtk_dialog_run (GTK_DIALOG (dialog));
 if (result == GTK_RESPONSE_OK || result == GTK_RESPONSE_APPLY)
 {
  PangoFontDescription *font_desc;
  gchar *fontname =
   gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG
                                            (dialog));
  font_desc = pango_font_description_from_string (fontname);
  double dpi = gdk_screen_get_resolution (gdk_screen_get_default ());
  if (TRUE == pango_font_description_get_size_is_absolute (font_desc))
  {
   SG_GraphFontSize =
    pango_font_description_get_size (font_desc) / PANGO_SCALE;
  }
  else
  {
   SG_GraphFontSize =
    (dpi * pango_font_description_get_size (font_desc) / PANGO_SCALE) / 72.0;
  }
  SG_DBGOUT_INT ("New font size:", SG_GraphFontSize);
  pango_layout_set_font_description (SG_PangoLayout, font_desc);
  pango_font_description_free (font_desc);
  g_free (fontname);
 }
 gtk_widget_destroy (dialog);
}

///////////////////////////////////////////
void main_DataPointSize ()
{
 double val = (double) SG_DataPointSize;
 main_AskUserForNumberDialog (main_sTmp, "Data Point Size", &val);
 SG_DataPointSize = (int) val;
}

///////////////////////////////////////////
//added 20101006 to handle SG's magnetic pointer
void main_on_checkbutton_MagneticPointer_toggled (GtkToggleButton *
                                                  togglebutton)
{
 SG_MagneticPointerflag = gtk_toggle_button_get_active (togglebutton);
}

#ifndef GNAURAL_MACOSX
#ifndef GNAURAL_WIN32
//////////////////////////////////////////////
//20101109: Returns elapsed time since previous call. First call it 
//returns 0.
//NOTE: used to use gettimeofday (&newtime, NULL), but this is better.
double Stopwatch ()
{
#define BILLION  1000000000L;
 struct timespec newtime;
 static struct timespec oldtime;
 double result;

 if (clock_gettime (CLOCK_REALTIME, &newtime) == -1)
 {
  SG_DBGOUT ("clock gettime");
  return 0;
 }

 result = (newtime.tv_sec - oldtime.tv_sec)
  + (double) (newtime.tv_nsec - oldtime.tv_nsec) / (double) BILLION;

 //deal with first-time call
 if (0 == oldtime.tv_nsec && 0 == oldtime.tv_sec)
 {
  result = 0;
 }

 oldtime.tv_sec = newtime.tv_sec;
 oldtime.tv_nsec = newtime.tv_nsec;

 return result;
}
#endif
#endif

unsigned int IP = 0;
unsigned short Port = 0;
//////////////////////////////////////////////
//20110221: BB will call this is function if non-NULL
void main_BB_UserFunc (int voice)
{
 if (GNAURALNET_RUNNING == GN_My.State)
 {
  char data = voice;
  GN_Socket_SendMessage (GN_My.Socket, (char *) &data,
                         sizeof (data), IP, Port);
 }
}

//////////////////////////////////////////////
//20110221: toggles state of function from valid to NULL
//so BB can pulse out phase info
void main_PhaseInfoToggle (unsigned int ip, unsigned short port)
{
 IP = ip;
 Port = port;
 if (BB_UserFunc != NULL)
 {
  BB_UserFunc = NULL;
 }
 else
 {
  BB_UserFunc = main_BB_UserFunc;
 }
}

//////////////////////////////////////////////
//20110222:hack tailored to an SG function used to getting mouse clicks
void main_DPPropertiesDialog ()
{
 GdkEventButton event;
 event.type = GDK_KEY_PRESS;
 event.window = NULL;
 event.send_event = 0;
 event.time = 0;
 event.x = -100.0;
 event.y = -100.0;
 event.axes = NULL;
 event.state = 0;
 event.button = 3;
 event.device = NULL;
 event.x_root = 0;
 event.y_root = 0;
 SG_button_press_event (main_drawing_area, &event);
}
