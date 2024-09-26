/*
   gnauralnet_main.c
   Central code to achieve cooperative clock sync'ing between peers on a network
   Depends on:
   gnauralnet.h
   gnauralnet_clocksync.c
   gnauralnet_lists.c
   gnauralnet_main.c
   gnauralnet_socket.c

   Copyright (C) 2009  Bret Logan

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

/////////////////////////////////////////////
//User creates these two:
//extern int GN_ProcessIncomingData ();
//extern int GN_MainLoop ();

////////////////////////////////////////
/*
   GN_start([user function]) and GN_stop() are all user must run, then
   for user function set up a Main Loop like this:

   while (GNAURALNET_RUNNING == GN_My.State)
   {
   GN_Socket_RecvMessage ([time to wait for data]);
   GN_ProcessOutgoingData (all the friends);//optional
   }
   GN_My.State = GNAURALNET_STOPPED;
   return GNAURALNET_SUCCESS;
   }

   to wait for recv's from GN_Socket_RecvMessage(), handling incoming/outgoing/
   invite/timing data with GN_Process**() functions
   NOTE: if GNAURALNET_GTHREADS is set, GN_start() starts a thread
   that stays in an internal loop calling GN_recv*Buffer(). If not set,
   user needs to call GN_recv*Buffer() in their own loop like
   while "(0 == GN_My.State) {}" until GN_stopped breaks it.

   To compile with the MinGW gcc cross compiler, be sure you have gnauralnet.h:
   i586-mingw32msvc-g++ -g -O2 -o gnauralnet.exe -mms-bitfields -mwindows \
   -I/usr/i586-mingw32msvc/include/glib-2.0 \
   -I/usr/i586-mingw32msvc/lib/glib-2.0/include \
   -DGNAURAL_WIN32 gnauralnet_main.c gnauralnet_clocksync.c gnauralnet_socket.c \
   gnauralnet_lists.c -lwsock32 

   You should strip it too:
   i586-mingw32msvc-strip gnauralnet.exe
 */
//VERSION INFO:
//This is the first

#include "gnauralnet.h"

//Global variables:
//this is the struct holding all major single-instance kinds of globals:
GN_Globals GN_My = { GNAURALNET_STOPPED };

int GN_DebugFlag = 0;
GN_CommBuffer GN_RecvBuffer;
GN_CommBuffer GN_SendBuffer;
unsigned short GN_ScheduleFingerprint = 0;
//If user doesn't set these next two manually in GN_start (), they 
//default to internals functions:
int (*GN_MainLoop) (void *ptr) = NULL;  //defaults to GN_MainLoop_default
void (*GN_ProcessIncomingData) (char *MsgBuf, int sizeof_MsgBuf, struct sockaddr_in * saddr_remote) = NULL;     //defaults to GN_ProcessIncomingData_default

///////////////////////////////////////////////////////////
//pass NULL to this and it just uses the built-in Main Loop, GN_MainLoop_default ()
int GN_start (int (*main_loop_func) (void *ptr),
              void (*incoming_data_func) (char *MsgBuf, int sizeof_MsgBuf,
                                          struct sockaddr_in * saddr_remote))
{
 if (NULL == main_loop_func)
 {
  GN_MainLoop = GN_MainLoop_default;
 }
 else
 {
  GN_MainLoop = main_loop_func;
 }

 if (NULL == incoming_data_func)
 {
  GN_ProcessIncomingData = GN_ProcessIncomingData_default;
 }
 else
 {
  GN_ProcessIncomingData = incoming_data_func;
 }

 GN_DBGOUT_INT ("Size of GN_CommBuffer:", sizeof (GN_CommBuffer));
 //init network:
 if (GN_init () != 0)
 {
  perror ("Init Network Failed\n");
  GN_My.State = GNAURALNET_STOPPED;
  return GNAURALNET_FAILURE;
 }
 GN_My.State = GNAURALNET_RUNNING;
#ifdef GNAURALNET_GTHREADS
 GThread *GN_serverthread = NULL;
 GError *thread_err = NULL;

