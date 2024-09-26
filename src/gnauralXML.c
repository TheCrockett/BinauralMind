/*
   gnauralXML.c
   code for managing XML data-styled data in the Gnaural context
   Depends on:
   main.h
   gnauralXML.h
   gnauralnet.h

   Copyright (C) 20100608  Bret Logan

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

#include <string.h>     //needed for strcpy
#include "main.h"
#include "gnauralnet.h"
#include "ScheduleXML.h"
#include "gnauralXML.h"
#include "voiceGUI.h"
#include "gnauralRecentMenu.h"

int gxml_ParserXML_VoiceCount = 0;      //this is a *true* count of Voices (as opposed to listed count in file) taken from  pre-reads of the XML file; has priority
int gxml_ParserXML_EntryCount = 0;      //this is a *true* count of Entries (as opposed to listed count in file) taken from  pre-reads of the XML file; has priority
int gxml_GnauralFile = 0;       //just a way to know internally if a file being opened isn't a valid Gnaurl2 file

/////////////////////////////////////////////////////
//Writes a Gnaural format XML file
void gxml_XMLWriteFile (char *filename)
{
 //First count how many DPs to copy. One reason to do this first is to be sure
 //there really are selected DPs before throwing away the old buffer:
 SG_DataPoint *curDP;
 int DP_count = 0;
 int voice_count = 0;
 SG_Voice *curVoice = SG_FirstVoice;

 while (curVoice != NULL)
 {
  curDP = curVoice->FirstDataPoint;
  ++voice_count;
  while (curDP != NULL)
  {
   ++DP_count;
   //all list loops need this:
   curDP = curDP->NextDataPoint;
  }
  curVoice = curVoice->NextVoice;
 }

 SG_DBGOUT_INT ("WriteFileXML: DataPoints to copy:", DP_count);
 SG_DBGOUT_INT ("WriteFileXML: Total Voices:", voice_count);

 //if there are no selected DPs, don't do anything:
 if (DP_count < 1)
  return;

 //prepare file access:
 FILE *stream;

 if ((stream = fopen (filename, "w")) == NULL)
 {
  SG_ERROUT ("Failed to open file for writing!");
  return;
 }

 gchar strbuf[G_ASCII_DTOSTR_BUF_SIZE];

 const int strsize = sizeof (strbuf);

 fprintf (stream, "<?xml version=\"1.0\"?>\n"); //added 20101010 to keep browswers from seeing datafiles as webpages
 fprintf (stream, "<!-- See http://gnaural.sourceforge.net -->\n");
 fprintf (stream, "<schedule>\n");
 fprintf (stream, "<gnauralfile_version>%s</gnauralfile_version>\n", gxml_VERSION_GNAURAL_XMLFILE);     //always keep this first
 fprintf (stream, "<gnaural_version>%s</gnaural_version>\n", VERSION);
 time_t curtime;

 struct tm *loctime;

 curtime = time (NULL);
 loctime = (struct tm *) localtime (&curtime);
 fprintf (stream, "<date>%s</date>\n", asctime (loctime));
 fprintf (stream, "<title>%s</title>\n",
          main_Info_Title == NULL ? "[none]" : main_Info_Title);
 fprintf (stream, "<schedule_description>%s</schedule_description>\n",
          main_Info_Description == NULL ? "[none]" : main_Info_Description);
 fprintf (stream, "<author>%s</author>\n",
          main_Info_Author == NULL ? "[none]" : main_Info_Author);
 fprintf (stream, "<totaltime>%s</totaltime>\n",
          g_ascii_formatd (strbuf, strsize, "%g", SG_TotalScheduleDuration));
 fprintf (stream, "<voicecount>%d</voicecount>\n", voice_count);
 fprintf (stream, "<totalentrycount>%d</totalentrycount>\n", DP_count);
 fprintf (stream, "<loops>%d</loops>\n", BB_Loops);
 fprintf (stream, "<overallvolume_left>%s</overallvolume_left>\n",
          g_ascii_formatd (strbuf, strsize, "%g", BB_VolumeOverall_left));
 fprintf (stream, "<overallvolume_right>%s</overallvolume_right>\n",
          g_ascii_formatd (strbuf, strsize, "%g", BB_VolumeOverall_right));
 fprintf (stream, "<stereoswap>%d</stereoswap>\n", BB_StereoSwap);
 fprintf (stream, "<graphview>%d</graphview>\n", SG_GraphType);

 //#define GXML_MAKEARRAYS
#ifdef GXML_MAKEARRAYS
 //this is for making a schedule internal: 
 FILE *array = fopen ("VoiceArray.h", "w");
#endif

 // Now put all the selected DPs in to the copy buffer:
 DP_count = 0;
 voice_count = 0;
 curVoice = SG_FirstVoice;
 while (curVoice != NULL)
 {
  //first count the number of DP's in this voice:
  curDP = curVoice->FirstDataPoint;
  int dpcount_local = 0;

  while (curDP != NULL)
  {
   ++dpcount_local;
   //all list loops need this:
   curDP = curDP->NextDataPoint;
  }

#ifdef GXML_MAKEARRAYS
  fprintf (array,
           "/* First three Type, Count, Mono,\nrest are Dur, VolL, VolR, Beat, Base */\n");
  fprintf (array, "static final float BB_ScheduleArray%d[] = {\n%d,%d,%d",
           voice_count, curVoice->type, dpcount_local, curVoice->mono);
