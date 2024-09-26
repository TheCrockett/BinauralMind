/*
   gnauralnet_running.c
   This should be the only place BinauralBeat.h variables are accessed 
   The point of this file is to contain only things that need BB vars.
   Depends on:
   gnauralnet.h
   gnauralnet_clocksync.c
   gnauralnet_lists.c
   gnauralnet_main.c
   gnauralnet_socket.c

   Copyright (C) 2008 Bret Logan

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

#include "gnauralnet.h"
#include "BinauralBeat.h"

////////////////////////////////////////
//this prepares various local vars that will be loaded as
//outgoing data in gnauralnet_main
//Gets called by GN_Time_ProcessOutgoingData
void GN_Running_ProcessOutgoingData (GN_Friend * curFriend)
{
 if (NULL == curFriend)
 {
  return;
 }
 GN_My.CurrentSampleCount = BB_CurrentSampleCount;
 if (0 > BB_LoopCount)
 {
  GN_My.LoopCount = -1;
 }
 else
 {
  if (0x7fff > BB_LoopCount)
  {
   GN_My.LoopCount = BB_LoopCount;
  }
  else
  {
   GN_My.LoopCount = 0x7fff;
  }
 }
}

////////////////////////////////////////
//Gets called by GN_Time_ProcessIncomingData
void GN_Running_ProcessIncomingData (GN_Friend * curFriend)
{
 if (NULL == curFriend)
 {
  return;
 }
 /*
    this will need to be in eventually:
    if (0 == GN_My.RunningID || curFriend->RunningID != GN_My.RunningID)
    {
    return;
    }
  */
 //  unsigned int CurrentSampleCount;
 //  int LoopCount;
 if (TRUE == BB_InCriticalLoopFlag)
 {
  GN_DBGOUT ("Not updating; BB in critical loop");
  return;
 }

 if (curFriend->ScheduleFingerprint != GN_My.ScheduleFingerprint)
 {
  GN_DBGOUT_INT ("NOT Same schedule:", curFriend->ScheduleFingerprint);
  GN_DBGOUT_INT ("               Me:", GN_My.ScheduleFingerprint);
 }
 else
 {
  GN_DBGOUT_INT ("Same schedule:", GN_My.ScheduleFingerprint);
 }

 //need to actually implement this:
 if (curFriend->RunningID != GN_My.RunningID)
 {
  GN_DBGOUT_INT ("NOT Same running:", curFriend->RunningID);
  GN_DBGOUT_INT ("              Me:", GN_My.RunningID);
 }
 else
 {
  GN_DBGOUT_INT ("Same running:", GN_My.RunningID);
 }

 //Now see if incoming data has seniority over mine:
 curFriend->CurrentSampleCount_diff = BB_CurrentSampleCount -
  curFriend->CurrentSampleCount;
 if (curFriend->RunningSeniority >
     (curFriend->Time_Recv.tv_sec - GN_My.RunningStartTime.tv_sec))
 {
  GN_DBGOUT_INT ("I Follow, sample diff:",
		 curFriend->CurrentSampleCount_diff);
  BB_CurrentSampleCount = curFriend->CurrentSampleCount;
 }
 else
 {
  GN_DBGOUT_INT ("I Lead, sample diff:", curFriend->CurrentSampleCount_diff);
 }
}

////////////////////////////////////////////////////////////////
//A sort of low-grade PRNG seeded from major Schedule data features,
//in order to give a way to always return the same arbitrary number 
//for a given schedule. 65k possibilities.
unsigned int GN_Running_GetScheduleFingerprint ()
{
 unsigned short sum;
 unsigned int i;
 unsigned int j;

 sum = (unsigned short) 0xffff & (unsigned long)
  (BB_Loops * BB_VoiceCount * BB_TotalDuration);

 for (i = 0; i < BB_VoiceCount; i++)
 {
  for (j = 0; j < BB_Voice[i].EntryCount; j++)
  {
   ++sum;
   sum += (unsigned short) 0xffff & (unsigned long)
    ((1 + BB_Voice[i].type) *
     BB_Voice[i].Entry[j].duration *
     BB_Voice[i].Entry[j].basefreq_start *
     BB_Voice[i].Entry[j].beatfreq_start_HALF);
  }
 }

 GN_ScheduleFingerprint = sum;
 return GN_ScheduleFingerprint;
}

/*
   //NOTE: THIS WAS NICE... except turns out that different endline
   /chars, etc., made it not valid across platforms.
   ////////////////////////////////////////////////////////////////
   //Basically just a PRNG based on the Adler-32 algo: a given seed will
   //always return the same arbitrary number for a given file.
   //Seed can be any unsigned int range value, but 0 seems a weak
   //choice (since  fingerprint wouldn't really begin until a non-zero 
   //input occurred, slightly reducing possible outcomes related to filesize).
   //This should be run ONLY when a new file is opened (not to, say, check a 
   //file). Also, only half of this number is sent over network (because
   //I can't imagine distinguishing between more than a hundred files).
   unsigned int GN_Running_GetScheduleFingerprint (char *filename, unsigned int seed)
   {
   const unsigned int BASE = 65521U;    // largest prime smaller than 65536 
   unsigned int sum;
   unsigned int file_len = 0;
   FILE *stream;

   if ((stream = fopen (filename, "rb")) == NULL)
   {
   printf ("Couldn't open requested file \"%s\"\n", filename);
   return 0;
   }

   // split to make Adler32 component sums:
   sum = (seed >> 16) & 0xffff;
   seed &= 0xffff;

   while (feof (stream) == 0)
   {
   seed += (unsigned char) fgetc (stream);
   if (seed >= BASE)
   seed -= BASE;
   sum += seed;
   if (sum >= BASE)
   sum -= BASE;
   ++file_len;
   }

   fclose (stream);
   printf ("Measured size of file (bytes): %u\n", file_len);
   GN_ScheduleFingerprint_FULL = seed | (sum << 16);
   return GN_ScheduleFingerprint_FULL;
   }
 */
