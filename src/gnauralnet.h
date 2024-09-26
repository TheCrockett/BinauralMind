/*
   gnauralnet.h
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

/*
 * This is the first attempt at GnauralNet, for now basically a hack 
 * to allow Gnaural to "talk" via TCP/IP sockets other programs. The
 * main approach is simply to assume all communicating programs are 
 * using copies of the same Schedule file, thus the only thing that 
 * needs to be shared is the CurrentSampleCount within the running of that 
 * schedule, plus enough general timing data to estimate clock offset.
 * 
 * */

//gnauralnet.h: the stuff that gnaural_*.c files will need
#ifndef _GNAURALNET_H_
#define _GNAURALNET_H_

//First the stuff that all platforms get:
#include <glib.h>
#include <gdk/gdk.h>    //needed for gdk_threads_init()
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <signal.h>     //needed for SIGINT

//Now the stuff that is platform dependent:
#ifdef GNAURAL_WIN32
//WIN32:
#include <w32api.h>
#include <windows.h>
#include <winsock.h>
#include <lmcons.h>
#define GNAURAL_CLOSE_SOCKET(socket) closesocket (socket)
#define GNAURAL_IS_SOCKET_ERROR(status) ((status) == SOCKET_ERROR)
#define GNAURAL_IS_INVALID_SOCKET(socket) ((socket) == INVALID_SOCKET)
#define GNAURAL_IS_CONNECT_STATUS_INPROGRESS() (WSAGetLastError () == WSAEWOULDBLOCK)
#else
//UNIX:
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>
#define SOCKET int
#define GNAURAL_CLOSE_SOCKET(socket) close (socket)
#define GNAURAL_IS_SOCKET_ERROR(status) ((status) == -1)
#define GNAURAL_IS_INVALID_SOCKET(socket) ((socket) < 0)
#define GNAURAL_IS_CONNECT_STATUS_INPROGRESS() (errno == EINPROGRESS)
#endif

/* Choosing port numbers: 
 * Port numbers 1-1023 are normally reserved for privilegded 
 * processes. These ports cannot be assigned to a local end 
 * of an TCP/IP connection by a normal process. Port numbers 
 * 1024-4999 are used as so called ephemeral port numbers. 
 * These port numbers are assigned automatically by TCP and 
 * UDP if no specific port number was requested. Therefore, 
 * any normal user service should be mapped to port numbers 
 * greater or equal than 5000.
 */
//g=7 n=14 a=1 u=21 r=18 a=1 l=12
#define GNAURALNET_PORT 7141    //Default port Gnauralnet uses; arbitrary
#define GN_MISSED_RECV_LIMIT 6
#define GNAURALNET_VERSION 0
#define GNAURALNET_FAILURE -1
#define GNAURALNET_SUCCESS 0
#define GNAURALNET_TRUE 1
#define GNAURALNET_FALSE 0
#define GNAURALNET_UNKNOWN 0    //KEEP ZERO, to avoid ntohl/htonl confusion
#define GN_MainLoopInterval_secs 1      //sets the send interval
#define GN_MainLoopInterval_usecs 0     //fractional part of send interval;NEVER >= 1e6

//###################
//Uncomment this one to make all the gnauralnet_* code run without Gnaural:
//#define GN_STANDALONE
#define GNAURALNET_GTHREADS
//###################

//these are the states gnauralnet can be in:
enum
{
 GNAURALNET_RUNNING,
 GNAURALNET_STOPPED,
 GNAURALNET_WAITINGTOSTOP
};

//these are the sorts of data a client can request as msgtype:
enum
{
 GN_MSGTYPE_TIMEINFO,
 GN_MSGTYPE_PHASEINFO,
 GN_MSGTYPE_EMPTY,
 GN_MSGTYPE_END
};

///////////
//START: Functions and variable users will want
extern int GN_start (int (*main_loop_function) (void *ptr), void (*GN_ProcessIncomingData) (char *MsgBuf, int sizeof_MsgBuf, struct sockaddr_in * saddr_remote));       //all you need to start gnauralnet
extern void GN_stop ();         //all you need to stop gnauralnet

//user creates the following 2; see examples in gnauralnet.c:
extern int (*GN_MainLoop) (void *ptr);  // this gets set to whatever the user wants
extern int GN_MainLoop_default (void *arg);     // the built-in Main Loop, used if GN_start (NULL)
extern void (*GN_ProcessIncomingData) (char *MsgBuf, int sizeof_MsgBuf,
                                       struct sockaddr_in * saddr_remote);
extern void GN_ProcessIncomingData_default (char *MsgBuf, int sizeof_MsgBuf,
                                            struct sockaddr_in *saddr_remote);

//Socket functions:
extern int GN_Socket_RecvMessage (SOCKET sok, char *MsgBuf, int sizeof_MsgBuf,
                                  unsigned int secs, unsigned int usecs);
extern int GN_Socket_SendMessage (SOCKET sok, char *MsgBuf, int sizeof_MsgBuf,
                                  unsigned int IP, unsigned short port);