#endif

  //now do it again with that info:
  curDP = curVoice->FirstDataPoint;
  fprintf (stream, "<voice>\n");
  fprintf (stream, "<description>%s</description>\n",
           curVoice->description == NULL ? "[none]" : curVoice->description);
  fprintf (stream, "<id>%d</id>\n", voice_count);
  fprintf (stream, "<type>%d</type>\n", curVoice->type);
  fprintf (stream, "<voice_state>%d</voice_state>\n", curVoice->state);
  fprintf (stream, "<voice_hide>%d</voice_hide>\n", curVoice->hide);
  fprintf (stream, "<voice_mute>%d</voice_mute>\n", curVoice->mute);
  fprintf (stream, "<voice_mono>%d</voice_mono>\n", curVoice->mono);
  fprintf (stream, "<entrycount>%d</entrycount>\n", dpcount_local);
  fprintf (stream, "<entries>\n");

  while (curDP != NULL)
  {
#ifdef GXML_MAKEARRAYS
   fprintf (array, ",\n%gf,%gf,%gf,%gf,%gf",
            curDP->duration,
            curDP->volume_left,
            curDP->volume_right, curDP->beatfreq, curDP->basefreq);
#endif

   //NEW STYLE FORMAT
   //START entry writing:
   fprintf (stream, "<entry parent=\"%d\" duration=\"%s\" ", voice_count,
            g_ascii_formatd (strbuf, strsize, "%g", curDP->duration));
   fprintf (stream, "volume_left=\"%s\" ",
            g_ascii_formatd (strbuf, strsize, "%g", curDP->volume_left));
   fprintf (stream, "volume_right=\"%s\" ",
            g_ascii_formatd (strbuf, strsize, "%g", curDP->volume_right));
   fprintf (stream, "beatfreq=\"%s\" ",
            g_ascii_formatd (strbuf, strsize, "%g", curDP->beatfreq));
   fprintf (stream, "basefreq=\"%s\" state=\"%d\"/>\n",
            g_ascii_formatd (strbuf, strsize, "%g", curDP->basefreq),
            curDP->state);
   //END entry writing:
   //END NEW STYLE FORMAT
   ++DP_count;
   //all list loops need this:
   curDP = curDP->NextDataPoint;
  }
  curVoice = curVoice->NextVoice;
  fprintf (stream, "</entries>\n");
  fprintf (stream, "</voice>\n");
  ++voice_count;

#ifdef GXML_MAKEARRAYS
  fprintf (array, "};\n\n");
#endif

 }
 fprintf (stream, "</schedule>\n");
 fclose (stream);
 SG_DBGOUT_INT ("Wrote DataPoints:", DP_count);

#ifdef GXML_MAKEARRAYS
 fclose (array);
#endif
}

