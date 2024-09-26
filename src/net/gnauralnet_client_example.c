/*
   gnauralnet_client_example.c
   Copyright (C) 2009  Bret Logan

   Basically, this just starts GnauralNet (which by design will
   automaticaly start befriending incomers), then waits in a loop until
   informed that GnauralNet had been halted (by a Ctrl-C). Use as
   an example for building your own GnauralNet clients.

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

/////////////////////////////////////////////////////////////
void main_InterceptCtrl_C (int sig)
{
 GN_DBGOUT ("Caught Ctrl-C");
 GN_stop ();    //since this is unthreaded, this waits unnecessarily
}

////////////////////////////////////////
//call like "GN_client_example 127.0.0.1"
int main (int argc, char **argv)
{
 //Make zero to avoid all the debugging junk:
 GN_DebugFlag = 1;

 //Trap Ctrl-C:
 if (signal (SIGINT, SIG_IGN) != SIG_IGN)
 {
  signal (SIGINT, main_InterceptCtrl_C);
 }

#ifdef GNAURALNET_GTHREADS
 GN_DBGOUT ("Starting threaded approach");
 g_thread_init (NULL);
 gdk_threads_init ();   // Called to initialize internal mutex "gdk_threads_mutex".
#endif

 if (GNAURALNET_SUCCESS != GN_start (NULL, NULL))
 {
  return GNAURALNET_FAILURE;
 }

 if (argc < 3)
 {
  g_print
   ("Normally you would specify the IP and port of another gnauralnet_client_example:\n");
  g_print (" gnauralnet_client_example 127.0.0.1 7141\n");
  g_print
   ("Since you did not, gnauralnet_client_example will start friendless\n");
 }
 else
 {
  //Ask for an invitation from this remote friend:
  GN_FriendList_Invite (inet_addr (argv[1]), htons (atoi (argv[2])));
  GN_DBGOUT ("Sent request for invitation to:");
  GN_PrintAddressInfo (GNAURALNET_UNKNOWN, inet_addr (argv[1]),
                       htons (atoi (argv[2])));
 }

 GN_Time_ElapsedSinceLastCall ();       //just to initialize this function

 //Get rid of everything between START and END if using this code in a real client
 //START
#ifdef GNAURALNET_GTHREADS
 while (GNAURALNET_STOPPED != GN_My.State)
 {
  g_usleep (500000);
 }
#else
 //the biggie:
 GN_MainLoop (NULL);
#endif

 GN_My.State = GNAURALNET_STOPPED;      //important
 GN_DBGOUT ("Stopped gnauralnet loop");
 GN_DBGOUT ("Goodbye");
 //END
 return GNAURALNET_SUCCESS;
}