extern SOCKET GN_Socket_MakeUDP (int temp_port);        //set to non-blocking 
extern void GN_Socket_SetBlocking (SOCKET sok, int yesflag);
extern int GN_Socket_IsReadyForRead (int sd, unsigned int secs,
                                     unsigned int usecs);
extern int GN_Socket_SetLocalInfo ();   //sets GN_My_IP, GN_My_Port, GN_My_ID

//END: Functions and variable users will want
/////////////////////////////////////////////

//////////////////////////////////////////////////////////////
//this is the basis for our Friend linked list.
typedef struct GN_Friend_type
{
 //First the stuff that only gets set upon initial meeting:
 unsigned int ID;               //Random Number, already in network order
 unsigned int IP;               //IP sockaddr_in.sin_addr.s_addr, already in network order
 unsigned short Port;           //already in network order
 GTimeVal TimeOfDiscovery;      //local timestamp of when friend was met

 //Next the stuff that gets set every receive from friend:
 unsigned int FriendCount;      //how many friends the friend knows
 unsigned int RecvCount;
 GTimeVal Time_Recv;            // my clock
 //unsigned int Time_DelayOnFriend;     //time (usecs) between friend recv'ing my msg to sending theirs
 unsigned int Time_PacketRoundTrip;     //round trip time (usecs) packed was actually on net from my send to friend's reply
 unsigned short RunningID;      //will be 0 if not Running
 unsigned short ScheduleFingerprint;    //will be 0 if not Running
 unsigned int RunningSeniority; //secs participating in this Running; used to determine who has most "right" samplecount
 double Time_AverageOffset;     //
 unsigned int CurrentSampleCount;
 unsigned int CurrentSampleCount_diff;
 int LoopCount;

 //Next the stuff that gets set on sends:
 unsigned int SendCount;
 GTimeVal Time_Send;            //time I sent (my clock)
 unsigned int Time_DelayOnMe;   //time from recv to my send in usecs; so far not really used internally

 //Next the stuff that gets set on both send/recv:
 int SendRecvTally;             //incremented on send, zeroed on recv
 //unsigned char PacketID;      //incremented every send, and set every recv -- by both friends. To have a reference to be sure packets are in-order
 //unsigned char PacketID_expected;     //will always be one-more than PacketID;  every send, and set every recv -- by both friends. To have a reference to be sure packets are in-order
 //char PacketID_reliability;   //keeps track of PacketID record -- incremented on correct receives, decremented on errors, limited at +/-127

 //Finally, the stuff needed to make a linked list:
 struct GN_Friend_type *NextFriend;
 struct GN_Friend_type *PrevFriend;
} GN_Friend;

//These are only needed internally or general utility:
extern int GN_init ();          //called by GN_start()
extern void GN_cleanup ();      //called by GN_stop()
extern void GN_sleep (unsigned int microsecs);
extern void GN_FriendList_DeleteFriend (GN_Friend * curFriend);
extern void GN_FriendList_DeleteAll ();
extern void GN_FriendList_Invite (unsigned int IP,      //MUST BE IN NET ORDER
                                  unsigned short Port); //MUST BE IN NET ORDER)
extern GN_Friend *GN_FriendList_AddFriend (unsigned int ID,     //MUST BE IN NET ORDER
                                           unsigned int IP,     //MUST BE IN NET ORDER
                                           unsigned short Port);        //MUST BE IN NET ORDER
extern GN_Friend *GN_FriendList_GetFriend (unsigned int ID,     //MUST BE IN NET ORDER
                                           unsigned int IP,     //MUST BE IN NET ORDER
                                           unsigned short Port);        //MUST BE IN NET ORDER
extern unsigned int GN_FriendList_FriendCount ();
extern GN_Friend *GN_FriendList_NextFriend ();
extern GN_Friend *GN_FriendList_RandomFriend (GN_Friend * excludeFriend);       //excludeFriend can be anything 
extern void GN_ProcessOutgoingData (GN_Friend * curFriend);     //a convenience 
extern void GN_ProcessInviteData (unsigned int InviteID,        //MUST BE IN NET ORDER
                                  unsigned int InviteIP,        //MUST BE IN NET ORDER
                                  unsigned short InvitePort);   //MUST BE IN NET ORDER
extern GN_Friend *GN_ProcessNewFriend (unsigned int ID, //MUST BE IN NET ORDER
                                       unsigned int IP, //MUST BE IN NET ORDER
                                       unsigned short Port);    //MUST BE IN NET ORDER)
extern unsigned int GN_Time_ElapsedSinceLastCall ();
extern void GN_Running_ProcessIncomingData (GN_Friend * curFriend);     //does the BB processing
extern void GN_Running_ProcessOutgoingData (GN_Friend * curFriend);     //does the BB processing
extern unsigned int GN_Running_GetScheduleFingerprint ();       //ONLY RUN AFTER BB IS LOADED
extern void GN_PrintAddressInfo (unsigned int id, unsigned int ip,
                                 unsigned short port);