///////////////////////////////////////////////
//this is only called by main_XMLParser; don't call yerself.
//point of this is to keep Event data separate, since it is the
//most numerous and thus should probably be done with Attributes
int gxml_XMLEventDataParser (const gchar * DataType,
                             const gchar * Value,
                             const int internal_EntryCount)
{
 //==START OF EVENT PROPERTIES
 if (!strcmp (DataType, "parent"))
 {
  //must add 1, because 0 would look to Restore() like a NULL pointer:
  SG_UndoRedo.DPdata[internal_EntryCount].parent =
   (void *) (atoi (Value) + 1);
  SG_DBGOUT_PNT ("parent:", SG_UndoRedo.DPdata[internal_EntryCount].parent);
  return 0;
 }

 if (!strcmp (DataType, "duration"))
 {
  SG_UndoRedo.DPdata[internal_EntryCount].duration =
   g_ascii_strtod (Value, NULL);
  SG_DBGOUT_FLT ("duration:",
                 SG_UndoRedo.DPdata[internal_EntryCount].duration);
  return 0;
 }

 if (!strcmp (DataType, "volume_left"))
 {
  SG_UndoRedo.DPdata[internal_EntryCount].volume_left =
   g_ascii_strtod (Value, NULL);
  SG_DBGOUT_FLT ("volume_left:",
                 SG_UndoRedo.DPdata[internal_EntryCount].volume_left);
  return 0;
 }

 if (!strcmp (DataType, "volume_right"))
 {
  SG_UndoRedo.DPdata[internal_EntryCount].volume_right =
   g_ascii_strtod (Value, NULL);
  SG_DBGOUT_FLT ("volume_right:",
                 SG_UndoRedo.DPdata[internal_EntryCount].volume_right);
  return 0;
 }

 if (!strcmp (DataType, "beatfreq"))
 {
  SG_UndoRedo.DPdata[internal_EntryCount].beatfreq =
   g_ascii_strtod (Value, NULL);
  SG_DBGOUT_FLT ("beatfreq:",
                 SG_UndoRedo.DPdata[internal_EntryCount].beatfreq);
  return 0;
 }

 if (!strcmp (DataType, "basefreq"))
 {
  SG_UndoRedo.DPdata[internal_EntryCount].basefreq =
   g_ascii_strtod (Value, NULL);
  SG_DBGOUT_FLT ("basefreq:",
                 SG_UndoRedo.DPdata[internal_EntryCount].basefreq);
  return 0;
 }

 if (!strcmp (DataType, "state"))
 {
  SG_UndoRedo.DPdata[internal_EntryCount].state = atoi (Value);
  SG_DBGOUT_INT ("state:", SG_UndoRedo.DPdata[internal_EntryCount].state);
  return 0;
 }
 //==END OF EVENT PROPERTIES
 //=========================
 return 1;
}