 if (NULL ==
     (GN_serverthread =
      g_thread_create ((GThreadFunc) GN_MainLoop, (void *) NULL, FALSE,
                       &thread_err)))
 {
  GN_ERROUT ("g_thread_create failed:");
  GN_ERROUT (thread_err->message);
  g_error_free (thread_err);
  return GNAURALNET_FAILURE;
 }
 else
 {
  GN_DBGOUT ("Gnauralnet server thread created successfully");
  return GNAURALNET_SUCCESS;
 }
#endif
 //at this point, users needs to set up their own send/recv loop, akin to 
 //the threaded GN_MainLoop ((void *) NULL) example.
 return GNAURALNET_SUCCESS;
}

/////////////////////////////////////////////////////////////
//This is the default internal one; user can create their own
//by setting 
int GN_MainLoop_default (void *arg)
{
 GN_DBGOUT ("Entering GnauralNet MainLoop ()...");
 while (GNAURALNET_RUNNING == GN_My.State)
 {
  //Receive and process packets for GN_MainLoopInterval_usecs time:
  GN_Socket_RecvMessage (GN_My.Socket, (char *) &GN_RecvBuffer,
                         sizeof (GN_RecvBuffer), GN_MainLoopInterval_secs,
                         GN_MainLoopInterval_usecs);

  //Send message to ONE friend on the FriendList if there are any:
  //NOTE: the do-loop is just to be sure a friend still gets hit even 
  //      if defriending occurs. 
  int RepeatFlag;
  GN_Friend *curFriend;

  do
  {
   RepeatFlag = GNAURALNET_FALSE;
   curFriend = GN_FriendList_NextFriend ();

   //see if we have no friends:
   if (NULL == curFriend)
   {
    break;      //having no friends isn't a failure!
   }

   //See if Friend has strayed enough to be defriended:
   if (GN_MISSED_RECV_LIMIT < ++(curFriend->SendRecvTally))
   {
    GN_DBGOUT ("Unresponsive, defriending:");
    GN_PrintAddressInfo (curFriend->ID, curFriend->IP, curFriend->Port);
    GN_FriendList_DeleteFriend (curFriend);
    RepeatFlag = GNAURALNET_TRUE;
   }
  }
  while (GNAURALNET_FALSE != RepeatFlag);

  //this does the send too (assuming curFriend != NULL):
  GN_ProcessOutgoingData (curFriend);
 }
 GN_My.State = GNAURALNET_STOPPED;
 return GNAURALNET_SUCCESS;
}

///////////////////////////////////////
//where incoming data gets processed
void GN_ProcessIncomingData_default (char *MsgBuf, int sizeof_MsgBuf,
                                     struct sockaddr_in *saddr_remote)
{
 //First check for the kind of data.
 //The only kind we currently expect is a GN_RecvBuffer:
 if (MsgBuf != (char *) &GN_RecvBuffer ||
     sizeof (GN_RecvBuffer) != sizeof_MsgBuf)
 {
  GN_ERROUT ("recv'd buffer not correct type");
  return;
 }

 //to debug:
 //GN_PrintPacketContents ();

 //Definitely a RecvBuffer, so can check if it is my ID:
 //NOTE: shouldn't get here.
 if (ntohl (GN_RecvBuffer.TimeInfo.ID) == GN_My.ID)
 {
  GN_ERROUT
   ("Apparently I have several IP addresses and am contacting myself!");
  return;
 }

 unsigned int ID = ntohl (GN_RecvBuffer.TimeInfo.ID);

 //first see if this is an IP/Port I've heard from before:
 //NOTE: Logically, this can prove it is a friend I know, but not
 //that it is is a friend I don't know (multiple IP issue 20080113)
 GN_Friend *curFriend =
  GN_FriendList_GetFriend (ID, saddr_remote->sin_addr.s_addr,
                           saddr_remote->sin_port);

