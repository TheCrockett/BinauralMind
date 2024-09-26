/*
   exportAudio.c
   code for writing audio files
   Depends on:
   exportAudio.h
   BinauralBeat.h

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

#include <sndfile.h>
#include "BinauralBeat.h"

////////////////////////////////////
//Typical major format types might be:
//SF_FORMAT_WAV
//SF_FORMAT_FLAC
//SF_FORMAT_AIFF
//SF_FORMAT_AU
//SF_FORMAT_RAW
//SF_FORMAT_OGG
//But the SubTypes can only be one of these:
//SF_FORMAT_PCM_16 
//SF_FORMAT_VORBIS [only used with SF_FORMAT_OGG majortype]

int exportAudioToFile (char *filename, int format)
{
 const int EA_FRAMES_TO_WRITE = 16;
 const int EA_BYTES_TO_FILL = EA_FRAMES_TO_WRITE * 4;
 SNDFILE *file;
 SF_INFO sfinfo;
 int *buffer[EA_FRAMES_TO_WRITE];

 sfinfo.samplerate = BB_AUDIOSAMPLERATE;
 sfinfo.frames =
  (unsigned int) (BB_Loops * BB_TotalDuration * BB_AUDIOSAMPLERATE + 0.5);
 sfinfo.channels = 2;
 sfinfo.format = format;
 sfinfo.sections = 1;
 sfinfo.seekable = 0;

 if (!(file = sf_open (filename, SFM_WRITE, &sfinfo)))
 {
  printf ("Unexpected Error : Not able to write that format, sorry.\n");
  return 1;
 };

 BB_WriteStopFlag = 0;

 BB_Reset ();   //make sure we start at absolute beginning

 while (!(BB_InfoFlag & BB_COMPLETED) && FALSE == BB_WriteStopFlag)
 {
  BB_MainLoop (buffer, EA_BYTES_TO_FILL);
  sf_writef_short (file, (short *) buffer, EA_FRAMES_TO_WRITE);
 }

 sf_close (file);
 return 0;
}
