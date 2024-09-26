/*
   gnauralnet_clocksync.c
   Core methods and variables for common timing on the GnauralNet network 
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

#include "gnauralnet.h"

////////////////////////////////////////
//This returns the number of microsecs (usecs) since last call;
//IMPORTANT: first call will return garbage, so it is 
//STRONGLY recommended to make that first call before anything
//critical.
unsigned int GN_Time_ElapsedSinceLastCall ()
{
 static GTimeVal lasttime = { 0, 0 };
 unsigned int result;
 GTimeVal curtime;

 g_get_current_time (&curtime);
 result =
  (((curtime.tv_sec - lasttime.tv_sec) * 1e6) +
   (curtime.tv_usec - lasttime.tv_usec));
 memcpy (&lasttime, &curtime, sizeof (GTimeVal));

 return result;
}

////////////////////////////////////////
//Update data for the friend's entry with this incoming Time data.
//returns GNAURALNET_SUCCESS if time info was legit (PacketID, etc.),
//GNAURALNET_FAILURE if not.
int GN_Time_ProcessIncomingData (GN_Friend * curFriend)
{
 if (NULL == curFriend)
 {
  return GNAURALNET_FAILURE;
 }

 //do stuff that gets recorded even if packet gets discarded:
 g_get_current_time (&(curFriend->Time_Recv));

 //make sure this wasn't first packet between us, or that
 //friend is remembering me from before I restarted:
 if (2 > curFriend->RecvCount)
 {
  GN_DBGOUT ("Haven't known Friend long enough to share Time");
  return GNAURALNET_FAILURE;
 }

 curFriend->Time_PacketRoundTrip =
  (((curFriend->Time_Recv.tv_sec - curFriend->Time_Send.tv_sec) * 1e6) +
   (curFriend->Time_Recv.tv_usec - curFriend->Time_Send.tv_usec));

 GN_DBGOUT_INT ("PacketRoundTrip time:", curFriend->Time_PacketRoundTrip);

 if (0 == curFriend->Time_AverageOffset)
 {
  curFriend->Time_AverageOffset = 0.5 * curFriend->Time_PacketRoundTrip;
  GN_DBGOUT_DBL ("New friend, starting Offset at:",
		 curFriend->Time_AverageOffset);
 }
 else
 {
  curFriend->Time_AverageOffset =
   (curFriend->Time_AverageOffset - (0.1 * curFriend->Time_AverageOffset)) +
   (0.05 * curFriend->Time_PacketRoundTrip);
  GN_DBGOUT_DBL ("Offset from Friend:", curFriend->Time_AverageOffset);
 }

 //do the BinauralBeat stuff if GN_STANDALONE is not defined:
 GN_Running_ProcessIncomingData (curFriend);
 return GNAURALNET_SUCCESS;
}

////////////////////////////////////////
//Give this the friend in question for this incoming Time data.
//returns GNAURALNET_SUCCESS if time info was legit (PacketID, etc.),
//GNAURALNET_FAILURE if not.
void GN_Time_ProcessOutgoingData (GN_Friend * curFriend)
{
 if (NULL == curFriend)
 {
  return;
 }
 
 //++curFriend->PacketID;       //uchar, so no need to net-order
 //curFriend->PacketID_expected = curFriend->PacketID + 1;      //uchar, so no need to net-order

 g_get_current_time (&(curFriend->Time_Send));

 //20080221: this segment is meaningless now:
 if (0 != curFriend->RecvCount)
 {
  curFriend->Time_DelayOnMe =
   ((curFriend->Time_Send.tv_sec - curFriend->Time_Recv.tv_sec) * 1e6) +
   (curFriend->Time_Send.tv_usec - curFriend->Time_Recv.tv_usec);
  GN_DBGOUT_UINT ("Delay on Me:", curFriend->Time_DelayOnMe);
 }

 //do the BinauralBeat stuff if GN_STANDALONE is not defined:
 GN_Running_ProcessOutgoingData (curFriend);
}

////////////////////////////////////////////////////////////////
//Called by anything that breaks continuity of a runing schedule.
//Like FF, RW, STOP, PAUSE, engine-reloading, etc.
void GN_Time_ResetSeniority ()
{
 g_get_current_time (&(GN_My.RunningStartTime));
}

////////////////////////////////////////////////////////////////
//The following was provided so that apps not requiring BB could
//be run; it was done by putting all calls that need BB vars in 
//gnauralnet_running.c
#ifdef GN_STANDALONE
////////////////////////////////////////
//THIS IS A DUMMY FUNCTIONl see gnauralnet_running.c for real one
void GN_Running_ProcessIncomingData (GN_Friend * curFriend)
{
 if (NULL == curFriend)
 {
  return;
 }

 if (0 == GN_My.RunningID || curFriend->RunningID != GN_My.RunningID)
 {
  return;
 }
 //  unsigned int CurrentSampleCount;
 //  int LoopCount;
}

////////////////////////////////////////
//THIS IS A DUMMY FUNCTION see gnauralnet_running.c for real one
void GN_Running_ProcessOutgoingData (GN_Friend * curFriend)
{
 if (NULL == curFriend)
 {
  return;
 }

 if (0 == GN_My.RunningID || curFriend->RunningID != GN_My.RunningID)
 {
  return;
 }
 //  unsigned int CurrentSampleCount;
 //  int LoopCount;
}
#endif