extern void GN_PrintFriends ();
extern void GN_PrintPacketContents ();
extern void GN_Time_ResetSeniority ();

//From gnauralnet_clocksync.c:
extern int GN_Time_ProcessIncomingData (GN_Friend * curFriend);
void GN_Time_ProcessOutgoingData (GN_Friend * curFriend);

//From gnauralnet_gui.c:
void GN_ShowInfo ();

//Global variables:
unsigned short GN_ScheduleFingerprint;
typedef struct GN_Globals_type
{
 int State;                     //DO NOT MOVE THIS; it gets initialized in gnauralnet_main
 short LoopCount;               //BB data
 unsigned int IP;               //IS IN NET ORDER
 unsigned int ID;               //random IS IN NET ORDER
 unsigned short FriendCount;    //holds snapshots/estimates of current number of friends in linked list
 unsigned short LoverCount;     //holds snapshots/estimates of current number of lovers in linked list
 unsigned short RunningID;      //0 if not in a running 
 unsigned short ScheduleFingerprint;    //gets set whenever a file is opened by Gnaural
 GTimeVal RunningStartTime;
 unsigned int CurrentSampleCount;       //BB data
 unsigned short Port;           //IS IN NET ORDER
 SOCKET Socket;                 //the one socket used by Gnaural
 GN_Friend *FirstFriend;
} GN_Globals;
extern GN_Globals GN_My;
extern int GN_DebugFlag;

//END: Functions and variable users will want
///////////

//////////////////////////////////////////////////////////////////////////
//This union holds all the kinds of data friends will share
//NOTE: compilers apparently don't necessarily pack structs
//the same way (aside from platform endianess -- which is another issue).
//TO AVOID ALL PROBLEMS, *ALWAYS* pack data in dword (32-bit/4-byte/int) sizes,
//because Linux always will anyway. . To illustrate, a struck like this:
// char, char, char, char, int
//or 
// int, char, char, char, char
//will be 8 bytes in size, while a struct like this:
// char, int, char, char, char
//will be 12 bytes. Yuck!
//BTW, one like this:
// char
//will be 1 byte, and one like this:
// char, char
//will be two bytes. 
//IMPORTANT NOTE: remember that anything bigger than a char 
//MUST be htonx for sending then ntohx when received:
typedef union
{
 unsigned char msgtype;         //first uchar is ALWAYS message type
 struct TimeInfo
 {
  unsigned char msgtype;        //first uchar is ALWAYS message type
  unsigned char empty;          //struct padding, can be for future use
  unsigned short InvitePort;    //Port for the InviteIP
  unsigned int InviteIP;        //IP address of a potential new friend
  unsigned int InviteID;        //ID of the Invite - only used to check for friend, since IDs aren't sent in Invites
  unsigned int ID;              //A pseudorandom number I call my ID
  unsigned int Time_DelayOnMe;  //time (usecs) from when I recived Friend's msg to sending my own back
  unsigned int CurrentSampleCount;
  unsigned short FriendCount;
  unsigned short RunningSeniority;      //secs participating in this Running
  unsigned short RunningID;     //will be 0 if not Running 
  unsigned short ScheduleFingerprint;   //just the lower-half of the uint
  short int LoopCount;
 } TimeInfo;
} GN_CommBuffer;

//Two are created because with one, overlapping/interleaved
//sends and recvs polluted each other:
extern GN_CommBuffer GN_SendBuffer;
extern GN_CommBuffer GN_RecvBuffer;

//////////////////////////////////////////////////////////////
extern int GN_DebugFlag;        //make !=0 to spit out debug info

#define GN_DBGFLAG if (0 != GN_DebugFlag)
#define GN_DBGOUT_STR(a,b)            GN_DBGFLAG  fprintf(stderr,"%s %d: %s %s\n",__FILE__,__LINE__,a, b);
#define GN_DBGOUT_INT(a,b)            GN_DBGFLAG  fprintf(stderr,"%s %d: %s %d\n",__FILE__,__LINE__,a, b);
#define GN_DBGOUT_UINT(a,b)           GN_DBGFLAG  fprintf(stderr,"%s %d: %s %u\n",__FILE__,__LINE__,a, b);
#define GN_DBGOUT_DBL(a,b)            GN_DBGFLAG  fprintf(stderr,"%s %d: %s %g\n",__FILE__,__LINE__,a, b);
#define GN_DBGOUT_PNT(a,b)            GN_DBGFLAG  fprintf(stderr,"%s %d: %s %p\n",__FILE__,__LINE__,a,b);
#define GN_DBGOUT(a)                  GN_DBGFLAG  fprintf(stderr,"%s %d: %s\n",__FILE__,__LINE__,a);
#define GN_DBG()                      GN_DBGFLAG  fprintf(stderr,"%s %d\n",__FILE__,__LINE__);
#define GN_ERROUT(a)                  GN_DBGFLAG  fprintf(stderr,"%s %d: #Error# %s\n",__FILE__,__LINE__,a);

#endif