 //we didn't know her, so see if she's a candidate for Friend:
 if (NULL == curFriend)
 {
  curFriend =
   GN_ProcessNewFriend (ID, saddr_remote->sin_addr.s_addr,
                        saddr_remote->sin_port);

  //check to see if she really got added:
  if (NULL == curFriend)
   return;
 }

 GN_DBGOUT ("Got data from:");
 GN_PrintAddressInfo (ID, saddr_remote->sin_addr.s_addr,
                      saddr_remote->sin_port);

 switch (GN_RecvBuffer.msgtype)
 {
 case GN_MSGTYPE_TIMEINFO:
  ++(curFriend->RecvCount);
  curFriend->SendRecvTally = 0;
  curFriend->ID = ntohl (GN_RecvBuffer.TimeInfo.ID);
  curFriend->FriendCount = ntohs (GN_RecvBuffer.TimeInfo.FriendCount);
  curFriend->RunningID = ntohs (GN_RecvBuffer.TimeInfo.RunningID);
  curFriend->ScheduleFingerprint =
   ntohs (GN_RecvBuffer.TimeInfo.ScheduleFingerprint);
  curFriend->RunningSeniority =
   ntohs (GN_RecvBuffer.TimeInfo.RunningSeniority);
  curFriend->CurrentSampleCount =
   ntohl (GN_RecvBuffer.TimeInfo.CurrentSampleCount);

  //do time stuff:
  GN_Time_ProcessIncomingData (curFriend);

  //check invitation and add if not already here
  GN_ProcessInviteData (ntohl (GN_RecvBuffer.TimeInfo.InviteID),
                        ntohl (GN_RecvBuffer.TimeInfo.InviteIP),
                        ntohs (GN_RecvBuffer.TimeInfo.InvitePort));
  break;

 default:
  GN_DBGOUT ("Message type NOT PROCESSED");
  break;
 }
}

/////////////////////////////////////////////////////
//This just does the generic things all outgoing packets need:
void GN_ProcessOutgoingData (GN_Friend * curFriend)
{
 if (NULL == curFriend)
  return;

 //dig up a random Friend to give receiver:
 unsigned int InviteIP = 0;
 unsigned short InvitePort = 0;
 unsigned int InviteID = 0;
 GN_Friend *randFriend = GN_FriendList_RandomFriend (curFriend);

 if (NULL != randFriend)
 {
  InviteIP = randFriend->IP;
  InvitePort = randFriend->Port;
  InviteID = randFriend->ID;
 }

 GN_DBGOUT_INT ("Current friend count:", GN_My.FriendCount);

 //Now do the critical timing stuff:
 GN_Time_ProcessOutgoingData (curFriend);

 //now put data in, setting proper byte order for network:
 GN_SendBuffer.msgtype = GN_MSGTYPE_TIMEINFO;
 GN_SendBuffer.TimeInfo.Time_DelayOnMe = htonl (curFriend->Time_DelayOnMe);
 GN_SendBuffer.TimeInfo.FriendCount = htons (GN_My.FriendCount);
 GN_SendBuffer.TimeInfo.InviteID = htonl (InviteID);
 GN_SendBuffer.TimeInfo.InviteIP = htonl (InviteIP);
 GN_SendBuffer.TimeInfo.InvitePort = htons (InvitePort);
 GN_SendBuffer.TimeInfo.ID = htonl (GN_My.ID);
 //now fill the Running data, set in GN_Running_ProcessOutgoingData:
 GN_SendBuffer.TimeInfo.RunningID = htons (GN_My.RunningID);
 GN_SendBuffer.TimeInfo.ScheduleFingerprint =
  htons (GN_My.ScheduleFingerprint);
 GN_SendBuffer.TimeInfo.RunningSeniority =
  htons (curFriend->Time_Send.tv_sec - GN_My.RunningStartTime.tv_sec);
 GN_SendBuffer.TimeInfo.CurrentSampleCount = htonl (GN_My.CurrentSampleCount);
 GN_SendBuffer.TimeInfo.LoopCount = htons (GN_My.LoopCount);

 //Now the actual send:
 GN_Socket_SendMessage (GN_My.Socket, (char *) &GN_SendBuffer,
                        sizeof (GN_SendBuffer), curFriend->IP,
                        curFriend->Port);
}