///////////////////////////////////////////////
//this is only called by main_XMLReadFile; don't call
void gxml_XMLParser (const gchar * CurrentElement,      //this must always be a valid string
                     const gchar * Attribute,   //beware: this will equal NULL if there are none
                     const gchar * Value)
{       //beware: this will equal NULL to announce end of CurrnetElement
 //Essential to do things in this order:
 // 1) Check if this call represents the end of CurrentElement; if so, increment counter vars then return
 // 2) See if there are Attributes with this CurrentElement; if so, assign them, then return;
 // 3) If I got here, just slog through Elements and Values to assign to vars
 static int internal_EntryCount = 0;

 static int internal_VoiceCount = 0;

 //See if this is the end of CurrentElement:
 if (Value == NULL)
 {
  if (0 == strcmp (CurrentElement, "entry"))
  {
   ++internal_EntryCount;
   SG_DBGOUT_STR ("==END ", CurrentElement);
  }
  else if (0 == strcmp (CurrentElement, "voice"))
  {
   ++internal_VoiceCount;
   SG_DBGOUT_STR ("==END ", CurrentElement);
  }
  return;
 }

 //Now deal with Attributes if there are any:
 if (Attribute != NULL)
 {
  // ParserXML_AbortFlag = TRUE;
  // SG_DBGOUT_STR(Attribute, Value);
  gxml_XMLEventDataParser (Attribute, Value, internal_EntryCount);
  return;
 }

 //got here, so must slog through assigning vars Element-by-Element (the old way).
 //The first line exists in order to allow old pre-Attribute file versions to
 //still be openable:
 if (0 ==
     gxml_XMLEventDataParser (CurrentElement, Value, internal_EntryCount))
 {
 }
 else if (!strcmp (CurrentElement, "description"))
 {
  //20070627: fixed bug here if user had no desc. (because Value would equal NULL)
  //  SG_UndoRedo.Voice[internal_VoiceCount].description =
  SG_StringAllocateAndCopy (&
                            (SG_UndoRedo.Voice[internal_VoiceCount].
                             description), Value);
  SG_DBGOUT_STR ("description:",
                 SG_UndoRedo.Voice[internal_VoiceCount].description);
 }
 else if (!strcmp (CurrentElement, "type"))
 {
  SG_UndoRedo.Voice[internal_VoiceCount].type = atoi (Value);
  SG_DBGOUT_INT ("voicetype:", SG_UndoRedo.Voice[internal_VoiceCount].type);
 }
 else if (!strcmp (CurrentElement, "voice_state"))
 {
  SG_UndoRedo.Voice[internal_VoiceCount].state = atoi (Value);
  SG_DBGOUT_INT ("voice_state:",
                 SG_UndoRedo.Voice[internal_VoiceCount].state);
 }
 else if (!strcmp (CurrentElement, "voice_hide"))
 {
  SG_UndoRedo.Voice[internal_VoiceCount].hide = atoi (Value);
  SG_DBGOUT_INT ("voice_hide:", SG_UndoRedo.Voice[internal_VoiceCount].hide);
 }
 else if (!strcmp (CurrentElement, "voice_mute"))
 {
  SG_UndoRedo.Voice[internal_VoiceCount].mute = atoi (Value);
  SG_DBGOUT_INT ("voice_mute:", SG_UndoRedo.Voice[internal_VoiceCount].mute);
 }
 else if (!strcmp (CurrentElement, "voice_mono"))
 {
  SG_UndoRedo.Voice[internal_VoiceCount].mono = atoi (Value);
  SG_DBGOUT_INT ("voice_mono:", SG_UndoRedo.Voice[internal_VoiceCount].mono);
 }
 //By the totalentrycount entry, I have all the info needed to make the critical memory allotments:
 else if (!strcmp (CurrentElement, "totalentrycount"))
 {
  int listedcount = atoi (Value);

  if (listedcount > gxml_ParserXML_EntryCount)
  {
   SG_ERROUT_INT ("Listed entry count larger than actual count by",
                  listedcount - gxml_ParserXML_EntryCount);
   SG_DBGOUT_INT ("  Listed count:", listedcount);
   SG_DBGOUT_INT ("  Actual count:", gxml_ParserXML_EntryCount);
  }
  else if (listedcount < gxml_ParserXML_EntryCount)
  {
   SG_ERROUT_INT ("Listed entry count smaller than actual count by",
                  gxml_ParserXML_EntryCount - listedcount);
   SG_DBGOUT_INT ("  Listed count:", listedcount);
   SG_DBGOUT_INT ("  Actual count:", gxml_ParserXML_EntryCount);
  }
  else
  {
   SG_DBGOUT_INT ("Listed entry count matched actual count:",
                  gxml_ParserXML_EntryCount);
  }

  //  SG_UndoRedo.TotalDataPoints = atoi (Value);   //this is weak, as this number could be wrong (I need to count myself)
  SG_UndoRedo.TotalDataPoints = gxml_ParserXML_EntryCount;      //I trust the read count more than listed
  SG_AllocateBackupData (&SG_UndoRedo, SG_UndoRedo.TotalVoices,
                         SG_UndoRedo.TotalDataPoints);
  SG_DBGOUT_INT ("TotalEntryCount:", SG_UndoRedo.TotalDataPoints);
  SG_DBGOUT ("==Allocated Loadup Data==");
 }
 else if (!strcmp (CurrentElement, "voicecount"))
 {
  int listedcount = atoi (Value);

  if (listedcount > gxml_ParserXML_VoiceCount)
  {
   SG_ERROUT_INT ("Listed voice count larger than actual count by",
                  listedcount - gxml_ParserXML_VoiceCount);
   SG_DBGOUT_INT ("  Listed count:", listedcount);
   SG_DBGOUT_INT ("  Actual count:", gxml_ParserXML_VoiceCount);
  }
  else if (listedcount < gxml_ParserXML_VoiceCount)
  {
   SG_ERROUT_INT ("Listed voice count smaller than actual count by",
                  gxml_ParserXML_VoiceCount - listedcount);
   SG_DBGOUT_INT ("  Listed count:", listedcount);
   SG_DBGOUT_INT ("  Actual count:", gxml_ParserXML_VoiceCount);
  }
  else
  {
   SG_DBGOUT_INT ("Listed voice count matched actual count:",
                  gxml_ParserXML_VoiceCount);
  }
  SG_UndoRedo.TotalVoices = gxml_ParserXML_VoiceCount;  // I trust the real count more than listed
 }

 //finally the rarely called, low-priority global entries:
 else if (!strcmp (CurrentElement, "title"))
 {
  SG_StringAllocateAndCopy (&main_Info_Title, Value);
 }
 else if (!strcmp (CurrentElement, "schedule_description"))
 {
  SG_StringAllocateAndCopy (&main_Info_Description, Value);
 }
 else if (!strcmp (CurrentElement, "author"))
 {
  SG_StringAllocateAndCopy (&main_Info_Author, Value);
 }
 else if (!strcmp (CurrentElement, "loops"))
 {
  main_SetLoops (atoi (Value));
  SG_DBGOUT_INT ("Loops:", BB_Loops);
 }
 else if (!strcmp (CurrentElement, "overallvolume_left"))
 {
  BB_VolumeOverall_left = g_ascii_strtod (Value, NULL);
  SG_DBGOUT_FLT ("BB_VolumeOverall_left", BB_VolumeOverall_left);
 }
 else if (!strcmp (CurrentElement, "overallvolume_right"))
 {
  BB_VolumeOverall_right = g_ascii_strtod (Value, NULL);
  SG_DBGOUT_FLT ("BB_VolumeOverall_right", BB_VolumeOverall_right);
 }
 else if (!strcmp (CurrentElement, "stereoswap"))
 {
  BB_StereoSwap = atoi (Value);
  SG_DBGOUT_INT ("BB_StereoSwap", BB_StereoSwap);
 }
 else if (!strcmp (CurrentElement, "graphview"))
 {
  SG_GraphType = atoi (Value);
  SG_DBGOUT_INT ("SG_GraphType", SG_GraphType);
 }
 else if (!strcmp (CurrentElement, "gnauralfile_version"))
 {
  gxml_GnauralFile = ~0;
  SG_DBGOUT ("Zeroing running voice and entry counts");
  internal_EntryCount = internal_VoiceCount = 0;
  SG_DBGOUT_STR ("File Version:", Value);
  if (strcmp (gxml_VERSION_GNAURAL_XMLFILE, Value))
  {
   //SG_ERROUT("File format version too old: Aborting!");
   //ParserXML_AbortFlag = 1;
   SG_ERROUT ("Unknown File Version:");
   SG_ERROUT ("Expected:");
   SG_ERROUT (gxml_VERSION_GNAURAL_XMLFILE);
   SG_ERROUT ("Got:");
   SG_ERROUT (Value);
   //    if (0 == main_MessageDialogBox ("Unknown Gnaural file version or format. Is this a Gnaural 1 file?", GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO)) { }
  }
  else
  {
   SG_DBGOUT ("Useable file version, continuing:");
  }
 }
}