///////////////////////////////////////
//this is here simply to reduce code redundancy
GN_Friend *GN_ProcessNewFriend (unsigned int ID,        //MUST BE IN NET ORDER
                                unsigned int IP,        //MUST BE IN NET ORDER
                                unsigned short Port)    //MUST BE IN NET ORDER)
{
 GN_Friend *curFriend = NULL;

 //add her:
 GN_DBGOUT ("Adding friend from:");
 GN_PrintAddressInfo (ID, IP, Port);
 curFriend = GN_FriendList_AddFriend (ID, IP, Port);

 //check to see if she really got added:
 if (NULL == curFriend)
  return NULL;

 //send him a quick greeting, as per Issue 20080112. NOTE:
 //20080113: fixed bug here; was using the one CommBuffer to send
 //BEFORE it had filed the data it recv'd. Solution: two commbuffs.
 GN_ProcessOutgoingData (curFriend);
 return curFriend;
}

///////////////////////////////////////
//these incoming vars are in NET ORDER
//Philosophy: Invites are not added directly as friends. Instead, their 
//address is sent to, and when it responds, it gets added. This is a 
//security feature, since it would be absurdly easy to introduce crap
//into the FriendLists via addresses.
void GN_ProcessInviteData (unsigned int InviteID,       //MUST BE IN NET ORDER
                           unsigned int InviteIP,       //MUST BE IN NET ORDER
                           unsigned short InvitePort)   //MUST BE IN NET ORDER
{
 //see if it is invalid:
 if (0 == InviteIP || 0 == InvitePort)
  return;

 //see if it is ME:
 if (InviteID == GN_My.ID)
 {
  GN_DBGOUT ("Invite was to myself!");
  return;
 }

 //see if I have that friend already:
 GN_Friend *curFriend =
  GN_FriendList_GetFriend (InviteID, InviteIP, InvitePort);

 //if it is not a friend already, send her an Invite and forget about her:
 if (NULL == curFriend)
 {
  GN_FriendList_Invite (InviteIP, InvitePort);
 }
 else
 {
  //  GN_DBGOUT ("NOT adding Invited friend at:");
  //  GN_PrintAddressInfo (InviteID, InviteIP, InvitePort);
 }
}

/////////////////////////////////////////////////////////////////////
//GN_init (): inits net subsystem, creates socket, 
//sets blocking behavior
int GN_init ()
{
 memset (&GN_My, 0, sizeof (GN_Globals));       //precautionary
 GN_My.Socket = 0;      //Win32: unsigned int, UNIX: int
 GN_My.IP = 0;  //THIS IS IN NET ORDER
 GN_My.Port = 0;        //THIS IS IN NET ORDER
 GN_My.ID = 0;  //THIS IS IN NET ORDER
 GN_My.RunningID = 0;   //0 if not in a running 
 GN_My.ScheduleFingerprint = GN_ScheduleFingerprint;
 GN_My.CurrentSampleCount = 0;  //BB data, set in Running
 GN_My.LoopCount = 0;   //BB data, set in Running
 GN_My.FriendCount = 0; //holds snapshots/estimates of current number of friends in linked list
 GN_My.LoverCount = 0;  //holds snapshots/estimates of current number of lovers in linked list
 GN_My.FirstFriend = NULL;
 GN_My.State = GNAURALNET_STOPPED;
 GN_Time_ResetSeniority ();

#ifdef GNAURAL_WIN32
 // initiate use of WINSOCK.DLL
 WSADATA wsadata;
 WORD wVer = MAKEWORD (1, 1);

 if (WSAStartup (wVer, &wsadata) != 0)
 {
  GN_ERROUT ("Error occurred on WSAStartup()");
  return GNAURALNET_FAILURE;
 }
 // Ensure the WINSOCK.DLL version is right
 if (LOBYTE (wsadata.wVersion) != 1 || HIBYTE (wsadata.wVersion) != 1)
 {
  GN_ERROUT ("WINSOCK.DLL wrong version");
  return GNAURALNET_FAILURE;
 }
#endif

 //create the one socket:
 //NOTE: if port is already in use, this will fail. For now, 
 //it is assumed that two Gnaurals are trying to
 //network on same machine. This is legal, but subsequent Gnaurals
 //get any unused port and wont be known externally:
 if (GNAURAL_IS_INVALID_SOCKET
     (GN_My.Socket = GN_Socket_MakeUDP (GNAURALNET_PORT)))
 {
  perror ("Socket creation failed: ");
  GN_DBGOUT ("Going to try socket creation on a different port:");
  if (GNAURAL_IS_INVALID_SOCKET (GN_My.Socket = GN_Socket_MakeUDP (0)))
  {
   perror ("GN_Socket_MakeUDP failed again:");
   GN_ERROUT ("Failed to start Gnauralnet");
   return GNAURALNET_FAILURE;
  }
 }

 //set local info globals:
 GTimeVal curtime;

 g_get_current_time (&curtime);
 srand (curtime.tv_sec + curtime.tv_usec);
 if (GNAURALNET_FAILURE == GN_Socket_SetLocalInfo ())
 {
  GN_ERROUT ("Couldn't get local address (shouldn't happen)");
  return GNAURALNET_FAILURE;
 }

 GN_DBGOUT ("Socket Creation successful on:");
 GN_PrintAddressInfo (GN_My.ID, GN_My.IP, GN_My.Port);

 //set the socket blocking behavior:
 //NOTE: using non-blocking sockets with select() 
 //and timeouts seems to be the best choice; for one thing,
 //timeouts allow me to unblock when trying to close the socket:
 GN_Socket_SetBlocking (GN_My.Socket, 0);
 GN_DBGOUT ("Successfully started Gnauralnet");
 return GNAURALNET_SUCCESS;
}

/////////////////////////////////////////////////////////////////////
//Note the handy G_USEC_PER_SEC
void GN_sleep (unsigned int microsecs)
{
#ifdef GNAURAL_WIN32
 Sleep (microsecs / 1000);
#else
 usleep (microsecs);
#endif
}

/////////////////////////////////////////////////////////////////
//don't call this directly; call GN_stop()
void GN_cleanup ()
{
 GN_DBGOUT ("Cleaning up gnauralnet");
 GNAURAL_CLOSE_SOCKET (GN_My.Socket);
 //GN_My.Socket = -1;
#ifdef GNAURAL_WIN32
 WSACleanup ();
 //GN_My.Socket = INVALID_SOCKET;
#endif
 GN_PrintFriends ();
 GN_FriendList_DeleteAll ();
}

/////////////////////////////////////////////////////////////////
//GN_start and GN_stop are all user must run.
//this sets GN_My.State to GNAURALNET_WAITINGTOSTOP to break 
//any recv loops, then shuts down the socket 
/////////////////////////////////////////////////////////////////
void GN_stop ()
{
 if (GNAURALNET_RUNNING == GN_My.State)
 {
  GN_My.State = GNAURALNET_WAITINGTOSTOP;
  int count = 4;

  while (GNAURALNET_WAITINGTOSTOP == GN_My.State && --count)
  {
   GN_DBGOUT ("Waiting for gnauralnet shutdown...");
   GN_sleep (500000);
  }
  if (0 == count)
   GN_DBGOUT ("Forcing gnauralnet shutdown");
 }
 GN_DBGOUT ("Connection to GnauralNet was terminated");
 GN_My.State = GNAURALNET_STOPPED;
 GN_cleanup ();
}