/////////////////////////////////////////////////////
//Returns 0 on success
//widget refers to main_drawing_area, BTW
//To maximize reusing code, this procedure actually fills
//ScheduleGUI's variable "SG_UndoRedo" with the incoming
//data, then treats that data like any ordinary restore.
//RETURNS 0 on success;
//20070705: changed a lot of address bugs with console mode
//trying to update a non-existent GUI.
//NOTE: if MergeRestore == TRUE, adds restore to whatever is already
//in SG; if FALSE, erases everything currently in SG
int gxml_XMLReadFile (char *filename,
                      GtkWidget * widget, gboolean MergeRestore)
{
 if (filename == NULL)
 {
  return -1;
 }

 //to check if maybe it is a Gnaural1 file:
 gxml_GnauralFile = 0;

 //Set a few things in case they're skipped later:
 main_SetLoops (1);
 main_Info_Author[0] = '\0';
 main_Info_Title[0] = '\0';
 main_Info_Description[0] = '\0';
 BB_ResetAllVoices ();  //never hurts to do this, and might help a lot
 //  BB_LoopCount = 1;

 //to parse a Gnaural file:
 // 1) zero any SG internal variables used to count voice/event progress (ESSENTIAL)
 // 2) Give ParserXML SG's internal callback function
 // 3) call ParserXML_parse_file_xml:
 //Mustn't forget this:
 gxml_ParserXML_VoiceCount = 0;
 gxml_ParserXML_EntryCount = 0;
 SetUserParserXMLHandler (gxml_XMLParser_counter);
 SG_DBGOUT ("Parsing XML file for true voice and entry counts");
 if (0 == ParserXML_parse_file_xml (filename))
 {
  SG_DBGOUT_INT ("Total new Voice count: ", gxml_ParserXML_VoiceCount);
  SG_DBGOUT_INT ("Total new Entry count: ", gxml_ParserXML_EntryCount);
 }
 else
 {
  SG_DBGOUT ("Something's wrong with the XML file, can't count");
 }
 SetUserParserXMLHandler (gxml_XMLParser);
 SG_DBGOUT ("Starting with real XML file parse:");
 if (0 == ParserXML_parse_file_xml (filename))
 {
  SG_DBGOUT ("XML file parsed, putting data in engine:");
  SG_DBGOUT_INT ("Total new Voice count: ", gxml_ParserXML_VoiceCount);
  SG_DBGOUT_INT ("Total new Entry count: ", gxml_ParserXML_EntryCount);
  SG_RestoreDataPoints (widget, MergeRestore);
  SG_DrawGraph (widget);
  SG_DBGOUT_STR ("Title:", main_Info_Title);
  SG_DBGOUT_STR ("Author:", main_Info_Author);
  SG_DBGOUT_STR ("Description:", main_Info_Description);
  main_UpdateGUI_UserDataInfo ();
  gnauralRecentMenu_add_utf8_filename (filename);       //a20101014

 }
 else
 {
  if (gxml_GnauralFile == 0)
  {
   SG_DBGOUT ("No Gnaural format file found");
   //try to open a Gnaural1 file:
   if (main_gnaural_guiflag == TRUE ||  //BUG20100928: was "FALSE"
       0 != main_MessageDialogBox ("Write default file?)",      //Is this a pre-2007 Gnaural file?",
                                   GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO))
   {
    main_WriteDefaultFile ();
   }
   else
   {
    SG_DBGOUT ("Processing as a pre-2006 Gnaural file");
    main_GNAURAL1FILE_SchedFilenameToSchedule (filename);
    //       SG_GetScheduleLimits();
    SG_ConvertDataToXY (widget);
    //    SG_DrawGraph (widget);
    SG_DataHasChanged = SG_GraphHasChanged = TRUE;
   }
  }
  else
  {
   return -1;   //THIS SHOULD BE CLEANED UP; it is confusing
  }
 }

 //Got here, so file is valid, so load BB and set main_sCurrentGnauralFilenameAndPath:
 strcpy (main_sCurrentGnauralFilenameAndPath, filename);
 main_LoadBinauralBeatSoundEngine ();

 //Loaded BB engine, so can get schedule fingerprint for file:
 GN_Running_GetScheduleFingerprint ();
 GN_DBGOUT_INT ("Full Schedule Fingerprint:", GN_ScheduleFingerprint);

 //if we have a GUI, might as well do this here:
 if (main_gnaural_guiflag == TRUE)
 {
  main_SetGraphToggle (main_drawing_area);
  main_UpdateGUI_FileInfo (filename);
  VG_DestroyTreeView ();        //20100624
  main_UpdateGUI_Voices (main_vboxVoices);
  main_OnButton_Stop (NULL);
 }
 return 0;
}

///////////////////////////////////////////////
//this is only called by ParserXML; don't call it has one job: 
//to count the number of voices and number of datapoints
void gxml_XMLParser_counter (const gchar * CurrentElement,      //this must always be a valid string
                             const gchar * Attribute,   //beware: this will equal NULL if there are none
                             const gchar * Value)       //beware: this will equal NULL to announce end of CurrnetElement
{
 //See if this is the end of CurrentElement:
 if (Value == NULL)
 {
  if (0 == strcmp (CurrentElement, "entry"))
  {
   ++gxml_ParserXML_EntryCount;
  }
  else if (0 == strcmp (CurrentElement, "voice"))
  {
   ++gxml_ParserXML_VoiceCount;
  }
  return;
 }
}