/////////////////////////////////////////////////////
//incoming vars MUST be in net order 
void GN_PrintAddressInfo (unsigned int id, unsigned int ip,
                          unsigned short port)
{
 if (0 == GN_DebugFlag)
 {
  return;
 }
 //start debug:
 struct in_addr tmp_in_addr;

 tmp_in_addr.s_addr = ip;
 fprintf (stderr, "IP: %s \t Port: %d \t ID: %u\n", inet_ntoa (tmp_in_addr),
          ntohs (port), id);
 //end debug
}

/////////////////////////////////////////////////////
//For debugging:
void GN_PrintPacketContents ()
{
 if (0 == GN_DebugFlag)
 {
  return;
 }

 //can do either Recv or Comm:
 //GN_CommBuffer *buf = &GN_SendBuffer;
 GN_CommBuffer *buf = &GN_RecvBuffer;

 switch (buf->msgtype)
 {
 case GN_MSGTYPE_TIMEINFO:
  fprintf (stderr, "=====TimeInfo:\n");
  fprintf (stderr, " msgtype: %d\n", buf->TimeInfo.msgtype);
  fprintf (stderr, " empty: %d\n", buf->TimeInfo.empty);
  fprintf (stderr, " InvitePort: %d\n", ntohs (buf->TimeInfo.InvitePort));
  fprintf (stderr, " InviteIP: %d\n", ntohl (buf->TimeInfo.InviteIP));
  fprintf (stderr, " InviteID: %d\n", ntohl (buf->TimeInfo.InviteID));
  fprintf (stderr, " ID: %d\n", ntohl (buf->TimeInfo.ID));
  fprintf (stderr, " Time_DelayOnMe: %d\n",
           ntohl (buf->TimeInfo.Time_DelayOnMe));
  fprintf (stderr, " FriendCount: %d\n", ntohs (buf->TimeInfo.FriendCount));
  fprintf (stderr, " RunningID: %d\n", ntohs (buf->TimeInfo.RunningID));
  fprintf (stderr, " ScheduleFingerprint: %d\n",
           ntohs (buf->TimeInfo.ScheduleFingerprint));
  fprintf (stderr, " RunningSeniority: %d\n",
           ntohs (buf->TimeInfo.RunningSeniority));
  fprintf (stderr, " CurrentSampleCount: %d\n",
           ntohl (buf->TimeInfo.CurrentSampleCount));
  fprintf (stderr, " LoopCount: %d\n", ntohs (buf->TimeInfo.LoopCount));
  break;
 }
}

/////////////////////////////////////////////////////
//this is strictly a debugging tool
void GN_PrintFriends ()
{
 if (0 == GN_DebugFlag)
 {
  return;
 }

 GN_Friend *curFriend = GN_My.FirstFriend;
 struct in_addr tmp_in_addr;

 GN_My.FriendCount = 0;
 GN_My.LoverCount = 0;

 while (curFriend != NULL)
 {
  ++GN_My.FriendCount;
  if (0 != curFriend->RunningID && curFriend->RunningID == GN_My.RunningID)
  {
   ++GN_My.LoverCount;
  }
  tmp_in_addr.s_addr = curFriend->IP;
  fprintf (stderr, "%d)IP:%s\tPort:%d\tID:%u\tOffset:%g\n",
           GN_My.FriendCount, inet_ntoa (tmp_in_addr),
           ntohs (curFriend->Port), curFriend->ID,
           curFriend->Time_AverageOffset);
  //all list passes need this:
  curFriend = curFriend->NextFriend;
 }
}
